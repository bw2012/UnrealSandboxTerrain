
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxVoxeldata.h"

#include "Transvoxel.h"

#include <cmath>
#include <vector>
#include <mutex>


//====================================================================================
// Voxel data impl
//====================================================================================

    VoxelData::VoxelData(int num, float size){
       // int s = num*num*num;

        density_data = NULL;
		density_state = VoxelDataFillState::ZERO;

		material_data = NULL;

		voxel_num = num;
        volume_size = size;
    }

    VoxelData::~VoxelData(){
        delete[] density_data;
		delete[] material_data;
    }

	void VoxelData::initializeDensity() {
		int s = voxel_num * voxel_num * voxel_num;
		density_data = new unsigned char[s];
		for (auto x = 0; x < voxel_num; x++) {
			for (auto y = 0; y < voxel_num; y++) {
				for (auto z = 0; z < voxel_num; z++) {
					if (density_state == VoxelDataFillState::ALL) {
						setDensity(x, y, z, 1);
					}

					if (density_state == VoxelDataFillState::ZERO) {
						setDensity(x, y, z, 0);
					}
				}
			}
		}
	}

	void VoxelData::initializeMaterial() {
		int s = voxel_num * voxel_num * voxel_num;
		material_data = new unsigned char[s];
		for (auto x = 0; x < voxel_num; x++) {
			for (auto y = 0; y < voxel_num; y++) {
				for (auto z = 0; z < voxel_num; z++) {
					setMaterial(x, y, z, base_fill_mat);
				}
			}
		}
	}

    void VoxelData::setDensity(int x, int y, int z, float density){
		if (density_data == NULL) {
			if (density_state == VoxelDataFillState::ZERO && density == 0){
				return;
			}

			if (density_state == VoxelDataFillState::ALL && density == 1) {
				return;
			}

			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

        if(x < voxel_num && y < voxel_num && z < voxel_num){
            int index = x * voxel_num * voxel_num + y * voxel_num + z;	

			if (density < 0) density = 0;
			if (density > 1) density = 1;

			unsigned char d = 255 * density;

			density_data[index] = d;
        }
    }

    float VoxelData::getDensity(int x, int y, int z) const {
		if (density_data == NULL) {
			if (density_state == VoxelDataFillState::ALL) {
				return 1;
			}

			return 0;
		}

        if(x < voxel_num && y < voxel_num && z < voxel_num){
			int index = x * voxel_num * voxel_num + y * voxel_num + z;

			float d = (float)density_data[index] / 255.0f;
            return d;
        } else {
            return 0;
        }
    }

	void VoxelData::setMaterial(int x, int y, int z, int material) {
		if (material_data == NULL) {
			initializeMaterial();
		}

		if (x < voxel_num && y < voxel_num && z < voxel_num) {
			int index = x * voxel_num * voxel_num + y * voxel_num + z;
			material_data[index] = material;
		}
	}

	int VoxelData::getMaterial(int x, int y, int z) const {
		if (material_data == NULL) {
			return base_fill_mat;
		}

		if (x < voxel_num && y < voxel_num && z < voxel_num) {
			int index = x * voxel_num * voxel_num + y * voxel_num + z;
			return material_data[index];
		} else {
			return 0;
		}
	}

	FVector VoxelData::voxelIndexToVector(int x, int y, int z) const {
		float step = size() / (num() - 1);
		float s = -size() / 2;
		FVector v(s, s, s);
		FVector a(x * step, y * step, z * step);
		v = v + a;
		return v;
	}

	void VoxelData::setOrigin(FVector o) {
		origin = o;
	}

	FVector VoxelData::getOrigin() const {
		return origin;
	}

    float VoxelData::size() const {
        return volume_size;
    }
    
    int VoxelData::num() const {
        return voxel_num;
    }

	VoxelPoint VoxelData::getVoxelPoint(int x, int y, int z) const {
		VoxelPoint vp;
		int index = x * voxel_num * voxel_num + y * voxel_num + z;

		vp.material = base_fill_mat;
		vp.density = 0;

		if (density_data != NULL) {
			vp.density = density_data[index];
		}

		if (material_data != NULL) {
			vp.material = material_data[index];
		}

		return vp;
	}

	void VoxelData::setVoxelPoint(int x, int y, int z, unsigned char density, unsigned char material) {
		if (density_data == NULL) {
			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

		if (material_data == NULL) {
			initializeMaterial();
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		material_data[index] = material;
		density_data[index] = density;
	}

	void VoxelData::setVoxelPointDensity(int x, int y, int z, unsigned char density) {
		if (density_data == NULL) {
			initializeDensity();
			density_state = VoxelDataFillState::MIX;
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		density_data[index] = density;
	}

	void VoxelData::setVoxelPointMaterial(int x, int y, int z, unsigned char material) {
		if (material_data == NULL) {
			initializeMaterial();
		}

		int index = x * voxel_num * voxel_num + y * voxel_num + z;
		material_data[index] = material;
	}

	void VoxelData::deinitializeDensity(VoxelDataFillState state) {
		if (state == VoxelDataFillState::MIX) {
			return;
		}

		density_state = state;
		if (density_data != NULL) {
			delete density_data;
		}

		density_data = NULL;
	}

	void VoxelData::deinitializeMaterial(unsigned char base_mat) {
		base_fill_mat = base_mat;

		if (material_data != NULL) {
			delete material_data;
		}

		material_data = NULL;
	}

	VoxelDataFillState VoxelData::getDensityFillState()	const {
		return density_state;
	}


	//====================================================================================
	
static FORCEINLINE FVector clcNormal(FVector &p1, FVector &p2, FVector &p3) {
    float A = p1.Y * (p2.Z - p3.Z) + p2.Y * (p3.Z - p1.Z) + p3.Y * (p1.Z - p2.Z);
    float B = p1.Z * (p2.X - p3.X) + p2.Z * (p3.X - p1.X) + p3.Z * (p1.X - p2.X);
    float C = p1.X * (p2.Y - p3.Y) + p2.X * (p3.Y - p1.Y) + p3.X * (p1.Y - p2.Y);
    // float D = -(p1.x * (p2.y * p3.z - p3.y * p2.z) + p2.x * (p3.y * p1.z - p1.y * p3.z) + p3.x * (p1.y * p2.z - p2.y
    // * p1.z));

    FVector n(A, B, C);
    n.Normalize();
    return n;
}

//#define FORCEINLINE FORCENOINLINE  

class VoxelMeshExtractor {
public:
	int ntriang = 0;
	int vertex_index = 0;

	MeshDataElement &mesh_data;
	const VoxelData &voxel_data;
	const VoxelDataParam voxel_data_param;
	TMap<FVector, int> VertexMap;

	VoxelMeshExtractor(MeshDataElement &a, const VoxelData &b, const VoxelDataParam c) : mesh_data(a), voxel_data(b), voxel_data_param(c) { }
    
private:
	double isolevel = 0.5f;

	struct Point {
		float density;
		int material_id;
	};

	struct TmpPoint {
		FVector v;
		int mat_id;
		float mat_weight=0;
	};

	FORCEINLINE Point getVoxelpoint(int x, int y, int z) {
		Point vp;
		vp.density = getDensity(x, y, z);
		vp.material_id = getMaterial(x, y, z);
		return vp;
	}

	FORCEINLINE float getDensity(int x, int y, int z) {
		int step = voxel_data_param.step();
		if (voxel_data_param.z_cut) {
			FVector p = voxel_data.voxelIndexToVector(x, y, z);
			p += voxel_data.getOrigin();
			if (p.Z > voxel_data_param.z_cut_level) {
				return 0;
			}
		}

		return voxel_data.getDensity(x, y, z);
	}

	FORCEINLINE int getMaterial(int x, int y, int z) {
		return voxel_data.getMaterial(x, y, z);
	}

	FORCEINLINE FVector vertexInterpolation(FVector p1, FVector p2, float valp1, float valp2) {
		if (std::abs(isolevel - valp1) < 0.00001) {
			return p1;
		}

		if (std::abs(isolevel - valp2) < 0.00001) {
			return p2;
		}

		if (std::abs(valp1 - valp2) < 0.00001) {
			return p1;
		}

		if (valp1 == valp2) {
			return p1;
		}

		float mu = (isolevel - valp1) / (valp2 - valp1);

		FVector tmp;
		tmp.X = p1.X + mu * (p2.X - p1.X);
		tmp.Y = p1.Y + mu * (p2.Y - p1.Y);
		tmp.Z = p1.Z + mu * (p2.Z - p1.Z);

		return tmp;
	}

	FORCEINLINE void materialCalculation(struct TmpPoint& tp, FVector p1, FVector p2, FVector p, int mat1, int mat2) {
		int base_mat = 1;

		if ((base_mat == mat1)&&(base_mat == mat2)) {
			tp.mat_id = base_mat;
			tp.mat_weight = 0;
			return;
		}

		if (mat1 == mat2) {
			tp.mat_id = base_mat;
			tp.mat_weight = 1;
			return;
		}

		FVector tmp(p1);
		tmp -= p2;
		float s = tmp.Size();

		p1 -= p;
		p2 -= p;

		float s1 = p1.Size();
		float s2 = p2.Size();

		if (mat1 != base_mat) {
			tp.mat_weight = s1 / s;
			return;
		}

		if (mat2 != base_mat) {
			tp.mat_weight = s2 / s;
			return;
		}
	}

	FORCEINLINE TmpPoint vertexClc(FVector p1, FVector p2, Point valp1, Point valp2) {
		struct TmpPoint ret;

		ret.v = vertexInterpolation(p1, p2, valp1.density, valp2.density);
		materialCalculation(ret, p1, p2, ret.v, valp1.material_id, valp2.material_id);
		return ret;
	}

	FORCEINLINE void addVertexTest(TmpPoint &point, FVector n, int &index) {
		FVector v = point.v;

		mesh_data.MeshSectionLOD[voxel_data_param.lod].ProcIndexBuffer.Add(index);

		int t = point.mat_weight * 255;

		FProcMeshVertex Vertex;
		Vertex.Position = v;
		Vertex.Normal = n;
		Vertex.UV0 = FVector2D(0.f, 0.f);
		Vertex.Color = FColor(t, 0, 0, 0);
		Vertex.Tangent = FProcMeshTangent();

		mesh_data.MeshSectionLOD[voxel_data_param.lod].SectionLocalBox += Vertex.Position;
		mesh_data.MeshSectionLOD[voxel_data_param.lod].ProcVertexBuffer.Add(Vertex);

		vertex_index++;
	}
   
    FORCEINLINE void addVertex(TmpPoint &point, FVector n, int &index){
		FVector v = point.v;

        if(VertexMap.Contains(v)){
            int vindex = VertexMap[v];
            
			FProcMeshSection& mesh = mesh_data.MeshSectionLOD[voxel_data_param.lod];
			FProcMeshVertex& Vertex = mesh.ProcVertexBuffer[vindex];
			FVector nvert = Vertex.Normal;

            FVector tmp(nvert);
            tmp += n;
            tmp /= 2;

			Vertex.Normal = tmp;
			mesh_data.MeshSectionLOD[voxel_data_param.lod].ProcIndexBuffer.Add(vindex);

        } else {
			mesh_data.MeshSectionLOD[voxel_data_param.lod].ProcIndexBuffer.Add(index);

			int t = point.mat_weight * 255;

			FProcMeshVertex Vertex;
			Vertex.Position = v;
			Vertex.Normal = n;
			Vertex.UV0 = FVector2D(0.f, 0.f);
			Vertex.Color = FColor(t, 0, 0, 0);
			Vertex.Tangent = FProcMeshTangent();

			mesh_data.MeshSectionLOD[voxel_data_param.lod].SectionLocalBox += Vertex.Position;

			mesh_data.MeshSectionLOD[voxel_data_param.lod].ProcVertexBuffer.Add(Vertex);
        
			VertexMap.Add(v, index);
            vertex_index++;  
        }
    }

    FORCEINLINE void handleTriangle(TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
        //vp1 = vp1 + voxel_data.getOrigin();
        //vp2 = vp2 + voxel_data.getOrigin();
        //vp3 = vp3 + voxel_data.getOrigin();
        
		FVector n = clcNormal(tmp1.v, tmp2.v, tmp3.v);
		n = -n;
        
        addVertex(tmp1, n, vertex_index);
        addVertex(tmp2, n, vertex_index);
        addVertex(tmp3, n, vertex_index);

        ntriang++;
    }

	FORCEINLINE void getConrers(int8 (&corner)[8], Point (&d)[8]) {
		for (auto i = 0; i < 8; i++) {
			corner[i] = (d[i].density < isolevel) ? 0 : -127;
		}
	}

public:
	FORCEINLINE void generateCell(int x, int y, int z) {
        float isolevel = 0.5f;
		Point d[8];

		int step = voxel_data_param.step();

        d[0] = getVoxelpoint(x, y + step, z);
        d[1] = getVoxelpoint(x, y, z);
        d[2] = getVoxelpoint(x + step, y + step, z);
        d[3] = getVoxelpoint(x + step, y, z);
        d[4] = getVoxelpoint(x, y + step, z + step);
        d[5] = getVoxelpoint(x, y, z + step);
        d[6] = getVoxelpoint(x + step, y + step, z + step);
        d[7] = getVoxelpoint(x + step, y, z + step);
		
        FVector p[8];
        p[0] = voxel_data.voxelIndexToVector(x, y + step, z);
        p[1] = voxel_data.voxelIndexToVector(x, y, z);
        p[2] = voxel_data.voxelIndexToVector(x + step, y + step, z);
        p[3] = voxel_data.voxelIndexToVector(x + step, y, z);
        p[4] = voxel_data.voxelIndexToVector(x, y + step, z + step);
        p[5] = voxel_data.voxelIndexToVector(x, y, z + step);
        p[6] = voxel_data.voxelIndexToVector(x + step, y + step, z + step);
        p[7] = voxel_data.voxelIndexToVector(x + step, y, z + step);

		int8 corner[8];
		for (auto i = 0; i < 8; i++) {
			corner[i] = (d[i].density < isolevel) ? -127 : 0;
		}

		unsigned long caseCode = ((corner[0] >> 7) & 0x01)
			| ((corner[1] >> 6) & 0x02)
			| ((corner[2] >> 5) & 0x04)
			| ((corner[3] >> 4) & 0x08)
			| ((corner[4] >> 3) & 0x10)
			| ((corner[5] >> 2) & 0x20)
			| ((corner[6] >> 1) & 0x40)
			| (corner[7] & 0x80);

		if (caseCode == 0) {
			return;
		}

		unsigned int c = regularCellClass[caseCode];
		RegularCellData cd = regularCellData[c];
		std::vector<TmpPoint> vertexList;
		vertexList.reserve(cd.GetTriangleCount() * 3);

		for (int i = 0; i < cd.GetVertexCount(); i++) {
			const int edgeCode = regularVertexData[caseCode][i];
			const unsigned short v0 = (edgeCode >> 4) & 0x0F;
			const unsigned short v1 = edgeCode & 0x0F;
			struct TmpPoint tp = vertexClc(p[v0], p[v1], d[v0], d[v1]);
			vertexList.push_back(tp);
		}

		for (int i = 0; i < cd.GetTriangleCount() * 3; i += 3) {
			const int vertexIndex1 = cd.vertexIndex[i];
			const int vertexIndex2 = cd.vertexIndex[i + 1];
			const int vertexIndex3 = cd.vertexIndex[i + 2];

			TmpPoint tmp1 = vertexList[cd.vertexIndex[i]];
			TmpPoint tmp2 = vertexList[cd.vertexIndex[i + 1]];
			TmpPoint tmp3 = vertexList[cd.vertexIndex[i + 2]];

			handleTriangle(tmp1, tmp2, tmp3);
		}
    }
};

//#define VoxelMeshExtractorPtr std::shared_ptr<VoxelMeshExtractor> 
typedef std::shared_ptr<VoxelMeshExtractor> VoxelMeshExtractorPtr;

MeshDataPtr sandboxVoxelGenerateMesh(const VoxelData &vd, const VoxelDataParam &vdp) {
	MeshData* mesh_data = new MeshData();
	std::vector<VoxelMeshExtractorPtr> MeshExtractorLod;

	int max_lod = vdp.bGenerateLOD ? 7 : 1;

	for (auto i = 0; i < max_lod; i++) {
		VoxelDataParam me_vdp = vdp;
		me_vdp.lod = i;
		VoxelMeshExtractorPtr me_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->main_mesh, vd, me_vdp));
		MeshExtractorLod.push_back(me_ptr);
	}

	int step = vdp.step();

	for (auto x = 0; x < vd.num() - step; x += step) {
		for (auto y = 0; y < vd.num() - step; y += step) {
			for (auto z = 0; z < vd.num() - step; z += step) {
				// generate mesh for each LOD
				//==================================================================
				for (auto i = 0; i < max_lod; i++) {
					int s = 1 << i;
					if (x % s == 0 && y % s == 0 && z % s == 0) {
						VoxelMeshExtractorPtr me_ptr = MeshExtractorLod[i];
						me_ptr->generateCell(x, y, z);
					}
				}
				//==================================================================
			}
		}
	}

	return MeshDataPtr(mesh_data);
}

// =================================================================
// terrain
// =================================================================

static std::mutex terrain_map_mutex;
static TMap<FVector, VoxelData*> terrain_zone_map;


void sandboxRegisterTerrainVoxelData(VoxelData* vd, FVector index) {
	terrain_map_mutex.lock();
	terrain_zone_map.Add(index, vd);
	terrain_map_mutex.unlock();
}

VoxelData* sandboxGetTerrainVoxelDataByPos(FVector point) {
	FVector index = sandboxSnapToGrid(point, 1000) / 1000;

	terrain_map_mutex.lock();
	if (terrain_zone_map.Contains(index)) {
		VoxelData* vd = terrain_zone_map[index];
		terrain_map_mutex.unlock();
		return vd;
	}

	terrain_map_mutex.unlock();
	return NULL;
}

VoxelData* sandboxGetTerrainVoxelDataByIndex(FVector index) {
	terrain_map_mutex.lock();
	if (terrain_zone_map.Contains(index)) {
		VoxelData* vd = terrain_zone_map[index];
		terrain_map_mutex.unlock();
		return vd;
	}

	terrain_map_mutex.unlock();
	return NULL;
}


void sandboxSaveVoxelData(const VoxelData &vd, FString &fullFileName) {
	FBufferArchive binaryData;
	int32 num = vd.num();
	float size = vd.size();
	unsigned char volume_state = 0;


	binaryData << num;
	binaryData << size;

	// save density
	if (vd.getDensityFillState() == VoxelDataFillState::ZERO) {
		volume_state = 0;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == VoxelDataFillState::ALL) {
		volume_state = 1;
		binaryData << volume_state;
	}

	if (vd.getDensityFillState() == VoxelDataFillState::MIX) {
		volume_state = 2;
		binaryData << volume_state;
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					VoxelPoint vp = vd.getVoxelPoint(x, y, z);
					unsigned char density = vp.density;
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

	unsigned char base_mat = vd.base_fill_mat;

	binaryData << volume_state;
	binaryData << base_mat;

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					VoxelPoint vp = vd.getVoxelPoint(x, y, z);
					unsigned char mat_id = vp.material;
					binaryData << mat_id;
				}
			}
		}
	}

	int32 end_marker = 666999;
	binaryData << end_marker;

	if (FFileHelper::SaveArrayToFile(binaryData, *fullFileName)) {
		binaryData.FlushCache();
		binaryData.Empty();
	}
}

bool sandboxLoadVoxelData(VoxelData &vd, FString &fullFileName) {
	double start = FPlatformTime::Seconds();

	TArray<uint8> TheBinaryArray;
	if (!FFileHelper::LoadFileToArray(TheBinaryArray, *fullFileName)) {
		UE_LOG(LogTemp, Warning, TEXT("Zone file not found -> %s"), *fullFileName);
		return false;
	}
	
	if (TheBinaryArray.Num() <= 0) return false;

	FMemoryReader binaryData = FMemoryReader(TheBinaryArray, true); //true, free data after done
	binaryData.Seek(0);
	
	int32 num;
	float size;
	unsigned char volume_state;
	unsigned char base_mat;

	binaryData << num;
	binaryData << size;

	// load density
	binaryData << volume_state;

	if (volume_state == 0) {
		vd.deinitializeDensity(VoxelDataFillState::ZERO);
	}

	if (volume_state == 1) {
		vd.deinitializeDensity(VoxelDataFillState::ALL);
	}

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density;
					binaryData << density;
					vd.setVoxelPointDensity(x, y, z, density);
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
					unsigned char mat_id;
					binaryData << mat_id;
					vd.setVoxelPointMaterial(x, y, z, mat_id);
				}
			}
		}
	} else {
		vd.deinitializeMaterial(base_mat);
	}

	int32 end_marker;
	binaryData << end_marker;
	
	binaryData.FlushCache();
	TheBinaryArray.Empty();
	binaryData.Close();
	
	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("Loading terrain zone: %s -> %f ms"), *fileFullPath, time);
	
	return true;
}


extern FVector sandboxConvertVectorToCubeIndex(FVector vec) {
	return sandboxSnapToGrid(vec, 200);
}

extern FVector sandboxSnapToGrid(FVector vec, float grid_range) {
	FVector tmp(vec);
	tmp /= grid_range;
	//FVector tmp2(std::round(tmp.X), std::round(tmp.Y), std::round(tmp.Z));
	FVector tmp2((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
	tmp2 *= grid_range;
	return FVector((int)tmp2.X, (int)tmp2.Y, (int)tmp2.Z);
}

FVector sandboxGridIndex(FVector v, int range) {
	FVector tmp = sandboxSnapToGrid(v, range) / range;
	return FVector((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
}
