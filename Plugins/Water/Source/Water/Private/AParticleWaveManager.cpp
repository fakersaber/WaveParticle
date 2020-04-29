// Fill out your copyright notice in the Description page of Project Settings.


#include "AParticleWaveManager.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "WaveParticleTile.h"
#include "RHI.h"

static const float Y_PI = 3.1415926535897932f;
const FVector2D AParticleWaveManager::UVScale1 = FVector2D(0.125f,0.125f);
const FVector2D AParticleWaveManager::UVScale2 = FVector2D(0.25f, 0.25f);
const FVector2D AParticleWaveManager::UVScale3 = FVector2D(0.5f, 0.5f);


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
void AParticleWaveManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (WaveClassType && WaterMaterial && WaterMesh) {
		if (!bHasInit) {
			ClearResource();

			InitWaveParticle();
			bHasInit = true;
		}

		UpdateParticle(DeltaTime);
	}
}

void AParticleWaveManager::ClearResource() {

	ParticleContainer.Reset();

	UpdateTextureRegion2D.Reset();

	//#TODO 只有当改变与mesh相关参数才重新生成TileActor
	for (auto TileActor : WaveParticleTileContainer) {
		TileActor->Destroy();
	}

	WaveParticleTileContainer.Reset();
}
void AParticleWaveManager::Destroyed() {

	for (auto TileActor : WaveParticleTileContainer) {
		TileActor->Destroy();
	}

	Super::Destroyed();
}

void AParticleWaveManager::InitWaveParticle() {

	for (int i = 0; i < UE_ARRAY_COUNT(VectorField); ++i) {
		VectorField[i].SetNumZeroed(VectorFieldSize.X * VectorFieldSize.Y * 4);
		NormalMapData[i].SetNumZeroed(VectorFieldSize.X * VectorFieldSize.Y * 4);
	}


	WaveParticleTileContainer.Reserve(TileSize.X * TileSize.Y);

	//求单位向量场所的quad大小
	VectorFieldDensityX = static_cast<float>(PlaneSize.X) / VectorFieldSize.X;
	VectorFieldDensityY = static_cast<float>(PlaneSize.Y) / VectorFieldSize.Y;

	FWaveParticle::Size = 2 * 3.14151926f * ParticleScale;
	FWaveParticle::width = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityX);
	FWaveParticle::height = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityY);

	//init particle position and speed
	{
		//分段函数,速度转换的线性函数[0,0.5) 为负，[0.5,1]为正
		//f(x) = (max - min) * x +- min;
		auto SpeedK = MaxParticleSpeed - MinParticleSpeed;
		FMath::RandInit(static_cast<int32>(FDateTime::Now().GetTicks() % INT_MAX));
		for (int i = 0; i < ParticleNum; ++i) {
			//保证最小移动距离
			FVector2D Position(FMath::Rand() % PlaneSize.X, FMath::Rand() % PlaneSize.Y);
			auto RandomSpeedX = FMath::FRand() * 2.f - 1.f;
			auto RandomSpeedY = FMath::FRand() * 2.f - 1.f;
			auto Sign_X = FMath::Sign(RandomSpeedX);
			auto Sign_Y = FMath::Sign(RandomSpeedY);
			RandomSpeedX = Sign_Y < 0 ? RandomSpeedX * SpeedK - MinParticleSpeed : RandomSpeedX * SpeedK + MinParticleSpeed;
			RandomSpeedY = Sign_Y < 0 ? RandomSpeedY * SpeedK - MinParticleSpeed : RandomSpeedY * SpeedK + MinParticleSpeed;
			FVector2D Speed(RandomSpeedX, RandomSpeedY);
			ParticleContainer.Emplace(Position, Speed);
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


	//Spawn Actor
	{ 
		FVector2D TileMeshSize(PlaneSize.X * GridSize, PlaneSize.Y * GridSize);

		FVector2D HalfTileMeshSize(TileMeshSize * 0.5f);

		FVector2D Center(TileSize.X * HalfTileMeshSize.X, TileSize.Y * HalfTileMeshSize.Y);

		for (int y = 0; y < TileSize.Y; ++y) {
			for (int x = 0; x < TileSize.X; ++x) {

				FVector PositionOffset(FVector2D(x * TileMeshSize.X + HalfTileMeshSize.X, y * TileMeshSize.Y + HalfTileMeshSize.Y) - Center, 0.f);
				auto NewActor = GetWorld()->SpawnActor<AWaveParticleTile>(WaveClassType, GetActorLocation() + PositionOffset, GetActorRotation());
				NewActor->GeneratorStaticMesh(WaterMesh);

				//NewActor->GeneratorWaveMesh(GridSize, PlaneSize);

				UStaticMeshComponent* MeshComponent = NewActor->FindComponentByClass<UStaticMeshComponent>();
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


void AParticleWaveManager::UpdateParticle(float DeltaTime) {

	CurFrame = (CurFrame + 1ul) & 1ul;
	TArray<float>& CurFrameVectorField = VectorField[CurFrame];
	TArray<float>& CurFrameNormal = NormalMapData[CurFrame];

	FMemory::Memzero(CurFrameVectorField.GetData(), 4 * sizeof(float) * VectorFieldSize.X * VectorFieldSize.Y);
	FMemory::Memzero(CurFrameNormal.GetData(), 4 * sizeof(float) * VectorFieldSize.X * VectorFieldSize.Y);

	for (uint32 i = 0; i < ParticleNum; i++) {

		ParticleContainer[i].Position += ParticleContainer[i].Speed * DeltaTime;

		//理论边缘值
		FVector2D StartPosition = ParticleContainer[i].Position - FWaveParticle::Size * 0.5f;

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
				FVector2D offset(ParticleContainer[i].Position - CurPosition);
				FVector2D Direction;
				float length = 1.f;
				offset.ToDirectionAndLength(Direction, length);

				float param = FWaveParticle::Size * 0.5f < length ? 0.f : 1.f;
				float PlanePlacement = param * Beta * FMath::Sin(length / ParticleScale);
				FVector2D newpos = Direction * PlanePlacement;

				uint32 gridindex = (VectorFieldSize.X * ((StartPositionIndex.Y + y) % VectorFieldSize.Y) + ((x + StartPositionIndex.X) % VectorFieldSize.X)) * 4;
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
				
				uint32 LeftIndex = (x - 1) & (VectorFieldSize.X - 1);
				uint32 RightIndex = (x + 1) & (VectorFieldSize.X - 1);

				uint32 TopIndex = (y - 1) & (VectorFieldSize.Y - 1);
				uint32 DownIndex = (y + 1) & (VectorFieldSize.Y - 1);

				float leftZ = CurFrameVectorField[(y * VectorFieldSize.X + LeftIndex) * 4 + 2];
				float RightZ = CurFrameVectorField[(y * VectorFieldSize.X + RightIndex) * 4 + 2];

				float TopZ = CurFrameVectorField[(TopIndex * VectorFieldSize.Y + x) * 4 + 2];
				float DownZ = CurFrameVectorField[(DownIndex * VectorFieldSize.Y + x) * 4 + 2];
				
				//gradient
				int32 PlacementIndex = (y * VectorFieldSize.X + x) * 4;
				CurFrameNormal[PlacementIndex] = leftZ - RightZ;
				CurFrameNormal[PlacementIndex + 1] = TopZ - DownZ;
				CurFrameNormal[PlacementIndex + 2] = TILE_SIZE_X2;
				CurFrameNormal[PlacementIndex + 3] = 0.f;
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
static FName Name_PlaneSize = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, PlaneSize);
static FName Name_VectorFieldSize = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, VectorFieldSize);
static FName Name_MaxParticleSpeed = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, MaxParticleSpeed);
static FName Name_MinParticleSpeed = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, MinParticleSpeed);
static FName Name_ParticleNum = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, ParticleNum);
static FName Name_ParticleSize = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, ParticleScale);
static FName Name_Beta = GET_MEMBER_NAME_CHECKED( AParticleWaveManager, Beta);
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

//for (int i = 0; i < ParticleNum; ++i) {

//	float halfPaticleQuadSize = ParticleSize * 0.5f;

//	//转换到向量场单位，四舍五入
//	int halfParticleFieldSize_X = FMath::CeilToInt(halfPaticleQuadSize / VectorFieldDensityX);
//	int halfParticleFieldSize_Y = FMath::CeilToInt(halfPaticleQuadSize / VectorFieldDensityY);

//	//转换Position,为向量场单位，直接截取到整数
//	FIntPoint CurParticleVectorFieldPos(
//		ParticleContainer[i].Position.X / VectorFieldDensityX,
//		ParticleContainer[i].Position.Y / VectorFieldDensityY
//	);

//	int StartFieldIndex_X = FMath::Max(CurParticleVectorFieldPos.X - halfParticleFieldSize_X, 0);
//	int EndFieldIndex_X = FMath::Min(CurParticleVectorFieldPos.X + halfParticleFieldSize_X, VectorFieldSize.X - 1);
//	int StartFieldIndex_Y = FMath::Max(CurParticleVectorFieldPos.Y - halfParticleFieldSize_Y, 0);
//	int EndFieldIndex_Y = FMath::Min(CurParticleVectorFieldPos.Y + halfParticleFieldSize_Y, VectorFieldSize.Y - 1);

//	//注意这个k换算的是向量场 hlsl rsqrt
//	float k_x = Y_PI / halfParticleFieldSize_X;
//	float k_y = Y_PI / halfParticleFieldSize_Y;
//	float k_z = Y_PI * FMath::InvSqrt(static_cast<float>(halfParticleFieldSize_X * halfParticleFieldSize_X + halfParticleFieldSize_Y * halfParticleFieldSize_Y));

//	for (int FieldIndex_X = StartFieldIndex_X; FieldIndex_X <= EndFieldIndex_X; ++FieldIndex_X) {
//		for (int FieldIndex_Y = StartFieldIndex_Y; FieldIndex_Y <= EndFieldIndex_Y; ++FieldIndex_Y) {

//			FIntPoint offset(FieldIndex_X - CurParticleVectorFieldPos.X, FieldIndex_Y - CurParticleVectorFieldPos.Y);

//			float CurSize = FMath::Sqrt(offset.X * offset.X + offset.Y * offset.Y);
//			float t_x = FMath::Clamp(-offset.X * k_x, -Y_PI, Y_PI);
//			float t_y = FMath::Clamp(-offset.Y * k_y, -Y_PI, Y_PI); ;
//			float t_z = FMath::Clamp(CurSize * k_z, 0.f, Y_PI);
//			
//			int Index = (FieldIndex_Y * VectorFieldSize.X + FieldIndex_X) * 4;

//			VectorField[Index] += (t_x - Beta * FMath::Sin(t_x)) / Y_PI ;
//			VectorField[Index + 1] += (t_y - Beta * FMath::Sin(t_y)) / Y_PI;
//			VectorField[Index + 2] += 0.5f * (FMath::Cos(t_z) + 1.f);
//			VectorField[Index + 3] = 0.f;
//		}
//	}

//	//更新粒子位置,移动单位为Field，但Position单位为quad
//	float NewPosition_X = ParticleContainer[i].Position.X + DeltaTime * ParticleContainer[i].Speed.X * VectorFieldDensityX;
//	float NewPosition_Y = ParticleContainer[i].Position.Y + DeltaTime * ParticleContainer[i].Speed.Y * VectorFieldDensityY;

//	//边界值
//	float LeftStart_X = -halfPaticleQuadSize;
//	float RightStart_X = halfPaticleQuadSize + PlaneSize.X;
//	float TopStart_Y = -halfPaticleQuadSize;
//	float DownStart_Y = halfPaticleQuadSize + PlaneSize.Y;

//	//为了gpu加速
//	NewPosition_X = NewPosition_X < LeftStart_X ? RightStart_X : NewPosition_X;
//	NewPosition_X = NewPosition_X > RightStart_X ? LeftStart_X : NewPosition_X;
//	NewPosition_Y = NewPosition_Y < TopStart_Y ? DownStart_Y : NewPosition_Y;
//	NewPosition_Y = NewPosition_Y > DownStart_Y ? TopStart_Y : NewPosition_Y;

//	ParticleContainer[i].Position.X = NewPosition_X;
//	ParticleContainer[i].Position.Y = NewPosition_Y;
//}