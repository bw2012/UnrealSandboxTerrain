
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxVoxeldata.h"

#include "Transvoxel.h"

#include <cmath>
#include <vector>
#include <mutex>
#include <set>

//#include <unordered_map>
#include <map>


//====================================================================================
// Voxel data impl
//====================================================================================

    TVoxelData::TVoxelData(int num, float size){
       // int s = num*num*num;

        density_data = NULL;
		density_state = TVoxelDataFillState::ZERO;

		material_data = NULL;

		voxel_num = num;
        volume_size = size;
    }

    TVoxelData::~TVoxelData(){
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

	FORCEINLINE void TVoxelData::setDensity(int x, int y, int z, float density){
		if (density_data == NULL) {
			if (density_state == TVoxelDataFillState::ZERO && density == 0){
				return;
			}

			if (density_state == TVoxelDataFillState::ALL && density == 1) {
				return;
			}

			initializeDensity();
			density_state = TVoxelDataFillState::MIX;
		}

        if(x < voxel_num && y < voxel_num && z < voxel_num){
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

        if(x < voxel_num && y < voxel_num && z < voxel_num){
			int index = x * voxel_num * voxel_num + y * voxel_num + z;

			float d = (float)density_data[index] / 255.0f;
            return d;
        } else {
            return 0;
        }
    }

	FORCEINLINE unsigned char TVoxelData::getRawDensity(int x, int y, int z) const {
		auto index = x * voxel_num * voxel_num + y * voxel_num + z;
		return density_data[index];
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
		} else {
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

	FORCEINLINE TVoxelPoint TVoxelData::getVoxelPoint(int x, int y, int z) const {
		TVoxelPoint vp;
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

	FORCEINLINE void TVoxelData::deinitializeDensity(TVoxelDataFillState state) {
		if (state == TVoxelDataFillState::MIX) {
			return;
		}

		density_state = state;
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
		static unsigned char isolevel = 127;

		const int rx = x - step;
		const int ry = y - step;
		const int rz = z - step;

		density[0] = getRawDensity(x, y - step, z);
		density[1] = getRawDensity(x, y, z);
		density[2] = getRawDensity(x - step, y - step, z);
		density[3] = getRawDensity(x - step, y, z);
		density[4] = getRawDensity(x, y - step, z - step);
		density[5] = getRawDensity(x, y, z - step);
		density[6] = getRawDensity(rx, ry, rz);
		density[7] = getRawDensity(x - step, y, z - step);

		if (density[0] > isolevel &&
			density[1] > isolevel &&
			density[2] > isolevel &&
			density[3] > isolevel &&
			density[4] > isolevel &&
			density[5] > isolevel &&
			density[6] > isolevel &&
			density[7] > isolevel) {
			return false;
		}

		if (density[0] <= isolevel &&
			density[1] <= isolevel &&
			density[2] <= isolevel &&
			density[3] <= isolevel &&
			density[4] <= isolevel &&
			density[5] <= isolevel &&
			density[6] <= isolevel &&
			density[7] <= isolevel) {
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

//####################################################################################################################################
//
//	VoxelMeshExtractor
//
//####################################################################################################################################

//#define FORCEINLINE FORCENOINLINE  //debug

class VoxelMeshExtractor {

private:

	TMeshLodSection &mesh_data;
	const TVoxelData &voxel_data;
	const TVoxelDataParam voxel_data_param;

	typedef struct PointAddr {
		uint8 x = 0;
		uint8 y = 0;
		uint8 z = 0;

		PointAddr(const uint8 x0, const uint8 y0, const uint8 z0) : x(x0), y(y0), z(z0) { }
		PointAddr() { }


		FORCEINLINE const PointAddr operator-(const PointAddr& rv) const {
			return PointAddr(x - rv.x, y - rv.y, z - rv.z);
		}

		FORCEINLINE const PointAddr operator+(const PointAddr& rv) const {
			return PointAddr(x + rv.x, y + rv.y, z + rv.z);
		}

		FORCEINLINE const PointAddr operator/(int val) const {
			return PointAddr(x / val, y / val, z / val);
		}

		FORCEINLINE void operator = (const PointAddr &a) {
			x = a.x;
			y = a.y;
			z = a.z;
		}

	} PointAddr;


	struct Point {
		PointAddr adr;
		FVector pos;
		float density;
		unsigned short material_id;
	};

	struct TmpPoint {
		FVector v;
		unsigned short mat_id;
		float mat_weight = 0;
	};

	class MeshHandler {

	private:
		FProcMeshSection* meshSection;
		VoxelMeshExtractor* extractor;
		TMaterialSectionMap* materialSectionMapPtr;
		TMaterialTransitionSectionMap* materialTransitionSectionMapPtr;

		// transition material
		unsigned short transitionMaterialIndex = 0;
		TMap<FString, unsigned short> transitionMaterialDict;

		int ntriang = 0;
		int vertex_index = 0;

		TMap<FVector, int> VertexMap;

		struct VertexInfo {
			FVector normal;

			std::map<unsigned short, int32> indexInMaterialSectionMap;
			std::map<unsigned short, int32> indexInMaterialTransitionSectionMap;
		};

		TMap<FVector, VertexInfo> vertexInfoMap;

	public:
		MeshHandler(VoxelMeshExtractor* e, FProcMeshSection* s, TMaterialSectionMap* ms, TMaterialTransitionSectionMap* mts) :
			extractor(e), meshSection(s), materialSectionMapPtr(ms), materialTransitionSectionMapPtr(mts) { }

	private:
		FORCEINLINE void addVertexTest(TmpPoint &point, FVector n, int &index) {
			FVector v = point.v;

			meshSection->ProcIndexBuffer.Add(index);

			int t = point.mat_weight * 255;

			FProcMeshVertex Vertex;
			Vertex.Position = v;
			Vertex.Normal = n;
			Vertex.UV0 = FVector2D(0.f, 0.f);
			Vertex.Color = FColor(t, 0, 0, 0);
			Vertex.Tangent = FProcMeshTangent();

			meshSection->SectionLocalBox += Vertex.Position;
			meshSection->ProcVertexBuffer.Add(Vertex);

			vertex_index++;
		}

		FORCEINLINE void addVertex(const TmpPoint &point, const FVector& n, int &index) {
			FVector v = point.v;

			if (VertexMap.Contains(v)) {
				int vindex = VertexMap[v];

				FProcMeshVertex& Vertex = meshSection->ProcVertexBuffer[vindex];
				FVector nvert = Vertex.Normal;

				FVector tmp(nvert);
				tmp += n;
				tmp /= 2;

				Vertex.Normal = tmp;
				meshSection->ProcIndexBuffer.Add(vindex);

			} else {
				meshSection->ProcIndexBuffer.Add(index);

				int t = point.mat_weight * 255;

				FProcMeshVertex Vertex;
				Vertex.Position = v;
				Vertex.Normal = n;
				Vertex.UV0 = FVector2D(0.f, 0.f);
				Vertex.Color = FColor(t, 0, 0, 0);
				Vertex.Tangent = FProcMeshTangent();

				meshSection->SectionLocalBox += Vertex.Position;

				meshSection->ProcVertexBuffer.Add(Vertex);

				VertexMap.Add(v, index);
				vertex_index++;
			}
		}

		FORCEINLINE void addVertexMat(unsigned short matId, const TmpPoint &point, const FVector& n) {
			const FVector& v = point.v;

			VertexInfo& vertexInfo = vertexInfoMap.FindOrAdd(v);

			if (vertexInfo.normal.IsZero()) {
				vertexInfo.normal = n;
			} else {
				FVector tmp(vertexInfo.normal);
				tmp += n;
				tmp /= 2;
				vertexInfo.normal = tmp;
			}

			// get current mat section
			TMeshMaterialSection& matSectionRef = materialSectionMapPtr->FindOrAdd(matId);
			matSectionRef.MaterialId = matId; // update mat id (if case of new section was created by FindOrAdd)

			if (vertexInfo.indexInMaterialSectionMap.find(matId) != vertexInfo.indexInMaterialSectionMap.end()) {
				// vertex exist in mat section
				// just get vertex index and put to index buffer
				int32 vertexIndex = vertexInfo.indexInMaterialSectionMap[matId];
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(vertexIndex);
			} else { // vertex not exist in mat section
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(matSectionRef.vertexIndexCounter);

				FProcMeshVertex Vertex;
				Vertex.Position = v;
				Vertex.Normal = vertexInfo.normal;
				Vertex.UV0 = FVector2D(0.f, 0.f);
				Vertex.Color = FColor(0, 0, 0, 0);
				Vertex.Tangent = FProcMeshTangent();

				matSectionRef.MaterialMesh.SectionLocalBox += Vertex.Position;
				matSectionRef.MaterialMesh.ProcVertexBuffer.Add(Vertex);

				vertexInfo.indexInMaterialSectionMap[matId] = matSectionRef.vertexIndexCounter;
				matSectionRef.vertexIndexCounter++;
			}
		}

		FORCEINLINE void addVertexMatTransition(unsigned short matId, const TmpPoint &point, const FVector& n) {
			const FVector& v = point.v;

			VertexInfo& vertexInfo = vertexInfoMap.FindOrAdd(v);

			if (vertexInfo.normal.IsZero()) {
				vertexInfo.normal = n;
			}
			else {
				FVector tmp(vertexInfo.normal);
				tmp += n;
				tmp /= 2;
				vertexInfo.normal = tmp;
			}

			// get current mat section
			TMeshMaterialTransitionSection& matSectionRef = materialTransitionSectionMapPtr->FindOrAdd(matId);
			matSectionRef.MaterialId = matId; // update mat id (if case of new section was created by FindOrAdd)

			if (vertexInfo.indexInMaterialTransitionSectionMap.find(matId) != vertexInfo.indexInMaterialTransitionSectionMap.end()) {
				// vertex exist in mat section
				// just get vertex index and put to index buffer
				int32 vertexIndex = vertexInfo.indexInMaterialTransitionSectionMap[matId];
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(vertexIndex);
			}
			else { // vertex not exist in mat section
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(matSectionRef.vertexIndexCounter);

				FProcMeshVertex Vertex;
				Vertex.Position = v;
				Vertex.Normal = vertexInfo.normal;
				Vertex.UV0 = FVector2D(0.f, 0.f);
				Vertex.Color = FColor(0, 0, 0, 0);
				Vertex.Tangent = FProcMeshTangent();

				matSectionRef.MaterialMesh.SectionLocalBox += Vertex.Position;
				matSectionRef.MaterialMesh.ProcVertexBuffer.Add(Vertex);

				vertexInfo.indexInMaterialTransitionSectionMap[matId] = matSectionRef.vertexIndexCounter;
				matSectionRef.vertexIndexCounter++;
			}
		}

	public:
		FORCEINLINE unsigned short getTransitionMaterialIndex(FString& transitionMaterialName) {
			if (transitionMaterialDict.Contains(transitionMaterialName)) {
				return transitionMaterialDict[transitionMaterialName];
			}
			else {
				unsigned short idx = transitionMaterialIndex;
				transitionMaterialDict.Add(transitionMaterialName, idx);
				transitionMaterialIndex++;

				TMeshMaterialTransitionSection& sectionRef = materialTransitionSectionMapPtr->FindOrAdd(idx);
				sectionRef.TransitionName = transitionMaterialName;

				return idx;
			}
		}

		FORCEINLINE void addTriangle(TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			const FVector n = -clcNormal(tmp1.v, tmp2.v, tmp3.v);

			addVertex(tmp1, n, vertex_index);
			addVertex(tmp2, n, vertex_index);
			addVertex(tmp3, n, vertex_index);

			ntriang++;
		}

		FORCEINLINE void addTriangleMat(unsigned short matId, TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			const FVector n = -clcNormal(tmp1.v, tmp2.v, tmp3.v);

			addVertexMat(matId, tmp1, n);
			addVertexMat(matId, tmp2, n);
			addVertexMat(matId, tmp3, n);
		}

		FORCEINLINE void addTriangleMatTransition(unsigned short matId, TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			const FVector n = -clcNormal(tmp1.v, tmp2.v, tmp3.v);

			addVertexMatTransition(matId, tmp1, n);
			addVertexMatTransition(matId, tmp2, n);
			addVertexMatTransition(matId, tmp3, n);
		}

	};


	MeshHandler* mainMeshHandler;
	TArray<MeshHandler*> transitionHandlerArray;

public:
	VoxelMeshExtractor(TMeshLodSection &a, const TVoxelData &b, const TVoxelDataParam c) : mesh_data(a), voxel_data(b), voxel_data_param(c) {
		mainMeshHandler = new MeshHandler(this, &a.mainMesh, &a.MaterialSectionMap, &a.MaterialTransitionSectionMap);

		for (auto i = 0; i < 6; i++) {
			transitionHandlerArray.Add(new MeshHandler(this, &a.transitionMeshArray[i], nullptr, nullptr));
		}
	}

	~VoxelMeshExtractor() {
		delete mainMeshHandler;

		for (MeshHandler* transitionHandler : transitionHandlerArray) {
			delete transitionHandler;
		}
	}
    
private:
	double isolevel = 0.5f;

	FORCEINLINE Point getVoxelpoint(PointAddr adr) {
		return getVoxelpoint(adr.x, adr.y, adr.z);
	}

	FORCEINLINE Point getVoxelpoint(uint8 x, uint8 y, uint8 z) {
		Point vp;
		vp.adr = PointAddr(x,y,z);
		vp.density = getDensity(x, y, z);
		vp.material_id = getMaterial(x, y, z);
		vp.pos = voxel_data.voxelIndexToVector(x, y, z);
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

	FORCEINLINE unsigned short getMaterial(int x, int y, int z) {
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

	FORCEINLINE void materialCalculation(struct TmpPoint& tp, Point& point1, Point& point2) {
		static const unsigned short base_mat = 1; // dirt is base material

		FVector p1 = point1.pos;
		FVector p2 = point2.pos;
		const unsigned short mat1 = point1.material_id;
		const unsigned short mat2 = point2.material_id;
		
		if (mat1 == mat2) {
			//tp.mat_weight = 1; //grass
			tp.mat_id = mat1;
			return;
		}

		FVector tmp(p1);
		tmp -= p2;
		float s = tmp.Size();

		p1 -= tp.v;
		p2 -= tp.v;

		float s1 = p1.Size();
		float s2 = p2.Size();

		if (s1 > s2) {
			tp.mat_id = mat2;
		} else {
			tp.mat_id = mat1;
		}
	}

	FORCEINLINE void materialCalculation2(struct TmpPoint& tp, Point& point1, Point& point2) {
		static const int base_mat = 1; // dirt

		float mu = (isolevel - point1.density) / (point2.density - point1.density);

		PointAddr tmp;
		tmp.x = point1.adr.x + mu * (point2.adr.x - point1.adr.x);
		tmp.y = point1.adr.y + mu * (point2.adr.y - point1.adr.y);
		tmp.z = point1.adr.z + mu * (point2.adr.z - point1.adr.z);

		tp.mat_id = getMaterial(tmp.x, tmp.y, tmp.z);

		if (base_mat == tp.mat_id) {
			tp.mat_weight = 0;
		} else {
			tp.mat_weight = 1;
		}
	}

	FORCEINLINE TmpPoint vertexClc(Point& point1, Point& point2) {
		struct TmpPoint ret;

		ret.v = vertexInterpolation(point1.pos, point2.pos, point1.density, point2.density);

		if (voxel_data_param.lod == 0) {
			materialCalculation(ret, point1, point2);
		} else {
			materialCalculation2(ret, point1, point2);
		}
		return ret;
	}

	FORCEINLINE void getConrers(int8 (&corner)[8], Point (&d)[8]) {
		for (auto i = 0; i < 8; i++) {
			corner[i] = (d[i].density < isolevel) ? 0 : -127;
		}
	}

	FORCEINLINE PointAddr clcMediumAddr(const PointAddr& adr1, const PointAddr& adr2) {
		uint8 x = (adr2.x - adr1.x) / 2 + adr1.x;
		uint8 y = (adr2.y - adr1.y) / 2 + adr1.y;
		uint8 z = (adr2.z - adr1.z) / 2 + adr1.z;

		return PointAddr(x, y, z);
	}

	FORCEINLINE void extractRegularCell(Point (&d)[8]) {
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

		std::set<unsigned short> materialIdSet;

		for (int i = 0; i < cd.GetVertexCount(); i++) {
			const int edgeCode = regularVertexData[caseCode][i];
			const unsigned short v0 = (edgeCode >> 4) & 0x0F;
			const unsigned short v1 = edgeCode & 0x0F;
			struct TmpPoint tp = vertexClc(d[v0], d[v1]);
			vertexList.push_back(tp);
			materialIdSet.insert(tp.mat_id);
		}

		bool isTransitionMaterialSection = materialIdSet.size() > 1;
		unsigned short transitionMatId = 0;

		// if transition material
		if (isTransitionMaterialSection) {
			FString test = TEXT("");
			FString separator = TEXT("");
			for (unsigned short matId : materialIdSet) {
				test = FString::Printf(TEXT("%s%s%d"), *test, *separator, matId);
				separator = TEXT("-");
			}

			transitionMatId = mainMeshHandler->getTransitionMaterialIndex(test);
			//UE_LOG(LogTemp, Warning, TEXT("transition material section -> %d -> %s"), transitionMatId, *test);
		}

		for (int i = 0; i < cd.GetTriangleCount() * 3; i += 3) {
			TmpPoint tmp1 = vertexList[cd.vertexIndex[i]];
			TmpPoint tmp2 = vertexList[cd.vertexIndex[i + 1]];
			TmpPoint tmp3 = vertexList[cd.vertexIndex[i + 2]];

			mainMeshHandler->addTriangle(tmp1, tmp2, tmp3);

			if (isTransitionMaterialSection) {
				// add transition material section
				mainMeshHandler->addTriangleMatTransition(transitionMatId, tmp1, tmp2, tmp3);
			} else {
				// always one iteration
				for (unsigned short matId : materialIdSet) {
					// add regular material section
					mainMeshHandler->addTriangleMat(matId, tmp1, tmp2, tmp3);
				}
			}

		}
	}

	FORCEINLINE void extractTransitionCell(int sectionNumber, Point& d0, Point& d2, Point& d6, Point& d8) {
		Point d[14];

		d[0] = d0;
		d[1] = getVoxelpoint(clcMediumAddr(d2.adr, d0.adr));
		d[2] = d2;

		PointAddr a3 = clcMediumAddr(d6.adr, d0.adr);
		PointAddr a5 = clcMediumAddr(d8.adr, d2.adr);

		d[3] = getVoxelpoint(a3);
		d[4] = getVoxelpoint(clcMediumAddr(a5, a3));
		d[5] = getVoxelpoint(a5);

		d[6] = d6;
		d[7] = getVoxelpoint(clcMediumAddr(d8.adr, d6.adr));
		d[8] = d8;

		d[9] = d0;
		d[0xa] = d2;
		d[0xb] = d6;
		d[0xc] = d8;


		for (auto i = 0; i < 9; i++) {
			//mesh_data.DebugPointList.Add(d[i].pos);
		}

		int8 corner[9];
		for (auto i = 0; i < 9; i++) {
			corner[i] = (d[i].density < isolevel) ? -127 : 0;
		}

		static const int caseCodeCoeffs[9] = { 0x01, 0x02, 0x04, 0x80, 0x100, 0x08, 0x40, 0x20, 0x10 };
		static const int charByteSz = sizeof(int8) * 8;

		unsigned long caseCode = 0;
		for (auto ci = 0; ci < 9; ++ci) {
			// add the coefficient only if the value is negative
			caseCode += ((corner[ci] >> (charByteSz - 1)) & 1) * caseCodeCoeffs[ci];
		}

		if (caseCode == 0) {
			return;
		}

		unsigned int classIndex = transitionCellClass[caseCode];

		const bool inverse = (classIndex & 128) != 0;

		TransitionCellData cellData = transitionCellData[classIndex & 0x7F];

		std::vector<TmpPoint> vertexList;
		vertexList.reserve(cellData.GetTriangleCount() * 3);

		for (int i = 0; i < cellData.GetVertexCount(); i++) {
			const int edgeCode = transitionVertexData[caseCode][i];
			const unsigned short v0 = (edgeCode >> 4) & 0x0F;
			const unsigned short v1 = edgeCode & 0x0F;

			struct TmpPoint tp = vertexClc(d[v0], d[v1]);
			vertexList.push_back(tp);

			mesh_data.DebugPointList.Add(tp.v);
		}

		for (int i = 0; i < cellData.GetTriangleCount() * 3; i += 3) {
			TmpPoint tmp1 = vertexList[cellData.vertexIndex[i]];
			TmpPoint tmp2 = vertexList[cellData.vertexIndex[i + 1]];
			TmpPoint tmp3 = vertexList[cellData.vertexIndex[i + 2]];

			MeshHandler* meshHandler = transitionHandlerArray[sectionNumber];

			if (inverse) {
				meshHandler->addTriangle(tmp3, tmp2, tmp1);
			} else {
				meshHandler->addTriangle(tmp1, tmp2, tmp3);
			}
			
		}
	}

public:
	FORCEINLINE void generateCell(int x, int y, int z) {
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
		
		extractRegularCell(d);

		if (voxel_data_param.bGenerateLOD) {
			if (voxel_data_param.lod > 0) {
				const int e = voxel_data.num() - step - 1;

				if (x == 0) extractTransitionCell(0, d[1], d[0], d[5], d[4]); // X+
				if (x == e) extractTransitionCell(1, d[2], d[3], d[6], d[7]); // X-
				if (y == 0) extractTransitionCell(2, d[3], d[1], d[7], d[5]); // Y-
				if (y == e) extractTransitionCell(3, d[0], d[2], d[4], d[6]); // Y+
				if (z == 0) extractTransitionCell(4, d[3], d[2], d[1], d[0]); // Z-
				if (z == e) extractTransitionCell(5, d[6], d[7], d[4], d[5]); // Z+
			}
		}

    }

};

typedef std::shared_ptr<VoxelMeshExtractor> VoxelMeshExtractorPtr;

//####################################################################################################################################

TMeshDataPtr polygonizeCellSubstanceCacheNoLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));

	int step = vdp.step();
	for (auto it = vd.substanceCacheLOD[0].cellList.cbegin(); it != vd.substanceCacheLOD[0].cellList.cend(); ++it) {
		int index = *it;

		int x = index / (vd.num() * vd.num());
		int y = (index / vd.num()) % vd.num();
		int z = index % vd.num();

		mesh_extractor_ptr->generateCell(x, y, z);
	}

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].mainMesh;

	return TMeshDataPtr(mesh_data);
}


TMeshDataPtr polygonizeCellSubstanceCacheLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	static const int max_lod = LOD_ARRAY_SIZE;

	// create mesh extractor for each LOD
	for (auto lod = 0; lod < max_lod; lod++) {
		TVoxelDataParam me_vdp = vdp;
		me_vdp.lod = lod;

		VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[lod], vd, me_vdp));

		int step = vdp.step();
		for (auto it = vd.substanceCacheLOD[lod].cellList.cbegin(); it != vd.substanceCacheLOD[lod].cellList.cend(); ++it) {
			int index = *it;

			int x = index / (vd.num() * vd.num());
			int y = (index / vd.num()) % vd.num();
			int z = index % vd.num();

			mesh_extractor_ptr->generateCell(x, y, z);
		}
	}

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].mainMesh;

	return TMeshDataPtr(mesh_data);
}


TMeshDataPtr polygonizeVoxelGridNoLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));

	int step = vdp.step();

	for (auto x = 0; x < vd.num() - step; x += step) {
		for (auto y = 0; y < vd.num() - step; y += step) {
			for (auto z = 0; z < vd.num() - step; z += step) {
				mesh_extractor_ptr->generateCell(x, y, z);
			}
		}
	}

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].mainMesh;

	return TMeshDataPtr(mesh_data);
}

TMeshDataPtr polygonizeVoxelGridWithLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	std::vector<VoxelMeshExtractorPtr> MeshExtractorLod;
	//VoxelMeshExtractorPtr MeshExtractorLod[LOD_ARRAY_SIZE];

	static const int max_lod = LOD_ARRAY_SIZE;

	// create mesh extractor for each LOD
	for (auto lod = 0; lod < max_lod; lod++) {
		TVoxelDataParam me_vdp = vdp;
		me_vdp.lod = lod;
		VoxelMeshExtractorPtr me_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[lod], vd, me_vdp));
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

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].mainMesh;

	return TMeshDataPtr(mesh_data);
}

TMeshDataPtr sandboxVoxelGenerateMesh(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	if (vd.isSubstanceCacheValid()) {
		for (auto lod = 0; lod < LOD_ARRAY_SIZE; lod++) {
			//UE_LOG(LogTemp, Warning, TEXT("SubstanceCacheLOD -> %d ---> %f %f %f -> %d elenents"), lod, vd.getOrigin().X, vd.getOrigin().Y, vd.getOrigin().Z, vd.substanceCacheLOD[lod].cellList.size());
		}

		return vdp.bGenerateLOD ? polygonizeCellSubstanceCacheLOD(vd, vdp) : polygonizeCellSubstanceCacheNoLOD(vd, vdp);
	}

	return vdp.bGenerateLOD ? polygonizeVoxelGridWithLOD(vd, vdp) : polygonizeVoxelGridNoLOD(vd, vdp);
}

// =================================================================
// terrain
// =================================================================

void sandboxSaveVoxelData(const TVoxelData &vd, FString &fullFileName) {

	UE_LOG(LogTemp, Warning, TEXT("sandboxSaveVoxelData -> %s"), *fullFileName);

	FBufferArchive binaryData;
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
					TVoxelPoint vp = vd.getVoxelPoint(x, y, z);
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

	unsigned short base_mat = vd.base_fill_mat;

	binaryData << volume_state;
	binaryData << base_mat;

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					TVoxelPoint vp = vd.getVoxelPoint(x, y, z);
					unsigned short mat_id = vp.material;
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

bool sandboxLoadVoxelData(TVoxelData &vd, FString &fullFileName) {
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
					unsigned short mat_id;
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


