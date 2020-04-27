// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AParticleWaveManager.generated.h"

class UProceduralMeshComponent;
class AWaveParticleTile;
class UMaterialInterface;
struct FUpdateTextureRegion2D;

struct FWaveParticle {
	FWaveParticle(const FVector2D& position, const FVector2D& Speed)
		: Position(position), Speed(Speed)
	{

	}


	FVector2D Position;
	FVector2D Speed;

	//����ͨ������
	static float Size;
	static uint32 width;
	static uint32 height;
};


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

	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UTexture2D* VectorFieldTex;

	UPROPERTY(VisibleAnywhere, Category = WaveParticleParam)
		UTexture2D* NormalMapTex;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		float GridSize;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		FIntPoint TileSize;

	UPROPERTY(EditAnywhere, Category = WaveParticleParam)
		UMaterialInterface* WaterMaterial;

	UPROPERTY(EditAnywhere,Category = WaveParticleParam)
		TSubclassOf<AWaveParticleTile> WaveClassType;



private:
	TArray<FWaveParticle> ParticleContainer;

	TSharedPtr<FUpdateTextureRegion2D> UpdateTextureRegion2D;

	float VectorFieldDensityX;

	float VectorFieldDensityY;


	//#TODO use R11G11B10 format
	TArray<float> VectorField;

	TArray<float> NormalMapData;

	TArray<AWaveParticleTile*> WaveParticleTileContainer;

	bool bHasInit;

	float TILE_SIZE_X2;


public:
	const static FVector2D UVScale1;
	const static FVector2D UVScale2;
	const static FVector2D UVScale3;
};
