#ifndef __SANDBOXMOBILE_VOXELDATA_H__
#define __SANDBOXMOBILE_VOXELDATA_H__

#include "EngineMinimal.h"
#include "ProcMeshData.h"

#include <list>
#include <array>
#include <memory>
#include <set>
#include <mutex>

#define LOD_ARRAY_SIZE 7

typedef struct TVoxelPoint {
	unsigned char density;
	unsigned short material;
} TVoxelPoint;


typedef struct TVoxelCell {
	TVoxelPoint point[8];
} TVoxelCell;


enum TVoxelDataFillState{
	ZERO, ALL, MIX
};

typedef struct TSubstanceCache {
	std::list<int> cellList;
} TSubstanceCache;

enum TVoxelDataState {
	UNDEFINED, NEW_GENERATED, NEW_LOADED, NORMAL
};


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

    void setDensity(int x, int y, int z, float density);
    float getDensity(int x, int y, int z) const;
	unsigned char getRawDensity(int x, int y, int z) const;

	void setMaterial(const int x, const int y, const int z, unsigned short material);
	unsigned short getMaterial(int x, int y, int z) const;

    float size() const;
    int num() const;

	FVector voxelIndexToVector(int x, int y, int z) const;

	void setOrigin(FVector o);
	FVector getOrigin() const;

	FVector getLower() const { return lower; };
	FVector getUpper() const { return upper; };

	TVoxelPoint getVoxelPoint(int x, int y, int z) const;
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

	TVoxelDataState DataState = TVoxelDataState::UNDEFINED;

	// mesh is generated
	bool isNewGenerated() {
		return DataState == TVoxelDataState::NEW_GENERATED;
	}

	bool isNewLoaded() {
		return DataState == TVoxelDataState::NEW_LOADED;
	}

	friend void sandboxSaveVoxelData(const TVoxelData &vd, FString &fileName);
	friend bool sandboxLoadVoxelData(TVoxelData &vd, FString &fileName);

	friend void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData);
	friend void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData);

};

typedef struct TMeshMaterialSection {

	unsigned short MaterialId = 0;

	FProcMeshSection MaterialMesh;

	int32 vertexIndexCounter = 0;

} TMeshMaterialSection;


typedef struct TMeshMaterialTransitionSection : TMeshMaterialSection {

	FString TransitionName;

	std::set<unsigned short> MaterialIdSet;

	static FString GenerateTransitionName(std::set<unsigned short>& MaterialIdSet) {
		FString TransitionMaterialName = TEXT("");
		FString Separator = TEXT("");
		for (unsigned short MaterialId : MaterialIdSet) {
			TransitionMaterialName = FString::Printf(TEXT("%s%s%d"), *TransitionMaterialName, *Separator, MaterialId);
			Separator = TEXT("-");
		}

		return TransitionMaterialName;
	}

} TMeshMaterialTransitionSection;


typedef TMap<unsigned short, TMeshMaterialSection> TMaterialSectionMap;
typedef TMap<unsigned short, TMeshMaterialTransitionSection> TMaterialTransitionSectionMap;

typedef struct TMeshContainer {

	// single materials map
	TMaterialSectionMap MaterialSectionMap;

	// materials with blending
	TMaterialTransitionSectionMap MaterialTransitionSectionMap;

} TMeshContainer;

typedef struct TMeshLodSection {

	// whole mesh (collision only)
	FProcMeshSection WholeMesh; 

	// used only for render main mesh
	TMeshContainer RegularMeshContainer;

	// used for render transition 1 to 1 LOD patch mesh 
	TArray<TMeshContainer> TransitionPatchArray;		

	// just point to draw debug. remove it after release
	TArray<FVector> DebugPointList;

	TMeshLodSection() {
		TransitionPatchArray.SetNum(6);
	}

} TMeshLodSection;


typedef struct TMeshData {
	TArray<TMeshLodSection> MeshSectionLodArray;
	FProcMeshSection* CollisionMeshPtr;

	TMeshData() {
		MeshSectionLodArray.SetNum(LOD_ARRAY_SIZE); // 64
		CollisionMeshPtr = nullptr;
	}

	~TMeshData() {
		// for memory leaks checking
		//UE_LOG(LogTemp, Warning, TEXT("MeshData destructor"));
	}

} TMeshData;

typedef std::shared_ptr<TMeshData> TMeshDataPtr;

typedef struct TVoxelDataParam {
	bool bGenerateLOD = false;

	int collisionLOD = 0;

	int lod = 0;
	float z_cut_level = 0;
	bool z_cut = false;

	FORCEINLINE int step() const {
		return 1 << lod;
	}

} TVoxelDataParam;

std::shared_ptr<TMeshData> sandboxVoxelGenerateMesh(const TVoxelData &vd, const TVoxelDataParam &vdp);

void sandboxSaveVoxelData(const TVoxelData &vd, FString &fileName);
bool sandboxLoadVoxelData(TVoxelData &vd, FString &fileName);

extern FVector sandboxSnapToGrid(FVector vec, float grid_range);
extern FVector sandboxConvertVectorToCubeIndex(FVector vec);

extern FVector sandboxGridIndex(const FVector& v, int range);

#endif