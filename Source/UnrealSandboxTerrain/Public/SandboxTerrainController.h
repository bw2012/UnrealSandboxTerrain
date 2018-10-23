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
class UVoxelMeshComponent;
class UTerrainZoneComponent;
struct TInstMeshTransArray;
class UVdClientComponent;

typedef TMap<int32, TInstMeshTransArray> TInstMeshTypeMap;
typedef std::shared_ptr<TMeshData> TMeshDataPtr;
typedef kvdb::KvFile<TVoxelIndex, TValueData> TKvFile;

#define TH_STATE_NEW		0
#define TH_STATE_RUNNING	1
#define TH_STATE_STOP		2
#define TH_STATE_FINISHED	3

typedef struct TControllerTask {

	volatile bool bIsFinished = false;

	std::function<void()> Function;
	
	static void WaitForFinish(TControllerTask* Task) {
		while (!Task->bIsFinished) {};
	}
	
} TControllerTask;

typedef std::shared_ptr<TControllerTask> TControllerTaskTaskPtr;

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
	TVoxelDataInfo() {
		LoadVdMutexPtr = std::make_shared<std::mutex>();
	}

	~TVoxelDataInfo() {	}

	TVoxelData* Vd = nullptr;

	TVoxelDataState DataState = TVoxelDataState::UNDEFINED;

	std::shared_ptr<std::mutex> LoadVdMutexPtr;

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
	friend UTerrainGeneratorComponent;
	friend UVdClientComponent;

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
	// networking
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Network")
	uint32 ServerPort;

	//========================================================================================
	// save/load
	//========================================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void SaveMapAsync();

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 SaveGeneratedZones;

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

	void DigTerrainRoundHole(const FVector& Origin, float Radius, float Strength);

	void DigTerrainCubeHole(const FVector& Origin, float Extend);

	void FillTerrainCube(const FVector& Origin, float Extend, int MatId);

	void FillTerrainRound(const FVector& Origin, float Extend, int MatId);

	TVoxelIndex GetZoneIndex(const FVector& Pos);

	FVector GetZonePos(const TVoxelIndex& Index);

	UTerrainZoneComponent* GetZoneByVectorIndex(const TVoxelIndex& Index);

	template<class H>
	void PerformTerrainChange(H handler);

	template<class H>
	void EditTerrain(const H& ZoneHandler);

	UMaterialInterface* GetRegularTerrainMaterial(uint16 MaterialId);

	UMaterialInterface* GetTransitionTerrainMaterial(FString& TransitionName, std::set<unsigned short>& MaterialIdSet);

	//===============================================================================
	// async tasks
	//===============================================================================

	TControllerTaskTaskPtr InvokeSafe(std::function<void()> Function);

	void RunThread(std::function<void(FAsyncThread&)> Function);

	//========================================================================================
	// network
	//========================================================================================

	void NetworkSerializeVd(FBufferArchive& Buffer, const TVoxelIndex& VoxelIndex);

	void NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData);

private:

	void BeginServer();

	void BeginClient();

	void DigTerrainRoundHole_Internal(const FVector& Origin, float Radius, float Strength);

	template<class H>
	FORCEINLINE void PerformZoneEditHandler(TVoxelData* Vd, H handler, std::function<void(TMeshDataPtr)> OnComplete);


	volatile bool bIsGeneratingTerrain = false;

	volatile float GeneratingProgress;

	volatile int  GeneratedVdConter;

	//===============================================================================
	// save/load
	//===============================================================================

	void Save();

	void SaveJson();

	void LoadJson();

	bool OpenFile();
	
	TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	void SpawnInitialZone();

	void SpawnZone(const TVoxelIndex& pos);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

	FLoadInitialZonesThread* InitialZoneLoader;

	//===============================================================================
	// async tasks
	//===============================================================================

	void InvokeZoneMeshAsync(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr);

	void InvokeLazyZoneAsync(TVoxelIndex& Index, TMeshDataPtr MeshDataPtr);

	void AddAsyncTask(TControllerTaskTaskPtr TaskPtr);

	TControllerTaskTaskPtr GetAsyncTask();

	bool HasNextAsyncTask();

	std::shared_mutex AsyncTaskListMutex;

	std::queue<TControllerTaskTaskPtr> AsyncTaskList;

	std::shared_mutex ThreadListMutex;

	std::list<FAsyncThread*> ThreadList;

	//===============================================================================
	// voxel data storage
	//===============================================================================

	TKvFile VdFile;

	TKvFile MdFile;

	TKvFile ObjFile;

	std::shared_mutex VoxelDataMapMutex;

	std::unordered_map<TVoxelIndex, TVoxelDataInfo> VoxelDataIndexMap;

	void RegisterTerrainVoxelData(TVoxelDataInfo VdInfo, TVoxelIndex Index);

	TVoxelData* GetVoxelDataByPos(const FVector& Pos);

	TVoxelData* GetVoxelDataByIndex(const TVoxelIndex& Index);

	bool HasVoxelData(const TVoxelIndex& Index);

	TVoxelDataInfo* GetVoxelDataInfo(const TVoxelIndex& Index);

	void ClearVoxelData();

	TVoxelData* LoadVoxelDataByIndex(const TVoxelIndex& Index);

	std::shared_ptr<TMeshData> GenerateMesh(TVoxelData* Vd);

	//===============================================================================
	// mesh data storage
	//===============================================================================

	TMeshDataPtr LoadMeshDataByIndex(const TVoxelIndex& Index);

	void LoadObjectDataByIndex(UTerrainZoneComponent* Zone, TInstMeshTypeMap& ZoneInstMeshMap);

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