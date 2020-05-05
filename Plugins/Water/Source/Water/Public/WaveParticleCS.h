#pragma once

#include "CoreMinimal.h"
#include "RHICommandList.h"

class FTextureRenderTargetResource;

struct FReadBuffer;

struct FWaveParticle {
	//粒子通用属性
	static float Size;
	static uint32 width;
	static uint32 height;
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
		FTextureRenderTargetResource* Filed_RT,
		ERHIFeatureLevel::Type FeatureLevel
	);

	void InitWaveParticlePosResource();
private:

	TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe> SharedParticlePos;
	TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe> SharedParticleSpeed;
	FReadBuffer* WaveParticlePosBuffer;
};