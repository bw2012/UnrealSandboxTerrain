#pragma once

#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
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

struct TMeshData;
class UVoxelMeshComponent;
class UTerrainZoneComponent;
struct TInstanceMeshArray;
class UVdClientComponent;
class TTerrainData;
class TCheckAreaMap;
class TTerrainGenerator;
class TVoxelDataInfo;
class TTerrainAreaPipeline;
class TTerrainLoadPipeline;
class TTerrainGeneratorPipeline;

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

UENUM(BlueprintType)
enum class ESandboxFoliageType : uint8 {
	Grass = 0	UMETA(DisplayName = "Grass"),
	Tree = 1   UMETA(DisplayName = "Tree"),
	Cave = 2   UMETA(DisplayName = "Cave foliage"),
	Custom = 3   UMETA(DisplayName = "Custom"),
};


USTRUCT()
struct FSandboxFoliage {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	ESandboxFoliageType Type;

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

UCLASS(BlueprintType, Blueprintable)
class UNREALSANDBOXTERRAIN_API USandboxTarrainFoliageMap : public UDataAsset {
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	TMap<uint32, FSandboxFoliage> FoliageMap;
};


UENUM(BlueprintType)
enum class FSandboxTerrainMaterialType : uint8 {
	SOIL = 0	UMETA(DisplayName = "Soil"),
	ROCK = 1	UMETA(DisplayName = "Rock"),
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

USTRUCT()
struct FTerrainUndergroundLayer {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    int32 MatId;

    UPROPERTY(EditAnywhere)
    float StartDepth;

    UPROPERTY(EditAnywhere)
    FString Name;
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
    float Distance4 = 8000;
    
    UPROPERTY(EditAnywhere)
    float Distance5 = 10000;
    
    UPROPERTY(EditAnywhere)
    float Distance6 = 12000;
};

typedef uint8 TTerrainLodMask;

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


typedef struct TVoxelDensityFunctionData {
    float Density;
    float GroundLelel;
    FVector WorldPos;
    FVector LocalPos;
    TVoxelIndex ZoneIndex;
} TVoxelDensityFunctionData;


UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor {
	GENERATED_UCLASS_BODY()

public:
    ASandboxTerrainController();
    
    friend UTerrainZoneComponent;
	friend TTerrainGenerator;
	friend UVdClientComponent;
	friend TTerrainAreaPipeline;
	friend TTerrainLoadPipeline;
	friend TTerrainGeneratorPipeline;

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
    bool bEnableLOD;
    
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
	int32 SaveGeneratedZones;

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
	
	//static bool CheckZoneBounds(FVector Origin, float Size);

	//========================================================================================

	void DigTerrainRoundHole(const FVector& Origin, float Radius, float Strength);

	void DigTerrainCubeHole(const FVector& Origin, float Extend, const FRotator& Rotator = FRotator());

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

private:
    
	void GenerateNewZoneVd(std::shared_ptr<TVoxelDataInfo> VdInfoPtr, const TVoxelIndex& Index);

	void StartPostLoadTimers();

    TCheckAreaMap* CheckAreaMap;

    FTimerHandle TimerSwapArea;
    
    void PerformCheckArea();
    
    void StartCheckArea();
    
	void BeginClient();

	void DigTerrainRoundHole_Internal(const FVector& Origin, float Radius, float Strength);

	template<class H>
	FORCEINLINE void PerformZoneEditHandler(std::shared_ptr<TVoxelDataInfo> VdInfoPtr, H handler, std::function<void(TMeshDataPtr)> OnComplete);

	volatile bool bIsWorkFinished = false;

	bool IsWorkFinished() { return bIsWorkFinished; };

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

	//===============================================================================
	// pipeline
	//===============================================================================

	int GeneratePipeline(const TVoxelIndex& Index);

	int SpawnZonePipeline(const TVoxelIndex& pos, const TTerrainLodMask TerrainLodMask = 0);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

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
    
    TTerrainGenerator* Generator;
    
    TTerrainData* TerrainData;

	TKvFile VdFile;

	TKvFile MdFile;

	TKvFile ObjFile;

	//TVoxelData* GetVoxelDataByPos(const FVector& Pos);

	std::shared_ptr<TVoxelDataInfo> GetVoxelDataInfo(const TVoxelIndex& Index);

	TVoxelData* LoadVoxelDataByIndex(const TVoxelIndex& Index);

	std::shared_ptr<TMeshData> GenerateMesh(TVoxelData* Vd);

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

	int GetCollisionMeshSectionLodIndex() {
		if (bEnableLOD) {
			if (CollisionSection > 6) return 6;
			return CollisionSection;
		}
		return 0;
	}

    void OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);

    void OnLoadZone(UTerrainZoneComponent* Zone);
       
    TVoxelData* NewVoxelData();
    
protected:

	FMapInfo MapInfo;

	void RunGenerateTerrainPipeline(std::function<void()> OnFinish = nullptr, std::function<void(uint32, uint32)> OnProgress = nullptr);

	bool LoadJson();

	bool OpenFile();

	void CloseFile();

	virtual void BeginTerrainLoad();

	virtual void InitializeTerrainController();

	virtual void BeginPlayServer();

	float PerlinNoise(const FVector& Pos) const;

	float NormalizedPerlinNoise(const FVector& Pos) const;
       
    //===============================================================================
    // virtual functions
    //===============================================================================
    
    virtual bool OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale);
    
    virtual float GeneratorDensityFunc(const TVoxelDensityFunctionData& FunctionData);
    
    virtual bool GeneratorForcePerformZone(const TVoxelIndex& ZoneIndex);

	virtual FSandboxFoliage GeneratorFoliageOverride(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos);

	virtual bool GeneratorUseCustomFoliage(const TVoxelIndex& Index);

	virtual bool GeneratorSpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform);

	virtual bool IsOverrideGroundLevel(const TVoxelIndex& Index);

	virtual float GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel);

	virtual void OnOverlapActorDuringTerrainEdit(const FHitResult& OverlapResult, const FVector& Pos);

	virtual void OnFinishGenerateNewZone(const TVoxelIndex& Index);

	//virtual void OnFinishGenerateNewVd(const TVoxelIndex& Index, TVoxelData* Vd);

	//virtual void GeneratorEachSubstanceCell(const TVoxelIndex& Index, const FVector& WorldPos);
	
};
