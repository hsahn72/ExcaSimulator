#include "SoilClump.h"

#include "DigTerrain.h"
#include "ExcaAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SoilDensityKgPerCm3 = 0.0016f;
	constexpr float SettleSpeed = 25.f;
	constexpr float SettleTime = 0.25f;
	constexpr float MaxAirTime = 6.f;
	constexpr float MaxLifetime = 30.f;
}

ASoilClump::ASoilClump()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(ExcaAssets::SpherePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(ExcaAssets::BaseMaterialPath);
	Mesh->SetStaticMesh(SphereFinder.Object);
	BaseMaterial = MaterialFinder.Object;
}

void ASoilClump::Init(float InVolumeCm3, ADigTerrain* InTerrain)
{
	VolumeCm3 = InVolumeCm3;
	Terrain = InTerrain;
	Radius = FMath::Pow(3.f * VolumeCm3 / (4.f * UE_PI), 1.f / 3.f);

	// The engine sphere is 100 cm across.
	SetActorScale3D(FVector(Radius / 50.f));
	Mesh->SetMaterial(0, ExcaAssets::MakeColor(BaseMaterial, this, ExcaAssets::SoilColor * 1.15f));
	Mesh->SetMassOverrideInKg(NAME_None, FMath::Max(1.f, VolumeCm3 * SoilDensityKgPerCm3), true);
}

void ASoilClump::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	ADigTerrain* T = Terrain.Get();
	const FVector Loc = GetActorLocation();
	const bool bOnGround = T && T->IsInside(FVector2D(Loc)) && Loc.Z - Radius <= T->GetHeightAt(FVector2D(Loc)) + 30.f;

	if (bOnGround && Age > 0.3f && Mesh->GetPhysicsLinearVelocity().Size() < SettleSpeed)
	{
		StillTime += DeltaSeconds;
	}
	else
	{
		StillTime = 0.f;
	}

	// Lumps resting on the ground become terrain. Lumps stuck elsewhere (in
	// the bucket, on rubble) stay physical for a while longer.
	if (StillTime > SettleTime || (bOnGround && Age > MaxAirTime) || Age > MaxLifetime)
	{
		if (T && T->IsInside(FVector2D(Loc)))
		{
			T->Deposit(Loc - FVector(0.f, 0.f, Radius), VolumeCm3, FMath::Max(60.f, Radius * 3.f));
		}
		Destroy();
	}
}
