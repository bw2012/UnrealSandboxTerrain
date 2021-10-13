#pragma once

#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#include "SandboxTerrainCommon.h"
#include "SandboxTerrainGenerator.h"
#include <memory>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <set>
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
class UVdClientComponent;
class TTerrainData;
class TCheckAreaMap;

//TODO refactor
class TBaseTerrainGenerator;
class TDefaultTerrainGenerator;

class TVoxelDataInfo;
class TTerrainAreaPipeline;
class TTerrainLoadPipeline;

typedef TMap<int32, TInstanceMeshArray> TInstanceMeshTypeMap;
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

	UPROPERTY()
	int32 MeshTypeId;

	UPROPERTY()
	UStaticMesh* Mesh;

	UPROPERTY()
	int32 StartCullDistance;

	UPROPERTY()
	int32 EndCullDistance;
};

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
    int TerrainSizeMinZ = 5;
    
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

	//TODO refactor
	friend TDefaultTerrainGenerator;

	friend UVdClientComponent;
	friend TTerrainAreaPipeline;
	friend TTerrainLoadPipeline;

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
	// 
	//========================================================================================

	//UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Start Build Sandbox Terrain"))
	//void OnStartBuildTerrain();

	//UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Finish Build Sandbox Terrain"))
	//void OnFinishBuildTerrain();

	//UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Progress Build Sandbox Terrain"))
	//void OnProgressBuildTerrain(float Progress);

	//========================================================================================
	// save/load
	//========================================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void SaveMapAsync();

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
    
    //========================================================================================
    // networking
    //========================================================================================

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Network")
    uint32 ServerPort;

	//========================================================================================

	TBaseTerrainGenerator* GetTerrainGenerator();

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
    
	//void GenerateNewZoneVd(std::shared_ptr<TVoxelDataInfo> VdInfoPtr, const TVoxelIndex& Index);

	void StartPostLoadTimers();

    TCheckAreaMap* CheckAreaMap;

    FTimerHandle TimerSwapArea;
    
    void PerformCheckArea();
    
    void StartCheckArea();
    
	void BeginClient();

	template<class H>
	FORCEINLINE void PerformZoneEditHandler(std::shared_ptr<TVoxelDataInfo> VdInfoPtr, H handler, std::function<void(TMeshDataPtr)> OnComplete);

	volatile bool bIsWorkFinished = false;

	//===============================================================================
	// save/load
	//===============================================================================
    
    FTimerHandle TimerAutoSave;
    
    std::mutex FastSaveMutex;

	bool bIsLoadFinished;

	void Save();
    
    void FastSave();
    
    void AutoSaveByTimer();

	void SaveJson();
	
	void SpawnInitialZone();

	TValueDataPtr SerializeVd(TVoxelData* Vd);

	//===============================================================================
	// async tasks
	//===============================================================================

	void ExecGameThreadZoneApplyMesh(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0x0);

	void ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0x0, const uint32 State = 0);

	//===============================================================================
	// threads
	//===============================================================================

	std::shared_timed_mutex ThreadListMutex;

	FGraphEventArray TerrainControllerEventList;

	//===============================================================================
	// voxel data storage
	//===============================================================================
    
    TBaseTerrainGenerator* Generator;
    
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

    void OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);

    void OnLoadZone(UTerrainZoneComponent* Zone);
    
	//===============================================================================
	// pipeline
	//===============================================================================

	int SpawnZone(const TVoxelIndex& Index, const TTerrainLodMask TerrainLodMask);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

protected:

	FMapInfo MapInfo;

	virtual void BeginTerrainLoad();

	virtual void InitializeTerrainController();

	virtual void BeginPlayServer();

	bool IsWorkFinished() { return bIsWorkFinished; };

	//===============================================================================
	// save/load
	//===============================================================================

	bool LoadJson();

	bool OpenFile();

	void CloseFile();

	void ForceSave(const TVoxelIndex& ZoneIndex, TVoxelData* Vd, TMeshDataPtr MeshDataPtr, const TInstanceMeshTypeMap& InstanceObjectMap);

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

	virtual TBaseTerrainGenerator* NewTerrainGenerator();
    
	virtual void OnOverlapActorDuringTerrainEdit(const FHitResult& OverlapResult, const FVector& Pos);

	virtual void OnFinishGenerateNewZone(const TVoxelIndex& Index);

	//===============================================================================
	// pipeline
	//===============================================================================

	//int GeneratePipeline(const TVoxelIndex& Index);

	void BatchSpawnZone(const TArray<TSpawnZoneParam>& SpawnZoneParamArray);

	void BatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList);

	virtual void BatchGenerateNewVd(const TArray<TSpawnZoneParam>& GenerationList, TArray<TVoxelData*>& NewVdArray);

	std::list<TChunkIndex> MakeChunkListByAreaSize(const uint32 AreaRadius);
	
	
};
