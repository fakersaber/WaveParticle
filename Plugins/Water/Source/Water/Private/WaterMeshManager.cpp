#include "WaterMeshManager.h"
#include "WaterInstanceMeshComponent.h"
#include "ConvexVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#define INSTANCE_COUNT 1024



FVector FWaterInstanceQuadTree::MinBound = FVector(0.f);

FWaterInstanceQuadTree::FWaterInstanceQuadTree(const FBoxSphereBounds& InNodeBound)
	:
	NodeBound(InNodeBound)
{
	FMemory::Memzero(SubTrees, sizeof(SubTrees));
}

FWaterInstanceQuadTree::~FWaterInstanceQuadTree() {

	for (auto& SubNode : SubTrees) {
		delete SubNode;
		SubNode = nullptr;
	}

}

bool IntersectBox8Plane(const FVector& InOrigin, const FVector& InExtent, const FPlane* PermutedPlanePtr)
{
	// this removes a lot of the branches as we know there's 8 planes
	// copied directly out of ConvexVolume.cpp
	const VectorRegister Origin = VectorLoadFloat3(&InOrigin);
	const VectorRegister Extent = VectorLoadFloat3(&InExtent);

	const VectorRegister PlanesX_0 = VectorLoadAligned(&PermutedPlanePtr[0]);
	const VectorRegister PlanesY_0 = VectorLoadAligned(&PermutedPlanePtr[1]);
	const VectorRegister PlanesZ_0 = VectorLoadAligned(&PermutedPlanePtr[2]);
	const VectorRegister PlanesW_0 = VectorLoadAligned(&PermutedPlanePtr[3]);

	const VectorRegister PlanesX_1 = VectorLoadAligned(&PermutedPlanePtr[4]);
	const VectorRegister PlanesY_1 = VectorLoadAligned(&PermutedPlanePtr[5]);
	const VectorRegister PlanesZ_1 = VectorLoadAligned(&PermutedPlanePtr[6]);
	const VectorRegister PlanesW_1 = VectorLoadAligned(&PermutedPlanePtr[7]);

	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Origin, 0);
	VectorRegister OrigY = VectorReplicate(Origin, 1);
	VectorRegister OrigZ = VectorReplicate(Origin, 2);
	// Splat the already abs Extent for the push out calculation
	VectorRegister AbsExtentX = VectorReplicate(Extent, 0);
	VectorRegister AbsExtentY = VectorReplicate(Extent, 1);
	VectorRegister AbsExtentZ = VectorReplicate(Extent, 2);

	// Calculate the distance (x * x) + (y * y) + (z * z) - w
	VectorRegister DistX_0 = VectorMultiply(OrigX, PlanesX_0);
	VectorRegister DistY_0 = VectorMultiplyAdd(OrigY, PlanesY_0, DistX_0);
	VectorRegister DistZ_0 = VectorMultiplyAdd(OrigZ, PlanesZ_0, DistY_0);
	VectorRegister Distance_0 = VectorSubtract(DistZ_0, PlanesW_0);
	// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
	VectorRegister PushX_0 = VectorMultiply(AbsExtentX, VectorAbs(PlanesX_0));
	VectorRegister PushY_0 = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY_0), PushX_0);
	VectorRegister PushOut_0 = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ_0), PushY_0);

	// Check for completely outside
	if (VectorAnyGreaterThan(Distance_0, PushOut_0))
	{
		return false;
	}

	// Calculate the distance (x * x) + (y * y) + (z * z) - w
	VectorRegister DistX_1 = VectorMultiply(OrigX, PlanesX_1);
	VectorRegister DistY_1 = VectorMultiplyAdd(OrigY, PlanesY_1, DistX_1);
	VectorRegister DistZ_1 = VectorMultiplyAdd(OrigZ, PlanesZ_1, DistY_1);
	VectorRegister Distance_1 = VectorSubtract(DistZ_1, PlanesW_1);
	// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
	VectorRegister PushX_1 = VectorMultiply(AbsExtentX, VectorAbs(PlanesX_1));
	VectorRegister PushY_1 = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY_1), PushX_1);
	VectorRegister PushOut_1 = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ_1), PushY_1);

	// Check for completely outside
	if (VectorAnyGreaterThan(Distance_1, PushOut_1))
	{
		return false;
	}
	return true;
}

//空间复杂度：O(n * log4(n))
//时间复杂度：最好O(1), 最坏O((4n - 1)/3)
void FWaterInstanceQuadTree::FrustumCull(
	FWaterInstanceQuadTree* RootNode, 
	FWaterInstanceMeshManager* ManagerPtr,
	const FMatrix& ProjMatrix,
	const FVector& ViewOrigin
) 
{
	const FPlane* PermutedPlanePtr = ManagerPtr->GetViewFrustum().PermutedPlanes.GetData();
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer = ManagerPtr->GetWaterMeshLODs();
	bool bIsCulling = IntersectBox8Plane(RootNode->NodeBound.Origin, RootNode->NodeBound.BoxExtent, PermutedPlanePtr);

	if (!bIsCulling) {
		for (const uint32 RemoveNodeIndex : RootNode->NodesIndeces) {
			FWaterMeshLeafNodeData* RemoveDataNode = ManagerPtr->GetWaterMeshNodeData().Find(RemoveNodeIndex);
			check(RemoveDataNode);
			if (RemoveDataNode->bIsFading) {
				continue;
			}
			RemoveInstanceNode(RemoveDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex(), ManagerPtr->GetWaterMeshNodeData());

			RemoveDataNode->bIsFading = true;
		}
		return;
	}

	for (FWaterInstanceQuadTree* NodePtr : RootNode->SubTrees) {
		if (NodePtr != nullptr) {
			FrustumCull(NodePtr, ManagerPtr, ProjMatrix, ViewOrigin);
		}
	}

	//通过测试的节点如果是最小粒度
	FVector2D CurBoundSize = FVector2D(RootNode->NodeBound.BoxExtent.X, RootNode->NodeBound.BoxExtent.Y);
	if (FMath::IsNearlyEqual(CurBoundSize.X, MinBound.X) && FMath::IsNearlyEqual(CurBoundSize.Y, MinBound.Y)) {

		FWaterMeshLeafNodeData* LeafDataNode = ManagerPtr->GetWaterMeshNodeData().Find(RootNode->NodesIndeces[0]);

		uint8 CurLODIndex = RootNode->CalclateLODIndex(ProjMatrix, ViewOrigin);

		//1. LastFrame是Culling状态，直接添加
		if (LeafDataNode->bIsFading) {
			AddInstanceNode(RootNode->NodesIndeces[0], CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		//2. LastFrame不是Culling状态，但是LOD发生变化
		else if (CurLODIndex != LeafDataNode->LastLodIndex) {
			RemoveInstanceNode(LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex(), ManagerPtr->GetWaterMeshNodeData());

			AddInstanceNode(RootNode->NodesIndeces[0], CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		LeafDataNode->LastLodIndex = CurLODIndex;

		LeafDataNode->bIsFading = false;
	}
}

void FWaterInstanceQuadTree::InitWaterMeshQuadTree(FWaterInstanceMeshManager* ManagerPtr, int32& LeafNodeIndex) {

	//保存当前节点的的起始NodeIndex
	int32 CurLeafNodeIndex = LeafNodeIndex;

	if (bIsLeafNode()) {

		NodesIndeces.Emplace(LeafNodeIndex);

		TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData = ManagerPtr->GetWaterMeshNodeData();

		WaterMeshNodeData.Emplace(LeafNodeIndex, FWaterMeshLeafNodeData(FTransform(NodeBound.Origin)));

		LeafNodeIndex += 1;

		return;
	}
		
	Split();

	for (auto NodePtr : SubTrees) {
		check(NodePtr);
		NodePtr->InitWaterMeshQuadTree(ManagerPtr, LeafNodeIndex);
	}

	for (int Index = CurLeafNodeIndex; Index < LeafNodeIndex; ++Index) {
		NodesIndeces.Emplace(Index);
	}


}

void FWaterInstanceQuadTree::MeshTransformBy(const FTransform LocalToWorld){

	NodeBound.TransformBy(LocalToWorld);

	for (auto NodePtr : SubTrees) {
		if (!NodePtr)
			continue;
		NodePtr->MeshTransformBy(LocalToWorld);
	}
}



bool FWaterInstanceQuadTree::bIsLeafNode() {

	return (FMath::IsNearlyEqual(NodeBound.BoxExtent.X, MinBound.X) && FMath::IsNearlyEqual(NodeBound.BoxExtent.Y, MinBound.Y));
}


void FWaterInstanceQuadTree::Split() {
	/************************************************************************
	*
	*   ------------>X
	*   |
	*	|
	*	|
	*	Y
	*************************************************************************/
	FVector HalfExtent = FVector(NodeBound.BoxExtent.X * 0.5f, NodeBound.BoxExtent.Y * 0.5f, FWaterInstanceQuadTree::MinBound.Z);

	FVector LeftTopOrigin = FVector(NodeBound.Origin.X - HalfExtent.X, NodeBound.Origin.Y - HalfExtent.Y, NodeBound.Origin.Z);
	FVector RightTopOrigin = FVector(NodeBound.Origin.X + HalfExtent.X, NodeBound.Origin.Y - HalfExtent.Y, NodeBound.Origin.Z);
	FVector LeftDownOrigin = FVector(NodeBound.Origin.X - HalfExtent.X, NodeBound.Origin.Y + HalfExtent.Y, NodeBound.Origin.Z);
	FVector RightDownOrigin = FVector(NodeBound.Origin.X + HalfExtent.X, NodeBound.Origin.Y + HalfExtent.Y, NodeBound.Origin.Z);

	SubTrees[LeftTop] = new FWaterInstanceQuadTree(FBoxSphereBounds(LeftTopOrigin, HalfExtent, HalfExtent.Size()));
	SubTrees[RightTop] = new FWaterInstanceQuadTree(FBoxSphereBounds(RightTopOrigin, HalfExtent, HalfExtent.Size()));
	SubTrees[LeftDown] = new FWaterInstanceQuadTree(FBoxSphereBounds(LeftDownOrigin, HalfExtent, HalfExtent.Size()));
	SubTrees[RightDown] = new FWaterInstanceQuadTree(FBoxSphereBounds(RightDownOrigin, HalfExtent, HalfExtent.Size()));
}

uint8 FWaterInstanceQuadTree::CalclateLODIndex(const FMatrix& ProjMatrix, const FVector& ViewOrigin){

	// ignore perspective foreshortening for orthographic projections
	//const float DistSqr = FVector::DistSquared(NodeBound.Origin, ViewOrigin) * ProjMatrix.M[2][3];

	////原范围(描述NDC)为[-1,1]，且NodeBound.Sphere必为正，所以直接根据比例缩小一半到[0,1]
	//const float ScreenMultiple = FMath::Max(0.5f * ProjMatrix.M[0][0], 0.5f * ProjMatrix.M[1][1]);

	//// 再透视除法，不过距离并非严格使用ViewSpace的Z,而是直接使用距离,原因未知
	//// 不过Dis >= Z
	//return FMath::Square(ScreenMultiple * NodeBound.SphereRadius) / FMath::Max(1.0f, DistSqr);

	return 0;

}


void FWaterInstanceQuadTree::RemoveInstanceNode(
	FWaterMeshLeafNodeData* RemoveDataNode,
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer, 
	TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex,
	TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData
){
	check(RemoveDataNode->InstanceIndex > 0);

	UWaterInstanceMeshComponent* InstanceComponent = InstanceLODContainer[RemoveDataNode->LastLodIndex];
	TMap<uint32, uint32>& InstanceToNodeMap = InstanceIdToNodeIndex[RemoveDataNode->LastLodIndex];

	const uint32 EndInstanceId = InstanceComponent->GetInstanceCount();
	uint32* EndNodeIndexPtr = InstanceToNodeMap.Find(EndInstanceId);
	uint32* RemoveNodeIndexPtr = InstanceToNodeMap.Find(RemoveDataNode->InstanceIndex);

	FWaterMeshLeafNodeData* EndDataNode = WaterMeshNodeData.Find(*EndNodeIndexPtr);

	//修改节点数据
	EndDataNode->InstanceIndex = RemoveDataNode->InstanceIndex;

	//修改索引表
	*RemoveNodeIndexPtr = *EndNodeIndexPtr;

	InstanceComponent->RemoveInstance(RemoveDataNode->InstanceIndex - 1);
}


void FWaterInstanceQuadTree::AddInstanceNode(
	uint32 NodeIndex, 
	uint8 CurLODIndex, 
	FWaterMeshLeafNodeData* LeafDataNode,
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer,
	TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex
){

	int32 InstanceIndex = InstanceLODContainer[CurLODIndex]->AddInstance(LeafDataNode->RelativeTransform);
	
	uint32* NodeIndexPtr = InstanceIdToNodeIndex[CurLODIndex].Find(InstanceIndex);

	LeafDataNode->InstanceIndex = InstanceIndex;

	*NodeIndexPtr = NodeIndex;
}


FWaterInstanceMeshManager::FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos){

	//Z暂定为Extent的一半
	FWaterInstanceQuadTree::MinBound = FVector(PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.25f);

	float ExtentSize = PlaneSize * GridSize * TileSize * 0.5f;

	FVector Extent = FVector(ExtentSize, ExtentSize, FWaterInstanceQuadTree::MinBound.Z);

	InstanceMeshTree = MakeShared<FWaterInstanceQuadTree>(
		FBoxSphereBounds(CenterPos, Extent, Extent.Size())
	);
}


void FWaterInstanceMeshManager::Initial(const FTransform& LocalToWorld, const TArray<UStaticMesh*>& LodAsset, uint32 InstanceCount, AActor* OuterActor, UMaterialInterface* WaterMaterial) {

	int32 NodeIndex = 0;

	//初始化四叉树并且填充WaterMeshLeafNodeData容器
	InstanceMeshTree.Get()->InitWaterMeshQuadTree(this, NodeIndex);
	check(NodeIndex == InstanceCount);

	//初始化Mesh的Transform,在此初始化原因是原始四叉树需要RelativeTransform
	InstanceMeshTree.Get()->MeshTransformBy(LocalToWorld);

	//填充InstanceIdToNodeIndex容器
	for (int32 i = 0; i < 3; ++i) {

		TMap<uint32, uint32>* CurMapContainer = new (InstanceIdToNodeIndex) TMap<uint32, uint32>();

		//InstanceId从1开始
		for (uint32 InstanceID = 1; InstanceID <= InstanceCount; ++InstanceID) {
			CurMapContainer->Emplace(InstanceID, 0xfffffffful);
		}
	}

	//填充Instance容器
	for (int32 LodIndex = 0; LodIndex < 3; ++LodIndex) {

		const auto Index = WaterMeshLODs.AddUninitialized(1);

		UWaterInstanceMeshComponent** CurMeshInstanceComponentPtr = &WaterMeshLODs[Index];

		*CurMeshInstanceComponentPtr = NewObject<UWaterInstanceMeshComponent>(OuterActor, UWaterInstanceMeshComponent::StaticClass(),NAME_None);

		(*CurMeshInstanceComponentPtr)->RegisterComponent();

		(*CurMeshInstanceComponentPtr)->SetWorldTransform(LocalToWorld);

		//(*CurMeshInstanceComponentPtr)->AddToRoot();

		(*CurMeshInstanceComponentPtr)->SetStaticMesh(LodAsset[LodIndex]);

		(*CurMeshInstanceComponentPtr)->SetMaterial(0, WaterMaterial);

		//MeshComponent->SetCustomPrimitiveDataVector2(0, FVector2D(FMath::Frac(AParticleWaveManager::UVScale1.X * x), FMath::Frac(AParticleWaveManager::UVScale1.Y * y)));
		//MeshComponent->SetCustomPrimitiveDataVector2(2, FVector2D(FMath::Frac(AParticleWaveManager::UVScale2.X * x), FMath::Frac(AParticleWaveManager::UVScale2.Y * y)));
		//MeshComponent->SetCustomPrimitiveDataVector2(4, FVector2D(FMath::Frac(AParticleWaveManager::UVScale3.X * x), FMath::Frac(AParticleWaveManager::UVScale3.Y * y)));
	}
	
}

void FWaterInstanceMeshManager::MeshCulling(ULocalPlayer* LocalPlayer){

	FSceneViewProjectionData SceneViewProjection;

	const auto& MiniInfo = LocalPlayer->GetPlayerController(nullptr)->PlayerCameraManager->GetCameraCachePOV();

	FViewport* Viewport = LocalPlayer->ViewportClient->Viewport;

	int32 X = FMath::TruncToInt(LocalPlayer->Origin.X * Viewport->GetSizeXY().X);
	int32 Y = FMath::TruncToInt(LocalPlayer->Origin.Y * Viewport->GetSizeXY().Y);

	X += Viewport->GetInitialPositionXY().X;
	Y += Viewport->GetInitialPositionXY().Y;

	uint32 SizeX = FMath::TruncToInt(LocalPlayer->Size.X * Viewport->GetSizeXY().X);
	uint32 SizeY = FMath::TruncToInt(LocalPlayer->Size.Y * Viewport->GetSizeXY().Y);

	FIntRect UnconstrainedRectangle = FIntRect(X, Y, X + SizeX, Y + SizeY);

	SceneViewProjection.SetViewRectangle(UnconstrainedRectangle);

	SceneViewProjection.ViewOrigin = MiniInfo.Location;

	SceneViewProjection.ViewRotationMatrix = FInverseRotationMatrix(MiniInfo.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));


	FMinimalViewInfo::CalculateProjectionMatrixGivenView(
		MiniInfo,
		LocalPlayer->AspectRatioAxisConstraint, 
		Viewport,
		SceneViewProjection
	);

	//FMatrix ViewMatrix = FMatrix::Identity; 
	//FMatrix ProjectMatrix = FMatrix::Identity;
	//FMatrix ViewProjectMatrix = FMatrix::Identity;

	//UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(MiniInfo, TOptional<FMatrix>(), ViewMatrix, ProjectMatrix, ViewProjectMatrix);

	GetViewFrustumBounds(ViewFrustum, SceneViewProjection.ComputeViewProjectionMatrix(), true);

	FWaterInstanceQuadTree::FrustumCull(InstanceMeshTree.Get(), this, SceneViewProjection.ProjectionMatrix, SceneViewProjection.ViewOrigin);
}

const FConvexVolume& FWaterInstanceMeshManager::GetViewFrustum() const{
	return ViewFrustum;
}

const TArray<UWaterInstanceMeshComponent*>& FWaterInstanceMeshManager::GetWaterMeshLODs() const{
	return WaterMeshLODs;
}

TArray<TMap<uint32, uint32>>& FWaterInstanceMeshManager::GetInstanceIdToNodeIndex(){
	return InstanceIdToNodeIndex;
}

TMap<uint32, FWaterMeshLeafNodeData>& FWaterInstanceMeshManager::GetWaterMeshNodeData(){
	return WaterMeshNodeData;
}



FWaterInstanceMeshManager::~FWaterInstanceMeshManager(){

	//填充Instance容器
	//for (int i = 0; i < 3; ++i) {
	//	WaterMeshLODs[i]->RemoveFromRoot();
	//	WaterMeshLODs[i] = nullptr;
	//}
}
