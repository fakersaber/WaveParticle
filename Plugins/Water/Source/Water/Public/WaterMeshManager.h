#pragma once

#include "CoreMinimal.h"

class UWaterInstanceMeshComponent;
class FWaterInstanceQuadTree;
class AParticleWaveManager;
struct FConvexVolume;

//ʵ�����ݽڵ�
struct FWaterMeshLeafNodeData {
	explicit FWaterMeshLeafNodeData(const TArray<float, TInlineAllocator<6>>& InPerInstanceData)
		:
		bIsFading(true),
		LastLodIndex(INDEX_NONE),
		InstanceIndex(INDEX_NONE),
		PerInstanceData(InPerInstanceData)
	{

	}

	bool bIsFading;

	uint8 LastLodIndex;

	uint32 InstanceIndex;

	TArray<float/*, TInlineAllocator<6>*/> PerInstanceData;

	//Ҷ�ӽڵ㲻��Ҫ�ٴ���RelativeTransform,ֻ��Ҫ���Ĳ����ڵ��д洢
	/*FTransform RelativeTransform;*/
};


class FWaterInstanceMeshManager {
public:
	FWaterInstanceMeshManager(uint32 PlaneSize, float GridSize, uint32 TileSize, FVector CenterPos);

	~FWaterInstanceMeshManager();

	void Initial(const FTransform& LocalToWorld, const TArray<UStaticMesh*>& LodAsset, uint32 InstanceCount, AParticleWaveManager* OuterActor);

	void MeshCulling(ULocalPlayer* LocalPlayer);

	const FConvexVolume& GetViewFrustum() const;

	const TArray<UWaterInstanceMeshComponent*>& GetWaterMeshLODs() const;

	TArray<TMap<uint32, uint32>>& GetInstanceIdToNodeIndex();

	TMap<uint32, FWaterMeshLeafNodeData>& GetWaterMeshNodeData();

	FWaterInstanceQuadTree* GetQuadTreeRootNode();

	//const FMatrix& GetCurProjMatrix() const;

private:
	TSharedPtr<FWaterInstanceQuadTree> QuadTreeRootNode;

	FConvexVolume ViewFrustum;

	TArray<UWaterInstanceMeshComponent*> WaterMeshLODs;

	TArray<TMap<uint32, uint32>> InstanceIdToNodeIndex;

	TMap<uint32, FWaterMeshLeafNodeData> WaterMeshNodeData;

};



//�Ĳ����ڵ�
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

	static void FrustumCull(FWaterInstanceQuadTree* RootNode, FWaterInstanceMeshManager* ManagerPtr, const FMatrix& ProjMatrix, const FVector& ViewOrigin);

	void InitWaterMeshQuadTree(FWaterInstanceMeshManager* ManagerPtr, int32& LeafNodeIndex);

	void BoundingBoxTransformBy(const FTransform LocalToWorld);

	//bool bIsLeafNode();

	void Split();

	uint8 CalclateLODIndex(const FMatrix& ProjMatrix, const FVector& ViewOrigin);

	const FBoxSphereBounds& GetNodeBounding() const;

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
		FWaterInstanceQuadTree* TobeAddNode,
		uint8 CurLODIndex, 
		FWaterMeshLeafNodeData* LeafDataNode,
		const TArray<UWaterInstanceMeshComponent*>& InstanceLODContainer, 
		TArray<TMap<uint32, uint32>>& InstanceIdToNodeIndex
	);


private:

	FWaterInstanceQuadTree* SubTrees[4];

	//���浱ǰ�ڵ�����ӽڵ��NodeIndex
	TArray<uint32> NodesIndeces;

	FTransform RelativeTransform;

	FBoxSphereBounds NodeBound;

	bool bIsLeafNode;

public:
	static FVector MinBound;
	static bool bIsTransformMinBound;
};