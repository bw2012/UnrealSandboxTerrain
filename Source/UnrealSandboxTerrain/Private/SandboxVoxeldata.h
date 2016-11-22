#ifndef __SANDBOXMOBILE_VOXELDATA_H__
#define __SANDBOXMOBILE_VOXELDATA_H__

#include "EngineMinimal.h"
#include "ProcMeshData.h"

struct VoxelPoint {
	unsigned char density;
	unsigned char material;
};
 
enum VoxelDataFillState{
	ZERO, ALL, MIX
};



class VoxelData{

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
	
	FVector origin = FVector(0.0f, 0.0f, 0.0f);
	void initializeDensity();
	void initializeMaterial();

public: 
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

	VoxelDataFillState getDensityFillState() const; 
	//VoxelDataFillState getMaterialFillState() const; 

	void deinitializeDensity(VoxelDataFillState density_state);
	void deinitializeMaterial(unsigned char base_mat);

	void setChanged() { last_change = FPlatformTime::Seconds(); }
	bool isChanged() { return last_change > last_save; }
	void resetLastSave() { last_save = FPlatformTime::Seconds(); }
	bool needToRegenerateMesh() { return last_change > last_mesh_generation; }
	void resetLastMeshRegenerationTime() { last_mesh_generation = FPlatformTime::Seconds(); }

	friend void sandboxSaveVoxelData(const VoxelData &vd, FString &fileName);
	friend bool sandboxLoadVoxelData(VoxelData &vd, FString &fileName);
};

typedef struct MeshDataSection {

	int mat_id = 0;
	FProcMeshSection MainMesh;

} MeshDataSection;


typedef struct MeshData {
	MeshData() {
		MeshDataSectionLOD.SetNum(7); // 64
	}

	TArray<MeshDataSection> MeshDataSectionLOD;
	FProcMeshSection* CollisionMesh;

	~MeshData() {
		UE_LOG(LogTemp, Warning, TEXT("MeshData destructor"));
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