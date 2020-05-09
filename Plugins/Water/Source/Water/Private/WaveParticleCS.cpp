#include "WaveParticleCS.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"
#include "RHIUtilities.h"
#include "ClearReplacementShaders.h"
#include "Engine/Texture2D.h"


#define WAVE_GROUP_THREAD_COUNTS 8
#define CLEAR_PARTICLE_GROUP_SIZE 8u

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


class FWaveParticleClearCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWaveParticleClearCS, Global)

public:
	FWaveParticleClearCS() {};

	FWaveParticleClearCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		ClearTarget.Bind(Initializer.ParameterMap, TEXT("VectorField"));
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
		OutEnvironment.SetDefine(TEXT("CLEAR_THREAD_GROUP_SIZE"), CLEAR_PARTICLE_GROUP_SIZE);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << ClearTarget;

		return bShaderHasOutdatedParameters;
	}

	uint32 GetResourceParamIndex() { return ClearTarget.GetBaseIndex(); }

public:
	FShaderResourceParameter ClearTarget;
};


IMPLEMENT_SHADER_TYPE(, FWaveParticleClearCS, TEXT("/Plugins/Shaders/Private/WaveParticleClearCS.usf"), TEXT("ClearComputeFieldCS"), SF_Compute);


class FWaveParticleCompressionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWaveParticleCompressionCS, Global)

public:
	FWaveParticleCompressionCS() {};

	FWaveParticleCompressionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		CompressionTarget.Bind(Initializer.ParameterMap, TEXT("CompressionTarget"));
		SINTVectorFieldTex.Bind(Initializer.ParameterMap, TEXT("VectorField"));
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
		OutEnvironment.SetDefine(TEXT("COMPORESSION_THREAD_GROUP_SIZE"), WAVE_GROUP_THREAD_COUNTS);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << SINTVectorFieldTex;
		Ar << CompressionTarget;

		return bShaderHasOutdatedParameters;
	}


public:
	FShaderResourceParameter SINTVectorFieldTex;
	FShaderResourceParameter CompressionTarget;
};


IMPLEMENT_SHADER_TYPE(, FWaveParticleCompressionCS, TEXT("/Plugins/Shaders/Private/WaveParticleCompression.usf"), TEXT("CompressionFieldCS"), SF_Compute);


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
	WaveParticleFieldComPression = new FTextureRWBuffer2D();
}


FWaveParticle_GPU::~FWaveParticle_GPU() {
	delete WaveParticlePosBuffer;
	delete WaveParticleFieldBuffer;
	delete WaveParticleFieldComPression;

	WaveParticlePosBuffer = nullptr;
	WaveParticleFieldBuffer = nullptr;
	WaveParticleFieldComPression = nullptr;
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


void FWaveParticle_GPU::UpdateWaveParticleFiled(FRHICommandListImmediate& RHICmdList,const FUpdateFieldStruct& StructData,ERHIFeatureLevel::Type FeatureLevel,UTexture2D* CopyVectorFieldTexPtr)
{
	RHICmdList.BeginComputePass(TEXT("ComputeField"));

	//Clear Pass
	{
		TShaderMapRef<FWaveParticleClearCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
		FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
		RHICmdList.SetComputeShader(ShaderRHI);
		SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->ClearTarget, WaveParticleFieldBuffer->UAV);
		DispatchComputeShader(
			RHICmdList,
			*ComputeShader,
			FMath::DivideAndRoundUp(WaveParticleFieldBuffer->Buffer->GetSizeX(), CLEAR_PARTICLE_GROUP_SIZE),
			FMath::DivideAndRoundUp(WaveParticleFieldBuffer->Buffer->GetSizeY(), CLEAR_PARTICLE_GROUP_SIZE),
			1
		);
		SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->ClearTarget, nullptr);
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, WaveParticleFieldBuffer->UAV);
	}


	//calculate VectorField
	{
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

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, WaveParticleFieldBuffer->UAV, nullptr);
	}


	//Comporession VectorField
	{
		TShaderMapRef<FWaveParticleCompressionCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
		FRHIComputeShader* ShaderRHI = ComputeShader->GetComputeShader();
		RHICmdList.SetComputeShader(ShaderRHI);
		SetTextureParameter(RHICmdList, ShaderRHI, ComputeShader->SINTVectorFieldTex, WaveParticleFieldBuffer->Buffer);
		SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->CompressionTarget, WaveParticleFieldComPression->UAV);

		RHICmdList.DispatchComputeShader(
			FMath::DivideAndRoundUp(WaveParticleFieldComPression->Buffer->GetSizeX(), CLEAR_PARTICLE_GROUP_SIZE),
			FMath::DivideAndRoundUp(WaveParticleFieldComPression->Buffer->GetSizeY(), CLEAR_PARTICLE_GROUP_SIZE),
			1
		);

		SetUAVParameter(RHICmdList, ShaderRHI, ComputeShader->CompressionTarget, nullptr);
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, WaveParticleFieldComPression->UAV);
	}

	FTexture2DRHIRef MaterialTextureRHI = reinterpret_cast<FTexture2DResource*>(CopyVectorFieldTexPtr->Resource)->GetTexture2DRHI();

	//#TODO: 可修改UTexture2D来避免复制，在InitRHI()时带上对应标志
	RHICmdList.CopyTexture(WaveParticleFieldComPression->Buffer, MaterialTextureRHI, FRHICopyTextureInfo());

	RHICmdList.EndComputePass();
}


void FWaveParticle_GPU::InitWaveParticlePosResource(const FIntPoint& FieldSize) {
	WaveParticlePosBuffer->Initialize(sizeof(FVector2D), SharedParticlePos->Num(), EPixelFormat::PF_G32R32F, BUF_Dynamic);
	WaveParticleFieldBuffer->Initialize(sizeof(uint32), FieldSize.X * 3, FieldSize.Y, EPixelFormat::PF_R32_SINT);
	WaveParticleFieldComPression->Initialize(sizeof(float) * 4, FieldSize.X, FieldSize.Y, EPixelFormat::PF_A32B32G32R32F);
}