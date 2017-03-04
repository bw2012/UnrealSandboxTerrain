#pragma once

#include "Engine.h"
#include "SandboxVoxelGenerator.h"
#include <memory>
#include <queue>
#include <mutex>
#include "SandboxTerrainController.generated.h"

struct TMeshData;
class TVoxelData;
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


USTRUCT()
struct FSandboxTerrainMaterial {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	UTexture* TextureTopMicro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureSideMicro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureMacro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureNormal;
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

	virtual void PostLoad() override;

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

	//========================================================================================
	// materials
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	UMaterialInterface* RegularMaterial;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	UMaterialInterface* TransitionMaterial;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	TMap<uint16, FSandboxTerrainMaterial> MaterialMap;

	//========================================================================================

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

	void fillTerrainRound(const FVector origin, const float r, const float strength, const int matId);

	FVector getZoneIndex(FVector v);

	UTerrainZoneComponent* getZoneByVectorIndex(FVector v);

	FVector GetRegionIndex(FVector v);

	UTerrainRegionComponent* GetRegionByVectorIndex(FVector v);

	template<class H>
	void editTerrain(FVector v, float radius, float s, H handler);

	template<class H>
	void performTerrainChange(FVector v, float radius, float s, H handler);

	virtual SandboxVoxelGenerator newTerrainGenerator(TVoxelData &voxel_data);

	UMaterialInterface* GetRegularTerrainMaterial(uint16 MaterialId);

	//UMaterialInterface* GetTransitionTerrainMaterial(TArray MaterialIdArray);

private:
	TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	TMap<FVector, UTerrainRegionComponent*> TerrainRegionMap;

	TSet<FVector> spawnInitialZone();

	UTerrainZoneComponent* addTerrainZone(FVector pos);

	TVoxelData* createZoneVoxeldata(FVector location);

	void generateTerrain(TVoxelData &voxel_data);

	FLoadInitialZonesThread* initial_zone_loader;

	void invokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<TMeshData> mesh_data_ptr);

	void invokeLazyZoneAsync(FVector index);

	void AddAsyncTask(TerrainControllerTask zone_make_task);

	TerrainControllerTask GetAsyncTask();

	bool HasNextAsyncTask();

	std::mutex AsyncTaskListMutex;

	std::queue<TerrainControllerTask> AsyncTaskList;

	std::mutex VoxelDataMapMutex;

	TMap<FVector, TVoxelData*> VoxelDataMap;

	void RegisterTerrainVoxelData(TVoxelData* vd, FVector index);

	TVoxelData* GetTerrainVoxelDataByPos(FVector point);

	TVoxelData* GetTerrainVoxelDataByIndex(FVector index);

	//===============================================================================
	// foliage
	//===============================================================================

	void GenerateNewFoliage(UTerrainZoneComponent* Zone);

	void LoadFoliage(UTerrainZoneComponent* Zone);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone);

	TMap<FString, UMaterialInterface*> TransitionMaterialCache;

	TMap<uint16, UMaterialInterface*> RegularMaterialCache;

protected:

	virtual void OnLoadZoneProgress(int progress, int total);

	virtual void OnLoadZoneListFinished();
		
	virtual void OnGenerateNewZone(UTerrainZoneComponent* Zone);

	virtual void OnLoadZone(UTerrainZoneComponent* Zone);
};