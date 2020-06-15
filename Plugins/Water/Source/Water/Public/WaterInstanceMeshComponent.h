// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "WaterInstanceMeshComponent.generated.h"


class FWaterInstanceMeshManager;

/**
 * 
 */
UCLASS()
class WATER_API UWaterInstanceMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
    virtual bool RemoveInstance(int32 InstanceIndex) override;

	virtual int32 AddInstance(const FTransform& InstanceTransform) override;

    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

    void SetMeshManager(FWaterInstanceMeshManager* ManagerPtr);

private:
    FWaterInstanceMeshManager* ManagerPtr;
};
