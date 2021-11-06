#pragma once


// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelData.h"
#include "VoxelIndex.h"
#include "SandboxTerrainCommon.h"
#include <unordered_map>
#include <mutex>
#include "TerrainGeneratorComponent.generated.h"


class TPerlinNoise;
class ASandboxTerrainController;
class TChunkData;
struct TInstanceMeshArray;
struct FSandboxFoliage;
typedef TMap<int32, TInstanceMeshArray> TInstanceMeshTypeMap;


struct TGenerateVdTempItm {

	int Idx = 0;

	TVoxelIndex ZoneIndex;
	TVoxelData* Vd;

	TChunkData* ChunkData;

	// partial generation
	int GenerationLOD = 0;

	// partial generation v2
	bool bSlightGeneration = false;

	int Type = 0;

};

struct TGenerateZoneResult {
	TVoxelData* Vd = nullptr;
	int Type = 0;
	int Method = 0;
};


typedef std::tuple<FVector, FVector, float, TMaterialId> ResultA;

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

	int32 ZoneHash(const FVector& ZonePos);

	virtual void Clean();

	virtual void Clean(TVoxelIndex& Index);

	//========================================================================================
	// foliage etc.
	//========================================================================================

	virtual void GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	virtual bool UseCustomFoliage(const TVoxelIndex& Index);

	virtual bool SpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform);

	virtual FSandboxFoliage FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos);

	virtual bool OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale);

	virtual bool IsOverrideGroundLevel(const TVoxelIndex& Index);

	virtual float GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel);

	virtual bool ForcePerformZone(const TVoxelIndex& ZoneIndex);

protected:

	TPerlinNoise* Pn;

	virtual void BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& GenPass2List);

	virtual void OnBatchGenerationFinished();

	virtual int ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkData* ChunkData);

private:

	TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;

	std::mutex ChunkDataMapMutex;

	std::unordered_map<TVoxelIndex, TChunkData*> ChunkDataCollection;

	TChunkData* GetChunkHeightMap(int X, int Y);

	virtual void GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkData* ChunkData);

	float ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const;

	void GenerateZoneVolume(const TGenerateVdTempItm& Itm) const;

	FORCEINLINE TMaterialId MaterialFuncion(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel) const;

	const FTerrainUndergroundLayer* GetMaterialLayer(float Z, float RealGroundLevel) const;

	int GetMaterialLayers(const TChunkData* ChunkData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const;

	void GenerateNewFoliage(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	//====

	ResultA A(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, const TChunkData* ChunkData) const;

	void B(const TVoxelIndex& Index, TVoxelData* VoxelData, const TChunkData* ChunkData) const;

	void GenerateLandscapeZoneSlight(const TGenerateVdTempItm& Itm) const;

};