#include "DigTerrain.h"

#include "ExcaAssets.h"
#include "Engine/CollisionProfile.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADigTerrain::ADigTerrain()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(ExcaAssets::BaseMaterialPath);
	BaseMaterial = MaterialFinder.Object;
}

void ADigTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ADigTerrain::BeginPlay()
{
	Super::BeginPlay();
	// Heights are not saved with the level, so a loaded actor starts empty.
	if (Heights.Num() != VertsPerSide() * VertsPerSide())
	{
		Rebuild();
	}
}

void ADigTerrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	int32 Budget = MaxChunkRebuildsPerTick;
	for (auto It = DirtyChunks.CreateIterator(); It && Budget > 0; ++It, --Budget)
	{
		BuildChunk(*It % ChunksPerSide, *It / ChunksPerSide);
		It.RemoveCurrent();
	}
}

void ADigTerrain::Rebuild()
{
	const int32 N = VertsPerSide();
	Heights.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			Heights[Index(X, Y)] = FMath::PerlinNoise2D(FVector2D(X, Y) * 0.07f) * NoiseAmplitude;
		}
	}

	SoilMaterial = ExcaAssets::MakeColor(BaseMaterial, this, ExcaAssets::SoilColor);
	Mesh->ClearAllMeshSections();
	DirtyChunks.Reset();
	for (int32 CY = 0; CY < ChunksPerSide; ++CY)
	{
		for (int32 CX = 0; CX < ChunksPerSide; ++CX)
		{
			BuildChunk(CX, CY);
		}
	}
}

FVector2D ADigTerrain::ToGrid(const FVector2D& WorldXY) const
{
	const FVector2D Local = WorldXY - FVector2D(GetActorLocation());
	return (Local + FVector2D(HalfSize())) / CellSize;
}

bool ADigTerrain::IsInside(const FVector2D& WorldXY) const
{
	const FVector2D G = ToGrid(WorldXY);
	const float Max = VertsPerSide() - 1;
	return G.X >= 0.f && G.Y >= 0.f && G.X <= Max && G.Y <= Max;
}

float ADigTerrain::GetHeightAt(const FVector2D& WorldXY) const
{
	const float BaseZ = GetActorLocation().Z;
	if (!IsInside(WorldXY) || Heights.Num() == 0)
	{
		return BaseZ;
	}

	const int32 N = VertsPerSide();
	const FVector2D G = ToGrid(WorldXY);
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(G.X), 0, N - 2);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(G.Y), 0, N - 2);
	const float FX = G.X - X0;
	const float FY = G.Y - Y0;

	const float H00 = Heights[Index(X0, Y0)];
	const float H10 = Heights[Index(X0 + 1, Y0)];
	const float H01 = Heights[Index(X0, Y0 + 1)];
	const float H11 = Heights[Index(X0 + 1, Y0 + 1)];
	return BaseZ + FMath::Lerp(FMath::Lerp(H00, H10, FX), FMath::Lerp(H01, H11, FX), FY);
}

float ADigTerrain::Dig(const FVector& Center, float Radius, float MaxVolume)
{
	if (MaxVolume <= 0.f || Radius <= 0.f || Heights.Num() == 0)
	{
		return 0.f;
	}

	const int32 N = VertsPerSide();
	const FVector2D G = ToGrid(FVector2D(Center));
	const float RadiusCells = Radius / CellSize;
	const int32 MinX = FMath::Clamp(FMath::FloorToInt(G.X - RadiusCells), 0, N - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt(G.X + RadiusCells), 0, N - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt(G.Y - RadiusCells), 0, N - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt(G.Y + RadiusCells), 0, N - 1);
	const float CutZ = Center.Z - GetActorLocation().Z;
	const float CellArea = CellSize * CellSize;

	TArray<TPair<int32, float>, TInlineAllocator<64>> Cuts;
	float Total = 0.f;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const float Dist = FVector2D::Distance(FVector2D(X, Y), G) * CellSize;
			if (Dist >= Radius)
			{
				continue;
			}
			const float Target = FMath::Max(CutZ + Radius - FMath::Sqrt(Radius * Radius - Dist * Dist), -MaxDigDepth);
			const float Depth = Heights[Index(X, Y)] - Target;
			if (Depth > 0.f)
			{
				Cuts.Emplace(Index(X, Y), Depth);
				Total += Depth * CellArea;
			}
		}
	}

	if (Total <= 0.f)
	{
		return 0.f;
	}

	const float Scale = FMath::Min(1.f, MaxVolume / Total);
	for (const TPair<int32, float>& Cut : Cuts)
	{
		Heights[Cut.Key] -= Cut.Value * Scale;
	}
	MarkDirty(MinX, MinY, MaxX, MaxY);
	return Total * Scale;
}

void ADigTerrain::Deposit(const FVector& Center, float VolumeCm3, float Radius)
{
	if (VolumeCm3 <= 0.f || Radius <= 0.f || Heights.Num() == 0)
	{
		return;
	}

	const int32 N = VertsPerSide();
	const FVector2D G = ToGrid(FVector2D(Center));
	const float RadiusCells = Radius / CellSize;
	const int32 MinX = FMath::Clamp(FMath::FloorToInt(G.X - RadiusCells), 0, N - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt(G.X + RadiusCells), 0, N - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt(G.Y - RadiusCells), 0, N - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt(G.Y + RadiusCells), 0, N - 1);

	float WeightSum = 0.f;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			WeightSum += FMath::Max(0.f, 1.f - FVector2D::Distance(FVector2D(X, Y), G) / RadiusCells);
		}
	}
	if (WeightSum <= 0.f)
	{
		return;
	}

	const float HeightPerWeight = VolumeCm3 / (WeightSum * CellSize * CellSize);
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const float W = FMath::Max(0.f, 1.f - FVector2D::Distance(FVector2D(X, Y), G) / RadiusCells);
			Heights[Index(X, Y)] += W * HeightPerWeight;
		}
	}

	const int32 Spread = FMath::CeilToInt(RadiusCells) + 2;
	const int32 RMinX = FMath::Max(0, MinX - Spread);
	const int32 RMinY = FMath::Max(0, MinY - Spread);
	const int32 RMaxX = FMath::Min(N - 1, MaxX + Spread);
	const int32 RMaxY = FMath::Min(N - 1, MaxY + Spread);
	Relax(RMinX, RMinY, RMaxX, RMaxY, 6);
	MarkDirty(RMinX, RMinY, RMaxX, RMaxY);
}

void ADigTerrain::Relax(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, int32 Iterations)
{
	// Moves soil downhill wherever the slope exceeds the angle of repose.
	// Pairwise transfers conserve volume.
	const float MaxStep = CellSize * FMath::Tan(FMath::DegreesToRadians(ReposeAngleDeg));
	static const FIntPoint Neighbours[] = { {1, 0}, {0, 1}, {-1, 0}, {0, -1} };
	const int32 N = VertsPerSide();

	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				float& H = Heights[Index(X, Y)];
				for (const FIntPoint& Off : Neighbours)
				{
					const int32 NX = X + Off.X;
					const int32 NY = Y + Off.Y;
					if (NX < 0 || NY < 0 || NX >= N || NY >= N)
					{
						continue;
					}
					float& HN = Heights[Index(NX, NY)];
					const float Excess = H - HN - MaxStep;
					if (Excess > 0.f)
					{
						const float Move = Excess * 0.25f;
						H -= Move;
						HN += Move;
					}
				}
			}
		}
	}
}

void ADigTerrain::MarkDirty(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY)
{
	// Normals read one vertex beyond the edit, and chunk-edge vertices are
	// shared by two chunks, so widen by one vertex before mapping to chunks.
	const int32 N = VertsPerSide();
	const int32 MinCX = FMath::Clamp((FMath::Max(MinX - 1, 0) - 1) / CellsPerChunk, 0, ChunksPerSide - 1);
	const int32 MinCY = FMath::Clamp((FMath::Max(MinY - 1, 0) - 1) / CellsPerChunk, 0, ChunksPerSide - 1);
	const int32 MaxCX = FMath::Clamp(FMath::Min(MaxX + 1, N - 1) / CellsPerChunk, 0, ChunksPerSide - 1);
	const int32 MaxCY = FMath::Clamp(FMath::Min(MaxY + 1, N - 1) / CellsPerChunk, 0, ChunksPerSide - 1);
	for (int32 CY = MinCY; CY <= MaxCY; ++CY)
	{
		for (int32 CX = MinCX; CX <= MaxCX; ++CX)
		{
			DirtyChunks.Add(CY * ChunksPerSide + CX);
		}
	}
}

void ADigTerrain::BuildChunk(int32 ChunkX, int32 ChunkY)
{
	const int32 N = VertsPerSide();
	const int32 V = CellsPerChunk + 1;
	const float Half = HalfSize();

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	Vertices.Reserve(V * V);
	Normals.Reserve(V * V);
	UVs.Reserve(V * V);
	Triangles.Reserve(CellsPerChunk * CellsPerChunk * 6);

	auto HeightAt = [this, N](int32 X, int32 Y)
	{
		return Heights[Index(FMath::Clamp(X, 0, N - 1), FMath::Clamp(Y, 0, N - 1))];
	};

	for (int32 LY = 0; LY < V; ++LY)
	{
		for (int32 LX = 0; LX < V; ++LX)
		{
			const int32 GX = ChunkX * CellsPerChunk + LX;
			const int32 GY = ChunkY * CellsPerChunk + LY;
			Vertices.Emplace(GX * CellSize - Half, GY * CellSize - Half, HeightAt(GX, GY));
			Normals.Add(FVector(HeightAt(GX - 1, GY) - HeightAt(GX + 1, GY),
				HeightAt(GX, GY - 1) - HeightAt(GX, GY + 1),
				2.f * CellSize).GetSafeNormal());
			UVs.Emplace(GX * 0.25f, GY * 0.25f);
		}
	}

	for (int32 LY = 0; LY < CellsPerChunk; ++LY)
	{
		for (int32 LX = 0; LX < CellsPerChunk; ++LX)
		{
			const int32 I0 = LY * V + LX;        // (x,   y)
			const int32 I1 = I0 + 1;             // (x+1, y)
			const int32 I3 = I0 + V;             // (x,   y+1)
			const int32 I2 = I3 + 1;             // (x+1, y+1)
			Triangles.Append({ I0, I1, I3, I1, I2, I3 });
		}
	}

	const int32 Section = ChunkY * ChunksPerSide + ChunkX;
	Mesh->CreateMeshSection_LinearColor(Section, Vertices, Triangles, Normals, UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	Mesh->SetMaterial(Section, SoilMaterial);
}
