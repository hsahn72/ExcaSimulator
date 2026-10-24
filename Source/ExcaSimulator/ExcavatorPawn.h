#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ExcavatorPawn.generated.h"

class ADigTerrain;
class UBoxComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

// Kinematic excavator built from basic shapes. Joints are driven by the two
// thumbsticks using the ISO control pattern:
//   Left stick  X: swing          Y: arm out (push) / in (pull)
//   Right stick X: bucket curl (left) / dump (right)   Y: boom down (push) / up (pull)
// Holding the left grip switches the sticks to the tracks (left Y / right Y).
UCLASS()
class EXCASIMULATOR_API AExcavatorPawn : public APawn
{
	GENERATED_BODY()

public:
	AExcavatorPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	// Joint speeds at full stick, deg/s.
	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float SwingSpeed = 30.f;

	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float BoomSpeed = 22.f;

	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float ArmSpeed = 30.f;

	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float BucketSpeed = 50.f;

	// Track speed at full stick, cm/s (150 = 5.4 km/h).
	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float TravelSpeed = 150.f;

	// How quickly joints reach the commanded speed; lower feels heavier.
	UPROPERTY(EditAnywhere, Category = "Excavator|Speed")
	float HydraulicResponse = 5.f;

	// Joint limits in degrees, relative to the parent link.
	UPROPERTY(EditAnywhere, Category = "Excavator|Limits")
	FVector2D BoomLimits = FVector2D(-25.f, 60.f);

	UPROPERTY(EditAnywhere, Category = "Excavator|Limits")
	FVector2D ArmLimits = FVector2D(-155.f, -35.f);

	UPROPERTY(EditAnywhere, Category = "Excavator|Limits")
	FVector2D BucketLimits = FVector2D(-160.f, 30.f);

	// 0.9 m^3 heaped bucket, in cm^3.
	UPROPERTY(EditAnywhere, Category = "Excavator|Digging")
	float BucketCapacity = 900000.f;

	UPROPERTY(EditAnywhere, Category = "Excavator|Digging")
	float DigRadius = 45.f;

	// Volume of each soil lump spilled when dumping, cm^3.
	UPROPERTY(EditAnywhere, Category = "Excavator|Digging")
	float ClumpVolume = 30000.f;

	// Spill rate with the bucket opening straight down, cm^3/s.
	UPROPERTY(EditAnywhere, Category = "Excavator|Digging")
	float SpillRate = 1200000.f;

	// Where the operator's eyes should be, relative to the upper structure.
	UPROPERTY(EditAnywhere, Category = "Excavator|VR")
	FVector SeatEyeLocation = FVector(25.f, -77.f, 135.f);

	float GetBucketFill() const { return BucketCapacity > 0.f ? BucketLoad / BucketCapacity : 0.f; }

protected:
	virtual void BeginPlay() override;

private:
	UStaticMeshComponent* AddPart(FName Name, USceneComponent* Parent, const FVector& Center, const FVector& SizeCm, const FLinearColor& Color);
	void CreateInputAssets();
	float ReadAxis(const UInputAction* Action) const;
	void UpdateJoints(float DeltaSeconds, float SwingCmd, float BoomCmd, float ArmCmd, float BucketCmd);
	void UpdateTravel(float DeltaSeconds, float LeftCmd, float RightCmd);
	void UpdateDigging(float DeltaSeconds);
	void UpdateButtons();
	void RecenterSeat();
	ADigTerrain* GetTerrain();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Undercarriage;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SwingPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> BoomPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ArmPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> BucketPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> BucketTip;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> BucketCavity;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BucketSoil;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY()
	TArray<FLinearColor> PartColors;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeftX;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeftY;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RightX;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RightY;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TravelHold;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Recenter;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResetLevel;

	TWeakObjectPtr<ADigTerrain> Terrain;
	FVector BucketSoilScale = FVector::OneVector;

	float SwingAngle = 0.f;
	float BoomAngle = 20.f;
	float ArmAngle = -110.f;
	float BucketAngle = -70.f;
	float SwingRate = 0.f;
	float BoomRate = 0.f;
	float ArmRate = 0.f;
	float BucketRate = 0.f;
	float LeftTrack = 0.f;
	float RightTrack = 0.f;

	float BucketLoad = 0.f;
	float PendingSpill = 0.f;

	float RecenterTimer = 1.f;
	bool bApplySeatOffset = false;
	bool bRecenterWasDown = false;
	bool bResetWasDown = false;
};
