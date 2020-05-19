// Fill out your copyright notice in the Description page of Project Settings.


#include "WaveParticleTile.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"

// Sets default values
AWaveParticleTile::AWaveParticleTile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	WaveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaveMesh"));

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


void AWaveParticleTile::GeneratorYLODMesh(uint32 GridSize, FIntPoint PlaneSize) {

	ProceduralWaveMesh->GetBodySetup()->bNeverNeedsCookedCollisionData = true;
	TArray<int32> IndexBuffer;
	TArray<FVector> WaveVertices;
	TArray<FVector2D> UVs;
	FVector2D Center(PlaneSize.X * 0.5f * GridSize, PlaneSize.Y * 0.5f * GridSize);

	for (int j = 0; j < PlaneSize.Y + 1; j++) {

		//起始列
		{
			UVs.Emplace(0.f, j / static_cast<float>(PlaneSize.Y));
			WaveVertices.Emplace(-Center.X, j * GridSize - Center.Y, 0.f);

			if (j != PlaneSize.Y) {
				uint32 CurStrideSize =  j == 0 ? PlaneSize.X + 1 : ((j & 0x1ul) ? (PlaneSize.X >> 1) + 2 : 2);

				uint32 Stride_0 = j == 0 ? 0ul : PlaneSize.X + 1;
				uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
				uint32 Stride_2 = (j - 1) / 2 * 2;
				uint32 offSet = 0;

				uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
				uint32 Index_1 = Index_0 + CurStrideSize;
				uint32 Index_2 = (j & 0x1ul) ? Index_0 + 1 : Index_1 + 1;

				IndexBuffer.Emplace(Index_0);
				IndexBuffer.Emplace(Index_1);
				IndexBuffer.Emplace(Index_2);
			}
		}

		//外圈
		if (j == 0 || j == PlaneSize.Y) {
			//这个循环要到最后一个顶点
			for (int i = 1; i < PlaneSize.X + 1; i++) {
				UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
				WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);

				if (j == 0) {
					//Top Handle

					uint32 Stride_0 = 0;
					uint32 Stride_1 = 0;
					uint32 Stride_2 = 0;
					uint32 offSet = i;

					uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
					uint32 Index_1 = Index_0 - 1;

					uint32 Index_2 = (i & 0x1ul)
						? Index_0 + (PlaneSize.X - i + 2 + (i - 1) / 2)
						: Index_1 + (PlaneSize.X - (i - 1) + 2 + (i - 2) / 2);


					IndexBuffer.Emplace(Index_0);
					IndexBuffer.Emplace(Index_1);
					IndexBuffer.Emplace(Index_2);

					// Top Area
					if (i != PlaneSize.X && (i & 0x1ul) == 0ul) {
						uint32 _Index_0 = Index_0;
						uint32 _Index_1 = _Index_0 + (PlaneSize.X - i + 2 + (i - 1) / 2);
						uint32 _Index_2 = _Index_1 + 1;

						IndexBuffer.Emplace(_Index_0);
						IndexBuffer.Emplace(_Index_1);
						IndexBuffer.Emplace(_Index_2);
					}
				}
				else {
					//Down Handle

					uint32 Stride_0 = PlaneSize.X + 1;
					uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
					uint32 Stride_2 = (j - 1) / 2 * 2;
					uint32 offSet = i;

					uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
					uint32 Index_2 = Index_0 - 1;

					uint32 Index_1 = (i & 0x1ul)
						? Index_0 - (i + (PlaneSize.X >> 1) + 2 - (i - 1) / 2 - 1)
						: Index_2 - (i - 1 + (PlaneSize.X >> 1) + 2 - (i - 2) / 2 - 1);

					IndexBuffer.Emplace(Index_0);
					IndexBuffer.Emplace(Index_1);
					IndexBuffer.Emplace(Index_2);


					// Down Area
					if (i != PlaneSize.X && (i & 0x1ul) == 0ul) {
						uint32 _Index_0 = Index_0;
						uint32 _Index_2 = _Index_0 - (i + (PlaneSize.X >> 1) + 2 - (i - 1) / 2 - 1);
						uint32 _Index_1 = _Index_2 + 1;

						IndexBuffer.Emplace(_Index_0);
						IndexBuffer.Emplace(_Index_1);
						IndexBuffer.Emplace(_Index_2);
					}
				}

			}
		}
		else {
			if (j & 0x1ul) {
				for (int i = 1; i < PlaneSize.X + 1; i += 2) {
					UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
					WaveVertices.Emplace(i * GridSize - Center.X, j * GridSize - Center.Y, 0.f);

					//边缘不再补充
					if (j != PlaneSize.Y - 1 && i != PlaneSize.X - 1) {
						uint32 CurStrideSize = (PlaneSize.X >> 1) + 2 + 2;

						uint32 Stride_0 = PlaneSize.X + 1;
						uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
						uint32 Stride_2 = (j - 1) / 2 * 2;
						uint32 offSet = (i + 1) / 2;

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
			}
			else {
				//Left Area
				{
					uint32 Stride_0 = PlaneSize.X + 1;
					uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
					uint32 Stride_2 = (j - 1) / 2 * 2;
					uint32 offSet = 0;

					uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
					uint32 Index_1 = Index_0 + 2 + 1;
					uint32 Index_2 = Index_0 - ((PlaneSize.X >> 1) + 1);
					

					IndexBuffer.Emplace(Index_0);
					IndexBuffer.Emplace(Index_1);
					IndexBuffer.Emplace(Index_2);
				}

				//Right Area
				{
					uint32 Stride_0 = j == 0 ? 0ul : PlaneSize.X + 1;
					uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
					uint32 Stride_2 = (j - 1) / 2 * 2;

					uint32 offSet = j == 0 ? PlaneSize.X : ((j & 0x1ul) ? (PlaneSize.X >> 1) + 1 : 1);

					uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
					uint32 Index_1 = Index_0 - 2 - 1;
					uint32 Index_2 = Index_0 + ((PlaneSize.X >> 1) + 1);

					IndexBuffer.Emplace(Index_0);
					IndexBuffer.Emplace(Index_1);
					IndexBuffer.Emplace(Index_2);
				}
			}
			UVs.Emplace(1.f, j / static_cast<float>(PlaneSize.Y));
			WaveVertices.Emplace(Center.X, j* GridSize - Center.Y, 0.f);
		}
		
		//终止列
		{
			if (j != PlaneSize.Y) {
				uint32 CurStrideSize = j == PlaneSize.Y - 1 ? PlaneSize.X + 1 : (j & 0x1ul) ? 2 : (PlaneSize.X >> 1) + 2;

				uint32 Stride_0 = j == 0 ? 0ul : PlaneSize.X + 1;
				uint32 Stride_1 = j / 2 * ((PlaneSize.X >> 1) + 2);
				uint32 Stride_2 = (j - 1) / 2 * 2;

				uint32 offSet = j == 0 ? PlaneSize.X : ((j & 0x1ul) ? (PlaneSize.X >> 1) + 1 : 1);

				uint32 Index_0 = Stride_0 + Stride_1 + Stride_2 + offSet;
				uint32 Index_2 = Index_0 + CurStrideSize;
				uint32 Index_1 = (j & 0x1ul) ? Index_0 - 1 : Index_2 - 1;

				IndexBuffer.Emplace(Index_0);
				IndexBuffer.Emplace(Index_1);
				IndexBuffer.Emplace(Index_2);
			}
		}
	}


	ProceduralWaveMesh->CreateMeshSection(0, WaveVertices, IndexBuffer, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
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
