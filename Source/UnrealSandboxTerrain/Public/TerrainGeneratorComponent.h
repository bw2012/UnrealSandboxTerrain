// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include <unordered_map>
#include "VoxelIndex.h"
#include "TerrainGeneratorComponent.generated.h"

class ASandboxTerrainController;
class TVoxelData;

class TZoneHeightMapData {
    
private:
    
    int Size;
    
    float* HeightLevelArray;
    
    float MaxHeightLevel;
    
    float MinHeightLevel;
    
public:
    
    TZoneHeightMapData(int size);
    
    ~TZoneHeightMapData();
    
    FORCEINLINE void SetHeightLevel(TVoxelIndex VoxelIndex, float HeightLevel);
    
    FORCEINLINE float GetHeightLevel(TVoxelIndex VoxelIndex) const ;
    
    FORCEINLINE float GetMaxHeightLevel() const { return this->MaxHeightLevel; };
    
    FORCEINLINE float GetMinHeightLevel() const { return this->MinHeightLevel; };
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

public:
	void GenerateVoxelTerrain (TVoxelData &VoxelData);

	float GroundLevelFunc(FVector v);

private:
    
    std::unordered_map<TVoxelIndex, TZoneHeightMapData*> ZoneHeightMapCollection;

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};
	
	float ClcDensityByGroundLevel(const FVector& v, const float gl) const;

	//float ClcDensityByGroundLevel(FVector v);

	float DensityFunc(const FVector& ZoneIndex, const FVector& LocalPos, const FVector& WorldPos);

	unsigned char MaterialFunc(const FVector& LocalPos, const FVector& WorldPos, const float GroundLevel);

	void GenerateZoneVolume(TVoxelData &VoxelData, const TZoneHeightMapData* ZoneHeightMapData);
};
