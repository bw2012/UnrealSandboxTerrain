
#include "VoxelData.h"
#include "serialization.hpp"
#include <string.h> // memcpy

// mem stat
std::atomic<int> vd_counter{ 0 };

//====================================================================================
// Voxel data impl
//====================================================================================

TVoxelData::TVoxelData() {
	density_data = nullptr;
	density_state = TVoxelDataFillState::ZERO;
	material_data = nullptr;

	voxel_num = 0;
	volume_size = 0;
	vd_counter++;
}

TVoxelData::TVoxelData(int num, float size) {
	density_data = nullptr;
	density_state = TVoxelDataFillState::ZERO;
	material_data = nullptr;

	voxel_num = num;
	volume_size = size;
	vd_counter++;
}

TVoxelData::~TVoxelData() {
	if (density_data != nullptr) delete[] density_data;
	if (material_data != nullptr) delete[] material_data;
	vd_counter--;
}

void TVoxelData::initCache() {
	for (auto lod = 0; lod < LOD_ARRAY_SIZE; lod++) {
		int n = (voxel_num - 1) >> lod;
		int s = n * n * 4; //  n * n * n;
		substanceCacheLOD[lod].resize(s);
	}
}

void TVoxelData::copyDataUnsafe(const TDensityVal* src_density_data, const TMaterialId* src_material_data) {
	const int s = voxel_num * voxel_num * voxel_num;
	density_data = new TDensityVal[s];
	material_data = new TMaterialId[s];

	memcpy(density_data, src_density_data, s * sizeof(TDensityVal));
	memcpy(material_data, src_material_data, s * sizeof(TMaterialId));

	density_state = TVoxelDataFillState::MIXED;
}

void TVoxelData::copyCacheUnsafe(const int* cache_data, const int* len) {
	clearSubstanceCache();

	int offset = 0;
	for (auto lod = 0; lod < LOD_ARRAY_SIZE; lod++) {
		int n = (num() - 1) >> lod;
		int l = len[lod];
		//UE_LOG(LogVt, Log, TEXT("test ----> %d"), l);
		substanceCacheLOD[lod].copy(offset + cache_data, l);
		offset += n * n * n;
	}

	setCacheToValid();
}

void TVoxelData::initializeDensity() {
	const int s = voxel_num * voxel_num * voxel_num;
	density_data = new TDensityVal[s];
	const TDensityVal d = (density_state == TVoxelDataFillState::FULL) ? 0xff : 0x00;

	for (auto x = 0; x < voxel_num; x++) {
		for (auto y = 0; y < voxel_num; y++) {
			for (auto z = 0; z < voxel_num; z++) {
				const int index = clcLinearIndex(x, y, z);
				density_data[clcLinearIndex(x, y, z)] = d;
			}
		}
	}
}

void TVoxelData::initializeMaterial() {
	const int s = voxel_num * voxel_num * voxel_num;
	material_data = new TMaterialId[s];

	for (auto x = 0; x < voxel_num; x++) {
		for (auto y = 0; y < voxel_num; y++) {
			for (auto z = 0; z < voxel_num; z++) {
				material_data[clcLinearIndex(x, y, z)] = base_fill_mat;
			}
		}
	}
}

TDensityVal TVoxelData::clcFloatToByte(float v) {
	TRIM_FLOAT_VAL(v);
	return FLOAT_TO_BYTE(v); 
}

float TVoxelData::clcByteToFloat(TDensityVal v) {
	return (float)v / 255.0f; //TODO separate function
}

void TVoxelData::setDensity(int x, int y, int z, float density) {
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

		density_data[index] = clcFloatToByte(density);
	}
}

void TVoxelData::setDensityAndMaterial(const TVoxelIndex& vi, float density, TMaterialId materialId) {
	const int index = clcLinearIndex(vi.X, vi.Y, vi.Z);
	density_state = TVoxelDataFillState::MIXED;
	density_data[index] = clcFloatToByte(density);
	material_data[index] = materialId;
}

float TVoxelData::getDensity(int x, int y, int z) const {
	if (density_data == NULL) {
		if (density_state == TVoxelDataFillState::FULL) {
			return 1;
		}

		return 0;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);
		return clcByteToFloat(density_data[index]);
	} else {
		return 0;
	}
}

void TVoxelData::setDensity(const TVoxelIndex& vi, float density) {
	setDensity(vi.X, vi.Y, vi.Z, density);
}

float TVoxelData::getDensity(const TVoxelIndex& vi) const {
	return getDensity(vi.X, vi.Y, vi.Z);
}

FORCEINLINE TDensityVal TVoxelData::getRawDensityUnsafe(int x, int y, int z) const {
	const int index = clcLinearIndex(x, y, z);
	return density_data[index];
}

FORCEINLINE unsigned short TVoxelData::getRawMaterialUnsafe(int x, int y, int z) const {
	const int index = clcLinearIndex(x, y, z);
	return material_data[index];
}

void TVoxelData::setMaterial(const int x, const int y, const int z, const unsigned short material) {
	if (material_data == NULL) {
		initializeMaterial();
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);
		material_data[index] = material;
	}
}

unsigned short TVoxelData::getMaterial(int x, int y, int z) const {
	if (material_data == NULL) {
		return base_fill_mat;
	}

	if (x < voxel_num && y < voxel_num && z < voxel_num) {
		const int index = clcLinearIndex(x, y, z);
		auto mat_id = material_data[index];
		if (mat_id == 0) {
			return base_fill_mat;
		}

		return mat_id;
	} else {
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
	} else {
		normal.Set(0, 0, 0);
	}
}

FVector TVoxelData::voxelIndexToVector(TVoxelIndex Idx) const {
	return voxelIndexToVector(Idx.X, Idx.Y, Idx.Z);
}

FVector TVoxelData::voxelIndexToVector(int x, int y, int z) const {
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

void TVoxelData::setOrigin(const FVector& o) {
	origin = o;
	lower = FVector(o.X - volume_size, o.Y - volume_size, o.Z - volume_size);
	upper = FVector(o.X + volume_size, o.Y + volume_size, o.Z + volume_size);
}

const FVector& TVoxelData::getOrigin() const {
	return origin;
}

FORCEINLINE float TVoxelData::size() const {
	return volume_size;
}

int TVoxelData::num() const {
	return voxel_num;
}

void TVoxelData::deinitializeDensity(TVoxelDataFillState State) {
	if (State == TVoxelDataFillState::MIXED) {
		return;
	}

	density_state = State;
	if (density_data != NULL) {
		delete density_data;
	}

	density_data = NULL;
}

void TVoxelData::deinitializeMaterial(unsigned short base_mat) {
	base_fill_mat = base_mat;

	if (material_data != NULL) {
		delete material_data;
	}

	material_data = NULL;
}

TVoxelDataFillState TVoxelData::getDensityFillState()	const {
	return density_state;
}

unsigned long TVoxelData::getCaseCode(int x, int y, int z, int step) const {
	TVoxelIndex d[8];
	int8 corner[8];

	vd::tools::makeIndexes(d, x, y, z, step);
	for (auto i = 0; i < 8; i++) {
		corner[7 - i] = (density_data[clcLinearIndex(d[i])] <= 127) ? -127 : 0;
	}

	return vd::tools::caseCode(corner);
}

bool TVoxelData::performCellSubstanceCaching(int x, int y, int z, int lod, int step) {
	unsigned long caseCode = getCaseCode(x, y, z, -step);
	if (caseCode == 0x0 || caseCode == 0xff) {
		return false;
	} else {
		TSubstanceCache& lodCache = substanceCacheLOD[lod];
		TSubstanceCacheItem* cacheItm = lodCache.emplace();
		cacheItm->index = clcLinearIndex(x - step, y - step, z - step);
		return true;
	}
}

FORCEINLINE void TVoxelData::performSubstanceCacheNoLOD(int x, int y, int z) {
	if (density_data == NULL) {
		return;
	}

	performCellSubstanceCaching(x, y, z, 0, 1);
}

void TVoxelData::performSubstanceCacheLOD(int x, int y, int z, int initial_lod) {
	if (density_data == NULL) {
		return;
	}

	for (auto lod = initial_lod; lod < LOD_ARRAY_SIZE; lod++) {
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
	initCache();

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

void TVoxelData::forEachCacheItem(const int lod, std::function<void(const TSubstanceCacheItem& itm)> func) const{
	substanceCacheLOD[lod].forEach([=](const TSubstanceCacheItem& itm) {
		func(itm);
	});
}

FORCEINLINE int TVoxelData::clcLinearIndex(int x, int y, int z) const {
	//return x * voxel_num * voxel_num + y * voxel_num + z;
	return vd::tools::clcLinearIndex(voxel_num, x, y, z);
};

FORCEINLINE int TVoxelData::clcLinearIndex(const TVoxelIndex& v) const {
	return vd::tools::clcLinearIndex(voxel_num, v.X, v.Y, v.Z);
};

void TVoxelData::clcVoxelIndex(uint32 idx, uint32& x, uint32& y, uint32& z) const {
	x = idx / (voxel_num * voxel_num);
	y = (idx / voxel_num) % voxel_num;
	z = idx % voxel_num;
};

void TVoxelData::makeSubstanceCache() {
	clearSubstanceCache();
	initCache();
	
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
	usbt::TFastUnsafeDeserializer deserializer(data.data());

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
	usbt::TFastUnsafeSerializer serializer;
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

TMaterialId TVoxelData::getBaseMatId() {
	return base_fill_mat;
}

void TVoxelData::setBaseMatId(TMaterialId base_mat_id) {
	base_fill_mat = base_mat_id;
}

bool TVoxelData::isSubstanceCacheValid() const {
	return cache_state >= 0;
}

void TVoxelData::setCacheToValid() {
	cache_state = 0;
}

void TVoxelData::clearSubstanceCache() {
	for (TSubstanceCache& lodCache : substanceCacheLOD) {
		lodCache.clear();
	}

	cache_state = -1;
};

TSubstanceCache::TSubstanceCache() {
	// FIXME
	//cellArray.resize(65 * 65 * 65);
	//cellArray.resize(16 * 16 * 16);
}

TSubstanceCacheItem* TSubstanceCache::emplace() {
	auto s = cellArray.size();
	if (s == idx) {
		cellArray.resize(s + s / 2 + 1);
	}

	TSubstanceCacheItem* res = &cellArray.data()[idx];
	idx++;
	return res;
}

void TSubstanceCache::resize(uint32 s) {
	if (s > 1) {
		cellArray.resize(s);
	} else {
		cellArray.resize(2);
	}
}

void TSubstanceCache::clear() {
	//cellList.clear();
	idx = 0;
}

void TSubstanceCache::forEach(std::function<void(const TSubstanceCacheItem& itm)> func) const {
	//for (const auto& itm : cellList) {
	for (int i = 0; i < idx; i++) {
		func(cellArray[i]);
	}
}

void TSubstanceCache::copy(const int* cache_data, const int len) {
	cellArray.resize(len);
	memcpy(cellArray.data(), cache_data, len * sizeof(TSubstanceCacheItem));
	idx = len;
}

int32 TSubstanceCache::size() const {
	return idx;
}

const TSubstanceCacheItem& TSubstanceCache::operator[](std::size_t index) const {
	return cellArray[index];
}


void vd::tools::unsafe::forceAddToCache(TVoxelData* vd, int x, int y, int z, int lod) {
	auto const index = vd->clcLinearIndex(x, y, z);
	TSubstanceCache& lodCache = vd->substanceCacheLOD[lod];
	TSubstanceCacheItem* cacheItm = lodCache.emplace();
	cacheItm->index = index;
}

void vd::tools::unsafe::setDensity(TVoxelData* vd, const TVoxelIndex& vi, float density) {
	const int index = vd::tools::clcLinearIndex(vd->num(), vi);
	vd->density_state = TVoxelDataFillState::MIXED;
	vd->density_data[index] = vd->clcFloatToByte(density);
}

void vd::tools::makeIndexes(TVoxelIndex(&d)[8], int x, int y, int z, int step) {
	d[0] = TVoxelIndex(x, y + step, z);
	d[1] = TVoxelIndex(x, y, z);
	d[2] = TVoxelIndex(x + step, y + step, z);
	d[3] = TVoxelIndex(x + step, y, z);
	d[4] = TVoxelIndex(x, y + step, z + step);
	d[5] = TVoxelIndex(x, y, z + step);
	d[6] = TVoxelIndex(x + step, y + step, z + step);
	d[7] = TVoxelIndex(x + step, y, z + step);
}

void vd::tools::makeIndexes(TVoxelIndex(&d)[8], const TVoxelIndex& vi, int step) {
	const int x = vi.X;
	const int y = vi.Y;
	const int z = vi.Z;
	vd::tools::makeIndexes(d, x, y, z, step);
}

unsigned long vd::tools::caseCode(const int8 (&corner)[8]) {
	unsigned long caseCode = ((corner[0] >> 7) & 0x01)
		| ((corner[1] >> 6) & 0x02)
		| ((corner[2] >> 5) & 0x04)
		| ((corner[3] >> 4) & 0x08)
		| ((corner[4] >> 3) & 0x10)
		| ((corner[5] >> 2) & 0x20)
		| ((corner[6] >> 1) & 0x40)
		| (corner[7] & 0x80);

	return caseCode;
}

int vd::tools::clcLinearIndex(int n,  int x, int y, int z) {
	return x * n * n + y * n + z;
};

int vd::tools::clcLinearIndex(int n, const TVoxelIndex& vi) {
	return vd::tools::clcLinearIndex(n, vi.X, vi.Y, vi.Z);
};


int vd::tools::memory::getVdCount() {
	return vd_counter;
};

size_t vd::tools::getCacheSize(const TVoxelData* vd, int lod) {
	return vd->substanceCacheLOD[lod].size();
};

const TSubstanceCacheItem& vd::tools::getCacheItmByNumber(const TVoxelData* vd, int lod, int number) {
	return vd->substanceCacheLOD[lod][number];
};
