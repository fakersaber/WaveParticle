// Fill out your copyright notice in the Description page of Project Settings.


#include "WaveMeshComponent.h"
#include "Engine/StaticMesh.h"

UWaveMeshComponent::~UWaveMeshComponent() {

}


FBoxSphereBounds UWaveMeshComponent::CalcBounds(const FTransform& LocalToWorld) const {
	if (GetStaticMesh())
	{
		FBoxSphereBounds NewBounds = GetStaticMesh()->GetBounds().TransformBy(LocalToWorld);

		//64x64 half size
		NewBounds.BoxExtent.Z = 3200.f * 0.5f;
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

