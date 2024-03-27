#pragma once


// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelData.h"
#include "VoxelIndex.h"
#include "SandboxTerrainCommon.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include "TerrainGeneratorComponent.generated.h"


class TPerlinNoise;
class ASandboxTerrainController;
class TChunkData;
struct TInstanceMeshArray;
struct FSandboxFoliage;

class UTerrainGeneratorComponent;
class TStructuresGenerator;
struct TZoneStructureHandler;

typedef std::shared_ptr<TChunkData> TChunkDataPtr;
typedef const std::shared_ptr<const TChunkData> TConstChunkData;
typedef std::tuple<float, TMaterialId> TGenerationResult;
typedef std::tuple<FVector, FVector, float, TMaterialId> ResultA;
typedef std::function<TGenerationResult(const float, const TMaterialId, const TVoxelIndex&, const FVector&, const FVector&)> TZoneGenerationFunction;
typedef TMap<uint64, TInstanceMeshArray> TInstanceMeshTypeMap;


enum TZoneGenerationType : int32 {
	AirOnly, // all points are 0
	FullSolidOneMaterial, // all points are 1 and one material
	FullSolidMultipleMaterials, // all points are 1 and multiple material
	Landscape, // landscape
	Other // all others
};

enum TGenerationMethod : int32 {
	NotDefined = 0x0, // for warning only
	FastSimple = 0x1, // loop over important point only
	SlowComplex = 0x2, // loop over each point
	UltraFastPartially = 0x3, // not used now
	Skip = 0x4, // full solid with multiple mats, will generated later
	SetEmpty = 0x5 // no mesh, air or full solid with single material 
};

struct TZoneOreData {
	TVoxelIndex ZoneIndex;
	FVector Origin;
	TMaterialId MatId = 0;
	uint32 MeshTypeId = 0;
};

typedef std::shared_ptr<TZoneOreData> TZoneOreDataPtr;

struct TGenerateVdTempItm {
	int Idx = 0;
	TVoxelIndex ZoneIndex;
	TVoxelData* Vd;
	TChunkDataPtr ChunkData;
	int GenerationLOD = 0; // UltraFastPartially, not used now
	TZoneGenerationType Type;
	TGenerationMethod Method;
	bool bHasStructures = false; // tunnels and etc

	TZoneOreDataPtr OreData = nullptr;
};

struct TGenerateZoneResult {
	TVoxelData* Vd = nullptr;
	TZoneGenerationType Type;
	TGenerationMethod Method;

	TZoneOreDataPtr OreData = nullptr;
};

struct TZoneStructureHandler {
	TVoxelIndex ZoneIndex;
	int Type = 0;
	TZoneGenerationFunction Function = nullptr;
	std::function<bool(const TVoxelIndex&, const FVector&, const FVector&)> LandscapeFoliageFilter = nullptr;
	FVector Pos;
	float Val1;
	float Val2;
};

struct TInstanceMeshSpawnParams {
	bool bIgnoreNormalNegativeZ = false;
};


class UNREALSANDBOXTERRAIN_API TMetaStructure {

protected:

	TVoxelIndex OriginIndex;

public:

	virtual ~TMetaStructure() { };

	virtual TArray<TVoxelIndex> GetRelevantZones(TStructuresGenerator* Generator) const = 0;

	virtual void MakeMetaData(TStructuresGenerator* Generator) const = 0;
};


struct TLandscapeZoneHandler {

	TVoxelIndex ZoneIndex;

	std::function<float(const float, const TVoxelIndex&, const FVector&)> Function = nullptr;
};


class UNREALSANDBOXTERRAIN_API TStructuresGenerator {

	friend UTerrainGeneratorComponent;

public:

	ASandboxTerrainController* GetController();

	bool HasStructures(const TVoxelIndex& ZoneIndex) const;

	void AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure);

	UTerrainGeneratorComponent* GetGeneratorComponent();

	void AddLandscapeStructure(const TLandscapeZoneHandler& Structure);

	float PerformLandscapeZone(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float Lvl) const;

private:

	UTerrainGeneratorComponent* MasterGenerator;

	std::unordered_map<TVoxelIndex, std::vector<TZoneStructureHandler>> StructureMap;

	std::unordered_map<TVoxelIndex, std::vector<TLandscapeZoneHandler>> LandscapeStructureMap;
};


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainGeneratorComponent : public UActorComponent {
	GENERATED_UCLASS_BODY()

	friend TStructuresGenerator;

public:
		
	UPROPERTY()
	int DfaultGrassMaterialId;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void FinishDestroy();

	ASandboxTerrainController* GetController() const;

	float PerlinNoise(const float X, const float Y, const float Z) const;

	float PerlinNoise(const FVector& Pos) const;

	float PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const;

	void ForceGenerateZone(TVoxelData* VoxelData, const TVoxelIndex& ZoneIndex);

	void BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& GenerationList, TArray<TGenerateZoneResult>& ResultArray);

	virtual float GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const;

	virtual float DensityFunctionExt(float Density, const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) const;

	int32 ZoneHash(const FVector& ZonePos) const;

	int32 ZoneHash(const TVoxelIndex& ZoneIndex) const;

	virtual void Clean();

	virtual void Clean(const TVoxelIndex& Index);

	//========================================================================================
	// foliage etc.
	//========================================================================================

	virtual void GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap, const TGenerateZoneResult& GenResult);

	virtual FSandboxFoliage FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos);

	virtual bool OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale);

	void AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure);

	TStructuresGenerator* GetStructuresGenerator();

protected:

	int32 ZoneVoxelResolution;

	TPerlinNoise* Pn;

	TStructuresGenerator* StructuresGenerator;

	//========================================================================================
	// tags
	//========================================================================================

	const FString* GetZoneTag(const TVoxelIndex& ZoneIndex, FString Name) const;

	bool CheckZoneTag(const TVoxelIndex& ZoneIndex, FString Name, FString Value) const;

	void SetZoneTag(const TVoxelIndex& ZoneIndex, FString Name, FString Value);

	void SetChunkTag(const TVoxelIndex& ChunkIndex, FString Name, FString Value);

	//========================================================================================

	virtual void BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& List);

	virtual void BatchGenerateSlightVd(TArray<TGenerateVdTempItm>& List);

	virtual void OnBatchGenerationFinished();

	TZoneGenerationType ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkDataPtr ChunkData);

	virtual void PrepareMetaData();

	virtual bool IsForcedComplexZone(const TVoxelIndex& ZoneIndex);

	virtual void PostGenerateNewInstanceObjects(const TVoxelIndex& ZoneIndex, const TZoneGenerationType ZoneType, const TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const;

	void GenerateRandomInstMesh(TInstanceMeshTypeMap& ZoneInstanceMeshMap, uint32 MeshTypeId, FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, int Min = 1, int Max = 1, const TInstanceMeshSpawnParams& Params = TInstanceMeshSpawnParams()) const;

	virtual TChunkDataPtr NewChunkData();

	virtual TChunkDataPtr GenerateChunkData(const TVoxelIndex& Index);

	virtual TMaterialId MaterialFuncionExt(const TGenerateVdTempItm* GenItm, const TMaterialId MatId, const FVector& WorldPos) const;

	virtual TGenerateVdTempItm CollectVdGenerationData(const TVoxelIndex& ZoneIndex);

	virtual void ExtVdGenerationData(TGenerateVdTempItm& VdGenerationData);

	virtual void GenerateNewFoliageLandscape(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	virtual TStructuresGenerator* NewStructuresGenerator();

	float PerformLandscapeZone(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float Lvl) const;

	FRandomStream MakeNewRandomStream(const FVector& ZonePos) const;

private:

	//========================================================================================
	// tags
	//========================================================================================

	TMap<TVoxelIndex, TMap<FString, FString>> ZoneTagData;

	TMap<TVoxelIndex, TMap<FString, FString>> ChunkTagData;

	//========================================================================================

	TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;

	std::mutex ChunkDataMapMutex;

#ifdef __cpp_lib_atomic_shared_ptr                      
	std::unordered_map<TVoxelIndex, std::atomic<TChunkDataPtr>> ChunkDataCollection;
#else
	std::unordered_map<TVoxelIndex, TChunkDataPtr> ChunkDataCollection;
#endif

	TChunkDataPtr GetChunkData(int X, int Y);

	virtual void GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkDataPtr ChunkData);

	float ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const;

	void GenerateZoneVolume(const TGenerateVdTempItm& Itm) const;

	void GenerateZoneVolumeWithFunction(const TGenerateVdTempItm& Itm, const std::vector<TZoneStructureHandler>& StructureList) const;

	TMaterialId MaterialFuncion(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float GroundLevel) const;

	const FTerrainUndergroundLayer* GetMaterialLayer(float Z, float RealGroundLevel) const;

	int GetMaterialLayers(const TChunkDataPtr ChunkData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const;

	//====

	ResultA A(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, const TGenerateVdTempItm& Itm) const;

	float B(const TVoxelIndex& Index, TVoxelData* VoxelData, TConstChunkData ChunkData) const;

	void GenerateLandscapeZoneSlight(const TGenerateVdTempItm& Itm) const;

	//====

	//float TestFunctionMakeCaveLayer(float Density, const FVector& WorldPos) const;

	//void Test(const TGenerateVdTempItm& Itm) const;

public:
	bool HasStructures(const TVoxelIndex& ZoneIndex) const;

	//void Test(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd) const;

	bool SelectRandomSpawnPoint(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, FVector& SectedLocation, FVector& SectedNormal, const TInstanceMeshSpawnParams& Params = TInstanceMeshSpawnParams()) const;

};
