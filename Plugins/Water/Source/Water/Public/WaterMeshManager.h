#pragma once

#include "CoreMinimal.h"

class UWaterInstanceMeshComponent;
class FWaterInstanceQuadTree;
struct FConvexVolume;


struct FWaterMeshLeafNodeData {
	explicit FWaterMeshLeafNodeData(const FTransform& RelativeRT) 
		:
		bIsFading(true),
		LastLodIndex(INDEX_NONE),
		InstanceIndex(INDEX_NONE),
		RelativeTransform(RelativeRT)
	{

	}

	uint8 CalclateLODIndex() {
		return 0;
	}

	bool bIsFading;

	uint8 LastLodIndex;

	uint32 InstanceIndex;

	FTransform RelativeTransform;
};



class FWaterInstanceMeshManager {
public:
	FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos);

	~FWaterInstanceMeshManager();

	void Initial(const FTransform& LocalToWorld);

	void MeshCulling(const FMatrix& ProjectMatrix);

	const FConvexVolume& GetViewFrustum() const;

	const TArray<UWaterInstanceMeshComponent*>& GetWaterMeshLODs() const;

	TArray<TMap<uint32, uint32>>& GetInstanceIdToNodeIndex();

	TMap<uint32, FWaterMeshLeafNodeData>& GetWaterMeshNodeData();

private:
	TSharedPtr<FWaterInstanceQuadTree> InstanceMeshTree;

	FConvexVolume ViewFrustum;

	TArray<UWaterInstanceMeshComponent*> WaterMeshLODs;

	TArray<TMap<uint32, uint32>> InstanceIdToNodeIndex;

	TMap<uint32, FWaterMeshLeafNodeData> WaterMeshNodeData;
};


class FWaterInstanceQuadTree {
public:
	enum  QuadNodeName {
		LeftTop = 0,
		RightTop = 1,
		LeftDown = 2,
		RightDown = 3
	};

	FWaterInstanceQuadTree(const FBoxSphereBounds& InNodeBound);

	~FWaterInstanceQuadTree();

	static void FrustumCull(FWaterInstanceQuadTree* RootNode, FWaterInstanceMeshManager* ManagerPtr);

	void InitWaterMeshQuadTree(FWaterInstanceMeshManager* ManagerPtr, int32& LeafNodeIndex);

	void MeshTransformBy(const FTransform LocalToWorld);

	bool bIsLeafNode();

	void Split();

private:

	static void RemoveInstanceNode
	(
		FWaterMeshLeafNodeData* RemoveDataNode,
		const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer,
		TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex,
		TMap<uint32, FWaterMeshLeafNodeData>& WaterMeshNodeData
	);
	
	static void AddInstanceNode
	(
		uint32 NodeIndex, uint8 CurLODIndex, 
		FWaterMeshLeafNodeData* LeafDataNode,
		const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer, 
		TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex
	);


private:
	FBoxSphereBounds NodeBound;

	FWaterInstanceQuadTree* SubTrees[4];

	//储存当前节点包括子节点的NodeIndex
	TArray<uint32> NodesIndeces;


public:
	static FVector MinBound;
};