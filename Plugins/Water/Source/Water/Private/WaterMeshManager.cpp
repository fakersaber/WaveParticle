#include "WaterMeshManager.h"

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



//空间复杂度：O(n * log4(n))
//时间复杂度：最好O(1), 最坏O(n * log4(n))
void FWaterInstanceQuadTree::FrustumCull(FWaterInstanceQuadTree* RootNode) {
	//if (!FrustumCullTest(RootNode)) {
	//	for (auto& : DataNodes) {
	//		CalclateLODIndex();
	//		//balabala
	//	}
	//	return;
	//}

	//for (auto NodePtr : RootNode->SubTrees) {
	//	if (NodePtr != nullptr) {
	//		FrustumCull(NodePtr);
	//	}
	//}

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
	FWaterInstanceQuadTree::MinBound = FVector
	(
		PlaneSize * GridSize * 0.5f, 
		PlaneSize * GridSize * 0.5f, 
		PlaneSize * GridSize * 0.25f
	);

	float ExtentSize = PlaneSize * GridSize * TileSize * 0.5f;

	FVector Extent = FVector(ExtentSize, ExtentSize, ExtentSize * 0.5f);
	
	InstanceMeshTree = MakeShared<FWaterInstanceQuadTree>(
		FBoxSphereBounds(CenterPos, Extent, Extent.Size())
	);
}

void FWaterInstanceMeshManager::Initial() {

	FWaterInstanceQuadTree::InitWaterMeshQuadTree(InstanceMeshTree.Get());

}


FWaterInstanceMeshManager::~FWaterInstanceMeshManager(){

}
