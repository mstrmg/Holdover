#include "Range/HoldoverPaperTarget.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverDebug.h"
#include "Core/HoldoverShapes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

AHoldoverPaperTarget::AHoldoverPaperTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AHoldoverPaperTarget::BeginPlay()
{
	Super::BeginPlay();
	BuildTarget();
}

void AHoldoverPaperTarget::BuildTarget()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	UStaticMesh* Cube = HoldoverShapes::Cube();
	UStaticMesh* Cyl = HoldoverShapes::Cylinder();

	// Two wooden legs (2x2 in) holding a cardboard backer.
	const float LegHeight = CentreHeight + PaperSize.Y * 0.5f + 5.f;
	const float LegY = PaperSize.X * 0.5f + 2.f;
	for (float Side : { -1.f, 1.f })
	{
		UStaticMeshComponent* Leg = HoldoverShapes::AddShape(this, Root, Cube,
			FVector(-2.5f, Side * LegY, LegHeight * 0.5f), FRotator::ZeroRotator,
			FVector(4.f, 4.f, LegHeight), HoldoverShapes::Wood, true);
		if (Leg)
		{
			Leg->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		}
	}

	// Cardboard backer with the paper face on its front (+X, toward the shooter).
	FaceFrontX = 0.f;
	Board = HoldoverShapes::AddShape(this, Root, Cube,
		FVector(FaceFrontX - BoardThickness * 0.5f, 0.f, CentreHeight), FRotator::ZeroRotator,
		FVector(BoardThickness, PaperSize.X + 4.f, PaperSize.Y + 4.f), HoldoverShapes::Paper, true);
	if (Board)
	{
		Board->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
	}

	// Printed rings as thin stacked discs (no texture needed). Cylinder axis is Z; pitch 90 turns it to face X.
	const FRotator FaceX(90.f, 0.f, 0.f);
	const float Step = 0.04f;
	float Layer = 0.f;
	auto Disc = [&](float Radius, const FLinearColor& Color)
	{
		Layer += Step;
		HoldoverShapes::AddShape(this, Root, Cyl,
			FVector(FaceFrontX + Layer, 0.f, CentreHeight), FaceX,
			FVector(Radius * 2.f, Radius * 2.f, 0.02f), Color, false);
	};

	const float Line = 0.15f;
	for (int32 i = RingRadii.Num() - 1; i >= 0; --i)
	{
		const float R = RingRadii[i];
		if (R > BlackRadius + KINDA_SMALL_NUMBER)
		{
			Disc(R, HoldoverShapes::Black);                 // ring line
			Disc(R - Line, HoldoverShapes::Paper);          // white inside it
		}
		else if (FMath::IsNearlyEqual(R, BlackRadius, 0.01f))
		{
			Disc(R, HoldoverShapes::Black);                 // the black aiming area
		}
		else
		{
			Disc(R, HoldoverShapes::Paper);                 // white line inside the black
			Disc(R - Line, HoldoverShapes::Black);
		}
	}
	// Small centre X mark.
	Disc(0.4f, HoldoverShapes::Paper);
	FaceFrontX += Layer + Step;
}

int32 AHoldoverPaperTarget::ScoreRadius(float RadiusCm, float BulletRadiusCm) const
{
	// A hole that touches a ring line scores the higher ring.
	for (int32 i = 0; i < RingRadii.Num(); ++i)
	{
		if (RadiusCm - BulletRadiusCm <= RingRadii[i])
		{
			return 10 - i;
		}
	}
	return 0;
}

void AHoldoverPaperTarget::AddHole(const FVector& LocalOnFace, float DiameterCm)
{
	UStaticMesh* Cyl = HoldoverShapes::Cylinder();
	const FRotator FaceX(90.f, 0.f, 0.f);
	const FVector Size(DiameterCm, DiameterCm, 0.02f);

	// Front hole just above the printed rings, back hole just behind the board.
	const FVector Front(FaceFrontX + 0.03f, LocalOnFace.Y, LocalOnFace.Z);
	const FVector Back(-BoardThickness - 0.03f, LocalOnFace.Y, LocalOnFace.Z);

	for (const FVector& Pos : { Front, Back })
	{
		UStaticMeshComponent* Hole = nullptr;
		if (Holes.Num() < MaxHoles * 2)
		{
			Hole = HoldoverShapes::AddShape(this, Root, Cyl, Pos, FaceX, Size, FLinearColor(0.02f, 0.015f, 0.01f), false);
			if (Hole)
			{
				Holes.Add(Hole);
			}
		}
		else
		{
			NextHole = NextHole % Holes.Num();
			Hole = Holes[NextHole++];
			if (Hole)
			{
				Hole->SetRelativeLocation(Pos);
				Hole->SetRelativeScale3D(Size / 100.f);
				Hole->SetVisibility(true);
			}
		}
	}
}

bool AHoldoverPaperTarget::ReceiveBulletHit(const FHoldoverBulletImpact& Impact, float& OutExitVelocityScale)
{
	OutExitVelocityScale = 0.97f;

	const FHitResult& Hit = Impact.Hit;
	if (Hit.GetComponent() != Board)
	{
		return false; // legs stop the bullet (and keep no holes for now)
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	const float FaceY = Local.Y;               // right as seen from the target is left for the shooter
	const float FaceZ = Local.Z - CentreHeight;

	// Holes only on the paper area; hits on the backer edge still pass through.
	if (FMath::Abs(FaceY) <= PaperSize.X * 0.5f && FMath::Abs(FaceZ) <= PaperSize.Y * 0.5f)
	{
		AddHole(Local, Impact.DiameterCm);

		FHoldoverPaperHit Record;
		Record.Ring = ScoreRadius(FMath::Sqrt(FaceY * FaceY + FaceZ * FaceZ), Impact.DiameterCm * 0.5f);
		Record.FacePosition = FVector2D(-FaceY, FaceZ);
		Record.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Hits.Add(Record);
		OnHit.Broadcast(this, Record);

		if (HoldoverDebug::Shots() && GEngine)
		{
			const float Dist = Impact.Shooter.IsValid() ? FVector::Dist(Impact.Shooter->GetActorLocation(), GetActorLocation()) / 100.f : 0.f;
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow,
				FString::Printf(TEXT("%.0f m target: %d ring (%.1f cm right, %.1f cm up)"),
					Dist, Record.Ring, Record.FacePosition.X, Record.FacePosition.Y));
		}
	}

	return true; // paper and cardboard do not stop a bullet
}

void AHoldoverPaperTarget::ResetPaper()
{
	for (UStaticMeshComponent* Hole : Holes)
	{
		if (Hole)
		{
			Hole->DestroyComponent();
		}
	}
	Holes.Reset();
	Hits.Reset();
	NextHole = 0;
}
