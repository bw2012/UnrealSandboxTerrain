
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "VoxelData.h"
#include "serialization.hpp"

//====================================================================================
// Voxel data impl
//====================================================================================

TVoxelData::TVoxelData() {
	density_data = nullptr;
	density_state = TVoxelDataFillState::ZERO;
	material_data = nullptr;

	voxel_num = 0;
	volume_size = 0;
}

TVoxelData::TVoxelData(int num, float size) {
	density_data = nullptr;
	density_state = TVoxelDataFillState::ZERO;
	material_data = nullptr;

	voxel_num = num;
	volume_size = size;
}

TVoxelData::~TVoxelData() {
	if (density_data != nullptr) delete[] density_data;
	if (material_data != nullptr) delete[] material_data;
}

FORCEINLINE void TVoxelData::initializeDensity() {
	const int s = voxel_num * voxel_num * voxel_num;
	density_data = new TDensityVal[s];
	for (auto x = 0; x < voxel_num; x++) {
		for (auto y = 0; y < voxel_num; y++) {
			for (auto z = 0; z < voxel_num; z++) {
				if (density_state == TVoxelDataFillState::FULL) {
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
	const int s = voxel_num * voxel_num * voxel_num;
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

		if (density_state == TVoxelDataFillState::FULL && density == 1) {
			return;
		}

		initializeDensity();
		density_state = TVoxelDataFillState::MIXED;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);

		if (density < 0) density = 0;
		if (density > 1) density = 1;

		TDensityVal d = 255 * density;

		density_data[index] = d;
	}
}

FORCEINLINE float TVoxelData::getDensity(int x, int y, int z) const {
	if (density_data == NULL) {
		if (density_state == TVoxelDataFillState::FULL) {
			return 1;
		}

		return 0;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);

		float d = (float)density_data[index] / 255.0f;
		return d;
	}
	else {
		return 0;
	}
}

FORCEINLINE TDensityVal TVoxelData::getRawDensityUnsafe(int x, int y, int z) const {
	const int index = clcLinearIndex(x, y, z);
	return density_data[index];
}

FORCEINLINE unsigned short TVoxelData::getRawMaterialUnsafe(int x, int y, int z) const {
	const int index = clcLinearIndex(x, y, z);
	return material_data[index];
}

FORCEINLINE void TVoxelData::setMaterial(const int x, const int y, const int z, const unsigned short material) {
	if (material_data == NULL) {
		initializeMaterial();
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);
		material_data[index] = material;
	}
}

FORCEINLINE unsigned short TVoxelData::getMaterial(int x, int y, int z) const {
	if (material_data == NULL) {
		return base_fill_mat;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);
		return material_data[index];
	}
	else {
		return 0;
	}
}

FORCEINLINE void TVoxelData::setNormal(int x, int y, int z, const FVector& normal) {
	if (normal_data.size() == 0) {
		normal_data.reserve(voxel_num * voxel_num * voxel_num);
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = x * voxel_num * voxel_num + y * voxel_num + z;
		normal_data[index] = normal;
	}
}

FORCEINLINE void TVoxelData::getNormal(int x, int y, int z, FVector& normal) const {
	if (normal_data.size() == 0) {
		normal.Set(0, 0, 0);
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = x * voxel_num * voxel_num + y * voxel_num + z;
		FVector tmp = normal_data[index];
		normal.Set(tmp.X, tmp.Y, tmp.Z);
	}
	else {
		normal.Set(0, 0, 0);
	}
}

FORCEINLINE FVector TVoxelData::voxelIndexToVector(int x, int y, int z) const {
	const float step = size() / (num() - 1);
	const float s = -size() / 2;
	FVector v(s, s, s);
	FVector a(x * step, y * step, z * step);
	v += a;
	return v;
}

void TVoxelData::vectorToVoxelIndex(const FVector& v, int& x, int& y, int& z) const {
	const float step = size() / (num() - 1);

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

FORCEINLINE void TVoxelData::getRawVoxelData(int x, int y, int z, TDensityVal& density, unsigned short& material) const {
	const int index = clcLinearIndex(x, y, z);

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

FORCEINLINE void TVoxelData::setVoxelPoint(int x, int y, int z, TDensityVal density, unsigned short material) {
	if (density_data == NULL) {
		initializeDensity();
		density_state = TVoxelDataFillState::MIXED;
	}

	if (material_data == NULL) {
		initializeMaterial();
	}

	const int index = clcLinearIndex(x, y, z);
	material_data[index] = material;
	density_data[index] = density;
}

FORCEINLINE void TVoxelData::setVoxelPointDensity(int x, int y, int z, TDensityVal density) {
	if (density_data == NULL) {
		initializeDensity();
		density_state = TVoxelDataFillState::MIXED;
	}

	const int index = clcLinearIndex(x, y, z);
	density_data[index] = density;
}

FORCEINLINE void TVoxelData::setVoxelPointMaterial(int x, int y, int z, unsigned short material) {
	if (material_data == NULL) {
		initializeMaterial();
	}

	const int index = clcLinearIndex(x, y, z);
	material_data[index] = material;
}

FORCEINLINE void TVoxelData::deinitializeDensity(TVoxelDataFillState State) {
	if (State == TVoxelDataFillState::MIXED) {
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

 bool TVoxelData::performCellSubstanceCaching(int x, int y, int z, int lod, int step) {
	TDensityVal density[8];
	density[7] = getRawDensityUnsafe(x, y - step, z);
	density[6] = getRawDensityUnsafe(x, y, z);
	density[5] = getRawDensityUnsafe(x - step, y - step, z);
	density[4] = getRawDensityUnsafe(x - step, y, z);
	density[3] = getRawDensityUnsafe(x, y - step, z - step);
	density[2] = getRawDensityUnsafe(x, y, z - step);
	density[1] = getRawDensityUnsafe(x - step, y - step, z - step);
	density[0] = getRawDensityUnsafe(x - step, y, z - step);

	int8 corner[8];
	for (auto i = 0; i < 8; i++) {
		corner[i] = (density[i] <= 127) ? -127 : 0;
	}

	unsigned long caseCode = ((corner[0] >> 7) & 0x01)
		| ((corner[1] >> 6) & 0x02)
		| ((corner[2] >> 5) & 0x04)
		| ((corner[3] >> 4) & 0x08)
		| ((corner[4] >> 3) & 0x10)
		| ((corner[5] >> 2) & 0x20)
		| ((corner[6] >> 1) & 0x40)
		| (corner[7] & 0x80);

	if (caseCode == 0 || caseCode == 255) return false;

	int index = clcLinearIndex(x - step, y - step, z - step);

	TSubstanceCache& lodCache = substanceCacheLOD[lod];
	TSubstanceCacheItem* cacheItm = lodCache.add2();
	//cacheItm->caseCode = caseCode;
	cacheItm->index = index;
	//cacheItm->x = x - step;
	//cacheItm->y = y - step;
	//cacheItm->z = z - step;

	/*
	TSubstanceCache& lodCache = substanceCacheLOD[lod];
	TSubstanceCacheItem cacheItm;
	cacheItm.caseCode = caseCode;
	cacheItm.index = index;
	cacheItm.x = x - step;
	cacheItm.y = y - step;
	cacheItm.z = z - step;
	lodCache.add(cacheItm);
	*/

	return true;
}


FORCEINLINE void TVoxelData::performSubstanceCacheNoLOD(int x, int y, int z) {
	if (density_data == NULL) {
		return;
	}

	performCellSubstanceCaching(x, y, z, 0, 1);
}

void TVoxelData::performSubstanceCacheLOD(int x, int y, int z) {
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

void TVoxelData::forEachCacheItem(const int lod, std::function<void(const TSubstanceCacheItem& itm)> func) {
	substanceCacheLOD[lod].forEach([=](const TSubstanceCacheItem& itm) {
		func(itm);
		//const int index = itm.index;
		//const int x = index / (vd.num() * vd.num());
		//const int y = (index / vd.num()) % vd.num();
		//const int z = index % vd.num();
	});
}

FORCEINLINE int TVoxelData::clcLinearIndex(int x, int y, int z) const {
	return x * voxel_num * voxel_num + y * voxel_num + z;
};

FORCEINLINE void TVoxelData::clcVoxelIndex(uint32 idx, uint32& x, uint32& y, uint32& z) const {
	x = idx / (voxel_num * voxel_num);
	y = (idx / voxel_num) % voxel_num;
	z = idx % voxel_num;
};

void TVoxelData::makeSubstanceCache() {
	/*
	const int s = num() * num() * num();
	for (int i = 0; i < s; i++) {
		uint32 x, y, z;
		clcVoxelIndex(i, x, y, z);
		performSubstanceCacheLOD(x, y, z);
	}
	*/
	
	for (int x = 0u; x < num(); x++) {
		for (int y = 0u; y < num(); y++) {
			for (int z = 0u; z < num(); z++) {
				performSubstanceCacheLOD(x, y, z);
			}
		}
	}
}

#define DATA_END_MARKER 0x000A2D77

bool deserializeVoxelData(TVoxelData* vd, std::vector<uint8>& data) {
	FastUnsafeDeserializer deserializer(data.data());

	TVoxelDataHeader header;
	deserializer >> header;

	vd->voxel_num = header.voxel_num;
	vd->volume_size = header.volume_size;
	vd->base_fill_mat = header.base_fill_mat;

	const size_t s = header.voxel_num * header.voxel_num * header.voxel_num;
	if (header.density_state == TVoxelDataFillState::MIXED) {
		vd->density_data = new TDensityVal[s];
		deserializer.read(vd->density_data, s);
		vd->density_state = TVoxelDataFillState::MIXED;
	} else {
		vd->deinitializeDensity(static_cast<TVoxelDataFillState>(header.density_state));
	}

	if (header.material_state == TVoxelDataFillState::MIXED) {
		vd->material_data = new TMaterialId[s];
		deserializer.read(vd->material_data, s);
	} else {
		vd->deinitializeMaterial(header.base_fill_mat);
	}

	uint32 end_marker;
	deserializer.readObj(end_marker);
	return (end_marker == DATA_END_MARKER);
}

std::shared_ptr<std::vector<uint8>> TVoxelData::serialize() {
	FastUnsafeSerializer serializer;
	const size_t s = num() * num() * num();
	const TVoxelDataFillState material_volume_state = (material_data) ? TVoxelDataFillState::MIXED : TVoxelDataFillState::ZERO;

	TVoxelDataHeader header;
	header.voxel_num = num();
	header.volume_size = size();
	header.density_state = getDensityFillState();
	header.material_state = material_volume_state;
	header.base_fill_mat = base_fill_mat;
	serializer << header;

	if (getDensityFillState() == TVoxelDataFillState::MIXED) {
		serializer.write(density_data, s);
	}

	if (material_volume_state == TVoxelDataFillState::MIXED) {
		serializer.write(material_data, s);
	}

	serializer << (uint32)DATA_END_MARKER;
	return serializer.data();
}