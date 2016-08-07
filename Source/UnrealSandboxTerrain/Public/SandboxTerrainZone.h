#pragma once

#include "EngineMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SandboxTerrainMeshComponent.h"
#include "SandboxTerrainZone.generated.h"

class VoxelData;
struct MeshData;

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainZone : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	ASandboxTerrainZone();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//===============================================================================

public:

	void makeTerrain();

	bool fillZone();

	VoxelData* getVoxelData() { return voxel_data; }

	void applyTerrainMesh(MeshData* voxel_data);

	MeshData* generateMesh(VoxelData &voxel_data);

	bool volatile isLoaded = false;

private:
	VoxelData* voxel_data;

	void init();

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox")
	USandboxTerrainMeshComponent* MainTerrainMesh;
	
};
