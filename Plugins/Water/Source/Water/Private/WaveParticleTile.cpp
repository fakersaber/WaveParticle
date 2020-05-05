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

	//WaveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaveMesh"));

	ProceduralWaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralWaveMesh"));
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
