// Fill out your copyright notice in the Description page of Project Settings.


#include "WaveParticleTile.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

// Sets default values
AWaveParticleTile::AWaveParticleTile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	WaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaveMesh"));
}

// Called when the game starts or when spawned
void AWaveParticleTile::BeginPlay()
{
	Super::BeginPlay();
}

void AWaveParticleTile::GeneratorWaveMesh(uint32 GridSize, FIntPoint PlaneSize) {
	WaveMesh->GetBodySetup()->bNeverNeedsCookedCollisionData = true;

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

	WaveMesh->CreateMeshSection(0, WaveVertices, Triangles, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}


void AWaveParticleTile::Destroyed() {

	Super::Destroyed();
}

// Called every frame
void AWaveParticleTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}






//float U = UV.x == 0.f ? UVScale * EdgeValue : 0.f;
//float V = UV.y == 0.f ? UVScale * EdgeValue : 0.f;
//float2 Result = UV + float2(U, V);
//return Result;
