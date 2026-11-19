#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;

/**
 * Gray-box building blocks made from the engine's basic shapes, so the game runs
 * before any art is imported. Engine shapes are 100 cm across and centred on their origin.
 */
namespace HoldoverShapes
{
	HOLDOVER_API UStaticMesh* Cube();
	HOLDOVER_API UStaticMesh* Cylinder(); // axis along local Z
	HOLDOVER_API UStaticMesh* Sphere();
	HOLDOVER_API UMaterialInterface* BaseMaterial();

	/**
	 * Creates, attaches and registers a shape at runtime.
	 * @param SizeCm  Final size in centimetres along X, Y, Z (converted to scale).
	 */
	HOLDOVER_API UStaticMeshComponent* AddShape(
		AActor* Owner,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FVector& LocationCm,
		const FRotator& Rotation,
		const FVector& SizeCm,
		const FLinearColor& Color,
		bool bCollide);

	/** Tints a component through the engine shape material's "Color" parameter. */
	HOLDOVER_API void SetColor(UPrimitiveComponent* Component, const FLinearColor& Color);

	// Palette for the gray box.
	inline const FLinearColor Ground(0.18f, 0.18f, 0.18f);
	inline const FLinearColor Berm(0.12f, 0.09f, 0.06f);
	inline const FLinearColor Wall(0.25f, 0.25f, 0.26f);
	inline const FLinearColor Wood(0.35f, 0.22f, 0.11f);
	inline const FLinearColor Paper(0.86f, 0.84f, 0.78f);
	inline const FLinearColor Black(0.01f, 0.01f, 0.01f);
	inline const FLinearColor GunSteel(0.04f, 0.04f, 0.045f);
	inline const FLinearColor Brass(0.75f, 0.55f, 0.2f);
	inline const FLinearColor Copper(0.7f, 0.35f, 0.18f);
	inline const FLinearColor White(0.9f, 0.9f, 0.9f);
}
