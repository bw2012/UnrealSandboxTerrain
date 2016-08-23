#pragma once

#include "EngineMinimal.h"
#include "SandboxVoxelGenerator.h"
#include "SandboxTerrainController.generated.h"

class ASandboxTerrainZone;
class VoxelData;

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	ASandboxTerrainController();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//===============================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool GenerateOnlySmallSpawnPoint = false;
	
	static void digTerrainRoundHole(FVector v, float radius, float s);

	static void digTerrainCubeHole(FVector origin, float r, float strength);

	static ASandboxTerrainController* instance;

	TArray<FVector> zone_queue;
	volatile int zone_queue_pos = 0;

	static FVector getZoneIndex(FVector v);

	static ASandboxTerrainZone* getZoneByVectorIndex(FVector v);

	template<class H>
	static void editTerrain(FVector v, float radius, float s, H handler);

	template<class H>
	static void performTerrainChange(FVector v, float radius, float s, H handler);

	virtual SandboxVoxelGenerator newTerrainGenerator(VoxelData &voxel_data);

private:
	static TMap<FVector, ASandboxTerrainZone*> terrain_zone_map;

	void spawnInitialZone();

	ASandboxTerrainZone* addTerrainZone(FVector pos);
};