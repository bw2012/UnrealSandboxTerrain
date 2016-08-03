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

private:
	VoxelData* voxel_data;

	void init();

	MeshData* generateMesh(VoxelData &voxel_data);

	void applyTerrainMesh(MeshData* voxel_data);

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox")
	USandboxTerrainMeshComponent* MainTerrainMesh;
	
};
