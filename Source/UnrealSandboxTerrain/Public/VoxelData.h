#pragma once

#include "EngineMinimal.h"

#include <list>
#include <array>
#include <memory>
#include <set>
#include <mutex>
#include <functional>
#include <vector>

#define LOD_ARRAY_SIZE 7

typedef unsigned char TDensityVal;
typedef unsigned short TMaterialId;

// density or material state
enum TVoxelDataFillState : uint8 {
	ZERO = 0,		// data contains only zero values
	FULL = 1,		// data contains only one same value
	MIXED = 2		// mixed state, any value in any point
};



typedef struct TSubstanceCacheItem {
	uint32 index = 0;
	//unsigned long caseCode = 0;
	//uint32 x = 0;
	//uint32 y = 0;
	//uint32 z = 0;
} TSubstanceCacheItem;

typedef class TSubstanceCache {

private:
	//std::list<TSubstanceCacheItem> cellList;
	std::vector<TSubstanceCacheItem> cellArray;
	int32 idx = 0;

public:

	TSubstanceCache();

	TSubstanceCacheItem* emplace();
	void resize(uint32 s);
	void clear();
	void forEach(std::function<void(const TSubstanceCacheItem& itm)> func) const;
	void copy(const int* cache_data, const int len);

} TSubstanceCache;



// POD structure. used in fast serialization
typedef struct TVoxelDataHeader {
	uint32 voxel_num;
	float volume_size;
	uint8 density_state;
	uint8 material_state;
	TMaterialId base_fill_mat;
} TVoxelDataHeader;



class TVoxelData;
typedef std::shared_ptr<TVoxelData> TVoxelDataPtr;

class UNREALSANDBOXTERRAIN_API TVoxelData {

private:
	TVoxelDataFillState density_state;
	unsigned short base_fill_mat = 0;

	int voxel_num;
	float volume_size;
	TDensityVal* density_data;
	TMaterialId* material_data;
	std::vector<FVector> normal_data;

	volatile int cache_state = -1;

	FVector origin = FVector(0.0f, 0.0f, 0.0f);
	FVector lower = FVector(0.0f, 0.0f, 0.0f);
	FVector upper = FVector(0.0f, 0.0f, 0.0f);

	void initializeDensity();
	void initializeMaterial();

	std::array<TSubstanceCache, LOD_ARRAY_SIZE> substanceCacheLOD;

	bool performCellSubstanceCaching(int x, int y, int z, int lod, int step);

public:

	TVoxelData();
	TVoxelData(int, float);
	~TVoxelData();

	void initCache();

	void copyDataUnsafe(const TDensityVal* density_data, const TMaterialId* material_data);
	void copyCacheUnsafe(const int* cache_data, const int* len);

	FORCEINLINE int clcLinearIndex(int x, int y, int z) const;
	FORCEINLINE void clcVoxelIndex(uint32 idx, uint32& x, uint32& y, uint32& z) const;

	void forEach(std::function<void(int x, int y, int z)> func);
	void forEachWithCache(std::function<void(int x, int y, int z)> func, bool enableLOD);
	void forEachCacheItem(const int lod, std::function<void(const TSubstanceCacheItem& itm)> func) const;

	void setDensity(int x, int y, int z, float density);
	float getDensity(int x, int y, int z) const;

	TDensityVal getRawDensityUnsafe(int x, int y, int z) const;
	unsigned short getRawMaterialUnsafe(int x, int y, int z) const;

	void setMaterial(const int x, const int y, const int z, unsigned short material);
	unsigned short getMaterial(int x, int y, int z) const;

	void setNormal(int x, int y, int z, const FVector& normal);
	void getNormal(int x, int y, int z, FVector& normal) const;

	float size() const;
	int num() const;

	FVector voxelIndexToVector(int x, int y, int z) const;
	void vectorToVoxelIndex(const FVector& v, int& x, int& y, int& z) const;

	void setOrigin(FVector o);
	FVector getOrigin() const;

	FVector getLower() const { return lower; };
	FVector getUpper() const { return upper; };

	void getRawVoxelData(int x, int y, int z, TDensityVal& density, unsigned short& material) const;
	void setVoxelPoint(int x, int y, int z, TDensityVal density, unsigned short material);
	void setVoxelPointDensity(int x, int y, int z, TDensityVal density);
	void setVoxelPointMaterial(int x, int y, int z, unsigned short material);

	void performSubstanceCacheNoLOD(int x, int y, int z);
	void performSubstanceCacheLOD(int x, int y, int z);

	TVoxelDataFillState getDensityFillState() const;
	//VoxelDataFillState getMaterialFillState() const; 

	void deinitializeDensity(TVoxelDataFillState density_state);
	void deinitializeMaterial(unsigned short base_mat);

	bool isSubstanceCacheValid() const;
	void setCacheToValid();
	void makeSubstanceCache();
	void clearSubstanceCache();

	std::shared_ptr<std::vector<uint8>> serialize();

	friend void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData);
	friend void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData);
	friend void deserializeVoxelDataFast(TVoxelData* vd, TArray<uint8>& Data, bool createSubstanceCache);

	friend bool deserializeVoxelData(TVoxelData* vd, std::vector<uint8>& data);
};


//bool deserializeVoxelData(TVoxelData* vd, std::vector<uint8>& data);