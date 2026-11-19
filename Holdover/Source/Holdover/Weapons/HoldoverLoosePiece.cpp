#include "Weapons/HoldoverLoosePiece.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverShapes.h"
#include "Engine/StaticMesh.h"

AHoldoverLoosePiece::AHoldoverLoosePiece()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Small fast objects: continuous collision so they do not fall through the floor.
	Mesh->BodyInstance.bUseCCD = true;
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetLinearDamping(0.05f);
	Mesh->SetAngularDamping(0.3f);
}

void AHoldoverLoosePiece::Setup(EHoldoverPieceKind InKind, UStaticMesh* InMesh, const FVector& FallbackSize, float MassKg)
{
	Kind = InKind;

	if (InMesh)
	{
		Mesh->SetStaticMesh(InMesh);
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		const bool bBox = (InKind == EHoldoverPieceKind::Magazine);
		Mesh->SetStaticMesh(bBox ? HoldoverShapes::Cube() : HoldoverShapes::Cylinder());
		Mesh->SetWorldScale3D(FallbackSize / 100.f);
		const FLinearColor Color =
			InKind == EHoldoverPieceKind::Magazine ? HoldoverShapes::GunSteel :
			InKind == EHoldoverPieceKind::LiveRound ? HoldoverShapes::Copper : HoldoverShapes::Brass;
		HoldoverShapes::SetColor(Mesh, Color);
	}

	// Bullets pass by casings and rounds (too small to matter) but can knock mags around.
	Mesh->SetCollisionResponseToChannel(ECC_HoldoverBullet,
		InKind == EHoldoverPieceKind::Magazine ? ECR_Block : ECR_Ignore);
	Mesh->SetMassOverrideInKg(NAME_None, FMath::Max(MassKg, 0.001f), true);
}

void AHoldoverLoosePiece::Launch(const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg)
{
	SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::ResetPhysics);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(AngularVelocityDeg);
	Mesh->WakeRigidBody();
}
