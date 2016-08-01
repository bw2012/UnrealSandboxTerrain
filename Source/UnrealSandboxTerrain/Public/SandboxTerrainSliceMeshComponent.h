// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "SandboxTerrainSliceMeshComponent.generated.h"

/**
 * 
 */
UCLASS()
class UNREALSANDBOXTERRAIN_API USandboxTerrainSliceMeshComponent : public USandboxTerrainMeshComponent
{
	GENERATED_BODY()
	
public:
	float getZLevel() { return z_level; };

	void setZLevel(float z) { z_level = z; };


private:
	float z_level;
	
};
