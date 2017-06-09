#pragma once

#include "Engine.h"
#include "SandboxVoxelGenerator.h"
#include "TerrainGeneratorComponent.h"
#include <memory>
#include <queue>
#include <mutex>
#include <set>
#include <list>
#include "SandboxTerrainController.generated.h"

struct TMeshData;
class TVoxelData;
class FLoadInitialZonesThread;
class FAsyncThread;
class USandboxTerrainMeshComponent;
class UTerrainZoneComponent;
class UTerrainRegionComponent;

#define TH_STATE_NEW		0
#define TH_STATE_RUNNING	1
#define TH_STATE_STOP		2
#define TH_STATE_FINISHED	3

typedef struct TerrainControllerTask {
	std::function<void()> Function;
} TerrainControllerTask;

UENUM(BlueprintType)
enum class ETerrainInitialArea : uint8 {
	TIA_1_1 = 0	UMETA(DisplayName = "1x1"),
	TIA_3_3 = 1	UMETA(DisplayName = "3x3"),
};

enum TVoxelDataState {
	UNDEFINED, 
	GENERATED, 
	LOADED,
	READY_TO_LOAD
};

struct TVoxelDataInfo {
	TVoxelData* Vd = nullptr;

	TVoxelDataState DataState = TVoxelDataState::UNDEFINED;

	// mesh is generated
	bool isNewGenerated() const {
		return DataState == TVoxelDataState::GENERATED;
	}

	bool isNewLoaded() const {
		return DataState == TVoxelDataState::LOADED;
	}

};

USTRUCT()
struct FTerrainInstancedMeshType {
	GENERATED_BODY()

	UPROPERTY()
	int32 MeshTypeId;

	UPROPERTY()
	UStaticMesh* Mesh;

	UPROPERTY()
	int32 StartCullDistance;

	UPROPERTY()
	int32 EndCullDistance;
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
	FString Name;

	UPROPERTY(EditAnywhere)
	float RockHardness;

	UPROPERTY(EditAnywhere)
	UTexture* TextureTopMicro;

	//UPROPERTY(EditAnywhere)
	//UTexture* TextureSideMicro;

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
	friend FAsyncThread;
	friend UTerrainZoneComponent;
	friend UTerrainRegionComponent;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PostLoad() override;

	//========================================================================================
	// debug only
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bGenerateOnlySmallSpawnPoint = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bShowZoneBounds = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bDisableFoliage = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	ETerrainInitialArea TerrainInitialArea = ETerrainInitialArea::TIA_3_3;

	//========================================================================================
	// debug only
	//========================================================================================

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Start Build Sandbox Terrain"))
	void OnStartBuildTerrain();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Finish Build Sandbox Terrain"))
	void OnFinishBuildTerrain();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Progress Build Sandbox Terrain"))
	void OnProgressBuildTerrain(float Progress);

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
	// general
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	FString MapName;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 TerrainSizeX;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 TerrainSizeY;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 TerrainSizeZ;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	bool bEnableLOD;

	//========================================================================================
	// collision
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Collision")
	unsigned int CollisionSection;

	//========================================================================================
	// foliage
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	TMap<uint32, FSandboxFoliage> FoliageMap;

	//========================================================================================
	
	void DigTerrainRoundHole(FVector v, float radius, float s);

	void DigTerrainCubeHole(FVector origin, float r, float strength);

	void FillTerrainCube(FVector origin, const float r, const int matId);

	void FillTerrainRound(const FVector origin, const float r, const int matId);

	FVector GetZoneIndex(FVector v);

	FVector GetZonePos(FVector Index);

	UTerrainZoneComponent* GetZoneByVectorIndex(FVector v);

	FVector GetRegionIndex(FVector v);

	FVector GetRegionPos(FVector Index);

	UTerrainRegionComponent* GetRegionByVectorIndex(FVector v);

	template<class H>
	void EditTerrain(FVector v, float radius, H handler);

	template<class H>
	void PerformTerrainChange(FVector v, float radius, H handler);

	UMaterialInterface* GetRegularTerrainMaterial(uint16 MaterialId);

	UMaterialInterface* GetTransitionTerrainMaterial(FString& TransitionName, std::set<unsigned short>& MaterialIdSet);

	void InvokeSafe(std::function<void()> Function);

private:
	volatile bool bIsGeneratingTerrain = false;

	volatile float GeneratingProgress;

	void Save();

	void SaveJson(const TSet<FVector>& RegionPosSet);

	void LoadJson(TSet<FVector>& RegionIndexSet);
	
	TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	TMap<FVector, UTerrainRegionComponent*> TerrainRegionMap;

	TSet<FVector> RegionIndexSet;

	TSet<FVector> SpawnInitialZone();

	void SpawnZone(const FVector& pos);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

	UTerrainRegionComponent* GetOrCreateRegion(FVector pos);

	TVoxelDataInfo FindOrCreateZoneVoxeldata(FVector location);

	FLoadInitialZonesThread* InitialZoneLoader;

	void InvokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<TMeshData> mesh_data_ptr);

	void InvokeLazyZoneAsync(FVector index);

	void AddAsyncTask(TerrainControllerTask zone_make_task);

	TerrainControllerTask GetAsyncTask();

	bool HasNextAsyncTask();

	std::mutex AsyncTaskListMutex;

	std::queue<TerrainControllerTask> AsyncTaskList;

	std::mutex VoxelDataMapMutex;

	TMap<FVector, TVoxelDataInfo> VoxelDataMap;

	void RegisterTerrainVoxelData(TVoxelDataInfo VdInfo, FVector Index);

	TVoxelData* GetTerrainVoxelDataByPos(FVector point);

	TVoxelData* GetTerrainVoxelDataByIndex(FVector index);

	std::mutex ThreadListMutex;

	std::list<FAsyncThread*> ThreadList;

	void RunThread(std::function<void(FAsyncThread&)> Function);

	//===============================================================================
	// foliage
	//===============================================================================

	void GenerateNewFoliage(UTerrainZoneComponent* Zone);

	void LoadFoliage(UTerrainZoneComponent* Zone);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone);

	//===============================================================================
	// materials
	//===============================================================================

	UPROPERTY()
	TMap<FString, UMaterialInterface*> TransitionMaterialCache;

	UPROPERTY()
	TMap<uint16, UMaterialInterface*> RegularMaterialCache;

	//===============================================================================
	// collision
	//===============================================================================

	int GetCollisionMeshSectionLodIndex() {
		if (bEnableLOD) {
			if (CollisionSection > 6) return 6;
			return CollisionSection;
		}
		return 0;
	}

protected:

	UPROPERTY()
	UTerrainGeneratorComponent* TerrainGeneratorComponent;
	
	virtual void OnGenerateNewZone(UTerrainZoneComponent* Zone);

	virtual void OnLoadZone(UTerrainZoneComponent* Zone);
};