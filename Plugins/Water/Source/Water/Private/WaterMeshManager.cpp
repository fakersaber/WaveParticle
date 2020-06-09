#include "WaterMeshManager.h"
#include "WaterInstanceMeshComponent.h"
#include "ConvexVolume.h"

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

FORCEINLINE bool IntersectBox8Plane(const FVector& InOrigin, const FVector& InExtent, const FPlane* PermutedPlanePtr)
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
void FWaterInstanceQuadTree::FrustumCull(FWaterInstanceQuadTree* RootNode, FWaterInstanceMeshManager* ManagerPtr) {

	const FPlane* PermutedPlanePtr = ManagerPtr->GetViewFrustum().PermutedPlanes.GetData();
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer = ManagerPtr->GetWaterMeshLODs();
	bool bIsCulling = IntersectBox8Plane(RootNode->NodeBound.Origin, RootNode->NodeBound.BoxExtent, PermutedPlanePtr);

	if (bIsCulling) {
		for (const uint32 RemoveNodeIndex : RootNode->NodesIndeces) {
			FWaterMeshLeafNodeData* RemoveDataNode = ManagerPtr->GetWaterMeshNodeData().Find(RemoveNodeIndex);
			check(RemoveDataNode);
			if (RemoveDataNode->bIsFading) {
				continue;
			}
			RemoveInstanceNode(RemoveDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex(), ManagerPtr->GetWaterMeshNodeData());
		}
		return;
	}

	for (FWaterInstanceQuadTree* NodePtr : RootNode->SubTrees) {
		if (NodePtr != nullptr) {
			FrustumCull(NodePtr, ManagerPtr);
		}
	}

	//通过测试的节点如果是最小粒度
	FVector2D CurBoundSize = FVector2D(RootNode->NodeBound.BoxExtent.X, RootNode->NodeBound.BoxExtent.Y);
	if (FMath::IsNearlyEqual(CurBoundSize.X, MinBound.X) && FMath::IsNearlyEqual(CurBoundSize.Y, MinBound.Y)) {
		FWaterMeshLeafNodeData* LeafDataNode = ManagerPtr->GetWaterMeshNodeData().Find(RootNode->NodesIndeces[0]);
		uint8 CurLODIndex = LeafDataNode->CalclateLODIndex();

		//1. LastFrame是Culling状态，直接添加
		if (LeafDataNode->bIsFading) {
			AddInstanceNode(RootNode->NodesIndeces[0], CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		//2. LastFrame不是Culling状态，但是LOD发生变化
		else if (CurLODIndex != LeafDataNode->LastLodIndex) {
			RemoveInstanceNode(LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex(), ManagerPtr->GetWaterMeshNodeData());
			AddInstanceNode(RootNode->NodesIndeces[0], CurLODIndex, LeafDataNode, InstanceLODContainer, ManagerPtr->GetInstanceIdToNodeIndex());
		}

		LeafDataNode->bIsFading = false;
	}
}

void FWaterInstanceQuadTree::InitWaterMeshQuadTree(FWaterInstanceMeshManager* ManagerPtr, int32& LeafNodeIndex) {
	if (bIsLeafNode()) {
		LeafNodeIndex += 1;
		NodesIndeces.Emplace(LeafNodeIndex);

		TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData = ManagerPtr->GetWaterMeshNodeData();

		WaterMeshNodeData.Emplace(LeafNodeIndex, FWaterMeshLeafNodeData(FTransform(NodeBound.Origin)));

		return;
	}
		
	Split();

	for (auto NodePtr : SubTrees) {
		NodePtr->InitWaterMeshQuadTree(ManagerPtr, LeafNodeIndex);
	}

	for (int i = 0; i < LeafNodeIndex; ++i) {
		NodesIndeces.Emplace(LeafNodeIndex);
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


void FWaterInstanceQuadTree::RemoveInstanceNode(
	FWaterMeshLeafNodeData* RemoveDataNode,
	const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer, 
	TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex,
	TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData
){

	UWaterInstanceMeshComponent* InstanceComponent = InstanceLODContainer[RemoveDataNode->LastLodIndex];
	TMap<uint32, uint32>& InstanceToNodeMap = InstanceIdToNodeIndex[RemoveDataNode->LastLodIndex];

	const uint32 EndInstanceId = InstanceComponent->GetInstanceCount() - 1;
	uint32* EndNodeIndexPtr = InstanceToNodeMap.Find(EndInstanceId);
	uint32* RemoveNodeIndexPtr = InstanceToNodeMap.Find(RemoveDataNode->InstanceIndex);

	FWaterMeshLeafNodeData* EndDataNode = WaterMeshNodeData.Find(*EndNodeIndexPtr);

	//修改节点数据
	EndDataNode->InstanceIndex = RemoveDataNode->InstanceIndex;

	//修改索引表
	*RemoveNodeIndexPtr = *EndNodeIndexPtr;

	InstanceComponent->RemoveInstance(RemoveDataNode->InstanceIndex);

	RemoveDataNode->bIsFading = true;
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


void FWaterInstanceMeshManager::Initial(const FTransform& LocalToWorld) {

	int32 NodeIndex = INDEX_NONE;

	//初始化四叉树并且填充WaterMeshLeafNodeData容器
	InstanceMeshTree.Get()->InitWaterMeshQuadTree(this, NodeIndex);
	check(NodeIndex == 1024 - 1);

	//初始化Mesh的Transform,在此初始化原因是原始四叉树需要RelativeTransform
	InstanceMeshTree.Get()->MeshTransformBy(LocalToWorld);

	//填充InstanceIdToNodeIndex容器
	for (int32 i = 0; i < 3; ++i) {
		const auto Index = InstanceIdToNodeIndex.AddUninitialized(1);
		TMap<uint32, uint32>* CurMapContainer = &InstanceIdToNodeIndex[Index];

		for (uint32 InstanceID = 0; InstanceID < INSTANCE_COUNT; ++InstanceID) {
			CurMapContainer->Emplace(InstanceID, 0xfffffffful);
		}
	}

	//

}

void FWaterInstanceMeshManager::MeshCulling(const FMatrix& ProjectMatrix){

	GetViewFrustumBounds(ViewFrustum, ProjectMatrix, true);

	FWaterInstanceQuadTree::FrustumCull(InstanceMeshTree.Get(), this);
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

}
