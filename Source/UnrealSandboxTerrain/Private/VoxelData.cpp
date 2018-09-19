
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "VoxelData.h"

//====================================================================================
// Voxel data impl
//====================================================================================

TVoxelData::TVoxelData(int num, float size) {
	// int s = num*num*num;

	density_data = NULL;
	density_state = TVoxelDataFillState::ZERO;

	material_data = NULL;

	voxel_num = num;
	volume_size = size;
}

TVoxelData::~TVoxelData() {
	delete[] density_data;
	delete[] material_data;
}

FORCEINLINE void TVoxelData::initializeDensity() {
	int s = voxel_num * voxel_num * voxel_num;
	density_data = new unsigned char[s];
	for (auto x = 0; x < voxel_num; x++) {
		for (auto y = 0; y < voxel_num; y++) {
			for (auto z = 0; z < voxel_num; z++) {
				if (density_state == TVoxelDataFillState::ALL) {
					setDensity(x, y, z, 1);
				}

				if (density_state == TVoxelDataFillState::ZERO) {
					setDensity(x, y, z, 0);
				}
			}
		}
	}
}

FORCEINLINE void TVoxelData::initializeMaterial() {
	int s = voxel_num * voxel_num * voxel_num;
	material_data = new unsigned short[s];
	for (auto x = 0; x < voxel_num; x++) {
		for (auto y = 0; y < voxel_num; y++) {
			for (auto z = 0; z < voxel_num; z++) {
				setMaterial(x, y, z, base_fill_mat);
			}
		}
	}
}

FORCEINLINE void TVoxelData::setDensity(int x, int y, int z, float density) {
	if (density_data == NULL) {
		if (density_state == TVoxelDataFillState::ZERO && density == 0) {
			return;
		}

		if (density_state == TVoxelDataFillState::ALL && density == 1) {
			return;
		}

		initializeDensity();
		density_state = TVoxelDataFillState::MIX;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		int index = x * voxel_num * voxel_num + y * voxel_num + z;

		if (density < 0) density = 0;
		if (density > 1) density = 1;

		unsigned char d = 255 * density;

		density_data[index] = d;
	}
}

FORCEINLINE float TVoxelData::getDensity(int x, int y, int z) const {
	if (density_data == NULL) {
		if (density_state == TVoxelDataFillState::ALL) {
			return 1;
		}

		return 0;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		int index = x * voxel_num * voxel_num + y * voxel_num + z;

		float d = (float)density_data[index] / 255.0f;
		return d;
	}
	else {
		return 0;
	}
}

FORCEINLINE unsigned char TVoxelData::getRawDensityUnsafe(int x, int y, int z) const {
	auto index = x * voxel_num * voxel_num + y * voxel_num + z;
	return density_data[index];
}

FORCEINLINE unsigned short TVoxelData::getRawMaterialUnsafe(int x, int y, int z) const {
	auto index = x * voxel_num * voxel_num + y * voxel_num + z;
	return material_data[index];
}

FORCEINLINE void TVoxelData::setMaterial(const int x, const int y, const int z, const unsigned short material) {
	if (material_data == NULL) {
		initializeMaterial();
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		material_data[index] = material;
	}
}

FORCEINLINE unsigned short TVoxelData::getMaterial(int x, int y, int z) const {
	if (material_data == NULL) {
		return base_fill_mat;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		return material_data[index];
	}
	else {
		return 0;
	}
}

FORCEINLINE FVector TVoxelData::voxelIndexToVector(int x, int y, int z) const {
	static const float step = size() / (num() - 1);
	static const float s = -size() / 2;
	FVector v(s, s, s);
	FVector a(x * step, y * step, z * step);
	v = v + a;
	return v;
}

void TVoxelData::vectorToVoxelIndex(const FVector& v, int& x, int& y, int& z) const {
	static const float step = size() / (num() - 1);

	x = (int)(v.X / step) + num() / 2 - 1;
	y = (int)(v.Y / step) + num() / 2 - 1;
	z = (int)(v.Z / step) + num() / 2 - 1;
}

void TVoxelData::setOrigin(FVector o) {
	origin = o;
	lower = FVector(o.X - volume_size, o.Y - volume_size, o.Z - volume_size);
	upper = FVector(o.X + volume_size, o.Y + volume_size, o.Z + volume_size);
}

FORCEINLINE FVector TVoxelData::getOrigin() const {
	return origin;
}

FORCEINLINE float TVoxelData::size() const {
	return volume_size;
}

FORCEINLINE int TVoxelData::num() const {
	return voxel_num;
}

FORCEINLINE void TVoxelData::getRawVoxelData(int x, int y, int z, unsigned char& density, unsigned short& material) const {
	int index = x * voxel_num * voxel_num + y * voxel_num + z;

	if (density_data != NULL) {
		density = density_data[index];
	} else {
		density = 0;
	}

	if (material_data != NULL) {
		material = material_data[index];
	} else {
		material = base_fill_mat;
	}
}

FORCEINLINE void TVoxelData::setVoxelPoint(int x, int y, int z, unsigned char density, unsigned short material) {
	if (density_data == NULL) {
		initializeDensity();
		density_state = TVoxelDataFillState::MIX;
	}

	if (material_data == NULL) {
		initializeMaterial();
	}

	int index = x * voxel_num * voxel_num + y * voxel_num + z;
	material_data[index] = material;
	density_data[index] = density;
}

FORCEINLINE void TVoxelData::setVoxelPointDensity(int x, int y, int z, unsigned char density) {
	if (density_data == NULL) {
		initializeDensity();
		density_state = TVoxelDataFillState::MIX;
	}

	int index = x * voxel_num * voxel_num + y * voxel_num + z;
	density_data[index] = density;
}

FORCEINLINE void TVoxelData::setVoxelPointMaterial(int x, int y, int z, unsigned short material) {
	if (material_data == NULL) {
		initializeMaterial();
	}

	int index = x * voxel_num * voxel_num + y * voxel_num + z;
	material_data[index] = material;
}

FORCEINLINE void TVoxelData::deinitializeDensity(TVoxelDataFillState State) {
	if (State == TVoxelDataFillState::MIX) {
		return;
	}

	density_state = State;
	if (density_data != NULL) {
		delete density_data;
	}

	density_data = NULL;
}

FORCEINLINE void TVoxelData::deinitializeMaterial(unsigned short base_mat) {
	base_fill_mat = base_mat;

	if (material_data != NULL) {
		delete material_data;
	}

	material_data = NULL;
}

FORCEINLINE TVoxelDataFillState TVoxelData::getDensityFillState()	const {
	return density_state;
}

FORCEINLINE bool TVoxelData::performCellSubstanceCaching(int x, int y, int z, int lod, int step) {
	if (x <= 0 || y <= 0 || z <= 0) {
		return false;
	}

	if (x < step || y < step || z < step) {
		return false;
	}

	unsigned char density[8];
	static unsigned char raw_isolevel = 127;

	const int rx = x - step;
	const int ry = y - step;
	const int rz = z - step;

	density[0] = getRawDensityUnsafe(x, y - step, z);
	density[1] = getRawDensityUnsafe(x, y, z);
	density[2] = getRawDensityUnsafe(x - step, y - step, z);
	density[3] = getRawDensityUnsafe(x - step, y, z);
	density[4] = getRawDensityUnsafe(x, y - step, z - step);
	density[5] = getRawDensityUnsafe(x, y, z - step);
	density[6] = getRawDensityUnsafe(rx, ry, rz);
	density[7] = getRawDensityUnsafe(x - step, y, z - step);

	if (density[0] > raw_isolevel &&
		density[1] > raw_isolevel &&
		density[2] > raw_isolevel &&
		density[3] > raw_isolevel &&
		density[4] > raw_isolevel &&
		density[5] > raw_isolevel &&
		density[6] > raw_isolevel &&
		density[7] > raw_isolevel) {
		return false;
	}

	if (density[0] <= raw_isolevel &&
		density[1] <= raw_isolevel &&
		density[2] <= raw_isolevel &&
		density[3] <= raw_isolevel &&
		density[4] <= raw_isolevel &&
		density[5] <= raw_isolevel &&
		density[6] <= raw_isolevel &&
		density[7] <= raw_isolevel) {
		return false;
	}

	int index = clcLinearIndex(rx, ry, rz);
	TSubstanceCache& lodCache = substanceCacheLOD[lod];
	lodCache.cellList.push_back(index);
	return true;
}


FORCEINLINE void TVoxelData::performSubstanceCacheNoLOD(int x, int y, int z) {
	if (density_data == NULL) {
		return;
	}

	performCellSubstanceCaching(x, y, z, 0, 1);
}

FORCEINLINE void TVoxelData::performSubstanceCacheLOD(int x, int y, int z) {
	if (density_data == NULL) {
		return;
	}

	for (auto lod = 0; lod < LOD_ARRAY_SIZE; lod++) {
		int s = 1 << lod;
		if (x >= s && y >= s && z >= s) {
			if (x % s == 0 && y % s == 0 && z % s == 0) {
				performCellSubstanceCaching(x, y, z, lod, s);
			}
		}
	}
}

void TVoxelData::forEach(std::function<void(int x, int y, int z)> func) {
	for (int x = 0; x < num(); x++)
		for (int y = 0; y < num(); y++)
			for (int z = 0; z < num(); z++)
				func(x, y, z);
}

void TVoxelData::forEachWithCache(std::function<void(int x, int y, int z)> func, bool LOD) {
	clearSubstanceCache();

	for (int x = 0; x < num(); x++) {
		for (int y = 0; y < num(); y++) {
			for (int z = 0; z < num(); z++) {
				func(x, y, z);

				if (LOD) {
					performSubstanceCacheLOD(x, y, z);
				} else {
					performSubstanceCacheNoLOD(x, y, z);
				}

			}
		}
	}
}

#define DATA_END_MARKER 666999

void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData) {
	int32 num = vd.num();
	float size = vd.size();
	unsigned char volume_state = 0;

	binaryData << num;
	binaryData << size;

	// save density
	if (vd.getDensityFillState() == TVoxelDataFillState::ZERO) {
		volume_state = 0;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == TVoxelDataFillState::ALL) {
		volume_state = 1;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == TVoxelDataFillState::MIX) {
		volume_state = 2;
		binaryData << volume_state;
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density = vd.getRawDensityUnsafe(x, y, z);
					binaryData << density;
				}
			}
		}
	}

	// save material
	if (vd.material_data == NULL) {
		volume_state = 0;
	} else {
		volume_state = 2;
	}

	unsigned short base_mat = vd.base_fill_mat;

	binaryData << volume_state;
	binaryData << base_mat;

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned short matId = vd.getRawMaterialUnsafe(x, y, z);
					binaryData << matId;
				}
			}
		}
	}

	int32 end_marker = DATA_END_MARKER;
	binaryData << end_marker;
}

void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData) {
	int32 num;
	float size;
	unsigned char volume_state;
	unsigned short base_mat;

	binaryData << num;
	binaryData << size;

	// load density
	binaryData << volume_state;

	if (volume_state == 0) {
		vd.deinitializeDensity(TVoxelDataFillState::ZERO);
	}

	if (volume_state == 1) {
		vd.deinitializeDensity(TVoxelDataFillState::ALL);
	}

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density;
					binaryData << density;
					vd.setVoxelPointDensity(x, y, z, density);
					vd.performSubstanceCacheLOD(x, y, z);
				}
			}
		}
	}

	// load material
	binaryData << volume_state;
	binaryData << base_mat;
	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned short matId;
					binaryData << matId;
					vd.setVoxelPointMaterial(x, y, z, matId);
				}
			}
		}
	} else {
		vd.deinitializeMaterial(base_mat);
	}

	int32 end_marker;
	binaryData << end_marker;

	if (end_marker != DATA_END_MARKER) {
		UE_LOG(LogTemp, Warning, TEXT("Broken data! - end marker is not found"));
	}
}
