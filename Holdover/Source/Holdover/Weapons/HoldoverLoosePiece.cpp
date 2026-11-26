#include "Weapons/HoldoverLoosePiece.h"

#include "Audio/HoldoverAudio.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverShapes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AHoldoverLoosePiece::AHoldoverLoosePiece()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	// Hits are reported so the piece can make a sound when it lands.
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->OnComponentHit.AddDynamic(this, &AHoldoverLoosePiece::OnPieceHit);
}

UPhysicalMaterial* AHoldoverLoosePiece::BrassMaterial()
{
	static TObjectPtr<UPhysicalMaterial> Brass;
	if (!Brass)
	{
		Brass = NewObject<UPhysicalMaterial>(GetTransientPackage(), TEXT("HoldoverBrass"));
		Brass->AddToRoot();
		// Empty brass is light and springy: it bounces a few times on hard ground and skitters before it rolls to a stop.
		Brass->Friction = 0.35f;
		Brass->StaticFriction = 0.4f;
		Brass->Restitution = 0.45f;
		Brass->RestitutionCombineMode = EFrictionCombineMode::Max;
		Brass->bOverrideRestitutionCombineMode = true;
		Brass->SurfaceType = SurfaceType8; // "Brass"
	}
	return Brass;
}

void AHoldoverLoosePiece::Setup(EHoldoverPieceKind InKind, UStaticMesh* InMesh, const FVector& FallbackSize, float InMassKg)
{
	Kind = InKind;
	MassKg = FMath::Max(InMassKg, 0.001f);

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
	Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);

	const bool bSmall = (InKind != EHoldoverPieceKind::Magazine);
	Mesh->SetPhysMaterialOverride(bSmall ? BrassMaterial() : nullptr);
	// Cases spin fast off the ejector; let them keep it.
	Mesh->BodyInstance.SetMaxAngularVelocityInRadians(FMath::DegreesToRadians(bSmall ? 7200.f : 1800.f), false);
	// The case tumbles in the air but should come to rest on the ground, not roll forever.
	Mesh->SetAngularDamping(bSmall ? 0.6f : 0.3f);

	LastSoundTime = -1.f;
	FirstPersonUntil = -1.f;
	if (Mesh->FirstPersonPrimitiveType != EFirstPersonPrimitiveType::None)
	{
		Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
		Mesh->MarkRenderStateDirty();
	}
}

void AHoldoverLoosePiece::Launch(const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg)
{
	SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::ResetPhysics);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(AngularVelocityDeg);
	Mesh->WakeRigidBody();
}

void AHoldoverLoosePiece::DrawWithGunFor(float Seconds)
{
	UWorld* World = GetWorld();
	if (!World || Seconds <= 0.f)
	{
		return;
	}
	FirstPersonUntil = World->GetTimeSeconds() + Seconds;
	Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	Mesh->MarkRenderStateDirty();
	SetActorTickEnabled(true);
}

void AHoldoverLoosePiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (FirstPersonUntil >= 0.f && World && World->GetTimeSeconds() >= FirstPersonUntil)
	{
		// Clear of the gun: from here on it is an ordinary object in the world (same spot on screen, normal depth).
		FirstPersonUntil = -1.f;
		Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
		Mesh->MarkRenderStateDirty();
		SetActorTickEnabled(false);
	}
}

void AHoldoverLoosePiece::OnPieceHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Change of speed from this hit (impulse / mass), in cm/s.
	const float Impact = NormalImpulse.Size() / FMath::Max(MassKg, 0.001f);
	const float Now = World->GetTimeSeconds();
	if (Impact < MinSoundImpact || Now - LastSoundTime < 0.045f)
	{
		return;
	}
	LastSoundTime = Now;

	// The surface of what it landed on (the ground, a bench, another case).
	EPhysicalSurface Surface = SurfaceType_Default;
	const FBodyInstance* OtherBody = OtherComp ? OtherComp->GetBodyInstance() : nullptr;
	if (const UPhysicalMaterial* OtherMat = OtherBody ? OtherBody->GetSimplePhysicalMaterial() : nullptr)
	{
		Surface = OtherMat->SurfaceType.GetValue();
	}
	const TCHAR* SurfaceName = HoldoverAudio::SurfaceSoundName(Surface);
	FString Prefix;
	switch (Kind)
	{
	case EHoldoverPieceKind::Magazine: Prefix = FString::Printf(TEXT("Mag_%s"), SurfaceName); break;
	case EHoldoverPieceKind::LiveRound: Prefix = FString::Printf(TEXT("Round_%s"), SurfaceName); break;
	default: Prefix = FString::Printf(TEXT("Brass_%s_%s"), *SoundSet.ToString(), SurfaceName); break;
	}
	const TArray<TObjectPtr<USoundBase>>& Set = HoldoverAudio::LoadSet(TEXT("Brass"), Prefix);
	USoundBase* Sound = HoldoverAudio::PickVariant(Set, LastVariant);

	// Loudness grows with the hit; small skitters are quiet and a little higher.
	const float A = FMath::Clamp((Impact - MinSoundImpact) / FMath::Max(FullSoundImpact - MinSoundImpact, 1.f), 0.f, 1.f);
	const float Volume = FMath::Lerp(0.12f, 1.f, FMath::Sqrt(A));
	const float Pitch = FMath::FRandRange(0.94f, 1.06f) * FMath::Lerp(1.06f, 1.f, A);
	HoldoverAudio::PlayAt(this, Sound, Hit.ImpactPoint, Volume, Pitch);
}
