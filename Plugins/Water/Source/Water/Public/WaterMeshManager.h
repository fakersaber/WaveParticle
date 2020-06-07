#pragma once

#include "CoreMinimal.h"

class UWaterInstanceMeshComponent;
class FWaterInstanceQuadTree;
struct FConvexVolume;


struct WaterMeshInstanceNodeData {
	WaterMeshInstanceNodeData() 
		:
		bIsFading(true),
		LastLodIndex(0),
		InstanceIndex(0)
	{

	}

	uint8 CalclateLODIndex() {
		return 0;
	}

	bool bIsFading;

	uint8 LastLodIndex;

	uint32 InstanceIndex;
};



class FWaterInstanceMeshManager {
public:
	FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos);

	~FWaterInstanceMeshManager();

	void Initial();

	void MeshCulling(const FMatrix& ProjectMatrix);

	const FConvexVolume& GetViewFrustum() const;

	const TArray<UWaterInstanceMeshComponent*>& GetWaterMeshLODs() const;

private:
	TSharedPtr<FWaterInstanceQuadTree> InstanceMeshTree;

	TArray<UWaterInstanceMeshComponent*> WaterMeshLODs;;

	FConvexVolume ViewFrustum;
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

	static void InitWaterMeshQuadTree(FWaterInstanceQuadTree* RootNode);

	static bool bIsLeafNode(FWaterInstanceQuadTree* CurNode);

	void Split();



private:
	FBoxSphereBounds NodeBound;

	FWaterInstanceQuadTree* SubTrees[4];

	//当前节点包含的所有实际数据,包括子节点数据,只存储Index
	TArray<uint32> NodesIndeces;


public:
	static FVector MinBound;

	static TMap<uint32, WaterMeshInstanceNodeData> WaterMeshNodeData;
	static TMap<uint32, uint32> InstanceIdToNodeIndex;
};