// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "WaterMeshComponent.generated.h"

/**
 * 
 */
UCLASS()
class WATER_API UWaveMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()
	
public:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;


	~UWaveMeshComponent();
	
};
