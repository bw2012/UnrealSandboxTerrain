// Copyright blackw 2025-2030

#pragma once

#include "EngineMinimal.h"
#include "TerrainGeneratorComponent.h"
#include "FlatTerrainGeneratorComponent.generated.h"



/**
*
*/
UCLASS(Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class UNREALSANDBOXTERRAIN_API UFlatTerrainGeneratorComponent : public UTerrainGeneratorComponent {
	GENERATED_UCLASS_BODY()

public:
		
	virtual float GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const;
	
};
