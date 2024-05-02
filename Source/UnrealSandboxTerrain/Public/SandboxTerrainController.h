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

struct TMeshData;
class UVoxelMeshComponent;
class UTerrainZoneComponent;
struct TInstanceMeshArray;
class TTerrainData;
class TCheckAreaMap;

class TVoxelDataInfo;
class TTerrainAreaHelper;
class TTerrainLoadHelper;

class UTerrainClientComponent;
class UTerrainServerComponent;

class UTerrainInstancedStaticMesh;

class ASandboxTerrainNetProxy;

class TThreadPool;
class TConveyour;

typedef TMap<uint64, TInstanceMeshArray> TInstanceMeshTypeMap;
typedef std::shared_ptr<TMeshData> TMeshDataPtr;
typedef kvdb::KvFile<TVoxelIndex, TValueData> TKvFile;


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

	UPROPERTY()
	int32 WorldSeed;
};

USTRUCT(BlueprintType, Blueprintable)
struct FTerrainDebugInfo {
	GENERATED_BODY()

	UPROPERTY()
	int CountVd = 0;

	UPROPERTY()
	int CountMd = 0;

	UPROPERTY()
	int CountCd = 0;

	UPROPERTY()
	int ConveyorSize = 0;

	UPROPERTY()
	int TaskPoolSize = 0;

	UPROPERTY()
	int OutOfSync = 0;

	UPROPERTY()
	int CountZones = 0;
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
	UTexture* TextureDiffuse;

	UPROPERTY(EditAnywhere)
	UTexture* TextureNormal;
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

USTRUCT()
struct FTerrainSwapAreaParams {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere)
    float Radius = 3000;
   
    
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
	Generated = 0, // Not used
	NoMesh = 1,
	NoVoxelData = 2,
	InternalSolid = 3,
};

typedef struct TKvFileZoneData {
	uint32 Flags = 0x0;
	uint32 LenMd = 0;

	bool Is(TZoneFlag Flag) {
		return (Flags >> (int)Flag) & 1U;
	};

	void SetFlag(int Flag) {
		Flags |= 1UL << Flag;
	};

} TKvFileZoneData;

struct TZoneModificationData {

	uint32 VStamp = 0;

};

struct TInstantMeshData {
	float X;
	float Y;
	float Z;

	float Roll;
	float Pitch;
	float Yaw;

	float ScaleX;
	float ScaleY;
	float ScaleZ;
};


UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainController : public AActor {
	GENERATED_UCLASS_BODY()

public:
    ASandboxTerrainController();
    
    friend UTerrainZoneComponent;
	friend TTerrainAreaHelper;
	friend TTerrainLoadHelper;
	friend UTerrainGeneratorComponent;
	friend UTerrainClientComponent;
	friend UTerrainServerComponent;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PostLoad() override;
    
    virtual void BeginDestroy() override;

	virtual void FinishDestroy() override;
       
    //========================================================================================
    // general
    //========================================================================================

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain General")
    FString MapName;

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain General")
    int32 WorldSeed = 0;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain General")
	uint32 ActiveAreaSize = 10;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain General")
	uint32 ActiveAreaDepth = 5;

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain General")
	double ConveyorMaxTime = 0.05;
              
	//========================================================================================
	// LOD
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain LOD")
	float LodRatio = .5f;

    //========================================================================================
    // Dynamic area streaming
    //========================================================================================
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Streaming")
    bool bEnableAreaStreaming;
    
    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Streaming")
    float PlayerLocationThreshold = 4000;

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
	bool bSaveOnEndPlay;

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
	// foliage
	//========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Foliage")
	USandboxTarrainFoliageMap* FoliageDataAsset;
   
    //========================================================================================
    // networking
    //========================================================================================

	UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Streaming")
	bool bAutoConnect = false;

    UPROPERTY(EditAnywhere, Category = "UnrealSandbox Terrain Network")
    uint32 ServerPort;

	//========================================================================================

	UPROPERTY()
	UTerrainGeneratorComponent* GeneratorComponent;

	UTerrainGeneratorComponent* GetTerrainGenerator();

	UPROPERTY()
	ASandboxTerrainNetProxy* NetProxy;

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void ForcePerformHardUnload();

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	void ForceTerrainNetResync();

	//========================================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandboxWorkaround")
	void UE51MaterialIssueWorkaround();

	//========================================================================================
	// basic coordinates and parameters
	//========================================================================================

	uint32 GetZoneVoxelResolution();

	float GetZoneSize();

	TVoxelIndex GetZoneIndex(const FVector& Pos);

	FVector GetZonePos(const TVoxelIndex& Index);

	UTerrainZoneComponent* GetZoneByVectorIndex(const TVoxelIndex& Index);

	TVoxelIndex ClcRegionByZoneIndex(const TVoxelIndex& ZoneIndex);

	uint32 GetRegionSize();

	TVoxelIndex ClcRegionOrigin(const TVoxelIndex& RegionIndex);

	//========================================================================================

	int CheckPlayerPositionZone(const FVector& Pos);

	//========================================================================================

	bool IsDebugModeOn();

	void DigTerrainRoundHole(const FVector& Origin, float Radius, bool bNoise = true);

	void DigCylinder(const FVector& Origin, const float Radius, const float Length, const FRotator& Rotator = FRotator(0), const bool bNoise = true);

	void DigTerrainCubeHoleComplex(const FVector& Origin, const FBox& Box, float Extend, const FRotator& Rotator = FRotator(0));

	void DigTerrainCubeHole(const FVector& Origin, float Extend, const FRotator& Rotator = FRotator(0));

	void FillTerrainCube(const FVector& Origin, float Extend, int MatId);

	void FillTerrainRound(const FVector& Origin, float Extend, int MatId);

	UTerrainInstancedStaticMesh* GetInstanceMeshComponent(TVoxelIndex ZoneIndex, uint32 TypeId, uint32 VariantId);

	UVoxelMeshComponent* GetVoxelMeshComponent(TVoxelIndex ZoneIndex);

	void RemoveInstanceAtMesh(UInstancedStaticMeshComponent* InstancedMeshComp, int32 ItemIndex);

	void RemoveInstanceAtMesh(TVoxelIndex ZoneIndex, uint32 TypeId, uint32 VariantId, int32 ItemIndex);

	template<class H>
	void PerformTerrainChange(H handler);

	template<class H>
	void EditTerrain(const H& ZoneHandler);

	bool GetTerrainMaterialInfoById(uint16 MaterialId, FSandboxTerrainMaterial& MaterialInfo);

	UMaterialInterface* GetRegularTerrainMaterial(uint16 MaterialId);

	UMaterialInterface* GetTransitionMaterial(const std::set<unsigned short>& MaterialIdSet);

	const FTerrainInstancedMeshType* GetInstancedMeshType(uint32 MeshTypeId, uint32 MeshVariantId = 0) const;

	//===============================================================================
	// async tasks
	//===============================================================================

	void AddAsyncTask(std::function<void()> Function);

	//========================================================================================
	// network
	//========================================================================================

	void BeginClientTerrainLoad(const TVoxelIndex& ZoneIndex);

	void NetworkSerializeZone(FBufferArchive& Buffer, const TVoxelIndex& VoxelIndex);

	void NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData);

	float ClcGroundLevel(const FVector& V);

	//===============================================================================
	// perlin noise
	//===============================================================================

	float PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const;

	float NormalizedPerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const;

	//===============================================================================
	// groud level
	//===============================================================================

	float GetGroundLevel(const FVector& Pos);

	//===============================================================================
	// debug / memory stat
	//===============================================================================

	UFUNCTION(BlueprintCallable, Category = "UnrealSandbox")
	FTerrainDebugInfo GetMemstat();

	//========================================================================================
	// collision
	//========================================================================================

	void OnFinishAsyncPhysicsCook(const TVoxelIndex& ZoneIndex);


	//===============================================================================
	// foliage
	//===============================================================================

	FSandboxFoliage GetFoliageById(uint32 FoliageId) const;

private:

	volatile bool bForceResync = false;

	volatile bool bInitialLoad = true;

	volatile bool bEnableConveyor = false;

	volatile bool bForcePerformHardUnload = false;
    
	std::unordered_set<TVoxelIndex> InitialLoadSet;

	void StartPostLoadTimers();

    TCheckAreaMap* CheckAreaMap;

    FTimerHandle TimerSwapArea;
    
    void PerformCheckArea();
    
    void StartCheckArea();

	volatile bool bIsWorkFinished = false;

	void LoadConsoleVars();

	//===============================================================================
	// modify terrain
	//===============================================================================

	template<class H>
	FORCEINLINE void PerformZoneEditHandler(const TVoxelIndex& Zoneindex, std::shared_ptr<TVoxelDataInfo> VdInfoPtr, H Handler, std::function<void(TMeshDataPtr)> OnComplete);

	void PerformEachZone(const FVector& Origin, const float Extend, std::function<void(TVoxelIndex, FVector, std::shared_ptr<TVoxelDataInfo>)>);

	//===============================================================================
	// save/load
	//===============================================================================
    
    FTimerHandle TimerAutoSave;
    
    std::mutex SaveMutex;

	bool bIsLoadFinished;
        
    void AutoSaveByTimer();

	void SaveJson();
	
	void SpawnInitialZone();

	TValueDataPtr SerializeVd(TVoxelData* Vd) const;

	void DeserializeVd(TValueDataPtr Data, TVoxelData* Vd) const;

	void DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstanceMeshTypeMap& ZoneInstMeshMap) const;

	//===============================================================================
	// async tasks
	//===============================================================================

	TConveyour* Conveyor;

	void AddTaskToConveyor(std::function<void()> Function);

	void ExecGameThreadZoneApplyMesh(const TVoxelIndex& Index, UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr);

	void ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const bool bIsNewGenerated = false, const bool bIsChanged = false);

	void ExecGameThreadMoMeshZoneSpawn(const TArray<TVoxelIndex>& IndexList);

	//void ExecGameThreadRestoreSoftUnload(const TVoxelIndex& ZoneIndex);

	//===============================================================================
	// threads
	//===============================================================================

	TThreadPool* ThreadPool = nullptr;

	//===============================================================================
	// voxel data storage
	//===============================================================================
        
    TTerrainData* TerrainData;

	mutable TKvFile TdFile;

	mutable TKvFile ObjFile;

	TKvFile VdFile;

	std::shared_ptr<TVoxelDataInfo> GetVoxelDataInfo(const TVoxelIndex& Index);

	TVoxelData* LoadVoxelDataByIndex(const TVoxelIndex& Index);

	//===============================================================================
	// mesh data storage
	//===============================================================================

	bool LoadMeshAndObjectDataByIndex(const TVoxelIndex& Index, TMeshDataPtr& MeshData, TInstanceMeshTypeMap& ZoneInstMeshMap) const;

	//===============================================================================
	// inst. meshes
	//===============================================================================

	TMap<uint64, FTerrainInstancedMeshType> InstMeshMap;

	//===============================================================================
	// foliage
	//===============================================================================

	TMap<uint32, FSandboxFoliage> FoliageMap;

	//===============================================================================
	// materials
	//===============================================================================

	UPROPERTY()
	TMap<uint64, UMaterialInterface*> TransitionMaterialCache;

	UPROPERTY()
	TMap<uint16, UMaterialInterface*> RegularMaterialCache;

	TMap<uint16, FSandboxTerrainMaterial> MaterialMap;

	//===============================================================================
	// 
	//===============================================================================

    void OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);

    void OnLoadZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone);
    
	//===============================================================================
	// 
	//===============================================================================

	void SpawnZone(const TVoxelIndex& Index);

	UTerrainZoneComponent* AddTerrainZone(FVector pos);

	void UnloadUnreachableZones(const TArray<TVoxelIndex>& UnreachableZones);

	void CheckUnreachableZones(const TArray<FVector>& PlayerLocationList);

	//===============================================================================
	// network
	//===============================================================================

	UTerrainClientComponent* TerrainClientComponent;

	UTerrainServerComponent* TerrainServerComponent;

	int32 GetMapVStamp();

	void SaveTerrainMetadata();

	void LoadTerrainMetadata();

	TArray<std::tuple<TVoxelIndex, TZoneModificationData>> NetworkServerMapInfo();

	void OnReceiveServerMapInfo(const TMap<TVoxelIndex, TZoneModificationData>& ServerDataMap);

	FTimerHandle TimerPingServer;

	void PingServer();

protected:

	bool bGenerateOnlySmallSpawnPoint = false;

	FMapInfo MapInfo;

	virtual void BeginServerTerrainLoad();

	FVector BeginServerTerrainLoadLocation;

	virtual void InitializeTerrainController();

	virtual void BeginPlayServer();

	virtual void BeginPlayClient();

	virtual void BeginNewWorld();

	bool IsWorkFinished();

	void AddInitialZone(const TVoxelIndex& ZoneIndex);

	//===============================================================================
	// save/load
	//===============================================================================

	FString GetSaveDir();

	bool LoadJson();

	bool OpenFile();

	void CloseFile();

	void ForceSave(const TVoxelIndex& ZoneIndex, TVoxelData* Vd, TMeshDataPtr MeshDataPtr, const TInstanceMeshTypeMap& InstanceObjectMap);

	void Save(std::function<void(uint32, uint32)> OnProgress = nullptr, std::function<void(uint32)> OnFinish = nullptr);

	void MarkZoneNeedsToSaveObjects(const TVoxelIndex& ZoneIndex);

	void ZoneHardUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex);

	void ZoneSoftUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex);

	virtual bool OnZoneSoftUnload(const TVoxelIndex& ZoneIndex);

	virtual void OnRestoreZoneSoftUnload(const TVoxelIndex& ZoneIndex);

	virtual bool OnZoneHardUnload(const TVoxelIndex& ZoneIndex);

	virtual void GetAnchorObjectsLocation(TArray<FVector>& List) const;

	void ClientStart();

	//===============================================================================
	// voxel data storage
	//===============================================================================

	std::shared_ptr<TMeshData> GenerateMesh(TVoxelData* Vd);

	//===============================================================================
	// NewVoxelData
	//===============================================================================
      
	TVoxelData* NewVoxelData();

    //===============================================================================
    // virtual functions
    //===============================================================================

	virtual UTerrainGeneratorComponent* NewTerrainGenerator();
    
	virtual void OnOverlapActorTerrainEdit(const FOverlapResult& OverlapResult, const FVector& Pos);

	virtual void OnFinishGenerateNewZone(const TVoxelIndex& Index);

	virtual void OnFinishLoadZone(const TVoxelIndex& Index);

	virtual void OnStartBackgroundSaveTerrain();

	virtual void OnFinishBackgroundSaveTerrain();

	virtual void OnProgressBackgroundSaveTerrain(float Progress);

	virtual void OnFinishInitialLoad();

	virtual void OnDestroyInstanceMesh(UTerrainInstancedStaticMesh* InstancedMeshComp, int32 ItemIndex);

	//===============================================================================
	// pipeline
	//===============================================================================

	//int GeneratePipeline(const TVoxelIndex& Index);

	void BatchSpawnZone(const TArray<TSpawnZoneParam>& SpawnZoneParamArray);

	void BatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList);

	void PostBatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList);

	std::list<TChunkIndex> MakeChunkListByAreaSize(const uint32 AreaRadius);

	//===============================================================================
	// core
	//===============================================================================

	void InvokeSafe(std::function<void()> Function);

	void ShutdownThreads();
};



UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainNetProxy : public AActor {
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRpcDigSphere(int32 MapVer, const FVector& Origin, float Radius, bool bNoise);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRpcDigCube(int32 MapVer, const FVector& Origin, float Extend, const FRotator& Rotator);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRpcDestroyInstanceMesh(int32 MapVer, int32 X, int32 Y, int32 Z, uint32 TypeId, uint32 VariantId, int32 ItemIndex);

protected:

	virtual void BeginPlay() override;

private:

	ASandboxTerrainController* Controller;

};
