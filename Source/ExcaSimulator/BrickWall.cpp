#include "BrickWall.h"

#include "ExcaAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SlabHeight = 20.f;
	constexpr float MortarGap = 0.5f;
}

ABrickWall::ABrickWall()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(ExcaAssets::CubePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(ExcaAssets::BaseMaterialPath);
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	Slab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slab"));
	Slab->SetupAttachment(Root);
	Slab->SetStaticMesh(CubeMesh);
	Slab->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

void ABrickWall::BeginPlay()
{
	Super::BeginPlay();

	const float Width = Columns * BrickSize.X;
	Slab->SetRelativeLocation(FVector(0.f, 0.f, SlabHeight * 0.5f));
	Slab->SetRelativeScale3D(FVector(Width + 100.f, BrickSize.Y + 120.f, SlabHeight) / 100.f);
	Slab->SetMaterial(0, ExcaAssets::MakeColor(BaseMaterial, this, FLinearColor(0.35f, 0.35f, 0.33f)));

	UMaterialInterface* BrickMaterials[] = {
		ExcaAssets::MakeColor(BaseMaterial, this, FLinearColor(0.45f, 0.12f, 0.07f)),
		ExcaAssets::MakeColor(BaseMaterial, this, FLinearColor(0.38f, 0.10f, 0.06f)),
		ExcaAssets::MakeColor(BaseMaterial, this, FLinearColor(0.50f, 0.17f, 0.10f)),
	};

	const FTransform WallTransform = GetActorTransform();
	const FVector BrickScale = (BrickSize - FVector(MortarGap)) / 100.f;
	int32 BrickIndex = 0;

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		// Running bond: every other course is shifted by half a brick.
		const bool bOffsetRow = (Row % 2) == 1;
		const int32 Count = bOffsetRow ? Columns - 1 : Columns;
		const float StartX = -Width * 0.5f + (bOffsetRow ? BrickSize.X : BrickSize.X * 0.5f);
		const float Z = SlabHeight + BrickSize.Z * (Row + 0.5f);

		for (int32 Col = 0; Col < Count; ++Col, ++BrickIndex)
		{
			const FVector Local(StartX + Col * BrickSize.X, 0.f, Z);

			UStaticMeshComponent* Brick = NewObject<UStaticMeshComponent>(this);
			Brick->SetStaticMesh(CubeMesh);
			Brick->SetMobility(EComponentMobility::Movable);
			Brick->SetRelativeTransform(FTransform(WallTransform.GetRotation(), WallTransform.TransformPosition(Local), BrickScale));
			Brick->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
			Brick->SetMaterial(0, BrickMaterials[BrickIndex % UE_ARRAY_COUNT(BrickMaterials)]);
			Brick->BodyInstance.bSimulatePhysics = true;
			Brick->BodyInstance.bStartAwake = false;
			Brick->RegisterComponent();
			AddInstanceComponent(Brick);
			Brick->SetMassOverrideInKg(NAME_None, BrickMassKg, true);
		}
	}
}
