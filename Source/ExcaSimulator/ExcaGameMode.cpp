#include "ExcaGameMode.h"

#include "BrickWall.h"
#include "DigTerrain.h"
#include "EngineUtils.h"
#include "ExcavatorPawn.h"

AExcaGameMode::AExcaGameMode()
{
	DefaultPawnClass = AExcavatorPawn::StaticClass();
}

void AExcaGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!TActorIterator<ADigTerrain>(World))
	{
		World->SpawnActor<ADigTerrain>(TerrainLocation, FRotator::ZeroRotator);
	}
	if (!TActorIterator<ABrickWall>(World))
	{
		World->SpawnActor<ABrickWall>(WallLocation, FRotator::ZeroRotator);
	}
}
