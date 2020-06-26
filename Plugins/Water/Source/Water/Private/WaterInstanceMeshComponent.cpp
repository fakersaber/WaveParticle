// Fill out your copyright notice in the Description page of Project Settings.


#include "WaterInstanceMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "WaterMeshManager.h"
#include "InstancedStaticMesh.h"

//class FWaterInstanceStaticMeshProxy final : public FInstancedStaticMeshSceneProxy {
//private:
//	FWaterInstanceMeshManager* ManagerPtr;
//
//public:
//	FWaterInstanceStaticMeshProxy(UWaterInstanceMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
//		: FInstancedStaticMeshSceneProxy(InComponent, InFeatureLevel),
//		  ManagerPtr(InComponent->GetMeshManager())
//	{
//
//	}
//
//	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
//	{
//		FPrimitiveViewRelevance Result;
//		Result = FStaticMeshSceneProxy::GetViewRelevance(View);
//		Result.bDynamicRelevance = true;
//		Result.bStaticRelevance = false;
//		return Result;
//	}
//
//	//遮挡查询是走的回读，如果使用soft occlusion则会失效
//	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
//
//	virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;
//
//	virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) override;
//
//	virtual bool HasSubprimitiveOcclusionQueries() const override
//	{
//		// 这个版本先恒返回true
//		return true;
//	}
//};




bool UWaterInstanceMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	// remove instance
	if (PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		PerInstanceSMData.RemoveAtSwap(InstanceIndex, 1, false);
		PerInstanceSMCustomData.RemoveAtSwap(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats, false);
	}

#if WITH_EDITOR
	// remove selection flag if array is filled in
	if (SelectedInstances.IsValidIndex(InstanceIndex))
	{
		SelectedInstances.RemoveAt(InstanceIndex);
	}
#endif

	// update the physics state
	if (bPhysicsStateCreated && InstanceBodies.IsValidIndex(InstanceIndex))
	{
		if (FBodyInstance*& InstanceBody = InstanceBodies[InstanceIndex])
		{
			InstanceBody->TermBody();
			delete InstanceBody;
			InstanceBody = nullptr;

			InstanceBodies.RemoveAt(InstanceIndex);

			// Re-target instance indices for shifting of array.
			for (int32 i = InstanceIndex; i < InstanceBodies.Num(); ++i)
			{
				InstanceBodies[i]->InstanceBodyIndex = i;
			}
		}
	}

	// Force recreation of the render data
	InstanceUpdateCmdBuffer.NumEdits += 1;

	MarkRenderStateDirty();
	return true;
}


int32 UWaterInstanceMeshComponent::AddInstance(const FTransform& InstanceTransform) {

	FInstancedStaticMeshInstanceData* NewInstanceData = new (PerInstanceSMData) FInstancedStaticMeshInstanceData();

	NewInstanceData->Transform = InstanceTransform.ToMatrixWithScale();

	//SetupNewInstanceData(*NewInstanceData, InstanceIndex, InstanceTransform);

	// Add custom data to instance
	PerInstanceSMCustomData.AddZeroed(NumCustomDataFloats);

#if WITH_EDITOR
	if (SelectedInstances.Num())
	{
		SelectedInstances.Add(false);
	}
#endif

	PartialNavigationUpdate(PerInstanceSMData.Num());

	InstanceUpdateCmdBuffer.NumEdits += 1;

	MarkRenderStateDirty();

	return PerInstanceSMData.Num();
}

FBoxSphereBounds UWaterInstanceMeshComponent::CalcBounds(const FTransform& LocalToWorld) const{

	if (GetStaticMesh() && PerInstanceSMData.Num() > 0){
		const auto& RootBounding = ManagerPtr->GetQuadTreeRootNode()->GetNodeBounding();
		return RootBounding;
	}
	else{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}


FPrimitiveSceneProxy* UWaterInstanceMeshComponent::CreateSceneProxy(){

	return Super::CreateSceneProxy();

	//LLM_SCOPE(ELLMTag::InstancedMesh);
	//ProxySize = 0;

	//// Verify that the mesh is valid before using it.
	//const bool bMeshIsValid =
	//	// make sure we have instances
	//	PerInstanceSMData.Num() > 0 &&
	//	// make sure we have an actual staticmesh
	//	GetStaticMesh() &&
	//	GetStaticMesh()->HasValidRenderData() &&
	//	// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
	//	// @todo: Level error or something to let LDs know this
	//	1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	//if (bMeshIsValid)
	//{
	//	check(InstancingRandomSeed != 0);

	//	// if instance data was modified, update GPU copy
	//	// generally happens only in editor 
	//	if (InstanceUpdateCmdBuffer.NumTotalCommands() != 0)
	//	{
	//		//Reset函数未导出，复制一份
	//		InstanceUpdateCmdBuffer.Cmds.Empty();
	//		InstanceUpdateCmdBuffer.NumAdds = 0;
	//		InstanceUpdateCmdBuffer.NumEdits = 0;

	//		FStaticMeshInstanceData RenderInstanceData = FStaticMeshInstanceData(GVertexElementTypeSupport.IsSupported(VET_Half2));
	//		BuildRenderData(RenderInstanceData, PerInstanceRenderData->HitProxies);
	//		PerInstanceRenderData->UpdateFromPreallocatedData(RenderInstanceData);
	//	}

	//	ProxySize = PerInstanceRenderData->ResourceSize;
	//	return ::new FWaterInstanceStaticMeshProxy(this, GetWorld()->FeatureLevel);
	//}
	//else
	//{
	//	return NULL;
	//}
}

void UWaterInstanceMeshComponent::SetMeshManager(FWaterInstanceMeshManager* InManagerPtr) {
	this->ManagerPtr = InManagerPtr;
}
//
//
//void FWaterInstanceStaticMeshProxy::GetDynamicMeshElements(
//	const TArray<const FSceneView *>& Views, 
//	const FSceneViewFamily& ViewFamily, 
//	uint32 VisibilityMap, 
//	FMeshElementCollector& Collector) const 
//{
//
//}
//
//const TArray<FBoxSphereBounds>* FWaterInstanceStaticMeshProxy::GetOcclusionQueries(const FSceneView * View) const
//{
//	return nullptr;
//}
//
//void FWaterInstanceStaticMeshProxy::AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults)
//{
//
//}
