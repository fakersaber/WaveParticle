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

	//�ռ临�Ӷȣ�O(n * log4(n))
	//ʱ�临�Ӷȣ����O(1), �O((4n - 1)/3)
	static void FrustumCull(FWaterInstanceQuadTree* RootNode);

	static void InitWaterMeshQuadTree(FWaterInstanceQuadTree* RootNode);

	//�ж��Ƿ�Ҷ�ӽڵ�
	static bool bIsLeafNode(FWaterInstanceQuadTree* CurNode);

	void Split();



private:
	//ÿ���ڵ�洢λ�ü��ɵ�ǰ��BoundingBox,����ֻ�洢λ�ã���Ϊ����չ��ֱ�Ӵ洢Box
	FBoxSphereBounds NodeBound;

	// Sub Node
	FWaterInstanceQuadTree* SubTrees[4];

	//��ǰ�ڵ����������ʵ������,�����ӽڵ�����
	TArray<WaterMeshInstanceNodeData> DataNodes;


public:
	static FVector MinBound;
};