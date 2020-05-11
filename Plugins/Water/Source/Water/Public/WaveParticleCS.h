#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

class FTextureRenderTargetResource;
class FRHICommandListImmediate;


struct FReadBuffer;
struct FTextureRWBuffer2D;

struct FWaveParticle {
	//粒子通用属性
	static float Size;
	static uint32 width;
	static uint32 height;
};


struct FUpdateFieldStruct {
	FIntPoint InThreadSize;
	FIntPoint InParticleQuadSize;
	FVector2D InVectorFieldDensity;
	uint32 InParticleNum;
	float InParticleSize;
	float InBeta;
	float InParticleScale;
};


//only render thread
class FWaveParticle_GPU {
public:
	FWaveParticle_GPU(
		const TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe>& SharedParticlePos, 
		const TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe>& SharedParticleSpeed
	);
	~FWaveParticle_GPU();

	FWaveParticle_GPU() = delete;


public:
	void UpdateWaveParticlePos(float TimeSecond);

	void UpdateWaveParticleFiled(
		FRHICommandListImmediate& RHICmdList,
		const FUpdateFieldStruct& StructData,
		ERHIFeatureLevel::Type FeatureLevel,
		UTexture2D* CopyVectorFieldTexPtr,
		UTexture2D* CopyNormal
	);

	void InitWaveParticlePosResource(const FIntPoint& FieldSize);
private:

	TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe> SharedParticlePos;
	TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe> SharedParticleSpeed;

	FReadBuffer* WaveParticlePosBuffer;
	FTextureRWBuffer2D* WaveParticleFieldBuffer;
	FTextureRWBuffer2D* WaveParticleFieldComPression;
	FTextureRWBuffer2D* WaveParticleNormal;
};