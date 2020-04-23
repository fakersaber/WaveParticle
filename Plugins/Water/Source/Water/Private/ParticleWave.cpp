// Fill out your copyright notice in the Description page of Project Settings.


#include "ParticleWave.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

static const float Y_PI = 3.1415926535897932f;
float FWaveParticle::Size = 0.f;
uint32 FWaveParticle::width = 0;
uint32 FWaveParticle::height = 0;

// Sets default values
AParticleWave::AParticleWave()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ParticleNum = 1;
	ParticleScale = 1.f;
	//32x32顶点的Plane
	PlaneSize.X = 63;
	PlaneSize.Y = 63;
	VectorFieldSize.X = 64;
	VectorFieldSize.Y = 64;
	MaxParticleSpeed = 1.f;
	MinParticleSpeed = 0.f;
	Beta = 0.5f;
	bHasInit = false;

	//WaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaveMesh"));
}

// Called when the game starts or when spawned
void AParticleWave::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AParticleWave::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	if (!bHasInit) {
		ClearResource();

		InitWaveParticle();
		bHasInit = true;
	}

	UpdateParticle(DeltaTime);
}

void AParticleWave::ClearResource() {

	ParticleContainer.Reset();

	UpdateTextureRegion2D.Reset();

	//if (WaveMesh){
	//	WaveMesh->ClearAllMeshSections();
	//}
}


void AParticleWave::InitWaveParticle() {


	VectorField.SetNumZeroed(VectorFieldSize.X * VectorFieldSize.Y * 4);

	//求单位向量场所的quad大小
	VectorFieldDensityX = static_cast<float>(PlaneSize.X) / VectorFieldSize.X;
	VectorFieldDensityY = static_cast<float>(PlaneSize.Y) / VectorFieldSize.Y;

	FWaveParticle::Size = 2 * 3.14151926f * ParticleScale;
	FWaveParticle::width = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityX);
	FWaveParticle::height = FMath::CeilToInt(FWaveParticle::Size / VectorFieldDensityY);


	//init Mesh
	{
		//GeneratorWaveMesh();
	}


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
		UpdateTextureRegion2D = MakeShared<FUpdateTextureRegion2D>(0, 0, 0, 0, VectorFieldSize.X, VectorFieldSize.Y);
	}

	//Init Material
	{
		//UStaticMeshComponent* StaticMeshComponent = FindComponentByClass<UStaticMeshComponent>();
		//check(StaticMeshComponent);
		TArray<UStaticMeshComponent*> AllMeshComponent;
		GetComponents(AllMeshComponent);

		for (UStaticMeshComponent* MeshComponent : AllMeshComponent) {
			UMaterialInterface* mat = MeshComponent->GetMaterial(0);
			UMaterialInstanceDynamic* DynamicMaterialInstance = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, mat);
			DynamicMaterialInstance->SetTextureParameterValue("VectorFiled", VectorFieldTex);
		}
	}

}


void AParticleWave::GeneratorWaveMesh() {

	WaveMesh->GetBodySetup()->bNeverNeedsCookedCollisionData = true;

	TArray<int32> Triangles;

	TArray<FVector> WaveVertices;

	TArray<FVector2D> UVs;

	FVector2D Center(PlaneSize.X * 0.5f, PlaneSize.Y * 0.5f);

	for (int j = 0; j < PlaneSize.Y + 1; j++) {
		for (int i = 0; i < PlaneSize.X + 1; i++) {
			UVs.Emplace(i / static_cast<float>(PlaneSize.X), j / static_cast<float>(PlaneSize.Y));
			WaveVertices.Emplace(i - Center.X, j - Center.Y, 0.f);
		}
	}

	for (int j = 0; j < PlaneSize.Y; j++) {
		for (int i = 0; i < PlaneSize.X; i++) {
			int idx = j * (PlaneSize.X + 1) + i;
			Triangles.Add(idx);
			Triangles.Add(idx + PlaneSize.X + 1);
			Triangles.Add(idx + 1);

			Triangles.Add(idx + 1);
			Triangles.Add(idx + PlaneSize.X + 1);
			Triangles.Add(idx + PlaneSize.X + 1 + 1);
		}
	}

	WaveMesh->CreateMeshSection(0, WaveVertices, Triangles, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}


void AParticleWave::UpdateParticle(float DeltaTime) {

	FMemory::Memzero(VectorField.GetData(), 4 * sizeof(float) * VectorFieldSize.X * VectorFieldSize.Y);


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
				float length = 1;
				offset.ToDirectionAndLength(Direction, length);

				float param = FWaveParticle::Size * 0.5f < length ? 0.f : 1.f;
				float PlanePlacement = param * Beta * FMath::Sin(length / ParticleScale);
				FVector2D newpos = Direction * PlanePlacement;

				uint32 gridindex = (VectorFieldSize.X * ((StartPositionIndex.Y + y) % VectorFieldSize.Y) + ((x + StartPositionIndex.X) % VectorFieldSize.X)) * 4;
				VectorField[gridindex] += newpos.X;
				VectorField[gridindex + 1] += newpos.Y;
				VectorField[gridindex + 2] += 0.5 * (FMath::Cos(length / ParticleScale) + 1) * param;
				VectorField[gridindex + 3] = 0.f;
			}
		}
	}

	VectorFieldTex->UpdateTextureRegions(
		0,
		1,
		UpdateTextureRegion2D.Get(),
		sizeof(float) * 4 * VectorFieldSize.X,
		16,
		reinterpret_cast<uint8*>(VectorField.GetData())
	);

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

}


static FName Name_PlaneSize = GET_MEMBER_NAME_CHECKED(AParticleWave, PlaneSize);
static FName Name_VectorFieldSize = GET_MEMBER_NAME_CHECKED(AParticleWave, VectorFieldSize);
static FName Name_MaxParticleSpeed = GET_MEMBER_NAME_CHECKED(AParticleWave, MaxParticleSpeed);
static FName Name_MinParticleSpeed = GET_MEMBER_NAME_CHECKED(AParticleWave, MinParticleSpeed);
static FName Name_ParticleNum = GET_MEMBER_NAME_CHECKED(AParticleWave, ParticleNum);
static FName Name_ParticleSize = GET_MEMBER_NAME_CHECKED(AParticleWave, ParticleScale);
static FName Name_Beta = GET_MEMBER_NAME_CHECKED(AParticleWave, Beta);

void AParticleWave::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {

	UProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;

	bool bWaveProperyChanged =
		MemberPropertyName == Name_PlaneSize ||
		MemberPropertyName == Name_VectorFieldSize ||
		MemberPropertyName == Name_MaxParticleSpeed ||
		MemberPropertyName == Name_MinParticleSpeed ||
		MemberPropertyName == Name_ParticleNum ||
		MemberPropertyName == Name_ParticleSize ||
		MemberPropertyName == Name_Beta;

	if (bWaveProperyChanged)
	{
		bHasInit = false;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

