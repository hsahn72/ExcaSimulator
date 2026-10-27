#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoilClump.generated.h"

class ADigTerrain;
class UStaticMeshComponent;
class UMaterialInterface;

// A physics lump of dumped soil. Once it comes to rest on the ground it is
// merged back into the terrain as a pile.
UCLASS()
class EXCASIMULATOR_API ASoilClump : public AActor
{
	GENERATED_BODY()

public:
	ASoilClump();

	void Init(float InVolumeCm3, ADigTerrain* InTerrain);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	TWeakObjectPtr<ADigTerrain> Terrain;
	float VolumeCm3 = 0.f;
	float Radius = 10.f;
	float Age = 0.f;
	float StillTime = 0.f;
};
