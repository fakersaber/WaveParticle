#include "WaterMeshManager.h"
#include "WaterInstanceMeshComponent.h"
#include "ConvexVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "AParticleWaveManager.h"

#define PerInstanceFloatSize 6

FVector FWaterInstanceQuadTree::MinBound = FVector(0.f);
bool FWaterInstanceQuadTree::bIsTransformMinBound = true;


FWaterInstanceQuadTree::FWaterInstanceQuadTree(const FBoxSphereBounds& InNodeBound)
	:
	NodeBound(InNodeBound),
	bIsLeafNode(false)
{
	for (auto& SubNode : SubTrees) {
		SubNode = nullptr;
	}
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
	FWaterInstanceQuadTree* CurRootNode, 
	FWaterInstanceMeshManager* ManagerPtr,
	const FMatrix& ProjMatrix,
	const FVector& ViewOrigin
) 
{
	const FPlane* PermutedPlanePtr = ManagerPtr->GetViewFrustum().PermutedPlanes.GetData();
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer = ManagerPtr->GetWaterMeshLODs();
	bool bIsCulling = IntersectBox8Plane(CurRootNode->NodeBound.Origin, CurRootNode->NodeBound.BoxExtent, PermutedPlanePtr);

	if (!bIsCulling) {
		for (const uint32 RemoveNodeIndex : CurRootNode->NodesIndeces) {
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

	for (FWaterInstanceQuadTree* NodePtr : CurRootNode->SubTrees) {
		if (NodePtr != nullptr) {
			FrustumCull(NodePtr, ManagerPtr, ProjMatrix, ViewOrigin);
		}
	}

	//通过测试的节点如果是最小粒度
	if (CurRootNode->bIsLeafNode) {
		FWaterMeshLeafNodeData* LeafDataNode = ManagerPtr->GetWaterMeshNodeData().Find(CurRootNode->NodesIndeces[0]);

		uint8 CurLODIndex = CurRootNode->CalclateLODIndex(ProjMatrix, ViewOrigin);

		//1. LastFrame是Culling状态，直接添加
		if (LeafDataNode->bIsFading) {
			AddInstanceNode(CurRootNode, CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		//2. LastFrame不是Culling状态，但是LOD发生变化
		else if (CurLODIndex != LeafDataNode->LastLodIndex) {
			RemoveInstanceNode(LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex(), ManagerPtr->GetWaterMeshNodeData());

			AddInstanceNode(CurRootNode, CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		LeafDataNode->LastLodIndex = CurLODIndex;

		LeafDataNode->bIsFading = false;
	}
}

void FWaterInstanceQuadTree::InitWaterMeshQuadTree(FWaterInstanceMeshManager* ManagerPtr, int32& LeafNodeIndex) {

	//保存当前节点的的起始NodeIndex
	int32 CurLeafNodeIndex = LeafNodeIndex;

	if (FMath::IsNearlyEqual(NodeBound.BoxExtent.X, MinBound.X) && FMath::IsNearlyEqual(NodeBound.BoxExtent.Y, MinBound.Y)) {

		FVector2D XYIndex = FVector2D
		(
			(NodeBound.Origin.X - FWaterInstanceQuadTree::MinBound.X + ManagerPtr->GetQuadTreeRootNode()->NodeBound.BoxExtent.X) / (2.f * FWaterInstanceQuadTree::MinBound.X),
			(NodeBound.Origin.Y - FWaterInstanceQuadTree::MinBound.Y + ManagerPtr->GetQuadTreeRootNode()->NodeBound.BoxExtent.Y) / (2.f * FWaterInstanceQuadTree::MinBound.Y)
		);
		TArray<float, TInlineAllocator<6>> PerInstanceData;
		//#TODO USE SSE
		PerInstanceData.Emplace(FMath::Frac(XYIndex.X * AParticleWaveManager::UVScale1.X));
		PerInstanceData.Emplace(FMath::Frac(XYIndex.Y * AParticleWaveManager::UVScale1.Y));
		PerInstanceData.Emplace(FMath::Frac(XYIndex.X * AParticleWaveManager::UVScale2.X));
		PerInstanceData.Emplace(FMath::Frac(XYIndex.Y * AParticleWaveManager::UVScale2.Y));
		PerInstanceData.Emplace(FMath::Frac(XYIndex.X * AParticleWaveManager::UVScale3.X));
		PerInstanceData.Emplace(FMath::Frac(XYIndex.Y * AParticleWaveManager::UVScale3.Y));
		TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData = ManagerPtr->GetWaterMeshNodeData();
		WaterMeshNodeData.Emplace(LeafNodeIndex, FWaterMeshLeafNodeData(PerInstanceData));

		//init Node Data
		this->NodesIndeces.Emplace(LeafNodeIndex);
		this->bIsLeafNode = true;

		//add leafIndex 
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



void FWaterInstanceQuadTree::BoundingBoxTransformBy(const FTransform LocalToWorld){

	this->RelativeTransform = FTransform(NodeBound.Origin);

	FTransform WorldTransform = this->RelativeTransform * LocalToWorld;

	FBoxSphereBounds CurWorldBound =  FBoxSphereBounds(FVector::ZeroVector, NodeBound.BoxExtent, NodeBound.BoxExtent.Size());

	this->NodeBound = CurWorldBound.TransformBy(WorldTransform);

	for (auto NodePtr : SubTrees) {
		if (!NodePtr)
			continue;
		NodePtr->BoundingBoxTransformBy(LocalToWorld);
	}
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

	//因为ViewOrigin为 FVector(0.0f, 0.0f, ViewDistance + Bounds.SphereRadius)，所以这里直接计算距离
	const float Dist = FVector::Dist(NodeBound.Origin, ViewOrigin) + NodeBound.SphereRadius;

	// 原范围(NDC xy)为[-1,1]，且NodeBound.Sphere必为正，所以直接根据比例缩小一半到[0,1]
	const float ScreenMultiple = FMath::Max(0.5f * ProjMatrix.M[0][0], 0.5f * ProjMatrix.M[1][1]);

	// Calculate screen-space projected radius
	const float ScreenRadius = ScreenMultiple * NodeBound.SphereRadius / FMath::Max(1.0f, Dist);

	// For clarity, we end up comparing the diameter
	const float ScreenSize = ScreenRadius * 2.0f;

	uint8 LODIndex = 0;

	if (ScreenSize > 0.5f)
		LODIndex = 0;
	else if (ScreenSize > 0.13f)
		LODIndex = 1;
	else
		LODIndex = 2;

	return LODIndex;

}

const FBoxSphereBounds& FWaterInstanceQuadTree::GetNodeBounding() const
{
	return NodeBound;
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
	FWaterInstanceQuadTree* TobeAddNode,
	uint8 CurLODIndex, 
	FWaterMeshLeafNodeData* LeafDataNode,
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer,
	TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex
){

	int32 InstanceIndex = InstanceLODContainer[CurLODIndex]->AddInstance(TobeAddNode->RelativeTransform);
	
	InstanceLODContainer[CurLODIndex]->SetCustomData(
		InstanceIndex - 1,
		LeafDataNode->PerInstanceData,
		InstanceLODContainer[CurLODIndex]->IsRenderStateDirty()
	);

	uint32* NodeIndexPtr = InstanceIdToNodeIndex[CurLODIndex].Find(InstanceIndex);

	LeafDataNode->InstanceIndex = InstanceIndex;

	//it must be leafNode
	check(TobeAddNode->NodesIndeces.Num() == 1)

	*NodeIndexPtr = TobeAddNode->NodesIndeces[0];
}


FWaterInstanceMeshManager::FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos){

	//Z暂定为Extent的一半
	FWaterInstanceQuadTree::MinBound = FVector(PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.25f);

	float ExtentSize = PlaneSize * GridSize * TileSize * 0.5f;

	FVector Extent = FVector(ExtentSize, ExtentSize, FWaterInstanceQuadTree::MinBound.Z);

	QuadTreeRootNode = MakeShared<FWaterInstanceQuadTree>(FBoxSphereBounds(CenterPos, Extent, Extent.Size()));
}


void FWaterInstanceMeshManager::Initial(const FTransform& LocalToWorld, const TArray<UStaticMesh*>& LodAsset, uint32 InstanceCount, AParticleWaveManager* OuterActor) {

	int32 NodeIndex = 0;

	//初始化四叉树并且填充WaterMeshLeafNodeData容器
	QuadTreeRootNode->InitWaterMeshQuadTree(this, NodeIndex);
	check(NodeIndex == InstanceCount);

	//更新BoundingBox的Transform
	QuadTreeRootNode->BoundingBoxTransformBy(LocalToWorld);

	//填充InstanceIdToNodeIndex容器
	for (int32 i = 0; i < OuterActor->WaterMeshs.Num(); ++i) {

		TMap<uint32, uint32>* CurMapContainer = new (InstanceIdToNodeIndex) TMap<uint32, uint32>();

		//InstanceId从1开始
		for (uint32 InstanceID = 1; InstanceID <= InstanceCount; ++InstanceID) {
			CurMapContainer->Emplace(InstanceID, 0xfffffffful);
		}
	}

	//填充Instance容器
	for (int32 LodIndex = 0; LodIndex < OuterActor->WaterMeshs.Num(); ++LodIndex) {

		const auto Index = WaterMeshLODs.AddUninitialized(1);

		UWaterInstanceMeshComponent** CurMeshInstanceComponentPtr = &WaterMeshLODs[Index];

		*CurMeshInstanceComponentPtr = NewObject<UWaterInstanceMeshComponent>(OuterActor, UWaterInstanceMeshComponent::StaticClass(),NAME_None);

		//AddInstance是使用RelativeTransform
		(*CurMeshInstanceComponentPtr)->SetWorldTransform(LocalToWorld);

		(*CurMeshInstanceComponentPtr)->SetStaticMesh(LodAsset[LodIndex]);

		(*CurMeshInstanceComponentPtr)->SetMaterial(0, OuterActor->WaterMaterial);

		(*CurMeshInstanceComponentPtr)->SetMeshManager(this);

		(*CurMeshInstanceComponentPtr)->NumCustomDataFloats = PerInstanceFloatSize;

		(*CurMeshInstanceComponentPtr)->RegisterComponent();
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

	GetViewFrustumBounds(ViewFrustum, SceneViewProjection.ComputeViewProjectionMatrix(), true);

	FWaterInstanceQuadTree::FrustumCull(QuadTreeRootNode.Get(), this, SceneViewProjection.ProjectionMatrix, SceneViewProjection.ViewOrigin);
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

FWaterInstanceQuadTree* FWaterInstanceMeshManager::GetQuadTreeRootNode()
{
	return QuadTreeRootNode.Get();
}



FWaterInstanceMeshManager::~FWaterInstanceMeshManager(){

}
