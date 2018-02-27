#pragma once

#include "Engine.h"
#include "TerrainGeneratorComponent.h"
#include <memory>
#include <queue>
#include <mutex>
#include <set>
#include <list>
#include <unordered_map>
#include "VoxelIndex.h"
#include "kvdb.h"
#include "SandboxTerrainController.generated.h"

struct TMeshData;
class FLoadInitialZonesThread;
class FAsyncThread;
class USandboxTerrainMeshComponent;
class UTerrainZoneComponent;
class UTerrainRegionComponent;

typedef std::shared_ptr<TMeshData> TMeshDataPtr;

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

class TVoxelDataInfo {

public:
	//TODO replace with share pointer
	TVoxelData* Vd = nullptr;

	TVoxelDataState DataState = TVoxelDataState::UNDEFINED;

	// mesh is generated
	bool IsNewGenerated() const {
		return DataState == TVoxelDataState::GENERATED;
	}

	bool IsNewLoaded() const {
		return DataState == TVoxelDataState::LOADED;
	}

	void Unload();
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
	float Probability = 1;

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

	UPROPERTY(EditAnywhere)
	UTerrainGeneratorComponent* TerrainGeneratorComponent;

	//========================================================================================
	// 
	//========================================================================================

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Start Build Sandbox Terrain"))
	void OnStartBuildTerrain();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Finish Build Sandbox Terrain"))
	void OnFinishBuildTerrain();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Progress Build Sandbox Terrain"))
	void OnProgressBuildTerrain(float Progress);

	//========================================================================================
	// save/load
	//========================================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void SaveMapAsync();

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
	
	//static bool CheckZoneBounds(FVector Origin, float Size);

	//========================================================================================

	float GetRealGroungLevel(float X, float Y);

	void DigTerrainRoundHole(FVector v, float radius, float s);

	void DigTerrainCubeHole(FVector origin, float r, float strength);

	void FillTerrainCube(FVector origin, const float r, const int matId);

	void FillTerrainRound(const FVector origin, const float r, const int matId);

	TVoxelIndex GetZoneIndex(const FVector& Pos);

	FVector GetZonePos(const TVoxelIndex& Index);

	UTerrainZoneComponent* GetZoneByVectorIndex(const TVoxelIndex& Index);

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

	//===============================================================================
	// save/load
	//===============================================================================

	void Save();

	void SaveJson(const TSet<FVector>& RegionPosSet);

	void LoadJson(TSet<FVector>& RegionIndexSet);

	bool OpenFile();
	
	TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	TMap<FVector, UTerrainRegionComponent*> TerrainRegionMap;

	TSet<FVector> RegionIndexSet;

	TSet<FVector> SpawnInitialZone();

	void SpawnZone(const FVector& pos);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

	UTerrainRegionComponent* GetOrCreateRegion(FVector pos);

	TVoxelDataInfo* FindOrCreateZoneVoxeldata(const TVoxelIndex& Index);

	FLoadInitialZonesThread* InitialZoneLoader;

	//===============================================================================
	// async tasks
	//===============================================================================

	void InvokeZoneMeshAsync(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr);

	void InvokeLazyZoneAsync(FVector index);

	void AddAsyncTask(TerrainControllerTask zone_make_task);

	TerrainControllerTask GetAsyncTask();

	bool HasNextAsyncTask();

	std::mutex AsyncTaskListMutex;

	std::queue<TerrainControllerTask> AsyncTaskList;

	std::mutex ThreadListMutex;

	std::list<FAsyncThread*> ThreadList;

	void RunThread(std::function<void(FAsyncThread&)> Function);

	//===============================================================================
	// voxel data storage
	//===============================================================================

	kvdb::KvFile<TVoxelIndex, TValueData> VdFile;

	kvdb::KvFile<TVoxelIndex, TValueData> MdFile;

	std::mutex VoxelDataMapMutex;

	std::unordered_map<TVoxelIndex, TVoxelDataInfo> VoxelDataIndexMap;

	void RegisterTerrainVoxelData(TVoxelDataInfo VdInfo, TVoxelIndex Index);

	TVoxelData* GetVoxelDataByPos(const FVector& Pos);

	TVoxelData* GetVoxelDataByIndex(const TVoxelIndex& Index);

	bool HasVoxelData(const TVoxelIndex& Index) const;

	TVoxelDataInfo* GetVoxelDataInfo(const TVoxelIndex& Index);

	void ClearVoxelData();

	TVoxelData* LoadVoxelDataByIndex(const TVoxelIndex& Index);

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
	
	virtual void OnGenerateNewZone(UTerrainZoneComponent* Zone);

	virtual void OnLoadZone(UTerrainZoneComponent* Zone);
};