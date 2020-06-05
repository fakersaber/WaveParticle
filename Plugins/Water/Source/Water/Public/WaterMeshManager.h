#pragma once

#include "CoreMinimal.h"

class WaterInstanceMeshComponent;
class FWaterInstanceQuadTree;

struct WaterMeshInstanceNodeData {
	uint32 CalclateLODIndex() {}
	uint32 CurLodIndex;
	uint32 InstanceIndex;
};



class FWaterInstanceMeshManager {
public:
	FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos);
	~FWaterInstanceMeshManager();

	void Initial();

private:
	TSharedPtr<FWaterInstanceQuadTree> InstanceMeshTree;

	TArray<WaterInstanceMeshComponent*> WaterLODs;
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

	//空间复杂度：O(n * log4(n))
	//时间复杂度：最好O(1), 最坏O((4n - 1)/3)
	static void FrustumCull(FWaterInstanceQuadTree* RootNode);

	static void InitWaterMeshQuadTree(FWaterInstanceQuadTree* RootNode);

	//判断是否叶子节点
	static bool bIsLeafNode(FWaterInstanceQuadTree* CurNode);

	void Split();



private:
	//每个节点存储位置即可当前的BoundingBox,可以只存储位置，但为了拓展性直接存储Box
	FBoxSphereBounds NodeBound;

	// Sub Node
	FWaterInstanceQuadTree* SubTrees[4];

	//当前节点包含的所有实际数据,包括子节点数据
	TArray<WaterMeshInstanceNodeData> DataNodes;


public:
	static FVector MinBound;
};