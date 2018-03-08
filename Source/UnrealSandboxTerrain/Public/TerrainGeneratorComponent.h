// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include <unordered_map>
#include "VoxelIndex.h"
#include "TerrainGeneratorComponent.generated.h"

class ASandboxTerrainController;
class TVoxelData;
class UTerrainZoneComponent;
struct FSandboxFoliage;

class TZoneHeightMapData {
    
private:
    
    int Size;
    
    float* HeightLevelArray;
    
    float MaxHeightLevel = -999999.f;
    
    float MinHeightLevel = 999999.f;
    
public:
    
    TZoneHeightMapData(int size);
    
    ~TZoneHeightMapData();
    
    FORCEINLINE void SetHeightLevel(TVoxelIndex VoxelIndex, float HeightLevel);
    
    FORCEINLINE float GetHeightLevel(TVoxelIndex VoxelIndex) const;
    
    FORCEINLINE float GetMaxHeightLevel() const { return this->MaxHeightLevel; };
    
    FORCEINLINE float GetMinHeightLevel() const { return this->MinHeightLevel; };
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

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainGeneratorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginDestroy();

	virtual void BeginPlay();

public:

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Generator")
	TArray<FTerrainUndergroundLayer> UndergroundLayers;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	FVector MaxTerrainBounds;

	void GenerateVoxelTerrain (TVoxelData &VoxelData);

	float GroundLevelFunc(FVector v);

	void GenerateNewFoliage(UTerrainZoneComponent* Zone);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone);

private:
    
	TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;

    std::unordered_map<TVoxelIndex, TZoneHeightMapData*> ZoneHeightMapCollection;

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};
	
	float ClcDensityByGroundLevel(const FVector& v, const float gl) const;

	float DensityFunc(const FVector& ZoneIndex, const FVector& LocalPos, const FVector& WorldPos);

	unsigned char MaterialFunc(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel);

	void GenerateZoneVolume(TVoxelData &VoxelData, const TZoneHeightMapData* ZoneHeightMapData);

	FTerrainUndergroundLayer* GetUndergroundMaterialLayer(float Z, float GroundLevel);

	int GetAllUndergroundMaterialLayers(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList);

	bool IsZoneOnGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin);

	bool IsZoneOverGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin);
};
