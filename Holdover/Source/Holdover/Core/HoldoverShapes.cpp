#include "Core/HoldoverShapes.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace HoldoverShapes
{
	static UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UStaticMesh* Cube() { return LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Cylinder() { return LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* Sphere() { return LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }

	UMaterialInterface* BaseMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	void SetColor(UPrimitiveComponent* Component, const FLinearColor& Color)
	{
		if (!Component)
		{
			return;
		}
		UMaterialInterface* Base = BaseMaterial();
		if (!Base)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Component->CreateDynamicMaterialInstance(0, Base))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	UStaticMeshComponent* AddShape(
		AActor* Owner,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FVector& LocationCm,
		const FRotator& Rotation,
		const FVector& SizeCm,
		const FLinearColor& Color,
		bool bCollide)
	{
		if (!Owner || !Mesh)
		{
			return nullptr;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
		Comp->SetStaticMesh(Mesh);
		Comp->SetMobility(EComponentMobility::Movable);
		if (Parent)
		{
			Comp->SetupAttachment(Parent);
		}
		Comp->SetRelativeLocation(LocationCm);
		Comp->SetRelativeRotation(Rotation);
		Comp->SetRelativeScale3D(SizeCm / 100.f);
		if (bCollide)
		{
			Comp->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetGenerateOverlapEvents(false);
		}
		Comp->RegisterComponent();
		Owner->AddInstanceComponent(Comp);
		SetColor(Comp, Color);
		return Comp;
	}
}
