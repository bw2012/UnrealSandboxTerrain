#pragma once

#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#include "SandboxTerrainCommon.h"
#include "TerrainGeneratorComponent.h"
#include <memory>
#include <queue>
#include <mutex>
#include <list>
#include <shared_mutex>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include "VoxelIndex.h"
#include "kvdb.hpp"
#include "VoxelData.h"
#include "SandboxTerrainController.generated.h"

#define USBT_MAX_MATERIAL_HARDNESS 99999.9f

struct TMeshData;
class UVoxelMeshComponent;
class UTerrainZoneComponent;
struct TInstanceMeshArray;
class TTerrainData;
class TCheckAreaMap;

class TVoxelDataInfo;
class TTerrainAreaPipeline;
class TTerrainLoadPipeline;

typedef TMap<uint64, TInstanceMeshArray> TInstanceMeshTypeMap;
typedef std::shared_ptr<TMeshData> TMeshDataPtr;
typedef kvdb::KvFile<TVoxelIndex, TValueData> TKvFile;


UENUM(BlueprintType)
enum class ETerrainInitialArea : uint8 {
	TIA_1_1 = 0	UMETA(DisplayName = "1x1"),
	TIA_3_3 = 1	UMETA(DisplayName = "3x3"),
};

USTRUCT()
struct FMapInfo {
	GENERATED_BODY()

	UPROPERTY()
	uint32 FormatVersion = 0;

	UPROPERTY()
	uint32 FormatSubversion = 0;

	UPROPERTY()
	double SaveTimestamp;

	UPROPERTY()
	FString Status;
};

USTRUCT()
struct FTerrainInstancedMeshType {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	uint32 MeshTypeId = 0;

	UPROPERTY(EditAnywhere)
	uint32 MeshVariantId = 0;

	UPROPERTY(EditAnywhere)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere)
	int32 StartCullDistance = 4000;

	UPROPERTY(EditAnywhere)
	int32 EndCullDistance = 8000;

	UPROPERTY(EditAnywhere)
	uint32 SandboxClassId = 0;

	static uint64 ClcMeshTypeCode(uint32 MeshTypeId, uint32 MeshVariantId) {
		union {
			uint32 A[2];
			uint64 B;
		};

		A[0] = MeshTypeId;
		A[1] = MeshVariantId;

		return B;
	}

	uint64 GetMeshTypeCode() const {
		return FTerrainInstancedMeshType::ClcMeshTypeCode(MeshTypeId, MeshVariantId);
	}
};


typedef struct TInstanceMeshArray {

	TArray<FTransform> TransformArray;

	FTerrainInstancedMeshType MeshType;

} TInstanceMeshArray;


typedef TMap<uint64, TInstanceMeshArray> TInstanceMeshTypeMap;


UCLASS(BlueprintType, Blueprintable)
class UNREALSANDBOXTERRAIN_API USandboxTarrainFoliageMap : public UDataAsset {
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	TMap<uint32, FSandboxFoliage> FoliageMap;
};


UENUM(BlueprintType)
enum class FSandboxTerrainMaterialType : uint8 {
	Soil = 0	UMETA(DisplayName = "Soil"),
	Rock = 1	UMETA(DisplayName = "Rock"),
};

USTRUCT()
struct FSandboxTerrainMaterial {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Name;

	UPROPERTY(EditAnywhere)
	FSandboxTerrainMaterialType Type;

	UPROPERTY(EditAnywhere)
	float RockHardness;

	UPROPERTY(EditAnywhere)
	UTexture* TextureSideMicro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureSideMacro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureSideNormal;

	UPROPERTY(EditAnywhere)
	UTexture* TextureTopMicro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureTopMacro;

	UPROPERTY(EditAnywhere)
	UTexture* TextureTopNormal;

};

UCLASS(Blueprintable)
class UNREALSANDBOXTERRAIN_API USandboxTerrainParameters : public UDataAsset {
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	TMap<uint16, FSandboxTerrainMaterial> MaterialMap;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Generator")
    TArray<FTerrainUndergroundLayer> UndergroundLayers;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Inst. Meshes")
	TArray<FTerrainInstancedMeshType> InstanceMeshes;

};

extern float GlobalTerrainZoneLOD[LOD_ARRAY_SIZE];

USTRUCT()
struct FSandboxTerrainLODDistance {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    float Distance1 = 1500;
    
    UPROPERTY(EditAnywhere)
    float Distance2 = 3000;
    
    UPROPERTY(EditAnywhere)
    float Distance3 = 6000;
    
    UPROPERTY(EditAnywhere)
    float Distance4 = 12000;
    
    UPROPERTY(EditAnywhere)
    float Distance5 = 16000;
    
    UPROPERTY(EditAnywhere)
    float Distance6 = 20000;
};

UENUM(BlueprintType)
enum class ETerrainLodMaskPreset : uint8 {
    All      = 0            UMETA(DisplayName = "Show all"),
    Medium   = 0b00000011   UMETA(DisplayName = "Show medium"),
    Far      = 0b00011111   UMETA(DisplayName = "Show far"),
};

USTRUCT()
struct FTerrainSwapAreaParams {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    float Radius = 3000;
    
    UPROPERTY(EditAnywhere)
    float FullLodDistance = 1000;
    
    UPROPERTY(EditAnywhere)
    int TerrainSizeMinZ = -5;
    
    UPROPERTY(EditAnywhere)
    int TerrainSizeMaxZ = 5;
};

typedef struct TChunkIndex {
	int X, Y;

	TChunkIndex(int x, int y) : X(x), Y(y) { };

} TChunkIndex;

enum class TZoneFlag : int {
	Generated = 0,
	NoMesh = 1,
	NoVoxelData = 2,
};

typedef struct TKvFileZodeData {
	uint32 Flags = 0x0;
	uint32 LenMd = 0;
	uint32 LenVd = 0;
	uint32 LenObj = 0;

	bool Is(TZoneFlag Flag) {
		return (Flags >> (int)Flag) & 1U;
	};

	void SetFlag(int Flag) {
		Flags |= 1UL << Flag;
	};

} TKvFileZodeData;

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor {
	GENERATED_UCLASS_BODY()

public:
    ASandboxTerrainController();
    
    friend UTerrainZoneComponent;
	friend TTerrainAreaPipeline;
	friend TTerrainLoadPipeline;
	friend UTerrainGeneratorComponent;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PostLoad() override;
    
    virtual void BeginDestroy() override;

	virtual void FinishDestroy() override;
    
	//========================================================================================
	// debug only
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bGenerateOnlySmallSpawnPoint = false;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
    ETerrainInitialArea TerrainInitialArea = ETerrainInitialArea::TIA_3_3;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bShowZoneBounds = false;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
    bool bShowInitialArea = false;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
    bool bShowStartSwapPos = false;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Debug")
	bool bShowApplyZone = false;
    
    //========================================================================================
    // general
    //========================================================================================

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    FString MapName;

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    int32 Seed;

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    FTerrainSwapAreaParams InitialLoadArea;
           
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    FSandboxTerrainLODDistance LodDistance;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	bool bSaveOnEndPlay;
    
    //========================================================================================
    // Dynamic area swapping
    //========================================================================================
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    bool bEnableAreaSwapping;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    float PlayerLocationThreshold = 1000;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    FTerrainSwapAreaParams DynamicLoadArea;

	//========================================================================================
	// save/load
	//========================================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void SaveMapAsync();

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	bool bSaveAfterInitialLoad;

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
    int32 AutoSavePeriod;
    
	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain")
	int32 SaveGeneratedZones; // TODO refactor

	//========================================================================================
	// materials
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	UMaterialInterface* RegularMaterial;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	UMaterialInterface* TransitionMaterial;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Material")
	USandboxTerrainParameters* TerrainParameters;

	//========================================================================================
	// collision
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Collision")
	unsigned int CollisionSection;

	void OnFinishAsyncPhysicsCook(const TVoxelIndex& ZoneIndex);

	//========================================================================================
	// foliage
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	USandboxTarrainFoliageMap* FoliageDataAsset;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	int MaxConveyorTasks = 5;
   
    //========================================================================================
    // networking
    //========================================================================================

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Network")
    uint32 ServerPort;

	//========================================================================================

	UPROPERTY()
	UTerrainGeneratorComponent* GeneratorComponent;

	UTerrainGeneratorComponent* GetTerrainGenerator();

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void ForcePerformHardUnload();

	//========================================================================================

	uint32 GetZoneVoxelResolution();

	float GetZoneSize();

	void DigTerrainRoundHole(const FVector& Origin, float Radius, float Strength);

	void DigCylinder(const FVector& Origin, const float Radius, const float Length, const FRotator& Rotator = FRotator(0), const float Strength = 1.f, const bool bNoise = true);

	void DigTerrainCubeHole(const FVector& Origin, const FBox& Box, float Extend, const FRotator& Rotator = FRotator(0));

	void DigTerrainCubeHole(const FVector& Origin, float Extend, const FRotator& Rotator = FRotator(0));

	void FillTerrainCube(const FVector& Origin, float Extend, int MatId);

	void FillTerrainRound(const FVector& Origin, float Extend, int MatId);

	TVoxelIndex GetZoneIndex(const FVector& Pos);

	FVector GetZonePos(const TVoxelIndex& Index);

	UTerrainZoneComponent* GetZoneByVectorIndex(const TVoxelIndex& Index);

	template<class H>
	void PerformTerrainChange(H handler);

	template<class H>
	void EditTerrain(const H& ZoneHandler);

	bool GetTerrainMaterialInfoById(uint16 MaterialId, FSandboxTerrainMaterial& MaterialInfo);

	UMaterialInterface* GetRegularTerrainMaterial(uint16 MaterialId);

	UMaterialInterface* GetTransitionTerrainMaterial(const std::set<unsigned short>& MaterialIdSet);

	const FTerrainInstancedMeshType* GetInstancedMeshType(uint32 MeshTypeId, uint32 MeshVariantId = 0) const;

	//===============================================================================
	// async tasks
	//===============================================================================

	void RunThread(TUniqueFunction<void()> Function);

	//========================================================================================
	// network
	//========================================================================================

	void NetworkSerializeVd(FBufferArchive& Buffer, const TVoxelIndex& VoxelIndex);

	void NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData);

	float ClcGroundLevel(const FVector& V);

private:

	bool bForcePerformHardUnload = false;
    
	std::unordered_set<TVoxelIndex> InitialLoadSet;

	void StartPostLoadTimers();

    TCheckAreaMap* CheckAreaMap;

    FTimerHandle TimerSwapArea;
    
    void PerformCheckArea();
    
    void StartCheckArea();
    
	void BeginClient();

	template<class H>
	FORCEINLINE void PerformZoneEditHandler(std::shared_ptr<TVoxelDataInfo> VdInfoPtr, H Handler, std::function<void(TMeshDataPtr)> OnComplete);

	volatile bool bIsWorkFinished = false;

	//===============================================================================
	// save/load
	//===============================================================================
    
    FTimerHandle TimerAutoSave;
    
    std::mutex SaveMutex;

	bool bIsLoadFinished;
        
    void AutoSaveByTimer();

	void SaveJson();
	
	void SpawnInitialZone();

	TValueDataPtr SerializeVd(TVoxelData* Vd);

	//===============================================================================
	// async tasks
	//===============================================================================

	std::mutex ConveyorMutex;

	std::list<std::function<void()>> ConveyorList;

	void AddTaskToConveyor(std::function<void()> Function);

	void ExecGameThreadZoneApplyMesh(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0x0);

	void ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0x0, const bool bIsNewGenerated = false);

	//void ExecGameThreadRestoreSoftUnload(const TVoxelIndex& ZoneIndex);

	//===============================================================================
	// threads
	//===============================================================================

	std::shared_timed_mutex ThreadListMutex;

	FGraphEventArray TerrainControllerEventList;

	//===============================================================================
	// voxel data storage
	//===============================================================================
        
    TTerrainData* TerrainData;

	TKvFile TdFile;

	std::shared_ptr<TVoxelDataInfo> GetVoxelDataInfo(const TVoxelIndex& Index);

	TVoxelData* LoadVoxelDataByIndex(const TVoxelIndex& Index);

	//===============================================================================
	// mesh data storage
	//===============================================================================

	TMeshDataPtr LoadMeshDataByIndex(const TVoxelIndex& Index);

	void LoadObjectDataByIndex(UTerrainZoneComponent* Zone, TInstanceMeshTypeMap& ZoneInstMeshMap);


	//===============================================================================
	// inst. meshes
	//===============================================================================

	TMap<uint64, FTerrainInstancedMeshType> InstMeshMap;

	//===============================================================================
	// foliage
	//===============================================================================

	TMap<uint32, FSandboxFoliage> FoliageMap;

	void LoadFoliage(UTerrainZoneComponent* Zone);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone);

	//===============================================================================
	// materials
	//===============================================================================

	UPROPERTY()
	TMap<uint64, UMaterialInterface*> TransitionMaterialCache;

	UPROPERTY()
	TMap<uint16, UMaterialInterface*> RegularMaterialCache;

	TMap<uint16, FSandboxTerrainMaterial> MaterialMap;

	//===============================================================================
	// collision
	//===============================================================================

	int GetCollisionMeshSectionLodIndex();

	//===============================================================================
	// 
	//===============================================================================

    void OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);

    void OnLoadZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);
    
	//===============================================================================
	// pipeline
	//===============================================================================

	void SpawnZone(const TVoxelIndex& Index, const TTerrainLodMask TerrainLodMask);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

	void UnloadFarZones(FVector PlayerLocation, float Radius);

protected:

	FMapInfo MapInfo;

	virtual void BeginTerrainLoad();

	FVector BeginTerrainLoadLocation;

	virtual void InitializeTerrainController();

	virtual void BeginPlayServer();

	bool IsWorkFinished();

	void AddInitialZone(const TVoxelIndex& ZoneIndex);

	//===============================================================================
	// save/load
	//===============================================================================

	bool LoadJson();

	bool OpenFile();

	void CloseFile();

	void ForceSave(const TVoxelIndex& ZoneIndex, TVoxelData* Vd, TMeshDataPtr MeshDataPtr, const TInstanceMeshTypeMap& InstanceObjectMap);

	void Save(std::function<void(uint32, uint32)> OnProgress = nullptr, std::function<void(uint32)> OnFinish = nullptr);

	void MarkZoneNeedsToSave(TVoxelIndex ZoneIndex);

	void ZoneHardUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex);

	void ZoneSoftUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex);

	virtual bool OnZoneSoftUnload(const TVoxelIndex& ZoneIndex);

	virtual void OnRestoreZoneSoftUnload(const TVoxelIndex& ZoneIndex);

	virtual bool OnZoneHardUnload(const TVoxelIndex& ZoneIndex);

	//===============================================================================
	// voxel data storage
	//===============================================================================

	bool IsVdExistsInFile(const TVoxelIndex& ZoneIndex);

	std::shared_ptr<TMeshData> GenerateMesh(TVoxelData* Vd);

	//===============================================================================
	// perlin noise
	//===============================================================================

	float PerlinNoise(const FVector& Pos) const;

	float NormalizedPerlinNoise(const FVector& Pos) const;

	//===============================================================================
	// NewVoxelData
	//===============================================================================
      
	TVoxelData* NewVoxelData();

    //===============================================================================
    // virtual functions
    //===============================================================================

	virtual UTerrainGeneratorComponent* NewTerrainGenerator();
    
	virtual void OnOverlapActorDuringTerrainEdit(const FHitResult& OverlapResult, const FVector& Pos);

	virtual void OnFinishGenerateNewZone(const TVoxelIndex& Index);

	virtual void OnFinishLoadZone(const TVoxelIndex& Index);

	virtual void OnStartBackgroundSaveTerrain();

	virtual void OnFinishBackgroundSaveTerrain();

	virtual void OnProgressBackgroundSaveTerrain(float Progress);

	//===============================================================================
	// pipeline
	//===============================================================================

	//int GeneratePipeline(const TVoxelIndex& Index);

	void BatchSpawnZone(const TArray<TSpawnZoneParam>& SpawnZoneParamArray);

	void BatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList);

	std::list<TChunkIndex> MakeChunkListByAreaSize(const uint32 AreaRadius);

	//===============================================================================
	// core
	//===============================================================================

	void ShutdownThreads();

	//===============================================================================
	// foliage
	//===============================================================================

	const FSandboxFoliage& GetFoliageById(uint32 FoliageId) const;
};
