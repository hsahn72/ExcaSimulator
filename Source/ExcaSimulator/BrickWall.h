#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrickWall.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

// Demolition target for the prototype: a dry-stacked wall of physics bricks
// on a concrete slab. Bricks start asleep and wake when something hits them.
// The wall runs along the actor's X axis.
UCLASS()
class EXCASIMULATOR_API ABrickWall : public AActor
{
	GENERATED_BODY()

public:
	ABrickWall();

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (ClampMin = "1"))
	int32 Columns = 15;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (ClampMin = "1"))
	int32 Rows = 14;

	// Length (X), thickness (Y), height (Z) in cm.
	UPROPERTY(EditAnywhere, Category = "Wall")
	FVector BrickSize = FVector(40.f, 20.f, 20.f);

	UPROPERTY(EditAnywhere, Category = "Wall")
	float BrickMassKg = 12.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Slab;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;
};
