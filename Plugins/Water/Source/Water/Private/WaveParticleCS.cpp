#include "WaveParticleCS.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"

#define WAVE_GROUP_THREAD_COUNTS 8

class FWaveParticlePlacementCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWaveParticlePlacementCS, Global)

public:
	FWaveParticlePlacementCS() {};

	FWaveParticlePlacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		WaveParticlePos_SRV.Bind(Initializer.ParameterMap, TEXT("RWWaveParticlePos"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Paramers)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), WAVE_GROUP_THREAD_COUNTS);
	}

	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		uint32 InThreadWidth,
		FIntPoint InPariticleSize,
		FVector2D InVectorFieldDensity,
		float InParticleSize,
		float InBeta,
		float InParticleScale,
		FRHIShaderResourceView* InWaveParticlePos_SRV,
		FRHIUnorderedAccessView* InWaveParticleField_UAV
	)
	{
		SetShaderValue(RHICmdList, GetComputeShader(), ThreadWidth, InThreadWidth);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleWidth, InPariticleSize.X);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleHeight, InPariticleSize.Y);
		SetShaderValue(RHICmdList, GetComputeShader(), VectorFieldDensity, InVectorFieldDensity);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleSize, InPariticleSize);
		SetShaderValue(RHICmdList, GetComputeShader(), Beta, InBeta);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleScale, InParticleScale);

		SetSRVParameter(RHICmdList, GetComputeShader(), WaveParticlePos_SRV, InWaveParticlePos_SRV);
		if (WaveParticleField_UAV.IsBound()) {
			RHICmdList.SetUAVParameter(GetComputeShader(), WaveParticleField_UAV.GetBaseIndex(), InWaveParticleField_UAV);
		}
	}

	void UnbindUAV(FRHICommandList& RHICmdList){

		if (WaveParticleField_UAV.IsBound()) {
			RHICmdList.SetUAVParameter(GetComputeShader(), WaveParticleField_UAV.GetBaseIndex(), nullptr);
		}
		
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << ThreadWidth;
		Ar << ParticleWidth;
		Ar << ParticleHeight;
		Ar << VectorFieldDensity;
		Ar << ParticleSize;
		Ar << Beta;
		Ar << ParticleScale;
		Ar << WaveParticlePos_SRV;
		Ar << WaveParticleField_UAV;

		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter ThreadWidth;

	FShaderParameter ParticleWidth;
	FShaderParameter ParticleHeight;
	FShaderParameter VectorFieldDensity;
	FShaderParameter ParticleSize;
	FShaderParameter Beta;
	FShaderParameter ParticleScale;

	FShaderResourceParameter WaveParticlePos_SRV;
	FShaderResourceParameter WaveParticleField_UAV;
};

IMPLEMENT_SHADER_TYPE(, FWaveParticlePlacementCS, TEXT("/Plugins/Shaders/Private/WaveParticleCS.usf"), TEXT("ComputeFieldCS"), SF_Compute)


FWaveParticle_GPU::FWaveParticle_GPU(
	const TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe>& SharedParticlePos, 
	const TSharedPtr<TArray<FVector2D>, ESPMode::ThreadSafe>& SharedParticleSpeed
)
	:
	SharedParticlePos(SharedParticlePos),
	SharedParticleSpeed(SharedParticleSpeed)
{
	WaveParticlePosBuffer = new FReadBuffer();
}


FWaveParticle_GPU::~FWaveParticle_GPU() {
	delete WaveParticlePosBuffer;
	WaveParticlePosBuffer = nullptr;
}

void FWaveParticle_GPU::UpdateWaveParticlePos(float ParticleTickTime)
{
	for (auto i = 0; i < SharedParticlePos->Num(); ++i) {
		(*SharedParticlePos)[i] += ParticleTickTime * (*SharedParticleSpeed)[i];
	}
	void* ParticlePosData = RHILockVertexBuffer(WaveParticlePosBuffer->Buffer, 0, SharedParticlePos->Num() * sizeof(FVector2D), RLM_WriteOnly);
	FMemory::Memcpy(ParticlePosData, SharedParticlePos->GetData(), SharedParticlePos->Num() * sizeof(FVector2D));
	RHIUnlockVertexBuffer(WaveParticlePosBuffer->Buffer);

	//RHICmdList.BeginComputePass(TEXT("WaveParticlePos"));
	//TShaderMapRef<FWaveParticlePosCS> WaveParticlePosShader(GetGlobalShaderMap(FeatureLevel));
	//RHICmdList.SetComputeShader(WaveParticlePosShader->GetComputeShader());
	////PhillipsSpecturmShader->SetParameters(RHICmdList, WaveSize, GridLength, WaveAmplitude, WindSpeed, RandomTableSRV, Spectrum, SpectrumConj);
	////DispatchComputeShader(RHICmdList, *WaveParticlePosShader, FMath::DivideAndRoundUp(WaveSize + 1, WAVE_GROUP_THREAD_COUNTS), FMath::DivideAndRoundUp(WaveSize + 1, WAVE_GROUP_THREAD_COUNTS), 1);
	////PhillipsSpecturmShader->UnbindUAV(RHICmdList);
	//RHICmdList.EndComputePass();
}


void FWaveParticle_GPU::UpdateWaveParticleFiled
(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* Filed_RT,
	ERHIFeatureLevel::Type FeatureLevel
) 
{
	//UTexture中拥有fence，不会在渲染线程中释放
	FRHITexture* FiledTexture_RT = Filed_RT->GetRenderTargetTexture();
	FUnorderedAccessViewRHIRef Filed_UAV = RHICreateUnorderedAccessView(FiledTexture_RT);

	RHICmdList.BeginComputePass(TEXT("ComputeField"));
	TShaderMapRef<FWaveParticlePlacementCS> WaveParticlePlacementShader(GetGlobalShaderMap(FeatureLevel));
	RHICmdList.SetComputeShader(WaveParticlePlacementShader->GetComputeShader());

	WaveParticlePlacementShader->SetParameters(
		RHICmdList,
		WaveParticlePosBuffer->SRV,
		Filed_UAV
	);


	RHICmdList.EndComputePass();
}


void FWaveParticle_GPU::InitWaveParticlePosResource() {

	WaveParticlePosBuffer->Initialize(sizeof(FVector2D), SharedParticlePos->Num(), EPixelFormat::PF_G32R32F, BUF_Static);

	void* ParticlePosData = RHILockVertexBuffer(WaveParticlePosBuffer->Buffer, 0, SharedParticlePos->Num() * sizeof(FVector2D), RLM_WriteOnly);
	FMemory::Memcpy(ParticlePosData, SharedParticlePos->GetData(), SharedParticlePos->Num() * sizeof(FVector2D));
	RHIUnlockVertexBuffer(WaveParticlePosBuffer->Buffer);
}