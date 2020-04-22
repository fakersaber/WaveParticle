// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParticleWave.generated.h"


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
class WATER_API AParticleWave : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AParticleWave();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void UpdateParticle(float DeltaTime);

	void InitWaveParticle();

	void ClearResource();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual bool ShouldTickIfViewportsOnly() const override
	{
		return true;
	}
#endif


public:
	//��άƽ���С
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

private:
	TArray<FWaveParticle> ParticleContainer;

	TSharedPtr<FUpdateTextureRegion2D> UpdateTextureRegion2D;

	float VectorFieldDensityX;

	float VectorFieldDensityY;

	TArray<float> VectorField;

	bool bHasInit;
};
