#include "WaterMeshManager.h"
#include "WaterInstanceMeshComponent.h"
#include "ConvexVolume.h"

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
			WaterMeshInstanceNodeData* RemoveDataNode = WaterMeshNodeData.Find(RemoveNodeIndex);
			check(RemoveDataNode);
			if (RemoveDataNode->bIsFading) {
				continue;
			}

			//找到InctanceID为InstanceCount
			uint8 CurLODIndex = RemoveDataNode->CalclateLODIndex();
			uint8 RemoveLODIndex = CurLODIndex != RemoveDataNode->LastLodIndex ? RemoveDataNode->LastLodIndex : CurLODIndex;
			const uint32 EndInstanceId = InstanceLODContainer[RemoveLODIndex]->GetInstanceCount() - 1;

			uint32* EndNodeIndexPtr = InstanceIdToNodeIndex.Find(EndInstanceId);
			uint32* RemoveNodeIndexPtr = InstanceIdToNodeIndex.Find(RemoveDataNode->InstanceIndex);

			*RemoveNodeIndexPtr = *EndNodeIndexPtr;
			RemoveDataNode->InstanceIndex = EndInstanceId;

			//修改End数据
			WaterMeshInstanceNodeData* EndDataNode = WaterMeshNodeData.Find(*EndNodeIndexPtr);
			*EndNodeIndexPtr = RemoveNodeIndex;
			EndDataNode->InstanceIndex = RemoveDataNode->InstanceIndex;

			//已经交换完成，使用交换过后的End
			InstanceLODContainer[RemoveLODIndex]->RemoveInstance(EndDataNode->InstanceIndex);

			RemoveDataNode->bIsFading = true;
		}
		return;
	}

	
	for (FWaterInstanceQuadTree* NodePtr : RootNode->SubTrees) {
		if (NodePtr != nullptr) {
			FrustumCull(NodePtr, ManagerPtr);
		}
	}

	////通过测试的节点如果是最小粒度
	//FVector2D CurBoundSize = FVector2D(RootNode->NodeBound.BoxExtent.X, RootNode->NodeBound.BoxExtent.Y);
	//if (FMath::IsNearlyEqual(CurBoundSize.X, MinBound.X) && FMath::IsNearlyEqual(CurBoundSize.Y, MinBound.Y)) {
	//	CurLOD = CalclateLOD();
	//	TArray<ElementType>& CurDataNodes = RootNode->DataNodes;
	//	if (CurDataNodes[0].LastLOD != CurLOD) {
	//		remove();
	//		add();
	//	}
	//}
}

void FWaterInstanceQuadTree::InitWaterMeshQuadTree(FWaterInstanceQuadTree* RootNode) {
	if (bIsLeafNode(RootNode))
		return;

	RootNode->Split();

	for (auto NodePtr : RootNode->SubTrees) {
		InitWaterMeshQuadTree(NodePtr);
	}
}



bool FWaterInstanceQuadTree::bIsLeafNode(FWaterInstanceQuadTree* CurNode) {
	FVector2D CurBoundSize = FVector2D(CurNode->NodeBound.BoxExtent.X, CurNode->NodeBound.BoxExtent.Y);
	return (FMath::IsNearlyEqual(CurBoundSize.X, MinBound.X) && FMath::IsNearlyEqual(CurBoundSize.Y, MinBound.Y));
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




FWaterInstanceMeshManager::FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos){

	//Z暂定为Extent的一半
	FWaterInstanceQuadTree::MinBound = FVector(PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.5f, PlaneSize * GridSize * 0.25f);

	float ExtentSize = PlaneSize * GridSize * TileSize * 0.5f;

	FVector Extent = FVector(ExtentSize, ExtentSize, FWaterInstanceQuadTree::MinBound.Z);

	InstanceMeshTree = MakeShared<FWaterInstanceQuadTree>(
		FBoxSphereBounds(CenterPos, Extent, Extent.Size())
	);
}


void FWaterInstanceMeshManager::Initial() {

	FWaterInstanceQuadTree::InitWaterMeshQuadTree(InstanceMeshTree.Get());

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


FWaterInstanceMeshManager::~FWaterInstanceMeshManager(){

}
