#include "WaveParticleCS.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "RHIDefinitions.h"

#define WAVE_GROUP_THREAD_COUNTS 8

class FWaveParticlePlacementCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWaveParticlePlacementCS, Global)

public:
	FWaveParticlePlacementCS() {};

	FWaveParticlePlacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		ThreadWidth.Bind(Initializer.ParameterMap, TEXT("ThreadWidth"));
		ParticleWidth.Bind(Initializer.ParameterMap, TEXT("ParticleWidth"));
		ParticleHeight.Bind(Initializer.ParameterMap, TEXT("ParticleHeight"));
		VectorFieldDensity.Bind(Initializer.ParameterMap, TEXT("VectorFieldDensity"));
		ParticleSize.Bind(Initializer.ParameterMap, TEXT("ParticleSize"));
		Beta.Bind(Initializer.ParameterMap, TEXT("Beta"));
		ParticleScale.Bind(Initializer.ParameterMap, TEXT("ParticleScale"));
		WaveParticlePos_SRV.Bind(Initializer.ParameterMap, TEXT("WaveParticlePos"));
		WaveParticleField_UAV.Bind(Initializer.ParameterMap, TEXT("VectorField"));
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
		const FUpdateFieldStruct& StructData,
		FRHIShaderResourceView* InWaveParticlePos_SRV,
		FRHIUnorderedAccessView* InWaveParticleField_UAV
	)
	{
		SetShaderValue(RHICmdList, GetComputeShader(), ThreadWidth, StructData.InThreadSize.X);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleWidth, StructData.InParticleQuadSize.X);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleHeight, StructData.InParticleQuadSize.Y);
		SetShaderValue(RHICmdList, GetComputeShader(), VectorFieldDensity, StructData.InVectorFieldDensity);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleSize, StructData.InParticleSize);
		SetShaderValue(RHICmdList, GetComputeShader(), Beta, StructData.InBeta);
		SetShaderValue(RHICmdList, GetComputeShader(), ParticleScale, StructData.InParticleScale);

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
	WaveParticleFieldBuffer = new FTextureRWBuffer2D();
}


FWaveParticle_GPU::~FWaveParticle_GPU() {
	delete WaveParticlePosBuffer;
	delete WaveParticleFieldBuffer;

	WaveParticlePosBuffer = nullptr;
	WaveParticleFieldBuffer = nullptr;
}

void FWaveParticle_GPU::UpdateWaveParticlePos(float ParticleTickTime)
{
	for (auto i = 0; i < SharedParticlePos->Num(); ++i) {
		(*SharedParticlePos)[i] += ParticleTickTime * (*SharedParticleSpeed)[i];
	}
	void* ParticlePosData = RHILockVertexBuffer(WaveParticlePosBuffer->Buffer, 0, SharedParticlePos->Num() * sizeof(FVector2D), RLM_WriteOnly);
	FMemory::Memcpy(ParticlePosData, SharedParticlePos->GetData(), SharedParticlePos->Num() * sizeof(FVector2D));
	RHIUnlockVertexBuffer(WaveParticlePosBuffer->Buffer);
}


void FWaveParticle_GPU::UpdateWaveParticleFiled
(
	FRHICommandListImmediate& RHICmdList,
	const FUpdateFieldStruct& StructData,
	ERHIFeatureLevel::Type FeatureLevel
) 
{

	RHICmdList.BeginComputePass(TEXT("ComputeField"));
	TShaderMapRef<FWaveParticlePlacementCS> WaveParticlePlacementShader(GetGlobalShaderMap(FeatureLevel));
	RHICmdList.SetComputeShader(WaveParticlePlacementShader->GetComputeShader());

	WaveParticlePlacementShader->SetParameters(
		RHICmdList,
		StructData,
		WaveParticlePosBuffer->SRV,
		WaveParticleFieldBuffer->UAV
	);

	DispatchComputeShader(
		RHICmdList,
		*WaveParticlePlacementShader,
		FMath::DivideAndRoundUp(StructData.InThreadSize.X, WAVE_GROUP_THREAD_COUNTS),
		FMath::DivideAndRoundUp(StructData.InThreadSize.Y, WAVE_GROUP_THREAD_COUNTS),
		1ul
		);


	WaveParticlePlacementShader->UnbindUAV(RHICmdList);
	RHICmdList.EndComputePass();
}


void FWaveParticle_GPU::InitWaveParticlePosResource(const FIntPoint& FieldSize) {
	WaveParticlePosBuffer->Initialize(sizeof(FVector2D), SharedParticlePos->Num(), EPixelFormat::PF_G32R32F, BUF_Dynamic);
	WaveParticleFieldBuffer->Initialize(sizeof(uint32), FieldSize.X * 3, FieldSize.Y * 3, EPixelFormat::PF_R32_UINT);
}