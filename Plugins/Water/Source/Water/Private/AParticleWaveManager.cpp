// Fill out your copyright notice in the Description page of Project Settings.


#include "AParticleWaveManager.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "RHI.h"
#include "Engine/Engine.h"

#include "WaveParticleTile.h"
#include "WaveParticleCS.h"

#include "Engine/TextureRenderTarget.h"


#define CPU_PARTICLE_VERSION 1


static const float Y_PI = 3.1415926535897932f;
const FVector2D AParticleWaveManager::UVScale1 = FVector2D(0.125f, 0.125f);
const FVector2D AParticleWaveManager::UVScale2 = FVector2D(0.25f, 0.25f);
const FVector2D AParticleWaveManager::UVScale3 = FVector2D(0.5f, 0.5f);
TSharedPtr<FUpdateFieldStruct> AParticleWaveManager::UpdateParticleData = nullptr;;

float FWaveParticle::Size = 0.f;
uint32 FWaveParticle::width = 0;
uint32 FWaveParticle::height = 0;

// Sets default values
AParticleWaveManager::AParticleWaveManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ParticleNum = 1;
	ParticleScale = 1.f;
	PlaneSize.X = 63;
	PlaneSize.Y = 63;
	VectorFieldSize.X = 64;
	VectorFieldSize.Y = 64;
	MaxParticleSpeed = 1.f;
	MinParticleSpeed = 0.f;
	Beta = 0.5f;
	bHasInit = false;

	TileSize.X = 1;
	TileSize.Y = 1;
	GridSize = 50.f;

	CurFrame = 0;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ManagerRootComponent"));

}

// Called when the game starts or when spawned
void AParticleWaveManager::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AParticleWaveManager::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (WaveClassType && WaterMaterial && WaterMesh) {
		if (!bHasInit) {
			ClearResource();
			InitWaveParticle();
			bHasInit = true;
		}
#if CPU_PARTICLE_VERSION
		UpdateParticle(DeltaTime);
#else
		UpdateParticle_GPU(DeltaTime);
#endif
	}
}

void AParticleWaveManager::ClearResource() {
#if !CPU_PARTICLE_VERSION
	//GPU Resource
	{
		//GPU资源的释放全部放在渲染线程,RemoveAll后GPU资源释放掉
		//即使不调用Remove，BoardCast依然会释放delegate中无效的的invoke
		ENQUEUE_RENDER_COMMAND(FReleaseWaveParticleResource)([this](FRHICommandListImmediate& RHICmdList) {
			UE_LOG(LogTemp, Log, TEXT("Destroy Delegate"));
			GEngine->PreRenderDelegate.RemoveAll(this);
		});
		WaveParticleGPU.Reset();
	}
#endif


#if CPU_PARTICLE_VERSION
	UpdateTextureRegion2D.Reset();
#endif

	//#TODO 只有当改变与mesh相关参数才重新生成TileActor
	for (auto TileActor : WaveParticleTileContainer) {
		TileActor->Destroy();
	}
	WaveParticleTileContainer.Reset();
	ParticlePosContainer.Reset();
	ParticleSpeedContainer.Reset();
}

void AParticleWaveManager::Destroyed() {
	ClearResource();
	Super::Destroyed();
}

void AParticleWaveManager::InitWaveParticle() {

	ParticlePosContainer = MakeShared<TArray<FVector2D>, ESPMode::ThreadSafe>();
	ParticleSpeedContainer = MakeShared<TArray<FVector2D>, ESPMode::ThreadSafe>();

	for (int i = 0; i < UE_ARRAY_COUNT(VectorField); ++i) {
		VectorField[i].SetNumZeroed(VectorFieldSize.X * VectorFieldSize.Y * 4);
		NormalMapData[i].SetNumZeroed(VectorFieldSize.X * VectorFieldSize.Y * 4);
	}


	WaveParticleTileContainer.Reserve(TileSize.X * TileSize.Y);

	//求单位向量场所的quad大小
	VectorFieldDensityX = static_cast<float>(PlaneSize.X) / VectorFieldSize.X;
	VectorFieldDensityY = static_cast<float>(PlaneSize.Y) / VectorFieldSize.Y;

	FWaveParticle::Size = 2 * 3.14151926f * ParticleScale;

	//检查是贴图大小是否为2的幂
	check(FMath::CountBits(VectorFieldSize.X) == 1);
	check(FMath::CountBits(VectorFieldSize.Y) == 1);
	
	//保证粒子大小
	//check(FVector2D(FWaveParticle::Size, FWaveParticle::Size) < FVector2D(VectorFieldSize));

	FWaveParticle::width = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityX);
	FWaveParticle::height = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityY);


	AParticleWaveManager::UpdateParticleData = MakeShared<FUpdateFieldStruct>();


	AParticleWaveManager::UpdateParticleData->InThreadSize = FIntPoint(
		FMath::CeilToInt(FMath::Sqrt(ParticleNum)) * FWaveParticle::width,
		FMath::CeilToInt(FMath::Sqrt(ParticleNum)) * FWaveParticle::height
	);
	AParticleWaveManager::UpdateParticleData->InParticleQuadSize = FIntPoint(FWaveParticle::width, FWaveParticle::height);
	AParticleWaveManager::UpdateParticleData->InVectorFieldDensity = FVector2D(VectorFieldDensityX, VectorFieldDensityY);
	AParticleWaveManager::UpdateParticleData->InParticleSize = FWaveParticle::Size;
	AParticleWaveManager::UpdateParticleData->InBeta = Beta;
	AParticleWaveManager::UpdateParticleData->InParticleScale = ParticleScale;

	//init particle position and speed
	{
		//分段函数,速度转换的线性函数[0,0.5) 为负，[0.5,1]为正
		//f(x) = (max - min) * x +- min;
		auto SpeedK = MaxParticleSpeed - MinParticleSpeed;
		FMath::RandInit(static_cast<int32>(FDateTime::Now().GetTicks() % INT_MAX));
		for (int i = 0; i < ParticleNum; ++i) {
			//保证最小移动距离
			FVector2D Position(FMath::Rand() % PlaneSize.X, FMath::Rand() % PlaneSize.Y);
			ParticlePosContainer->Emplace(Position);

			auto RandomSpeedX = FMath::FRand() * 2.f - 1.f;
			auto RandomSpeedY = FMath::FRand() * 2.f - 1.f;
			auto Sign_X = FMath::Sign(RandomSpeedX);
			auto Sign_Y = FMath::Sign(RandomSpeedY);
			RandomSpeedX = Sign_X < 0 ? RandomSpeedX * SpeedK - MinParticleSpeed : RandomSpeedX * SpeedK + MinParticleSpeed;
			RandomSpeedY = Sign_Y < 0 ? RandomSpeedY * SpeedK - MinParticleSpeed : RandomSpeedY * SpeedK + MinParticleSpeed;
			FVector2D Speed(RandomSpeedX, RandomSpeedY);
			ParticleSpeedContainer->Emplace(Speed);
		}
	}

	//Init Texture Resources
	{
		VectorFieldTex = UTexture2D::CreateTransient(VectorFieldSize.X, VectorFieldSize.Y, PF_A32B32G32R32F);
		VectorFieldTex->Filter = TF_Bilinear;
		VectorFieldTex->AddressX = TA_Wrap;
		VectorFieldTex->AddressY = TA_Wrap;
		VectorFieldTex->UpdateResource();

		NormalMapTex = UTexture2D::CreateTransient(VectorFieldSize.X, VectorFieldSize.Y, PF_A32B32G32R32F);
		NormalMapTex->Filter = TF_Bilinear;
		NormalMapTex->AddressX = TA_Wrap;
		NormalMapTex->AddressY = TA_Wrap;
		NormalMapTex->UpdateResource();
		UpdateTextureRegion2D = MakeShared<FUpdateTextureRegion2D>(0, 0, 0, 0, VectorFieldSize.X, VectorFieldSize.Y);
	}


#if !CPU_PARTICLE_VERSION
	//复制资源引用,不需要复制
	WaveParticleGPU = MakeShared<FWaveParticle_GPU, ESPMode::ThreadSafe>(ParticlePosContainer, ParticleSpeedContainer);
	TSharedPtr<FWaveParticle_GPU, ESPMode::ThreadSafe> WaveParticleShared(WaveParticleGPU);
	FIntPoint FieldSize(VectorFieldSize);
	ENQUEUE_RENDER_COMMAND(FComputeWaveParticle)([WaveParticleShared, FieldSize](FRHICommandListImmediate& RHICmdList) {
		WaveParticleShared->InitWaveParticlePosResource(FieldSize);
	});

#endif

	//Spawn Actor
	{
		FVector2D TileMeshSize(PlaneSize.X * GridSize, PlaneSize.Y * GridSize);

		FVector2D HalfTileMeshSize(TileMeshSize * 0.5f);

		FVector2D Center(TileSize.X * HalfTileMeshSize.X, TileSize.Y * HalfTileMeshSize.Y);

		for (int y = 0; y < TileSize.Y; ++y) {
			for (int x = 0; x < TileSize.X; ++x) {

				FVector PositionOffset(FVector2D(x * TileMeshSize.X + HalfTileMeshSize.X, y * TileMeshSize.Y + HalfTileMeshSize.Y) - Center, 0.f);
				auto NewActor = GetWorld()->SpawnActor<AWaveParticleTile>(WaveClassType, GetActorLocation() + PositionOffset, GetActorRotation());
				//staic mesh version
				//NewActor->GeneratorStaticMesh(WaterMesh);
				//UStaticMeshComponent* MeshComponent = NewActor->FindComponentByClass<UStaticMeshComponent>();

				//Procudual Mesh version
				NewActor->GeneratorWaveMesh(GridSize, PlaneSize);
				UProceduralMeshComponent* MeshComponent = NewActor->FindComponentByClass<UProceduralMeshComponent>();


				UMaterialInstanceDynamic* DynamicMaterialInstance = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, WaterMaterial);
				DynamicMaterialInstance->SetTextureParameterValue("VectorFiled", VectorFieldTex);
				DynamicMaterialInstance->SetTextureParameterValue("FieldNormal", NormalMapTex);

				//2^-1 2^-2 2^-3 均可以精确表示
				float EdgeValueX1 = FMath::Frac(AParticleWaveManager::UVScale1.X * x);
				float EdgeValueY1 = FMath::Frac(AParticleWaveManager::UVScale1.Y * y);

				float EdgeValueX2 = FMath::Frac(AParticleWaveManager::UVScale2.X * x);
				float EdgeValueY2 = FMath::Frac(AParticleWaveManager::UVScale2.Y * y);

				float EdgeValueX3 = FMath::Frac(AParticleWaveManager::UVScale3.X * x);
				float EdgeValueY3 = FMath::Frac(AParticleWaveManager::UVScale3.Y * y);


				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueX1", EdgeValueX1);
				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueY1", EdgeValueY1);

				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueX2", EdgeValueX2);
				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueY2", EdgeValueY2);

				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueX3", EdgeValueX3);
				DynamicMaterialInstance->SetScalarParameterValue("EdgeValueY3", EdgeValueY3);

				WaveParticleTileContainer.Emplace(NewActor);
				NewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
	//单位VectorField表示的虚幻长度
	TILE_SIZE_X2 = 2.f * PlaneSize.X * GridSize / 100.f / VectorFieldSize.X;

}

void AParticleWaveManager::UpdateParticle_GPU(float DeltaTime) {

	TSharedPtr<FWaveParticle_GPU, ESPMode::ThreadSafe> WaveParticleShared(WaveParticleGPU);
	UWorld* World = GetWorld();
	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	//保证一次数据的完整性
	FUpdateFieldStruct TempRenderData;
	FMemory::Memcpy(&TempRenderData, AParticleWaveManager::UpdateParticleData.Get(), sizeof(FUpdateFieldStruct));

	UTexture2D* CopyVectorFieldTexPtr = VectorFieldTex;

	ENQUEUE_RENDER_COMMAND(FWaveParticleBind)([this, FeatureLevel, WaveParticleShared, TempRenderData, CopyVectorFieldTexPtr](FRHICommandListImmediate& RHICmdList) {
		if (!GEngine->PreRenderDelegate.IsBoundToObject(this))
		{
			GEngine->PreRenderDelegate.AddWeakLambda(this, [FeatureLevel, WaveParticleShared, this, TempRenderData, CopyVectorFieldTexPtr]() {
				FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
				WaveParticleShared->UpdateWaveParticlePos(this->GetWorld()->DeltaTimeSeconds);
				WaveParticleShared->UpdateWaveParticleFiled(
					RHICmdList,
					TempRenderData,
					FeatureLevel,
					CopyVectorFieldTexPtr
				);
			});
		}
	});

}

void AParticleWaveManager::UpdateParticle(float DeltaTime) {

	CurFrame = (CurFrame + 1ul) & 1ul;
	TArray<float>& CurFrameVectorField = VectorField[CurFrame];
	TArray<float>& CurFrameNormal = NormalMapData[CurFrame];

	FMemory::Memzero(CurFrameVectorField.GetData(), 4 * sizeof(float) * VectorFieldSize.X * VectorFieldSize.Y);
	FMemory::Memzero(CurFrameNormal.GetData(), 4 * sizeof(float) * VectorFieldSize.X * VectorFieldSize.Y);

	float halfSize = FWaveParticle::Size * 0.5f;
	FVector2D MaxEdgeValue = FVector2D(VectorFieldSize) + halfSize;
	FVector2D MinEdgeValue(-halfSize, -halfSize);

	for (uint32 i = 0; i < ParticleNum; i++) {
		(*ParticlePosContainer)[i] += (*ParticleSpeedContainer)[i] * DeltaTime;

		//理论边缘值
		FVector2D StartPosition = (*ParticlePosContainer)[i] - FWaveParticle::Size * 0.5f;

		//对应边缘quad的向量场坐标
		FIntPoint StartPositionIndex(
			FMath::CeilToInt(StartPosition.X / VectorFieldDensityX),
			FMath::CeilToInt(StartPosition.Y / VectorFieldDensityY)
		);

		//对应边缘quad的实际Mesh坐标
		FVector2D RealStartPosition(StartPositionIndex.X * VectorFieldDensityX, StartPositionIndex.Y * VectorFieldDensityY);

		for (uint32 y = 0; y < FWaveParticle::height; y++) {
			for (uint32 x = 0; x < FWaveParticle::width; x++) {

				FVector2D CurPosition(RealStartPosition.X + x * VectorFieldDensityX, RealStartPosition.Y + y * VectorFieldDensityY);
				FVector2D offset((*ParticlePosContainer)[i] - CurPosition);
				FVector2D Direction;
				float length = 1.f;
				offset.ToDirectionAndLength(Direction, length);

				float param = FWaveParticle::Size * 0.5f < length ? 0.f : 1.f;
				float PlanePlacement = param * Beta * FMath::Sin(length / ParticleScale);
				FVector2D newpos = Direction * PlanePlacement;

				//等价于 2^32 - abs(x), x<0
				uint32 CurGridIndex_Y = static_cast<uint32>(StartPositionIndex.Y + y) & static_cast<uint32>(VectorFieldSize.Y - 1);
				uint32 CurGridIndex_X = static_cast<uint32>(StartPositionIndex.X + x) & static_cast<uint32>(VectorFieldSize.X - 1);

				uint32 gridindex = (VectorFieldSize.X * CurGridIndex_Y + CurGridIndex_X) * 4;
				CurFrameVectorField[gridindex] += newpos.X;
				CurFrameVectorField[gridindex + 1] += newpos.Y;
				CurFrameVectorField[gridindex + 2] += 0.5 * (FMath::Cos(length / ParticleScale) + 1.f) * param;
				CurFrameVectorField[gridindex + 3] = 0.f;
			}
		}
	}

	//calculate normal

	{
		for (int32 y = 0; y < VectorFieldSize.Y; ++y) {
			for (int32 x = 0; x < VectorFieldSize.X; ++x) {
				uint32 LeftIndex = static_cast<uint32>(x - 1) & (VectorFieldSize.X - 1);
				uint32 RightIndex = static_cast<uint32>(x + 1) & (VectorFieldSize.X - 1);
				uint32 TopIndex = static_cast<uint32>(y - 1) & (VectorFieldSize.Y - 1);
				uint32 DownIndex = static_cast<uint32>(y + 1) & (VectorFieldSize.Y - 1);
				//以当前点为原点，然后使在xy方向都增加dx dy
				//dx = dy = 1
				FVector CurPlacementWithHeight(
					CurFrameVectorField[(y * VectorFieldSize.X + x) * 4],
					CurFrameVectorField[(y * VectorFieldSize.X + x) * 4 + 1],
					CurFrameVectorField[(y * VectorFieldSize.X + x) * 4 + 2]
				);

				FVector RightPlacementWithHeight(
					CurFrameVectorField[(y * VectorFieldSize.X + RightIndex) * 4],
					CurFrameVectorField[(y * VectorFieldSize.X + RightIndex) * 4 + 1],
					CurFrameVectorField[(y * VectorFieldSize.X + RightIndex) * 4 + 2]
				);

				FVector DownPlacementWithHeight(
					CurFrameVectorField[(DownIndex * VectorFieldSize.Y + x) * 4 ],
					CurFrameVectorField[(DownIndex * VectorFieldSize.Y + x) * 4 + 1],
					CurFrameVectorField[(DownIndex * VectorFieldSize.Y + x) * 4 + 2]
				);

				FVector dx(1.0, 0.f, 0.f);
				FVector dy(0.f, 1.f, 0.f);

				FVector RightVec = dx + RightPlacementWithHeight - CurPlacementWithHeight;
				FVector DownVec = dy + DownPlacementWithHeight - CurPlacementWithHeight;

				FVector normal = FVector::CrossProduct(RightVec, DownVec).GetSafeNormal();

				int32 PlacementIndex = (y * VectorFieldSize.X + x) * 4;

				CurFrameNormal[PlacementIndex] = normal.X;
				CurFrameNormal[PlacementIndex + 1] = normal.Y;
				CurFrameNormal[PlacementIndex + 2] = normal.Z;
				CurFrameNormal[PlacementIndex + 3] = 0.f;

				//float leftZ = CurFrameVectorField[(y * VectorFieldSize.X + LeftIndex) * 4 + 2];
				//float RightZ = CurFrameVectorField[(y * VectorFieldSize.X + RightIndex) * 4 + 2];
				//float TopZ = CurFrameVectorField[(TopIndex * VectorFieldSize.Y + x) * 4 + 2];
				//float DownZ = CurFrameVectorField[(DownIndex * VectorFieldSize.Y + x) * 4 + 2];

				////gradient, only height
				//int32 PlacementIndex = (y * VectorFieldSize.X + x) * 4;
				//CurFrameNormal[PlacementIndex] = leftZ - RightZ;
				//CurFrameNormal[PlacementIndex + 1] = TopZ - DownZ;
				//CurFrameNormal[PlacementIndex + 2] = TILE_SIZE_X2;
				//CurFrameNormal[PlacementIndex + 3] = 0.f;
			}
		}
	}


	VectorFieldTex->UpdateTextureRegions(
		0,
		1,
		UpdateTextureRegion2D.Get(),
		sizeof(float) * 4 * VectorFieldSize.X,
		16,
		reinterpret_cast<uint8*>(CurFrameVectorField.GetData())
	);

	NormalMapTex->UpdateTextureRegions(
		0,
		1,
		UpdateTextureRegion2D.Get(),
		sizeof(float) * 4 * VectorFieldSize.X,
		16,
		reinterpret_cast<uint8*>(CurFrameNormal.GetData())
	);

}

#if WITH_EDITOR
static FName Name_PlaneSize = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, PlaneSize);
static FName Name_VectorFieldSize = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, VectorFieldSize);
static FName Name_MaxParticleSpeed = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, MaxParticleSpeed);
static FName Name_MinParticleSpeed = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, MinParticleSpeed);
static FName Name_ParticleNum = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, ParticleNum);
static FName Name_ParticleSize = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, ParticleScale);
static FName Name_Beta = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, Beta);
static FName Name_GridSize = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, GridSize);
static FName Name_TileSize = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, TileSize);
static FName Name_Material = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, WaterMaterial);
static FName Name_WaveClassType = GET_MEMBER_NAME_CHECKED(AParticleWaveManager, WaveClassType);

void AParticleWaveManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {

	UProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;

	bool bWaveProperyChanged =
		MemberPropertyName == Name_PlaneSize ||
		MemberPropertyName == Name_VectorFieldSize ||
		MemberPropertyName == Name_MaxParticleSpeed ||
		MemberPropertyName == Name_MinParticleSpeed ||
		MemberPropertyName == Name_ParticleNum ||
		MemberPropertyName == Name_ParticleSize ||
		MemberPropertyName == Name_Beta ||
		MemberPropertyName == Name_GridSize ||
		MemberPropertyName == Name_TileSize ||
		MemberPropertyName == Name_Material ||
		MemberPropertyName == Name_WaveClassType;

	if (bWaveProperyChanged)
	{
		bHasInit = false;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
