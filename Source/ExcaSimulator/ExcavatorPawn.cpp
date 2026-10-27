#include "ExcavatorPawn.h"

#include "DigTerrain.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ExcaAssets.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "SoilClump.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float GroundOffset = 45.f;     // Undercarriage half height.
	constexpr float TrackGauge = 220.f;
	constexpr float TrackHalfLength = 200.f;
	constexpr float BoomLength = 570.f;
	constexpr float ArmLength = 290.f;
	constexpr float StickDeadZone = 0.15f;

	const FLinearColor Yellow(0.80f, 0.45f, 0.02f);
	const FLinearColor DarkGrey(0.04f, 0.04f, 0.04f);
	const FLinearColor MidGrey(0.15f, 0.15f, 0.15f);
	const FLinearColor Steel(0.25f, 0.25f, 0.27f);
}

AExcavatorPawn::AExcavatorPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(ExcaAssets::CubePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(ExcaAssets::BaseMaterialPath);
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	Undercarriage = CreateDefaultSubobject<UBoxComponent>(TEXT("Undercarriage"));
	Undercarriage->SetBoxExtent(FVector(225.f, 140.f, GroundOffset));
	Undercarriage->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	RootComponent = Undercarriage;

	AddPart(TEXT("TrackLeft"), Undercarriage, FVector(0.f, -110.f, 0.f), FVector(450.f, 60.f, 90.f), DarkGrey);
	AddPart(TEXT("TrackRight"), Undercarriage, FVector(0.f, 110.f, 0.f), FVector(450.f, 60.f, 90.f), DarkGrey);
	AddPart(TEXT("Frame"), Undercarriage, FVector(0.f, 0.f, 10.f), FVector(300.f, 160.f, 50.f), MidGrey);

	// Upper structure. Pivot sits 1 m above the ground.
	SwingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwingPivot"));
	SwingPivot->SetupAttachment(Undercarriage);
	SwingPivot->SetRelativeLocation(FVector(0.f, 0.f, 55.f));

	AddPart(TEXT("Deck"), SwingPivot, FVector(-20.f, 0.f, 10.f), FVector(300.f, 250.f, 20.f), Yellow);
	AddPart(TEXT("EngineHouse"), SwingPivot, FVector(-90.f, 0.f, 70.f), FVector(160.f, 250.f, 100.f), Yellow);
	AddPart(TEXT("Counterweight"), SwingPivot, FVector(-185.f, 0.f, 60.f), FVector(50.f, 260.f, 100.f), MidGrey);
	AddPart(TEXT("ToolBox"), SwingPivot, FVector(60.f, 85.f, 55.f), FVector(140.f, 80.f, 70.f), Yellow);

	// Open-sided cab on the left so the operator can see out.
	AddPart(TEXT("CabRoof"), SwingPivot, FVector(55.f, -77.f, 200.f), FVector(150.f, 95.f, 8.f), Yellow);
	AddPart(TEXT("CabBack"), SwingPivot, FVector(-20.f, -77.f, 110.f), FVector(6.f, 95.f, 180.f), Yellow);
	AddPart(TEXT("CabPostLeft"), SwingPivot, FVector(128.f, -122.f, 110.f), FVector(5.f, 5.f, 180.f), DarkGrey);
	AddPart(TEXT("CabPostRight"), SwingPivot, FVector(128.f, -32.f, 110.f), FVector(5.f, 5.f, 180.f), DarkGrey);
	AddPart(TEXT("Seat"), SwingPivot, FVector(-5.f, -77.f, 45.f), FVector(45.f, 50.f, 50.f), DarkGrey);

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(SwingPivot);
	VROrigin->SetRelativeLocation(SeatEyeLocation);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	Camera->bLockToHmd = true;

	// Front attachment: boom -> arm -> bucket, each pitching about its pivot.
	BoomPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoomPivot"));
	BoomPivot->SetupAttachment(SwingPivot);
	BoomPivot->SetRelativeLocation(FVector(110.f, 30.f, 50.f));
	AddPart(TEXT("Boom"), BoomPivot, FVector(BoomLength * 0.5f, 0.f, 0.f), FVector(BoomLength, 50.f, 60.f), Yellow);

	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(BoomPivot);
	ArmPivot->SetRelativeLocation(FVector(BoomLength, 0.f, 0.f));
	AddPart(TEXT("Arm"), ArmPivot, FVector(ArmLength * 0.5f, 0.f, 0.f), FVector(ArmLength, 40.f, 40.f), Yellow);

	// Bucket frame: X runs from the pin to the cutting edge, the opening faces -Z.
	BucketPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BucketPivot"));
	BucketPivot->SetupAttachment(ArmPivot);
	BucketPivot->SetRelativeLocation(FVector(ArmLength, 0.f, 0.f));
	AddPart(TEXT("BucketBack"), BucketPivot, FVector(60.f, 0.f, -3.f), FVector(120.f, 100.f, 6.f), Steel);
	AddPart(TEXT("BucketTop"), BucketPivot, FVector(3.f, 0.f, -30.f), FVector(6.f, 100.f, 60.f), Steel);
	AddPart(TEXT("BucketSideLeft"), BucketPivot, FVector(55.f, -48.f, -28.f), FVector(110.f, 4.f, 56.f), Steel);
	AddPart(TEXT("BucketSideRight"), BucketPivot, FVector(55.f, 48.f, -28.f), FVector(110.f, 4.f, 56.f), Steel);
	for (int32 Tooth = 0; Tooth < 4; ++Tooth)
	{
		AddPart(*FString::Printf(TEXT("BucketTooth%d"), Tooth), BucketPivot,
			FVector(122.f, -36.f + Tooth * 24.f, -6.f), FVector(12.f, 8.f, 8.f), DarkGrey);
	}

	BucketTip = CreateDefaultSubobject<USceneComponent>(TEXT("BucketTip"));
	BucketTip->SetupAttachment(BucketPivot);
	BucketTip->SetRelativeLocation(FVector(125.f, 0.f, -6.f));

	BucketCavity = CreateDefaultSubobject<USceneComponent>(TEXT("BucketCavity"));
	BucketCavity->SetupAttachment(BucketPivot);
	BucketCavity->SetRelativeLocation(FVector(55.f, 0.f, -28.f));

	BucketSoil = AddPart(TEXT("BucketSoil"), BucketCavity, FVector::ZeroVector, FVector(100.f, 88.f, 50.f), ExcaAssets::SoilColor);
	BucketSoil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BucketSoil->SetVisibility(false);
	BucketSoilScale = BucketSoil->GetRelativeScale3D();
}

UStaticMeshComponent* AExcavatorPawn::AddPart(FName Name, USceneComponent* Parent, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color)
{
	// The engine cube is 100 cm on each side, centred on its origin.
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(CubeMesh);
	Part->SetRelativeLocation(Center);
	Part->SetRelativeScale3D(SizeCm / 100.f);
	Part->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Parts.Add(Part);
	PartColors.Add(Color);
	return Part;
}

void AExcavatorPawn::BeginPlay()
{
	Super::BeginPlay();

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		Parts[i]->SetMaterial(0, ExcaAssets::MakeColor(BaseMaterial, this, PartColors[i]));
	}
	UpdateJoints(0.f, 0.f, 0.f, 0.f, 0.f);
}

void AExcavatorPawn::CreateInputAssets()
{
	if (Mapping)
	{
		return;
	}

	// Actions are built in code so the prototype needs no editor assets.
	Mapping = NewObject<UInputMappingContext>(this, TEXT("ExcavatorMapping"));

	auto MakeAction = [this](const TCHAR* Name)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = EInputActionValueType::Axis1D;
		return Action;
	};
	LeftX = MakeAction(TEXT("LeftX"));
	LeftY = MakeAction(TEXT("LeftY"));
	RightX = MakeAction(TEXT("RightX"));
	RightY = MakeAction(TEXT("RightY"));
	TravelHold = MakeAction(TEXT("TravelHold"));
	Recenter = MakeAction(TEXT("Recenter"));
	ResetLevel = MakeAction(TEXT("ResetLevel"));

	auto Stick = [this](UInputAction* Action, const FKey& Key)
	{
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Mapping);
		DeadZone->LowerThreshold = StickDeadZone;
		DeadZone->Type = EDeadZoneType::Axial;
		Mapping->MapKey(Action, Key).Modifiers.Add(DeadZone);
	};
	auto Button = [this](UInputAction* Action, const FKey& Key, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& KeyMapping = Mapping->MapKey(Action, Key);
		if (bNegate)
		{
			KeyMapping.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
		}
	};

	// Quest Touch controllers, then gamepad and keyboard for desktop testing.
	Stick(LeftX, EKeys::OculusTouch_Left_Thumbstick_X);
	Stick(LeftY, EKeys::OculusTouch_Left_Thumbstick_Y);
	Stick(RightX, EKeys::OculusTouch_Right_Thumbstick_X);
	Stick(RightY, EKeys::OculusTouch_Right_Thumbstick_Y);
	Stick(LeftX, EKeys::Gamepad_LeftX);
	Stick(LeftY, EKeys::Gamepad_LeftY);
	Stick(RightX, EKeys::Gamepad_RightX);
	Stick(RightY, EKeys::Gamepad_RightY);

	Button(LeftX, EKeys::D);
	Button(LeftX, EKeys::A, true);
	Button(LeftY, EKeys::W);
	Button(LeftY, EKeys::S, true);
	Button(RightX, EKeys::Right);
	Button(RightX, EKeys::Left, true);
	Button(RightY, EKeys::Up);
	Button(RightY, EKeys::Down, true);

	Button(TravelHold, EKeys::OculusTouch_Left_Grip_Click);
	Button(TravelHold, EKeys::Gamepad_LeftShoulder);
	Button(TravelHold, EKeys::LeftShift);

	Button(Recenter, EKeys::OculusTouch_Right_A_Click);
	Button(Recenter, EKeys::Gamepad_FaceButton_Top);
	Button(Recenter, EKeys::R);

	Button(ResetLevel, EKeys::OculusTouch_Right_B_Click);
	Button(ResetLevel, EKeys::Gamepad_Special_Left);
	Button(ResetLevel, EKeys::BackSpace);
}

void AExcavatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateInputAssets();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("ExcavatorPawn needs EnhancedInputComponent as the default input component class."));
		return;
	}
	for (UInputAction* Action : { LeftX.Get(), LeftY.Get(), RightX.Get(), RightY.Get(), TravelHold.Get(), Recenter.Get(), ResetLevel.Get() })
	{
		Input->BindActionValue(Action);
	}
}

void AExcavatorPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	CreateInputAssets();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(Mapping, 0);
		}
	}
}

float AExcavatorPawn::ReadAxis(const UInputAction* Action) const
{
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	return Input && Action ? Input->GetBoundActionValue(Action).Get<float>() : 0.f;
}

void AExcavatorPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float LX = FMath::Clamp(ReadAxis(LeftX), -1.f, 1.f);
	const float LY = FMath::Clamp(ReadAxis(LeftY), -1.f, 1.f);
	const float RX = FMath::Clamp(ReadAxis(RightX), -1.f, 1.f);
	const float RY = FMath::Clamp(ReadAxis(RightY), -1.f, 1.f);

	if (ReadAxis(TravelHold) > 0.5f)
	{
		UpdateJoints(DeltaSeconds, 0.f, 0.f, 0.f, 0.f);
		UpdateTravel(DeltaSeconds, LY, RY);
	}
	else
	{
		UpdateJoints(DeltaSeconds, LX, -RY, LY, RX);
		UpdateTravel(DeltaSeconds, 0.f, 0.f);
	}

	UpdateDigging(DeltaSeconds);
	UpdateButtons();

	if (bApplySeatOffset)
	{
		// The HMD pose after the reset is now in the camera's relative
		// location; shift the origin so the eyes land on the seat.
		bApplySeatOffset = false;
		VROrigin->SetRelativeLocation(SeatEyeLocation - Camera->GetRelativeLocation());
	}
	if (RecenterTimer > 0.f)
	{
		RecenterTimer -= DeltaSeconds;
		if (RecenterTimer <= 0.f)
		{
			RecenterSeat();
		}
	}
}

void AExcavatorPawn::UpdateJoints(float DeltaSeconds, float SwingCmd, float BoomCmd, float ArmCmd, float BucketCmd)
{
	// Rates ease toward the command, which reads like hydraulic lag.
	auto Drive = [DeltaSeconds, this](float& Angle, float& Rate, float Cmd, float Speed, const FVector2D& Limits, bool bLimited)
	{
		Rate = FMath::FInterpTo(Rate, Cmd * Speed, DeltaSeconds, HydraulicResponse);
		Angle += Rate * DeltaSeconds;
		if (bLimited && (Angle < Limits.X || Angle > Limits.Y))
		{
			Angle = FMath::Clamp(Angle, static_cast<float>(Limits.X), static_cast<float>(Limits.Y));
			Rate = 0.f;
		}
	};
	Drive(SwingAngle, SwingRate, SwingCmd, SwingSpeed, FVector2D::ZeroVector, false);
	Drive(BoomAngle, BoomRate, BoomCmd, BoomSpeed, BoomLimits, true);
	Drive(ArmAngle, ArmRate, ArmCmd, ArmSpeed, ArmLimits, true);
	Drive(BucketAngle, BucketRate, BucketCmd, BucketSpeed, BucketLimits, true);
	SwingAngle = FRotator::NormalizeAxis(SwingAngle);

	SwingPivot->SetRelativeRotation(FRotator(0.f, SwingAngle, 0.f));
	BoomPivot->SetRelativeRotation(FRotator(BoomAngle, 0.f, 0.f));
	ArmPivot->SetRelativeRotation(FRotator(ArmAngle, 0.f, 0.f));
	BucketPivot->SetRelativeRotation(FRotator(BucketAngle, 0.f, 0.f));
}

void AExcavatorPawn::UpdateTravel(float DeltaSeconds, float LeftCmd, float RightCmd)
{
	LeftTrack = FMath::FInterpTo(LeftTrack, LeftCmd, DeltaSeconds, HydraulicResponse * 0.5f);
	RightTrack = FMath::FInterpTo(RightTrack, RightCmd, DeltaSeconds, HydraulicResponse * 0.5f);

	const float Speed = (LeftTrack + RightTrack) * 0.5f * TravelSpeed;
	const float YawRateDeg = FMath::RadiansToDegrees((LeftTrack - RightTrack) * TravelSpeed / TrackGauge);

	FRotator Rotation = GetActorRotation();
	Rotation.Yaw += YawRateDeg * DeltaSeconds;
	FVector Location = GetActorLocation() + Rotation.Vector() * Speed * DeltaSeconds;

	// Sit on the highest ground under the four track ends.
	if (ADigTerrain* T = GetTerrain())
	{
		const FVector Forward = Rotation.Vector() * TrackHalfLength;
		const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TrackGauge * 0.5f);
		float GroundZ = -UE_BIG_NUMBER;
		for (const FVector& Corner : { Forward + Right, Forward - Right, -Forward + Right, -Forward - Right })
		{
			GroundZ = FMath::Max(GroundZ, T->GetHeightAt(FVector2D(Location + Corner)));
		}
		Location.Z = FMath::FInterpTo(Location.Z, GroundZ + GroundOffset, DeltaSeconds, 8.f);
	}

	SetActorLocationAndRotation(Location, Rotation);
}

void AExcavatorPawn::UpdateDigging(float DeltaSeconds)
{
	ADigTerrain* T = GetTerrain();
	if (!T)
	{
		return;
	}

	// The opening faces the bucket's -Z, so a positive up.Z means it is
	// tipped toward the ground and the load pours out.
	const float Tilt = BucketPivot->GetUpVector().Z - 0.15f;
	if (Tilt > 0.f && (BucketLoad > 0.f || PendingSpill > 0.f))
	{
		const float Out = FMath::Min(BucketLoad, SpillRate * Tilt * DeltaSeconds);
		BucketLoad -= Out;
		PendingSpill += Out;

		const bool bEmpty = BucketLoad <= UE_KINDA_SMALL_NUMBER;
		while (PendingSpill >= ClumpVolume || (bEmpty && PendingSpill > 0.f))
		{
			const float Volume = FMath::Min(PendingSpill, ClumpVolume);
			PendingSpill -= Volume;

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Jitter = FMath::VRand() * 10.f;
			if (ASoilClump* Clump = GetWorld()->SpawnActor<ASoilClump>(BucketCavity->GetComponentLocation() + Jitter, FRotator::ZeroRotator, Params))
			{
				Clump->Init(Volume, T);
			}
		}
	}
	else
	{
		const FVector Tip = BucketTip->GetComponentLocation();
		if (Tip.Z < T->GetHeightAt(FVector2D(Tip)))
		{
			BucketLoad += T->Dig(Tip, DigRadius, BucketCapacity - BucketLoad);
		}
	}

	const float Fill = GetBucketFill();
	BucketSoil->SetVisibility(Fill > 0.02f);
	BucketSoil->SetRelativeScale3D(BucketSoilScale * FMath::Pow(FMath::Clamp(Fill, 0.f, 1.f), 1.f / 3.f));
}

void AExcavatorPawn::UpdateButtons()
{
	const bool bRecenterDown = ReadAxis(Recenter) > 0.5f;
	if (bRecenterDown && !bRecenterWasDown)
	{
		RecenterSeat();
	}
	bRecenterWasDown = bRecenterDown;

	const bool bResetDown = ReadAxis(ResetLevel) > 0.5f;
	if (bResetDown && !bResetWasDown)
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
	bResetWasDown = bResetDown;
}

void AExcavatorPawn::RecenterSeat()
{
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.f, EOrientPositionSelector::Orientation);
		bApplySeatOffset = true;
	}
	else
	{
		VROrigin->SetRelativeLocation(SeatEyeLocation);
	}
}

ADigTerrain* AExcavatorPawn::GetTerrain()
{
	if (!Terrain.IsValid())
	{
		TActorIterator<ADigTerrain> It(GetWorld());
		Terrain = It ? *It : nullptr;
	}
	return Terrain.Get();
}
