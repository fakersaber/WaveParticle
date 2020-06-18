// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AParticleWaveManager.generated.h"


class UProceduralMeshComponent;
class AWaveParticleTile;
class UMaterialInterface;
class UTextureRenderTarget2D;

class FWaveParticle_GPU;
class FWaterInstanceMeshManager;
struct FUpdateTextureRegion2D;
struct FUpdateFieldStruct;


UCLASS()
class WATER_API AParticleWaveManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AParticleWaveManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Destroyed() override;

	void UpdateParticle(float DeltaTime);

	void InitWaveParticle();

	void ClearResource();

	//GPU Version 
	void UpdateParticle_GPU(float DeltaTime);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	//virtual bool ShouldTickIfViewportsOnly() const override
	//{
	//	return true;
	//}
#endif


public:
	//Tile Mesh Size
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		FIntPoint PlaneSize;

	//��������С
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		FIntPoint VectorFieldSize;

	//Max Speed Field/s
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float MaxParticleSpeed;

	//Min Speed Field/s
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float MinParticleSpeed;

	//��������
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		uint16 ParticleNum;

	//���Ӵ�С����
	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float ParticleScale;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float Beta;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		UTextureRenderTarget2D* VectorFieldTex;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		UTextureRenderTarget2D* NormalMapTex;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float GridSize;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		FIntPoint TileSize;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		UMaterialInterface* WaterMaterial;

	UPROPERTY(EditAnywhere,Category = WaveParticleParam)
		TSubclassOf<AWaveParticleTile> WaveClassType;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		UStaticMesh* WaterMesh;
	
	//UPROPERTY(EditAnywhere, Category = WaveParticleParam)
	//	TArray<UStaticMesh*> WaterMeshs;

private:
	
	//Mesh Manager
	TSharedPtr<FWaterInstanceMeshManager> WaterMeshManager;

	TSharedPtr<FUpdateTextureRegion2D> UpdateTextureRegion2D;

	float VectorFieldDensityX;

	float VectorFieldDensityY;

	//#TODO use R11G11B10 format
	TArray<float> VectorField[2];

	TArray<float> NormalMapData[2];

	TArray<AWaveParticleTile*> WaveParticleTileContainer;

	bool bHasInit;

	uint32 CurFrame;

	

//GPU Resource
private:
	//����ActorҲ��һ��������ӵ��fence����destroyǰfence������ִ�����
	//��Actor���п��ܲ��ᱻ���٣������������³�ʼ��������ʹ��SharedPtr
	TSharedPtr<TArray<FVector2D>,ESPMode::ThreadSafe>  ParticlePosContainer;
	TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe> ParticleSpeedContainer;
	TSharedPtr<FWaveParticle_GPU, ESPMode::ThreadSafe> WaveParticleGPU;

public:
	const static FVector2D UVScale1;
	const static FVector2D UVScale2;
	const static FVector2D UVScale3;
	static TSharedPtr<FUpdateFieldStruct> UpdateParticleData;

};

