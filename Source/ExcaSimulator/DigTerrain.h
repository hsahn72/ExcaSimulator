#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigTerrain.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Deformable soil: a height grid rendered as procedural mesh chunks.
// The bucket cuts it down (Dig) and dumped soil piles up on it (Deposit).
// The grid is centred on the actor location; actor rotation is ignored.
UCLASS()
class EXCASIMULATOR_API ADigTerrain : public AActor
{
	GENERATED_BODY()

public:
	ADigTerrain();

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "4"))
	int32 CellsPerChunk = 20;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "1"))
	int32 ChunksPerSide = 8;

	// Grid spacing in cm. 20 cells x 8 chunks x 20 cm = 32 m square.
	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "5.0"))
	float CellSize = 20.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxDigDepth = 500.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float NoiseAmplitude = 4.f;

	// Dumped soil slumps until no slope is steeper than this.
	UPROPERTY(EditAnywhere, Category = "Terrain")
	float ReposeAngleDeg = 34.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 MaxChunkRebuildsPerTick = 6;

	// World Z of the surface; actor Z when outside the grid.
	float GetHeightAt(const FVector2D& WorldXY) const;
	bool IsInside(const FVector2D& WorldXY) const;

	// Carves a bowl whose lowest point is Center.Z. Returns the removed volume
	// in cm^3, never more than MaxVolume.
	float Dig(const FVector& Center, float Radius, float MaxVolume);

	// Adds VolumeCm3 of soil in a cone around Center, then lets it slump.
	void Deposit(const FVector& Center, float VolumeCm3, float Radius);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	int32 VertsPerSide() const { return CellsPerChunk * ChunksPerSide + 1; }
	float HalfSize() const { return CellsPerChunk * ChunksPerSide * CellSize * 0.5f; }
	int32 Index(int32 X, int32 Y) const { return Y * VertsPerSide() + X; }
	FVector2D ToGrid(const FVector2D& WorldXY) const;

	void Rebuild();
	void BuildChunk(int32 ChunkX, int32 ChunkY);
	void MarkDirty(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY);
	void Relax(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, int32 Iterations);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SoilMaterial;

	// Local Z per grid vertex; 0 is the untouched surface.
	TArray<float> Heights;
	TSet<int32> DirtyChunks;
};
