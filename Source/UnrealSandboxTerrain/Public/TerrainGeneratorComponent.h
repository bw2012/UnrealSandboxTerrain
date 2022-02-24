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
#include "TerrainGeneratorComponent.generated.h"


class TPerlinNoise;
class ASandboxTerrainController;
class TChunkData;
struct TInstanceMeshArray;
struct FSandboxFoliage;
typedef TMap<int32, TInstanceMeshArray> TInstanceMeshTypeMap;


enum TZoneGenerationType : int32 {
	AirOnly,
	FullSolidOneMaterial,
	FullSolidMultipleMaterials,
	Landscape, 
	Other
};


enum TGenerationMethod : int32 {
	FastSimple = 0x1,
	SlowComplex = 0x2,
	UltraFastPartially = 0x3,
	Skip = 0x4
};

struct TGenerateVdTempItm {
	int Idx = 0;
	TVoxelIndex ZoneIndex;
	TVoxelData* Vd;
	TChunkData* ChunkData;
	int GenerationLOD = 0; // UltraFastPartially
	TZoneGenerationType Type;
	TGenerationMethod Method;
	bool bHasStructures = false;
};

struct TGenerateZoneResult {
	TVoxelData* Vd = nullptr;
	TZoneGenerationType Type;
	TGenerationMethod Method;
};

typedef std::tuple<float, TMaterialId> TGenerationResult;

typedef std::tuple<FVector, FVector, float, TMaterialId> ResultA;

struct TZoneStructureHandler;

typedef std::function<TGenerationResult(const float, const TMaterialId, const TVoxelIndex&, const FVector&, const FVector&)> TZoneGenerationFunction;

struct TZoneStructureHandler {
	TVoxelIndex ZoneIndex;
	int Type = 0;
	TZoneGenerationFunction Function = nullptr;
	std::function<bool(const TVoxelIndex&, const FVector&, const FVector&)> LandscapeFoliageHandler = nullptr;
	FBox Box;
	FVector Pos;
	float Val1;
	float Val2;
};


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainGeneratorComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

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

	virtual void Clean();

	virtual void Clean(TVoxelIndex& Index);

	//========================================================================================
	// foliage etc.
	//========================================================================================

	virtual void GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	virtual bool UseCustomFoliage(const TVoxelIndex& Index);

	virtual FSandboxFoliage FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos);

	virtual bool OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale);

	virtual bool IsOverrideGroundLevel(const TVoxelIndex& Index);

	virtual float GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel);

	void AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure);

protected:

	int32 ZoneVoxelResolution;

	TPerlinNoise* Pn;

	virtual void BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& List);

	virtual void BatchGenerateSlightVd(TArray<TGenerateVdTempItm>& List);

	virtual void OnBatchGenerationFinished();

	virtual TZoneGenerationType ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkData* ChunkData);

	virtual void PrepareMetaData();

	virtual bool IsForcedComplexZone(const TVoxelIndex& ZoneIndex);

	virtual bool SpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform);

	virtual void PostGenerateNewInstanceObjects(const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const;

private:

	TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;

	std::mutex ChunkDataMapMutex;

	std::unordered_map<TVoxelIndex, TChunkData*> ChunkDataCollection;

	TChunkData* GetChunkHeightMap(int X, int Y);

	virtual void GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkData* ChunkData);

	float ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const;

	void GenerateZoneVolume(const TGenerateVdTempItm& Itm) const;

	void GenerateZoneVolumeWithFunction(const TGenerateVdTempItm& Itm, const std::vector<TZoneStructureHandler>& StructureList) const;

	FORCEINLINE TMaterialId MaterialFuncion(const FVector& WorldPos, float GroundLevel) const;

	const FTerrainUndergroundLayer* GetMaterialLayer(float Z, float RealGroundLevel) const;

	int GetMaterialLayers(const TChunkData* ChunkData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const;

	void GenerateNewFoliageLandscape(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	//====

	ResultA A(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, const TChunkData* ChunkData) const;

	float B(const TVoxelIndex& Index, TVoxelData* VoxelData, const TChunkData* ChunkData) const;

	void GenerateLandscapeZoneSlight(const TGenerateVdTempItm& Itm) const;

	std::unordered_map<TVoxelIndex, std::vector<TZoneStructureHandler>> StructureMap;

	//====

	//float TestFunctionMakeCaveLayer(float Density, const FVector& WorldPos) const;

	//void Test(const TGenerateVdTempItm& Itm) const;

public:
	bool HasStructures(const TVoxelIndex& ZoneIndex) const;

	//void Test(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd) const;

	bool SelectRandomSpawnPoint(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, FVector& SectedLocation, FVector& SectedNormal) const;

};