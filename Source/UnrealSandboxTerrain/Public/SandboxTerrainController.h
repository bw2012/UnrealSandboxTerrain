#pragma once

#include "EngineMinimal.h"
#include "SandboxVoxelGenerator.h"
#include <memory>
#include <queue>
#include <mutex>
#include "SandboxTerrainController.generated.h"

struct MeshData;
class VoxelData;
class FLoadInitialZonesThread;
class USandboxTerrainMeshComponent;
class UTerrainZoneComponent;
class UTerrainRegionComponent;

#define TH_STATE_NEW		0
#define TH_STATE_RUNNING	1
#define TH_STATE_STOP		2
#define TH_STATE_FINISHED	3

typedef struct TerrainControllerTask {
	std::function<void()> f;
} TerrainControllerTask;

UENUM(BlueprintType)
enum class ETerrainInitialArea : uint8 {
	TIA_1_1 = 0	UMETA(DisplayName = "1x1"),
	TIA_3_3 = 1	UMETA(DisplayName = "3x3"),
};

UENUM(BlueprintType)	
enum class EVoxelDimEnum : uint8 {
	VS_8  = 9	UMETA(DisplayName = "8"),
	VS_16 = 17	UMETA(DisplayName = "16"),
	VS_32 = 33	UMETA(DisplayName = "32"),
	VS_64 = 65 	UMETA(DisplayName = "64"),
};

USTRUCT()
struct FTerrainInstancedMeshType {
	GENERATED_BODY()

	UPROPERTY()
	int32 MeshTypeId;

	UPROPERTY()
	UStaticMesh* Mesh;
};

USTRUCT()
struct FSandboxFoliage {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere)
	int32 SpawnStep = 25;

	UPROPERTY(EditAnywhere)
	int32 StartCullDistance = 100;

	UPROPERTY(EditAnywhere)
	int32 EndCullDistance = 500;

	UPROPERTY(EditAnywhere)
	float OffsetRange = 10.0f;

	UPROPERTY(EditAnywhere)
	float ScaleMinZ = 0.5f;

	UPROPERTY(EditAnywhere)
	float ScaleMaxZ = 1.0f;
};

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor {
	GENERATED_UCLASS_BODY()

public:
	ASandboxTerrainController();

	friend FLoadInitialZonesThread;
	friend UTerrainZoneComponent;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//===============================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bGenerateOnlySmallSpawnPoint = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bShowZoneBounds = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bDisableFoliage = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	ETerrainInitialArea TerrainInitialArea = ETerrainInitialArea::TIA_3_3;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	FString MapName;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	UMaterialInterface* TerrainMaterial;

	//UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	EVoxelDimEnum ZoneGridDimension;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 TerrainSize;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	bool bEnableLOD;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	TMap<uint32, FSandboxFoliage> FoliageMap;

	FString getZoneFileName(int tx, int ty, int tz);
		
	void digTerrainRoundHole(FVector v, float radius, float s);

	void digTerrainCubeHole(FVector origin, float r, float strength);

	FVector getZoneIndex(FVector v);

	UTerrainZoneComponent* getZoneByVectorIndex(FVector v);

	FVector GetRegionIndex(FVector v);

	UTerrainRegionComponent* GetRegionByVectorIndex(FVector v);

	template<class H>
	void editTerrain(FVector v, float radius, float s, H handler);

	template<class H>
	void performTerrainChange(FVector v, float radius, float s, H handler);

	virtual SandboxVoxelGenerator newTerrainGenerator(VoxelData &voxel_data);

private:
	TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	TMap<FVector, UTerrainRegionComponent*> TerrainRegionMap;

	void spawnInitialZone();

	UTerrainZoneComponent* addTerrainZone(FVector pos);

	VoxelData* createZoneVoxeldata(FVector location);

	void generateTerrain(VoxelData &voxel_data);

	FLoadInitialZonesThread* initial_zone_loader;

	void invokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<MeshData> mesh_data_ptr);

	void invokeLazyZoneAsync(FVector index);

	void AddAsyncTask(TerrainControllerTask zone_make_task);

	TerrainControllerTask GetAsyncTask();

	bool HasNextAsyncTask();

	std::mutex AsyncTaskListMutex;

	std::queue<TerrainControllerTask> AsyncTaskList;

	std::mutex VoxelDataMapMutex;

	TMap<FVector, VoxelData*> VoxelDataMap;

	void RegisterTerrainVoxelData(VoxelData* vd, FVector index);

	VoxelData* GetTerrainVoxelDataByPos(FVector point);

	VoxelData* GetTerrainVoxelDataByIndex(FVector index);

	//===============================================================================
	// foliage
	//===============================================================================

	void GenerateNewFoliage(UTerrainZoneComponent* Zone);

	void LoadFoliage(UTerrainZoneComponent* Zone);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone);

protected:

	virtual void OnLoadZoneProgress(int progress, int total);

	virtual void OnLoadZoneListFinished();
		
	virtual void OnGenerateNewZone(UTerrainZoneComponent* Zone);

	virtual void OnLoadZone(UTerrainZoneComponent* Zone);
};