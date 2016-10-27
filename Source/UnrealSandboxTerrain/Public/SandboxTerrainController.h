#pragma once

#include "EngineMinimal.h"
#include "SandboxVoxelGenerator.h"
#include "SandboxTerrainController.generated.h"

class ASandboxTerrainZone;
class VoxelData;
struct MeshData;
class FLoadInitialZonesThread;

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	ASandboxTerrainController();

	friend FLoadInitialZonesThread;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//===============================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool GenerateOnlySmallSpawnPoint = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool ShowZoneBounds = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	int InitialSpawnSize = 1;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	FString MapName;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	UMaterialInterface* TerrainMaterial;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 ZoneGridSize;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 TerrainSize;

	FString getZoneFileName(int tx, int ty, int tz);
		
	void digTerrainRoundHole(FVector v, float radius, float s);

	void digTerrainCubeHole(FVector origin, float r, float strength);

	static ASandboxTerrainController* GetZoneInstance(AActor* zone);

	FVector getZoneIndex(FVector v);

	ASandboxTerrainZone* getZoneByVectorIndex(FVector v);

	template<class H>
	void editTerrain(FVector v, float radius, float s, H handler);

	template<class H>
	void performTerrainChange(FVector v, float radius, float s, H handler);

	virtual SandboxVoxelGenerator newTerrainGenerator(VoxelData &voxel_data);

private:
	TMap<FVector, ASandboxTerrainZone*> terrain_zone_map;

	void spawnInitialZone();

	ASandboxTerrainZone* addTerrainZone(FVector pos);

	VoxelData* createZoneVoxeldata(FVector location);

	void generateTerrain(VoxelData &voxel_data);

	FLoadInitialZonesThread* initial_zone_loader;

	void invokeZoneMeshAsync(ASandboxTerrainZone* zone, MeshData* md);

	void invokeLazyZoneAsync(FVector index);

protected:

	int getVoxeldataSize() { return 65; }

	float getVoxelDataVolume() { return 1000; }

	virtual void OnLoadZoneProgress(int progress, int total);

	virtual void OnLoadZoneListFinished();
		
};