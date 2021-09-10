#pragma once


#include "EngineMinimal.h"
#include "VoxelData.h"
#include "VoxelIndex.h"
#include "SandboxTerrainCommon.h"
#include <unordered_map>
#include <mutex>



class TPerlinNoise;
class ASandboxTerrainController;
class TChunkHeightMapData;
struct TInstanceMeshArray;
struct FSandboxFoliage;

typedef TMap<int32, TInstanceMeshArray> TInstanceMeshTypeMap;


class UNREALSANDBOXTERRAIN_API TBaseTerrainGenerator {
	
public:

	TBaseTerrainGenerator(ASandboxTerrainController* Controller);

	~TBaseTerrainGenerator();

	ASandboxTerrainController* GetController() const;

	virtual float PerlinNoise(const float X, const float Y, const float Z) const;

	virtual float PerlinNoise(const FVector& Pos) const;

	virtual float PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const;

	virtual void OnBeginPlay() = 0;

	virtual void BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& GenerationList, TArray<TVoxelData*>& NewVdArray) = 0;

	virtual float GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const = 0;

	virtual float DensityFunctionExt(float Density, const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) const = 0;

	virtual int32 ZoneHash(const FVector& ZonePos);

	virtual void Clean() = 0;

	virtual void Clean(TVoxelIndex& Index) = 0;

	//========================================================================================
	// foliage etc.
	//========================================================================================

	virtual void GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) = 0;


	virtual bool UseCustomFoliage(const TVoxelIndex& Index) {
		return false;
	}

	virtual bool SpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform) {
		return false;
	}

	virtual FSandboxFoliage FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos) {
		return FoliageType;
	}

	virtual bool OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale) {
		return true;
	}

	virtual bool IsOverrideGroundLevel(const TVoxelIndex& Index) {
		return false;
	}

	virtual float GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel) {
		return GroundLevel;
	}

	virtual bool ForcePerformZone(const TVoxelIndex& ZoneIndex) {
		return false;
	}

protected:

	ASandboxTerrainController* Controller;

	TPerlinNoise* Pn;

};

typedef struct TGenerateVdTempItm {

	int Idx = 0;

	TVoxelIndex ZoneIndex;
	TVoxelData* Vd;

	TChunkHeightMapData* ChunkData;

	// partial generation
	int GenerationLOD = 0;

} TGenerateVdTempItm;


class UNREALSANDBOXTERRAIN_API TDefaultTerrainGenerator : public TBaseTerrainGenerator {

	using TBaseTerrainGenerator::TBaseTerrainGenerator;


public:

	//TDefaultTerrainGenerator(ASandboxTerrainController* Controller) : TBaseTerrainGenerator(Controller) {

	//};

	//uint32 GetChunkDataMemSize();

	virtual void OnBeginPlay() override;

	virtual void BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& GenerationList, TArray<TVoxelData*>& NewVdArray) override;

	virtual float GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const override;

	virtual float DensityFunctionExt(float Density, const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) const override;

	virtual void Clean() override;

	virtual void Clean(TVoxelIndex& Index) override;


	//========================================================================================
	// foliage etc.
	//========================================================================================

	virtual void GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	//========================================================================================
	// private
	//========================================================================================

protected:

	virtual void BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& GenPass2List);

	virtual void OnBatchGenerationFinished();

private:

	TArray<FTerrainUndergroundLayer> UndergroundLayersTmp;

	std::mutex ZoneHeightMapMutex;

	std::unordered_map<TVoxelIndex, TChunkHeightMapData*> ChunkDataCollection;

	TChunkHeightMapData* GetChunkHeightMap(int X, int Y);

	virtual TVoxelDataFillState GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, TChunkHeightMapData** ChunkDataPtr);

	bool IsZoneOverGroundLevel(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin) const;

	bool IsZoneOnGroundLevel(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin) const;

	float ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const;

	void GenerateZoneVolume(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const TChunkHeightMapData* ChunkHeightMapData, const int LOD = 0) const;

	FORCEINLINE TMaterialId MaterialFuncion(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel) const;

	const FTerrainUndergroundLayer* GetMaterialLayer(float Z, float RealGroundLevel) const;

	int GetMaterialLayersCount(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const;

	void GenerateNewFoliage(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

	void GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap);

};