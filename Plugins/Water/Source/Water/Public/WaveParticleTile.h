// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveParticleTile.generated.h"

class UProceduralMeshComponent;

UCLASS()
class WATER_API AWaveParticleTile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWaveParticleTile();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Destroyed() override;

public:
	void GeneratorWaveMesh(uint32 GridSize, FIntPoint PlaneSize);

	void GeneratorStaticMesh(UStaticMesh* MeshAsset);

public:
	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UStaticMeshComponent* WaveMesh;

	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UProceduralMeshComponent* ProceduralWaveMesh;
};
