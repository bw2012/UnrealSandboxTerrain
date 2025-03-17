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

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	ESandboxFoliageType Type = ESandboxFoliageType::Grass;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	TArray<UStaticMesh*> MeshVariants;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 SpawnStep = 25;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	float Probability = 1;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 StartCullDistance = 100;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 EndCullDistance = 500;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	float OffsetRange = 10.0f;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	float ScaleMinZ = 0.5f;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	float ScaleMaxZ = 1.0f;
};

USTRUCT()
struct FTerrainUndergroundLayer {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 MatId = 0;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	float StartDepth = 0.f;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	FString Name;
};