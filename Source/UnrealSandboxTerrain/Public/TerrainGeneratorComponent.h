// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainGeneratorComponent.generated.h"


class ASandboxTerrainController;
class TVoxelData;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainGeneratorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:


public:
	void GenerateVoxelTerrain(TVoxelData &VoxelData);

	float GroundLevelFunc(FVector v);

private:

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};
	
	float ClcDensityByGroundLevel(FVector v);

	float DensityFunc(const FVector& ZoneIndex, const FVector& LocalPos, const FVector& WorldPos);

	unsigned char MaterialFunc(const FVector& LocalPos, const FVector& WorldPos);
};