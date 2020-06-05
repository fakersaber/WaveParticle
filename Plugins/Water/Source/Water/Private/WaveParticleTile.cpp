// Fill out your copyright notice in the Description page of Project Settings.


#include "WaveParticleTile.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "WaterMeshComponent.h"


#define _LOD1 0
#define _LOD2 1

// Sets default values
AWaveParticleTile::AWaveParticleTile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	WaveMesh = CreateDefaultSubobject<UWaveMeshComponent>(TEXT("WaveMesh"));

	//ProceduralWaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralWaveMesh"));
}

// Called when the game starts or when spawned
void AWaveParticleTile::BeginPlay()
{
	Super::BeginPlay();
}


void AWaveParticleTile::GeneratorStaticMesh(UStaticMesh* MeshAsset) {
	WaveMesh->SetStaticMesh(MeshAsset);
}



void AWaveParticleTile::GeneratorWaveMesh(uint32 GridSize, FIntPoint PlaneSize) {
	ProceduralWaveMesh->GetBodySetup()->bNeverNeedsCookedCollisionData = true;
	TArray<int32> Triangles;
	TArray<FVector> WaveVertices;
	TArray<FVector2D> UVs;
	FVector2D Center(PlaneSize.X * 0.5f * GridSize, PlaneSize.Y * 0.5f * GridSize);
	for (int j = 0; j < PlaneSize.Y + 1; j++) {
		for (int i = 0; i < PlaneSize.X + 1; i++) {
			UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
			WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);
		}
	}

	for (int j = 0; j < PlaneSize.Y; j++) {
		for (int i = 0; i < PlaneSize.X; i++) {
			int idx = j * (PlaneSize.X + 1) + i;
			Triangles.Emplace(idx);
			Triangles.Emplace(idx + PlaneSize.X + 1);
			Triangles.Emplace(idx + 1);

			Triangles.Emplace(idx + 1);
			Triangles.Emplace(idx + PlaneSize.X + 1);
			Triangles.Emplace(idx + PlaneSize.X + 1 + 1);
		}
	}

	ProceduralWaveMesh->CreateMeshSection(0, WaveVertices, Triangles, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}


uint32 AWaveParticleTile::GetCenterOffset(uint32 PlaneSize, uint32 CurColumn) {

	return CurColumn > (PlaneSize >> 1) ? 1ul : 0ul;
}

void AWaveParticleTile::HandleLODStart(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer) {
	//不会到最后一行
#if _LOD1
	uint32 CurStrideSize = CurColumn == 0 ? PlaneSize + 1 : ((CurColumn & 0x1ul) ? (PlaneSize >> 1) + 2 : 2);
	uint32 Stride_0 = CurColumn == 0 ? 0ul : PlaneSize + 1;
	uint32 Stride_1 = CurColumn / 2 * ((PlaneSize >> 1) + 2);
	uint32 Stride_2 = CurColumn == 0 ? 0ul : (CurColumn - 1) / 2 * 2;
	uint32 offSet = 0;
#endif

#if _LOD2
	//准确这种情况有4种StrideSize，这里把后两种合到Stride_2中
	uint32 CurStrideSize = 0ul;
	CurStrideSize = CurColumn == 0
		? PlaneSize + 1 
		: (CurColumn == 1 || CurColumn == PlaneSize - 1 
			? (PlaneSize >> 1) + 2 
			: ((CurColumn & 0x1ul) 
				? 4 
				: 2));



	uint32 Stride_0 = CurColumn == 0 ? 0ul : PlaneSize + 1;
	uint32 Stride_1 = CurColumn < 2  ? 0ul : (PlaneSize >> 1) + 2;
	uint32 Stride_2 = CurColumn < 2
		? 0ul 
		: (CurColumn - 1) / 2 * 2 + (CurColumn - 2) / 2 * 4;

	//Center Handle
	CurStrideSize = CurStrideSize + (CurColumn == (PlaneSize >> 1) ? 1ul : 0ul);
	uint32 offSet = CurColumn > (PlaneSize >> 1) ? 1ul : 0ul;
#endif


	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 + CurStrideSize;
	uint32 Index_2 = (CurColumn & 0x1ul) ? Index_0 + 1 : Index_1 + 1;

	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);
}

void AWaveParticleTile::HandleLODEnd(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer) {
#if _LOD1
	uint32 CurStrideSize = CurColumn == PlaneSize - 1 ? PlaneSize + 1 : (CurColumn & 0x1ul) ? 2 : (PlaneSize >> 1) + 2;
	uint32 Stride_0 = CurColumn == 0 ? 0ul : PlaneSize + 1;
	uint32 Stride_1 = CurColumn / 2 * ((PlaneSize >> 1) + 2);
	uint32 Stride_2 = CurColumn == 0 ? 0 : (CurColumn - 1) / 2 * 2;
	uint32 offSet = CurColumn == 0 ? PlaneSize : ((CurColumn & 0x1ul) ? (PlaneSize >> 1) + 1 : 1);
#endif

#if _LOD2
	uint32 CurStrideSize = 0;
	CurStrideSize = CurColumn == PlaneSize - 1 
		? PlaneSize + 1 
		: (CurColumn == 0 || CurColumn == PlaneSize - 2
			? (PlaneSize >> 1) + 2 
			: ((CurColumn & 0x1ul) 
				? 2 
				: 4));



	uint32 Stride_0 = CurColumn == 0 ? 0ul : PlaneSize + 1;
	uint32 Stride_1 = CurColumn < 2 ? 0ul : (PlaneSize >> 1) + 2;
	uint32 Stride_2 = CurColumn < 2
		? 0ul
		: (CurColumn - 1) / 2 * 2 + (CurColumn - 2) / 2 * 4;

	uint32 offSet = CurColumn == 0 
		? PlaneSize 
		: (CurColumn == 1 || CurColumn == PlaneSize - 1
			? (PlaneSize >> 1) + 1
			: ((CurColumn & 0x1ul)
				? 3
				: 1));

	//Center Handle
	offSet = CurColumn >= (PlaneSize >> 1) ? offSet + 1ul : offSet;
	CurStrideSize = CurStrideSize + (CurColumn + 1 == (PlaneSize >> 1) ? 1ul : 0ul);
#endif

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_2 = Index_0 + CurStrideSize;
	uint32 Index_1 = (CurColumn & 0x1ul) ? Index_0 - 1 : Index_2 - 1;

	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);
}

void AWaveParticleTile::HandleTopEdgeAndTrigle(uint32 PlaneSize, const uint32 CurRow, TArray<int32>& IndexBuffer) {

	uint32 Stride_0 = 0;
	uint32 Stride_1 = 0;
	uint32 Stride_2 = 0;
	uint32 offSet = CurRow;

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 - 1;

	uint32 Index_2 = (CurRow & 0x1ul)
		? Index_0 + (PlaneSize - CurRow + 2 + (CurRow - 1) / 2)
		: Index_1 + (PlaneSize - (CurRow - 1) + 2 + (CurRow - 2) / 2);

	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);

	// Top Area
	if (CurRow != PlaneSize && (CurRow & 0x1ul) == 0ul) {
		uint32 _Index_0 = Index_0;
		uint32 _Index_1 = _Index_0 + (PlaneSize - CurRow + 2 + (CurRow - 1) / 2);
		uint32 _Index_2 = _Index_1 + 1;

		IndexBuffer.Emplace(_Index_0);
		IndexBuffer.Emplace(_Index_1);
		IndexBuffer.Emplace(_Index_2);
	}
}

void AWaveParticleTile::HandleDownEdgeAndTrigle(uint32 PlaneSize, const uint32 CurRow, TArray<int32>& IndexBuffer) {
#if _LOD1
	uint32 Stride_0 = PlaneSize + 1;
	uint32 Stride_1 = PlaneSize / 2 * ((PlaneSize >> 1) + 2);
	uint32 Stride_2 = (PlaneSize - 1) / 2 * 2;
	uint32 offSet = CurRow;
#endif

#if _LOD2
	uint32 Stride_0 = PlaneSize + 1;
	uint32 Stride_1 = ((PlaneSize >> 1) + 2) * 2;
	uint32 Stride_2 = (PlaneSize - 2) / 2 * 2 + (PlaneSize - 3) / 2 * 4; //这里需要多减一，因为最后一行的上一行变换不同

	uint32 offSet = CurRow;
	//Center Handle
	offSet += 1;
#endif

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_2 = Index_0 - 1;

	uint32 Index_1 = (CurRow & 0x1ul)
		? Index_0 - (CurRow + (PlaneSize >> 1) + 2 - (CurRow - 1) / 2 - 1)
		: Index_2 - (CurRow - 1 + (PlaneSize >> 1) + 2 - (CurRow - 2) / 2 - 1);

	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);


	// Down Area
	if (CurRow != PlaneSize && (CurRow & 0x1ul) == 0ul) {
		uint32 _Index_0 = Index_0;
		uint32 _Index_2 = _Index_0 - (CurRow + (PlaneSize >> 1) + 2 - (CurRow - 1) / 2 - 1);
		uint32 _Index_1 = _Index_2 + 1;

		IndexBuffer.Emplace(_Index_0);
		IndexBuffer.Emplace(_Index_1);
		IndexBuffer.Emplace(_Index_2);
	}
}

void AWaveParticleTile::HandleLeftTrigle(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer) {
#if _LOD1
	uint32 Stride_0 = PlaneSize + 1;
	uint32 Stride_1 = CurColumn / 2 * ((PlaneSize >> 1) + 2);
	uint32 Stride_2 = (CurColumn - 1) / 2 * 2; 
	uint32 offSet = 0;
	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 + 2 + 1;
	uint32 Index_2 = Index_0 - ((PlaneSize >> 1) + 1);
#endif


#if _LOD2
	uint32 Stride_0 = PlaneSize + 1;
	uint32 Stride_1 = (PlaneSize >> 1) + 2;
	uint32 Stride_2 = (CurColumn - 1) / 2 * 2 + (CurColumn - 2) / 2 * 4;
	uint32 offSet = 0;

	//Center Handle
	offSet = CurColumn > (PlaneSize >> 1) ? offSet + 1 : offSet;
	uint32 CurStride = CurColumn == (PlaneSize >> 1) ? 3 + 1 : 3;

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 + CurStride;
	//第一个处理Div的offset
	uint32 DivOffset = CurColumn == 2 ? (PlaneSize >> 1) + 1 : 3;
	uint32 Index_2 = Index_0 - DivOffset;
#endif


	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);
}

void AWaveParticleTile::HandleRightTrigle(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer) {
#if _LOD1
	uint32 Stride_0 =PlaneSize + 1;
	uint32 Stride_1 = CurColumn / 2 * ((PlaneSize >> 1) + 2);
	uint32 Stride_2 =(CurColumn - 1) / 2 * 2;
	uint32 offSet = (CurColumn & 0x1ul) ? (PlaneSize >> 1) + 1 : 1;

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 - 2 - 1;
	uint32 Index_2 = Index_0 + ((PlaneSize >> 1) + 1);
#endif

#if _LOD2
	uint32 Stride_0 = PlaneSize + 1;
	uint32 Stride_1 = (PlaneSize >> 1) + 2;
	uint32 Stride_2 = (CurColumn - 1) / 2 * 2 + (CurColumn - 2) / 2 * 4;
	uint32 offSet = (CurColumn & 0x1ul) ? 3 : 1;

	//Center Handle
	offSet = CurColumn >= (PlaneSize >> 1) ? offSet + 1 : offSet;
	uint32 CurStride = CurColumn == (PlaneSize >> 1) ? 3 + 1 : 3;

	uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
	uint32 Index_1 = Index_0 - CurStride;
	//最后一个处理DIV的offset
	uint32 AddOffset = CurColumn == PlaneSize - 2 ? (PlaneSize >> 1) + 1 : 3;
	uint32 Index_2 = Index_0 + AddOffset;
	
#endif

	IndexBuffer.Emplace(Index_0);
	IndexBuffer.Emplace(Index_1);
	IndexBuffer.Emplace(Index_2);
}

void AWaveParticleTile::HandleMiddle(uint32 PlaneSize, const uint32 CurColumn, const uint32 CurRow, TArray<int32>& IndexBuffer) {
	//边缘不再补充
	if (CurColumn != PlaneSize - 1 && CurRow != PlaneSize - 1) {
		uint32 CurStrideSize = (PlaneSize >> 1) + 2 + 2;

		uint32 Stride_0 = PlaneSize + 1;
		uint32 Stride_1 = CurColumn / 2 * ((PlaneSize >> 1) + 2);
		uint32 Stride_2 = CurColumn == 0 ? 0 : (CurColumn - 1) / 2 * 2; //实际上这里不需要判断，因为column总是大于1
		uint32 offSet = (CurRow + 1) / 2;

		uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
		uint32 Index_1 = Index_0 + CurStrideSize;
		uint32 Index_2 = Index_0 + CurStrideSize + 1;
		IndexBuffer.Emplace(Index_0);
		IndexBuffer.Emplace(Index_1);
		IndexBuffer.Emplace(Index_2);

		uint32 Index_3 = Index_0 + 1;
		IndexBuffer.Emplace(Index_0);
		IndexBuffer.Emplace(Index_2);
		IndexBuffer.Emplace(Index_3);
	}
}


void AWaveParticleTile::GeneratorYLODMesh(uint32 GridSize, FIntPoint PlaneSize) {

	check(PlaneSize.X > 2);

	ProceduralWaveMesh->GetBodySetup()->bNeverNeedsCookedCollisionData = true;
	TArray<int32> IndexBuffer;
	TArray<FVector> WaveVertices;
	TArray<FVector2D> UVs;
	FVector2D Center(PlaneSize.X * 0.5f * GridSize, PlaneSize.Y * 0.5f * GridSize);


	for (int j = 0; j < PlaneSize.Y + 1; j++) {

		UVs.Emplace(0.f, j / static_cast<float>(PlaneSize.Y));
		WaveVertices.Emplace(-Center.X, j * GridSize - Center.Y, 0.f);
		if (j != PlaneSize.Y) {
			HandleLODStart(PlaneSize.X, j, IndexBuffer);
		}

		//外圈,这个循环要到最后一个顶点
		if (j == 0) {
			for (int i = 1; i < PlaneSize.X + 1; i++) {
				UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
				WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);
				HandleTopEdgeAndTrigle(PlaneSize.X, i, IndexBuffer);
			}
		}
		else if (j == PlaneSize.Y) {
			for (int i = 1; i < PlaneSize.X + 1; i++) {
				UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
				WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);
				HandleDownEdgeAndTrigle(PlaneSize.X, i, IndexBuffer);
			}
		}
		else {
			if (!(j & 0x1ul)) {
				HandleLeftTrigle(PlaneSize.X, j, IndexBuffer);
				HandleRightTrigle(PlaneSize.X, j, IndexBuffer);
			}
			else {
#if _LOD1
				for (int i = 1; i < PlaneSize.X + 1; i += 2) {
					UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
					WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);
					HandleMiddle(PlaneSize.X, j, i, IndexBuffer);
				}
#endif

#if _LOD2
				uint32 CurStep = j == 1 || j == PlaneSize.Y - 1 ? 2ul : PlaneSize.X - 2;
				for (int i = 1; i < PlaneSize.X + 1; i += CurStep) {
					UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
					WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);
				}
#endif
			}

#if _LOD2
			if (j == PlaneSize.X >> 1) {
				UVs.Emplace(0.5f, 0.5f);
				WaveVertices.Emplace(0.f,0.f,0.f);
			}
#endif
			UVs.Emplace(1.f, j / static_cast<float>(PlaneSize.Y));
			WaveVertices.Emplace(Center.X, j * GridSize - Center.Y, 0.f);
		}

		if (j != PlaneSize.Y) {
			HandleLODEnd(PlaneSize.X, j, IndexBuffer);
		}
	}

#if _LOD2
	uint32 HalfSize = (PlaneSize.X >> 1);
	uint32 EdgeSize = HalfSize - 1;
	uint32 CenterIndex = PlaneSize.X + 1 + HalfSize + 2 + (HalfSize - 1) / 2 * 2 + (HalfSize - 2) / 2 * 4 + 1;
	//Top
	uint32 IndexTop = PlaneSize.X + 1 + 1;
	for (uint32 i = 0; i < EdgeSize; ++i) {
		IndexBuffer.Emplace(CenterIndex);
		IndexBuffer.Emplace(IndexTop + 1);
		IndexBuffer.Emplace(IndexTop);

		IndexTop += 1;
	}

	//Right
	uint32 IndexRight = IndexTop;
	for (uint32 i = 0; i < EdgeSize; ++i) {

		auto Inc = i == EdgeSize - 1 ? (PlaneSize.X >> 1) + 2 + 2 : 2 + 4;
		Inc = i == (EdgeSize >> 1) ? Inc + 1 : Inc;

		IndexBuffer.Emplace(CenterIndex);
		IndexBuffer.Emplace(IndexRight + Inc);
		IndexBuffer.Emplace(IndexRight);
		IndexRight += Inc;
	}

	//Down
	uint32 IndexDown = IndexRight;
	for (uint32 i = 0; i < EdgeSize; ++i) {
		IndexBuffer.Emplace(CenterIndex);
		IndexBuffer.Emplace(IndexDown - 1);
		IndexBuffer.Emplace(IndexDown);
		IndexDown -= 1;
	}

	//Left
	uint32 IndexLeft = IndexDown;
	for (uint32 i = 0; i < EdgeSize; ++i) {

		auto Inc = i == EdgeSize - 1 ? (PlaneSize.X >> 1) + 2 + 2 : 2 + 4;
		Inc = i == (EdgeSize >> 1) ? Inc + 1 : Inc;

		IndexBuffer.Emplace(CenterIndex);
		IndexBuffer.Emplace(IndexLeft - Inc);
		IndexBuffer.Emplace(IndexLeft);
		IndexLeft -= Inc;
	}
#endif

	ProceduralWaveMesh->CreateMeshSection(0, WaveVertices, IndexBuffer, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}

void AWaveParticleTile::GeneratorYLODMesh_2(uint32 GridSize, FIntPoint PlaneSize) {

}

void AWaveParticleTile::Destroyed() {

	Super::Destroyed();
}

// Called every frame
void AWaveParticleTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}




//float U = UV.x * UVScale.x + EdgeValueX;
//float V = UV.y * UVScale.y + EdgeValuey;
//return float2(U,V);
//
//float U = UV.x == 0.f ? UVScale.x * EdgeValueX : UV.x * UVScale.x;
//float V = UV.y == 0.f ? UVScale.y * EdgeValueY : UV.y * UVScale.y;
//float2 Result = float2(U, V);
//return Result;

//float2 dDx = (Right - Left) * INV_TILE_SIZE;
//float2 dDy = (Bottom - Top) * INV_TILE_SIZE;
//float J = (1.0 + dDx.x) * (1.0 + dDy.y) - dDx.y * dDy.x;
//float Result = J < 0.f ? 1.f : J;
//return Result;
