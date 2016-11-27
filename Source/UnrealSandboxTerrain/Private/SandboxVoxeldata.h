#ifndef __SANDBOXMOBILE_VOXELDATA_H__
#define __SANDBOXMOBILE_VOXELDATA_H__

#include "EngineMinimal.h"
#include "ProcMeshData.h"

#include <list>
#include <array>

#define LOD_ARRAY_SIZE 7

typedef struct VoxelPoint {
	unsigned char density;
	unsigned char material;
} VoxelPoint;


typedef struct VoxelCell {
	VoxelPoint point[8];
} VoxelCell;


enum VoxelDataFillState{
	ZERO, ALL, MIX
};

typedef struct SubstanceCache {
	std::list<int> cellList;
} SubstanceCache;


class VoxelData {

private:
	VoxelDataFillState density_state;
	unsigned char base_fill_mat = 0;

    int voxel_num;
    float volume_size;
	unsigned char* density_data;
	unsigned char* material_data;

	double last_change;
	double last_save;
	double last_mesh_generation;
	double last_cache_check;
	
	FVector origin = FVector(0.0f, 0.0f, 0.0f);
	void initializeDensity();
	void initializeMaterial();

	FORCEINLINE int clcLinearIndex(int x, int y, int z) const {
		return x * voxel_num * voxel_num + y * voxel_num + z;
	};

	bool performCellSubstanceCaching(int x, int y, int z, int lod, int step);

public: 
	std::array<SubstanceCache, LOD_ARRAY_SIZE> substanceCacheLOD;

    VoxelData(int, float);
    ~VoxelData();

    void setDensity(int x, int y, int z, float density);
    float getDensity(int x, int y, int z) const;

	void setMaterial(int x, int y, int z, int material);
	int getMaterial(int x, int y, int z) const;

    float size() const;
    int num() const;

	FVector voxelIndexToVector(int x, int y, int z) const;

	void setOrigin(FVector o);
	FVector getOrigin() const;

	VoxelPoint getVoxelPoint(int x, int y, int z) const;
	void setVoxelPoint(int x, int y, int z, unsigned char density, unsigned char material);
	void setVoxelPointDensity(int x, int y, int z, unsigned char density);
	void setVoxelPointMaterial(int x, int y, int z, unsigned char material);

	void performSubstanceCacheLOD(int x, int y, int z);

	VoxelDataFillState getDensityFillState() const; 
	//VoxelDataFillState getMaterialFillState() const; 

	void deinitializeDensity(VoxelDataFillState density_state);
	void deinitializeMaterial(unsigned char base_mat);

	void setChanged() { last_change = FPlatformTime::Seconds(); }
	bool isChanged() { return last_change > last_save; }
	void resetLastSave() { last_save = FPlatformTime::Seconds(); }
	bool needToRegenerateMesh() { return last_change > last_mesh_generation; }
	void resetLastMeshRegenerationTime() { last_mesh_generation = FPlatformTime::Seconds(); }

	bool isSubstanceCacheValid() const { return last_change <= last_cache_check; }
	void setCacheToValid() { last_cache_check = FPlatformTime::Seconds(); }

	void clearSubstanceCache() { 
		for (SubstanceCache& lodCache : substanceCacheLOD) {
			lodCache.cellList.clear();
		}

		last_cache_check = -1;
	};

	friend void sandboxSaveVoxelData(const VoxelData &vd, FString &fileName);
	friend bool sandboxLoadVoxelData(VoxelData &vd, FString &fileName);
};

typedef struct MeshDataSection {

	int mat_id = 0;
	FProcMeshSection MainMesh;

} MeshDataSection;


typedef struct MeshData {
	MeshData() {
		MeshDataSectionLOD.SetNum(LOD_ARRAY_SIZE); // 64
	}

	TArray<MeshDataSection> MeshDataSectionLOD;
	FProcMeshSection* CollisionMesh;

	~MeshData() {
		// for memory leaks checking
		//UE_LOG(LogTemp, Warning, TEXT("MeshData destructor"));
	}

} MeshData;

typedef std::shared_ptr<MeshData> MeshDataPtr;

typedef struct VoxelDataParam {
	bool bGenerateLOD = false;

	int lod = 0;
	float z_cut_level = 0;
	bool z_cut = false;

	FORCEINLINE int step() const {
		return 1 << lod;
	}

} VoxelDataParam;

std::shared_ptr<MeshData> sandboxVoxelGenerateMesh(const VoxelData &vd, const VoxelDataParam &vdp);

void sandboxSaveVoxelData(const VoxelData &vd, FString &fileName);
bool sandboxLoadVoxelData(VoxelData &vd, FString &fileName);

extern FVector sandboxSnapToGrid(FVector vec, float grid_range);
extern FVector sandboxConvertVectorToCubeIndex(FVector vec);
FVector sandboxGridIndex(FVector v, int range);

#endif