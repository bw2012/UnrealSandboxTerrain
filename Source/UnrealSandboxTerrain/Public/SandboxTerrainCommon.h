#pragma once

#include "Engine.h"
#include "VoxelIndex.h"
#include "SandboxTerrainCommon.generated.h"

struct TSpawnZoneParam {

	TSpawnZoneParam() { };

	TSpawnZoneParam(const TVoxelIndex& Index_) : Index(Index_) { };

	TVoxelIndex Index;

};

UENUM(BlueprintType)
enum class ESandboxFoliageType : uint8 {
	Grass = 0			UMETA(DisplayName = "Grass"),
	Tree = 1			UMETA(DisplayName = "Tree"),
	Cave = 2			UMETA(DisplayName = "Cave foliage"),
	Custom = 3			UMETA(DisplayName = "Custom"),
	Bush = 4			UMETA(DisplayName = "Bush"),
	Flower = 5			UMETA(DisplayName = "Flower"),
	ForestFoliage = 6	UMETA(DisplayName = "Forest foliage"),
};

USTRUCT()
struct FSandboxFoliage {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	ESandboxFoliageType Type;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMesh*> MeshVariants;

	UPROPERTY(EditAnywhere)
	int32 SpawnStep = 25;

	UPROPERTY(EditAnywhere)
	float Probability = 1;

	UPROPERTY(EditAnywhere)
	int32 StartCullDistance = 100;

	UPROPERTY(EditAnywhere)
	int32 EndCullDistance = 500;

	UPROPERTY(EditAnywhere)
	float OffsetRange = 10.0f;

	UPROPERTY(EditAnywhere)
	float ScaleMinZ = 0.5f;

	UPROPERTY(EditAnywhere)
	float ScaleMaxZ = 1.0f;
};

USTRUCT()
struct FTerrainUndergroundLayer {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 MatId;

	UPROPERTY(EditAnywhere)
	float StartDepth;

	UPROPERTY(EditAnywhere)
	FString Name;
};