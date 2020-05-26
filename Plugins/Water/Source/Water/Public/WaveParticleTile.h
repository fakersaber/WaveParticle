// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveParticleTile.generated.h"

class UProceduralMeshComponent;
class UWaveMeshComponent;

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

	void GeneratorYLODMesh(uint32 GridSize, FIntPoint PlaneSize);

	void GeneratorYLODMesh_2(uint32 GridSize, FIntPoint PlaneSize);

private:

	void HandleLODStart(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer);
	void HandleLODEnd(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer);
	void HandleTopEdgeAndTrigle(uint32 PlaneSize, const uint32 CurRow, TArray<int32>& IndexBuffer);
	void HandleDownEdgeAndTrigle(uint32 PlaneSize, const uint32 CurRow, TArray<int32>& IndexBuffer);
	void HandleLeftTrigle(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer);
	void HandleRightTrigle(uint32 PlaneSize, const uint32 CurColumn, TArray<int32>& IndexBuffer);


	void HandleMiddle(uint32 PlaneSize, const uint32 CurColumn, const uint32 CurRow, TArray<int32>& IndexBuffer);



	uint32 GetCenterOffset(uint32 PlaneSize, uint32 CurColumn);

public:
	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UWaveMeshComponent* WaveMesh;

	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UProceduralMeshComponent* ProceduralWaveMesh;
};
