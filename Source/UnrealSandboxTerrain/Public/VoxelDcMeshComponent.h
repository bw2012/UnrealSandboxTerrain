// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "SandboxTerrainMeshComponent.h"
#include "VoxelDcMeshComponent.generated.h"




/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UVoxelDcMeshComponent : public UMeshComponent {
	GENERATED_UCLASS_BODY()


public:

	//virtual void BeginDestroy();

	virtual void BeginPlay() override;

	//virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
};