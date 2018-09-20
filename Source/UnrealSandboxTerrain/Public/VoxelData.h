#pragma once

#include "EngineMinimal.h"

#include <list>
#include <array>
#include <memory>
#include <set>
#include <mutex>
#include <functional>

#define LOD_ARRAY_SIZE 7

typedef unsigned char TDensityVal;
typedef unsigned short TMaterialId;

// density or material state
enum TVoxelDataFillState {
	ZERO,		// data contains only zero values
	FULL,		// data contains only one same value
	MIXED		// mixed state, any value in any point
};

typedef struct TSubstanceCache {
	std::list<int> cellList;
} TSubstanceCache;

class TVoxelData {

private:
	TVoxelDataFillState density_state;
	unsigned short base_fill_mat = 0;

	int voxel_num;
	float volume_size;
	unsigned char* density_data;
	unsigned short* material_data;

	volatile double last_change;
	volatile double last_save;
	volatile double last_mesh_generation;
	volatile double last_cache_check;

	FVector origin = FVector(0.0f, 0.0f, 0.0f);
	FVector lower = FVector(0.0f, 0.0f, 0.0f);
	FVector upper = FVector(0.0f, 0.0f, 0.0f);

	void initializeDensity();
	void initializeMaterial();

	bool performCellSubstanceCaching(int x, int y, int z, int lod, int step);

public:
	std::array<TSubstanceCache, LOD_ARRAY_SIZE> substanceCacheLOD;

	TVoxelData(int, float);
	~TVoxelData();

	std::mutex vd_edit_mutex;

	FORCEINLINE int clcLinearIndex(int x, int y, int z) const {
		return x * voxel_num * voxel_num + y * voxel_num + z;
	};

	void forEach(std::function<void(int x, int y, int z)> func);
	void forEachWithCache(std::function<void(int x, int y, int z)> func, bool enableLOD);

	void setDensity(int x, int y, int z, float density);
	float getDensity(int x, int y, int z) const;

	unsigned char getRawDensityUnsafe(int x, int y, int z) const;
	unsigned short getRawMaterialUnsafe(int x, int y, int z) const;

	void setMaterial(const int x, const int y, const int z, unsigned short material);
	unsigned short getMaterial(int x, int y, int z) const;

	float size() const;
	int num() const;

	FVector voxelIndexToVector(int x, int y, int z) const;
	void vectorToVoxelIndex(const FVector& v, int& x, int& y, int& z) const;

	void setOrigin(FVector o);
	FVector getOrigin() const;

	FVector getLower() const { return lower; };
	FVector getUpper() const { return upper; };

	void getRawVoxelData(int x, int y, int z, unsigned char& density, unsigned short& material) const;
	void setVoxelPoint(int x, int y, int z, unsigned char density, unsigned short material);
	void setVoxelPointDensity(int x, int y, int z, unsigned char density);
	void setVoxelPointMaterial(int x, int y, int z, unsigned short material);

	void performSubstanceCacheNoLOD(int x, int y, int z);
	void performSubstanceCacheLOD(int x, int y, int z);

	TVoxelDataFillState getDensityFillState() const;
	//VoxelDataFillState getMaterialFillState() const; 

	void deinitializeDensity(TVoxelDataFillState density_state);
	void deinitializeMaterial(unsigned short base_mat);

	void setChanged() { last_change = FPlatformTime::Seconds(); }
	bool isChanged() { return last_change > last_save; }
	void resetLastSave() { last_save = FPlatformTime::Seconds(); }
	bool needToRegenerateMesh() { return last_change > last_mesh_generation; }
	void resetLastMeshRegenerationTime() { last_mesh_generation = FPlatformTime::Seconds(); }

	bool isSubstanceCacheValid() const { return last_change <= last_cache_check; }
	void setCacheToValid() { last_cache_check = FPlatformTime::Seconds(); }

	void clearSubstanceCache() {
		for (TSubstanceCache& lodCache : substanceCacheLOD) {
			lodCache.cellList.clear();
		}

		last_cache_check = -1;
	};

	friend void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData);
	friend void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData);

};
