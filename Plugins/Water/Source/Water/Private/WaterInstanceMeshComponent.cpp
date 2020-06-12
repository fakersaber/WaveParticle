// Fill out your copyright notice in the Description page of Project Settings.


#include "WaterInstanceMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"


bool UWaterInstanceMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	// remove instance
	if (PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		PerInstanceSMData.RemoveAtSwap(InstanceIndex, 1, false);
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

FPrimitiveSceneProxy* UWaterInstanceMeshComponent::CreateSceneProxy(){




	return Super::CreateSceneProxy();
}
