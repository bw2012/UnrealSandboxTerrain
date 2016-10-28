#pragma once

#include "EngineMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SandboxTerrainMeshComponent.h"
#include "SandboxTerrainController.h"
#include <memory>
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

	ASandboxTerrainController* controller;

	void makeTerrain();

	VoxelData* getVoxelData() { return voxel_data; }

	void setVoxelData(VoxelData* vd) { this->voxel_data = vd; }

	void applyTerrainMesh(std::shared_ptr<MeshData> mesh_data_ptr);

	std::shared_ptr<MeshData> generateMesh(VoxelData &voxel_data);

	bool volatile isLoaded = false;

private:
	VoxelData* voxel_data;

	void init();

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox")
	USandboxTerrainMeshComponent* MainTerrainMesh;

};
