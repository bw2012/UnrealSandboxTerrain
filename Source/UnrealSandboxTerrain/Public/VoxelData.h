#pragma once

#include "EngineMinimal.h"

#include "UnrealSandboxTerrain.h"
#include "VoxelIndex.h"
#include <list>
#include <array>
#include <memory>
#include <set>
#include <mutex>
#include <functional>
#include <vector>

#define TRIM_FLOAT_VAL(V) if (V < 0.f) V = 0.f; if (V > 1.f) V = 1.f; 

#define FLOAT_TO_BYTE(V) (255 * V) 

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

	int32 size() const;

	const TSubstanceCacheItem& operator[](std::size_t idx) const;

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

namespace vd {
	namespace tools {
		namespace memory {
			int getVdCount();
		}

		void makeIndexes(TVoxelIndex(&d)[8], int x, int y, int z, int step);
		void makeIndexes(TVoxelIndex(&d)[8], const TVoxelIndex& vi, int step);
		unsigned long caseCode(const int8(&corner)[8]);
		int clcLinearIndex(int n, int x, int y, int z);
		int clcLinearIndex(int n, const TVoxelIndex& vi);
		size_t getCacheSize(const TVoxelData* vd, int lod);
		const TSubstanceCacheItem& getCacheItmByNumber(const TVoxelData* vd, int lod, int number);

		namespace unsafe {
			void forceAddToCache(TVoxelData* vd, int x, int y, int z, int lod);
			void setDensity(TVoxelData* vd, const TVoxelIndex& vi, float density);
		}
	}
};

class UNREALSANDBOXTERRAIN_API TVoxelData {

	friend void vd::tools::unsafe::forceAddToCache(TVoxelData*, int, int, int, int);
	friend void vd::tools::unsafe::setDensity(TVoxelData*, const TVoxelIndex&, float);
	friend size_t vd::tools::getCacheSize(const TVoxelData*, int);
	friend const TSubstanceCacheItem& vd::tools::getCacheItmByNumber(const TVoxelData*, int, int);

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

	std::array<TSubstanceCache, LOD_ARRAY_SIZE> substanceCacheLOD;

	bool performCellSubstanceCaching(int x, int y, int z, int lod, int step);

public:

	TVoxelData();
	TVoxelData(int, float);
	~TVoxelData();

	void initCache();

	void copyDataUnsafe(const TDensityVal* density_data, const TMaterialId* material_data);
	void copyCacheUnsafe(const int* cache_data, const int* len);

	void initializeDensity();
	void initializeMaterial();

	TMaterialId getBaseMatId();
	void setBaseMatId(TMaterialId base_mat_id);

	int clcLinearIndex(const TVoxelIndex& v) const;
	int clcLinearIndex(int x, int y, int z) const;
	void clcVoxelIndex(uint32 idx, uint32& x, uint32& y, uint32& z) const;

	static TDensityVal clcFloatToByte(float v);
	static float clcByteToFloat(TDensityVal v);

	void forEach(std::function<void(int x, int y, int z)> func);
	void forEachWithCache(std::function<void(int x, int y, int z)> func, bool enableLOD);
	void forEachCacheItem(const int lod, std::function<void(const TSubstanceCacheItem& itm)> func) const;

	void setDensity(int x, int y, int z, float density);
	float getDensity(int x, int y, int z) const;

	void setDensity(const TVoxelIndex& vi, float density);
	float getDensity(const TVoxelIndex& vi) const;

	void setDensityAndMaterial(const TVoxelIndex& vi, float density, TMaterialId materialId);

	TDensityVal getRawDensityUnsafe(int x, int y, int z) const;
	unsigned short getRawMaterialUnsafe(int x, int y, int z) const;

	void setMaterial(const int x, const int y, const int z, unsigned short material);
	unsigned short getMaterial(int x, int y, int z) const;

	void setNormal(int x, int y, int z, const FVector& normal);
	void getNormal(int x, int y, int z, FVector& normal) const;

	float size() const;
	int num() const;

	FVector voxelIndexToVector(TVoxelIndex Idx) const;
	FVector voxelIndexToVector(int x, int y, int z) const;
	void vectorToVoxelIndex(const FVector& v, int& x, int& y, int& z) const;

	void setOrigin(const FVector& o);
	const FVector& getOrigin() const;
	void getOrigin(FVector& o) const {
		o = origin;
	};

	FVector getLower() const { return lower; };
	FVector getUpper() const { return upper; };

	void performSubstanceCacheNoLOD(int x, int y, int z);
	void performSubstanceCacheLOD(int x, int y, int z, int initial_lod = 0);

	TVoxelDataFillState getDensityFillState() const;

	void deinitializeDensity(TVoxelDataFillState density_state);
	void deinitializeMaterial(unsigned short base_mat);

	bool isSubstanceCacheValid() const;
	void setCacheToValid();
	void makeSubstanceCache();
	void clearSubstanceCache();

	unsigned long getCaseCode(int x, int y, int z, int step) const;

	std::shared_ptr<std::vector<uint8>> serialize();

	friend bool deserializeVoxelData(TVoxelData* vd, std::vector<uint8>& data);
};