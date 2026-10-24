#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ExcaGameMode.generated.h"

// Prototype game mode: spawns the dig terrain and the brick wall if the level
// does not already contain them, and gives the player an excavator.
UCLASS()
class EXCASIMULATOR_API AExcaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExcaGameMode();

	virtual void StartPlay() override;

	UPROPERTY(EditAnywhere, Category = "Prototype")
	FVector TerrainLocation = FVector::ZeroVector;

	// On the excavator's right when it spawns at the origin facing +X.
	UPROPERTY(EditAnywhere, Category = "Prototype")
	FVector WallLocation = FVector(0.f, 1000.f, 0.f);
};
