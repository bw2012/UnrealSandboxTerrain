/**
* SandboxVoxelCore.cpp
*
* blackw
* 
* Created: Wed Aug 3 22:26:27 2016
* initial file name: SandboxVoxeldata.cpp
*
*/

#include "SandboxVoxelCore.h"
#include "Transvoxel.h"
#include "VoxelIndex.h"
#include <cmath>
#include <vector>
#include <mutex>
#include <iterator>
#include <map>


#define FORCEINLINE2 FORCEINLINE  
//#define FORCEINLINE2 FORCENOINLINE  //debug

typedef struct TVoxelDataGenerationParam {
    int lod = 0;
    bool bGenerateLOD = false;
	bool bIgnoreLodPatches = false;
    
	FORCEINLINE2 int step() const {
		return 1 << lod; 
	}

    TVoxelDataGenerationParam(const TVoxelDataParam& vdp) {
        bGenerateLOD = vdp.bGenerateLOD;
    }

} TVoxelDataGenerationParam;

//====================================================================================
	
static FORCEINLINE2 FVector clcNormal(FVector &p1, FVector &p2, FVector &p3) {
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

class VoxelMeshExtractor {

private:

	TMeshLodSection &mesh_data;
	const TVoxelData &voxel_data;
	const TVoxelDataGenerationParam voxel_data_param;

	//FIXME
	struct TPointInfo {
		TVoxelIndex adr;
		FVector pos;
		float density;
		unsigned short material_id;
	};

	struct TmpPoint {
		FVector v;
		unsigned short matId;
	};

	class MeshHandler {

	private:
		FProcMeshSection* generalMeshSection;
		VoxelMeshExtractor* extractor;

		TMeshContainer* meshMatContainer;

		TMaterialSectionMap* materialSectionMapPtr;
		TMaterialTransitionSectionMap* materialTransitionSectionMapPtr;

		// transition material
		unsigned short transitionMaterialIndex = 0;
		TMap<FString, unsigned short> transitionMaterialDict;
		std::map<uint64, uint16> transitionMaterialMap;

		int triangleCount = 0;

		int vertexGeneralIndex = 0;

	public:

		struct VertexInfo {
			FVector normal;

			std::map<unsigned short, int32> indexInMaterialSectionMap;
			std::map<unsigned short, int32> indexInMaterialTransitionSectionMap;

			int vertexIndex = 0;
		};

		TMap<FVector, VertexInfo> vertexInfoMap;

		MeshHandler(VoxelMeshExtractor* e, FProcMeshSection* s, TMeshContainer* mc) :
                        generalMeshSection(s), extractor(e), meshMatContainer(mc) {
			materialSectionMapPtr = &meshMatContainer->MaterialSectionMap;
			materialTransitionSectionMapPtr = &meshMatContainer->MaterialTransitionSectionMap;
		}

	private:

		FORCEINLINE void addVertexGeneral(const TmpPoint &point, const FVector& n) {
			const FVector v = point.v;
			VertexInfo& vertexInfo = vertexInfoMap.FindOrAdd(v);

			if (vertexInfo.normal.IsZero()) {
				// new vertex
				vertexInfo.normal = n;

				TMeshVertex Vertex{v, n, -1};
				generalMeshSection->ProcIndexBuffer.Add(vertexGeneralIndex);
				generalMeshSection->AddVertex(Vertex);
				vertexInfo.vertexIndex = vertexGeneralIndex;

				vertexGeneralIndex++;
			} else {
				// existing vertex
				FVector tmp(vertexInfo.normal);
				tmp += n;
				tmp /= 2;
				vertexInfo.normal = tmp;

				generalMeshSection->ProcIndexBuffer.Add(vertexInfo.vertexIndex);
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

				TMeshVertex Vertex{v, vertexInfo.normal, -1};
				matSectionRef.MaterialMesh.AddVertex(Vertex);

				vertexInfo.indexInMaterialSectionMap[matId] = matSectionRef.vertexIndexCounter;
				matSectionRef.vertexIndexCounter++;
			}
		}

		FORCEINLINE void addVertexMatTransition(std::set<unsigned short>& materialIdSet, unsigned short matId, const TmpPoint &point, const FVector& n) {
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
			TMeshMaterialSection& matSectionRef = materialTransitionSectionMapPtr->FindOrAdd(matId);
			matSectionRef.MaterialId = matId; // update mat id (if case of new section was created by FindOrAdd)

			if (vertexInfo.indexInMaterialTransitionSectionMap.find(matId) != vertexInfo.indexInMaterialTransitionSectionMap.end()) {
				// vertex exist in mat section
				// just get vertex index and put to index buffer
				int32 vertexIndex = vertexInfo.indexInMaterialTransitionSectionMap[matId];
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(vertexIndex);
			} else { // vertex not exist in mat section
				matSectionRef.MaterialMesh.ProcIndexBuffer.Add(matSectionRef.vertexIndexCounter);

				TMeshVertex Vertex{v, vertexInfo.normal, -1};

				int i = 0;
				int32 MatIdx = -1;
				for (unsigned short m : materialIdSet) {
					if (m == point.matId) {
						MatIdx = i;
						break;
					}

					i++;
				}

				Vertex.MatIdx = MatIdx;

				matSectionRef.MaterialMesh.AddVertex(Vertex);
				vertexInfo.indexInMaterialTransitionSectionMap[matId] = matSectionRef.vertexIndexCounter;
				matSectionRef.vertexIndexCounter++;
			}
		}

	public:
		FORCEINLINE unsigned short getTransitionMaterialIndex(std::set<unsigned short>& materialIdSet) {
			uint64 code = TMeshMaterialTransitionSection::GenerateTransitionCode(materialIdSet);
			if (transitionMaterialMap.find(code) == transitionMaterialMap.end()) {
				// not found
				unsigned short idx = transitionMaterialIndex;
				transitionMaterialMap.insert({ code, idx });
				transitionMaterialIndex++;

				TMeshMaterialTransitionSection& sectionRef = materialTransitionSectionMapPtr->FindOrAdd(idx);
				sectionRef.TransitionCode = code;
				sectionRef.MaterialIdSet = materialIdSet;

				return idx;
			} else {
				// found
				return transitionMaterialMap[code];
			}
		}

		// general mesh without material. used for collision only
		FORCEINLINE void addTriangleGeneral(const FVector& normal, TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			addVertexGeneral(tmp1, normal);
			addVertexGeneral(tmp2, normal);
			addVertexGeneral(tmp3, normal);

			triangleCount++;
		}

		// usual mesh with one material
		FORCEINLINE void addTriangleMat(const FVector& normal, unsigned short matId, TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			addVertexMat(matId, tmp1, normal);
			addVertexMat(matId, tmp2, normal);
			addVertexMat(matId, tmp3, normal);

			triangleCount++;
		}

		// transitional mesh between two or more meshes with different material
		FORCEINLINE void addTriangleMatTransition(const FVector& normal, std::set<unsigned short>& materialIdSet, unsigned short matId, TmpPoint &tmp1, TmpPoint &tmp2, TmpPoint &tmp3) {
			addVertexMatTransition(materialIdSet, matId, tmp1, normal);
			addVertexMatTransition(materialIdSet, matId, tmp2, normal);
			addVertexMatTransition(materialIdSet, matId, tmp3, normal);

			triangleCount++;
		}

	};

	MeshHandler* mainMeshHandler;
	TArray<MeshHandler*> transitionHandlerArray;

public:
	VoxelMeshExtractor(TMeshLodSection &a, const TVoxelData &b, const TVoxelDataGenerationParam c) : mesh_data(a), voxel_data(b), voxel_data_param(c) {
		mainMeshHandler = new MeshHandler(this, &a.WholeMesh, &a.RegularMeshContainer);

		for (auto i = 0; i < 6; i++) {
			transitionHandlerArray.Add(new MeshHandler(this, &a.WholeMesh, &a.TransitionPatchArray[i]));
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

	FORCEINLINE2 TPointInfo getVoxelpoint(TVoxelIndex Idx) {
		return getVoxelpoint(Idx.X, Idx.Y, Idx.Z);
	}

	FORCEINLINE2 TPointInfo getVoxelpoint(uint8 x, uint8 y, uint8 z) {
		TPointInfo vp;
		vp.adr = TVoxelIndex(x, y, z);
		vp.density = getDensity(x, y, z);
		vp.material_id = getMaterial(x, y, z);
		vp.pos = voxel_data.voxelIndexToVector(x, y, z);
		return vp;
	}

	FORCEINLINE2 float getDensity(int x, int y, int z) {
		return voxel_data.getDensity(x, y, z);
	}

	FORCEINLINE2 unsigned short getMaterial(int x, int y, int z) {
		return voxel_data.getMaterial(x, y, z);
	}

	FORCEINLINE2 FVector vertexInterpolation(FVector p1, FVector p2, float valp1, float valp2) {
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
		return p1 + (p2 - p1) *mu;
	}

	// fast material select for LOD0
	FORCEINLINE2 void selectMaterialLOD0(struct TmpPoint& tp, TPointInfo& point1, TPointInfo& point2) {
		if (point1.material_id == point2.material_id) {
			tp.matId = point1.material_id;
			return;
		}

		FVector p1 = point1.pos - tp.v;
		FVector p2 = point2.pos - tp.v;

		if (p1.Size() > p2.Size()) {
			tp.matId = point2.material_id;
		} else {
			tp.matId = point1.material_id;
		}
	}

	// calculate material for LOD1-4
	FORCEINLINE2 void selectMaterialLODMedium(struct TmpPoint& tp, TPointInfo& point1, TPointInfo& point2) {
		float mu = (isolevel - point1.density) / (point2.density - point1.density);
		TVoxelIndex tmp = point1.adr + (point2.adr - point1.adr) * mu;
		tp.matId = getMaterial(tmp.X, tmp.Y, tmp.Z);
	}

	// calculate material for LOD5-6
	FORCEINLINE2 void selectMaterialLODBig(struct TmpPoint& tp, TPointInfo& point1, TPointInfo& point2) {
		TVoxelIndex A;
		TVoxelIndex B;

		if (point1.density < isolevel) {
			// point1 - air, point2 - solid
			A = point1.adr;
			B = point2.adr;
		} else {
			// point1 - solid, point2 - air
			A = point2.adr;
			B = point1.adr;
		}

		TVoxelIndex S = B - A;
		if (S.X != 0) {
			S.X = S.X / abs(S.X);
		}

		if (S.Y != 0) {
			S.Y = S.Y / abs(S.Y);
		}

		if (S.Z != 0) {
			S.Z = S.Z / abs(S.Z);
		}

		// start from air point and find first solid point
		TVoxelIndex tmp = A;
		while (getDensity(tmp.X, tmp.Y, tmp.Z) < isolevel) {
			if (tmp == B) {
				break;
			}
			tmp = tmp + S;
		}

		tp.matId = getMaterial(tmp.X, tmp.Y, tmp.Z);
	}

	void convertToLod0(TPointInfo& point1, TPointInfo& point2, TPointInfo& new_point1, TPointInfo& new_point2) {
		TVoxelIndex A;
		TVoxelIndex B;

		if (point1.density < isolevel) {
			// point1 - air, point2 - solid
			A = point1.adr;
			B = point2.adr;
		} else {
			// point1 - solid, point2 - air
			A = point2.adr;
			B = point1.adr;
		}

		TVoxelIndex S = B - A;
		if (S.X != 0) {
			S.X = S.X / abs(S.X);
		}

		if (S.Y != 0) {
			S.Y = S.Y / abs(S.Y);
		}

		if (S.Z != 0) {
			S.Z = S.Z / abs(S.Z);
		}

		// start from air point and find first solid point
		TVoxelIndex tmp = A;
		while (getDensity(tmp.X, tmp.Y, tmp.Z) < isolevel) {
			if (tmp == B) {
				break;
			}

			tmp = tmp + S;
		}

		TVoxelIndex newA = tmp - S;
		TVoxelIndex newB = tmp;

		TPointInfo pointA = getVoxelpoint(newA);
		TPointInfo pointB = getVoxelpoint(newB);

		//mesh_data.DebugPointList.Add(voxel_data.voxelIndexToVector(newA));
		//mesh_data.DebugPointList.Add(voxel_data.voxelIndexToVector(newB));

		if (point1.density < isolevel) {
			// point1 - air, point2 - solid
			new_point1 = pointA;
			new_point2 = pointB;
		} else {
			// point1 - solid, point2 - air
			new_point1 = pointB;
			new_point2 = pointA;
		}
	}

	FORCEINLINE2 TmpPoint vertexClc(TPointInfo& point1, TPointInfo& point2) {
		struct TmpPoint ret;

		if (voxel_data_param.lod != 0) {
			TPointInfo new_point1, new_point2;
			convertToLod0(point1, point2, new_point1, new_point2);
			ret.v = vertexInterpolation(new_point1.pos, new_point2.pos, new_point1.density, new_point2.density);
		} else {
			ret.v = vertexInterpolation(point1.pos, point2.pos, point1.density, point2.density);
		}

		if (voxel_data_param.lod == 0) {
			selectMaterialLOD0(ret, point1, point2);
		} else {
			selectMaterialLODBig(ret, point1, point2);
		}

		/*
		if (voxel_data_param.lod == 0) {
			selectMaterialLOD0(ret, point1, point2);
		} else if (voxel_data_param.lod > 0 && voxel_data_param.lod < 5) {
			selectMaterialLODMedium(ret, point1, point2);
		} else {
			selectMaterialLODBig(ret, point1, point2);
		}
		*/

		return ret;
	}

	FORCEINLINE2 void getConrers(int8 (&corner)[8], TPointInfo(&d)[8]) {
		for (auto i = 0; i < 8; i++) {
			corner[i] = (d[i].density < isolevel) ? 0 : -127;
		}
	}

	FORCEINLINE2 TVoxelIndex clcMediumAddr(const TVoxelIndex& adr1, const TVoxelIndex& adr2) {
		return (adr2 - adr1) / 2 + adr1;
	}

	FORCEINLINE2 void extractRegularCell(TPointInfo(&d)[8], unsigned long caseCode) {
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
			materialIdSet.insert(tp.matId);
			vertexList.push_back(tp);
		}

		bool isTransitionMaterialSection = materialIdSet.size() > 1;
		unsigned short transitionMatId = 0;

		// if transition material
		if (isTransitionMaterialSection) {
			transitionMatId = mainMeshHandler->getTransitionMaterialIndex(materialIdSet);
		}

		for (int i = 0; i < cd.GetTriangleCount() * 3; i += 3) {
			TmpPoint tmp1 = vertexList[cd.vertexIndex[i]];
			TmpPoint tmp2 = vertexList[cd.vertexIndex[i + 1]];
			TmpPoint tmp3 = vertexList[cd.vertexIndex[i + 2]];

			// calculate normal
			const FVector n = -clcNormal(tmp1.v, tmp2.v, tmp3.v);

			// add to whole mesh
			mainMeshHandler->addTriangleGeneral(n, tmp1, tmp2, tmp3);

			if (isTransitionMaterialSection) {
				// add transition material section
				mainMeshHandler->addTriangleMatTransition(n, materialIdSet, transitionMatId, tmp1, tmp2, tmp3);
			} else {
				// always one iteration
				for (unsigned short matId : materialIdSet) {
					// add regular material section
					mainMeshHandler->addTriangleMat(n, matId, tmp1, tmp2, tmp3);
				}
			}
		}
	}

	FORCEINLINE2 void extractRegularCell(TPointInfo(&d)[8]) {
		int8 corner[8];
		for (auto i = 0; i < 8; i++) {
			corner[i] = (d[i].density < isolevel) ? -127 : 0;
		}

		unsigned long caseCode = vd::tools::caseCode(corner);
		extractRegularCell(d, caseCode);
	}

	FORCEINLINE2 void extractTransitionCell(int sectionNumber, TPointInfo& d0, TPointInfo& d2, TPointInfo& d6, TPointInfo& d8) {
		TPointInfo d[14];

		d[0] = d0;
		d[1] = getVoxelpoint(clcMediumAddr(d2.adr, d0.adr));
		d[2] = d2;

		TVoxelIndex a3 = clcMediumAddr(d6.adr, d0.adr);
		TVoxelIndex a5 = clcMediumAddr(d8.adr, d2.adr);

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
		std::set<unsigned short> materialIdSet;

		for (int i = 0; i < cellData.GetVertexCount(); i++) {
			const int edgeCode = transitionVertexData[caseCode][i];
			const unsigned short v0 = (edgeCode >> 4) & 0x0F;
			const unsigned short v1 = edgeCode & 0x0F;
			struct TmpPoint tp = vertexClc(d[v0], d[v1]);

			materialIdSet.insert(tp.matId);
			vertexList.push_back(tp);
			//mesh_data.DebugPointList.Add(tp.v);
		}

		bool isTransitionMaterialSection = materialIdSet.size() > 1;
		unsigned short transitionMatId = 0;

		// if transition material
		if (isTransitionMaterialSection) {
			transitionMatId = mainMeshHandler->getTransitionMaterialIndex(materialIdSet);
		}

		for (int i = 0; i < cellData.GetTriangleCount() * 3; i += 3) {
			TmpPoint tmp1 = vertexList[cellData.vertexIndex[i]];
			TmpPoint tmp2 = vertexList[cellData.vertexIndex[i + 1]];
			TmpPoint tmp3 = vertexList[cellData.vertexIndex[i + 2]];

			MeshHandler* meshHandler = transitionHandlerArray[sectionNumber];

			// calculate normal
			FVector n = -clcNormal(tmp1.v, tmp2.v, tmp3.v);

			if(mainMeshHandler->vertexInfoMap.Contains(tmp1.v)) {
				MeshHandler::VertexInfo& vertexInfo = mainMeshHandler->vertexInfoMap.FindOrAdd(tmp1.v);
				n = vertexInfo.normal;
			} else if (mainMeshHandler->vertexInfoMap.Contains(tmp2.v)) {
				MeshHandler::VertexInfo& vertexInfo = mainMeshHandler->vertexInfoMap.FindOrAdd(tmp2.v);
				n = vertexInfo.normal;
			} else if (mainMeshHandler->vertexInfoMap.Contains(tmp3.v)) {
				MeshHandler::VertexInfo& vertexInfo = mainMeshHandler->vertexInfoMap.FindOrAdd(tmp3.v);
				n = vertexInfo.normal;
			}

			if (isTransitionMaterialSection) {
				// add transition material section
				if (inverse) {
					meshHandler->addTriangleMatTransition(n, materialIdSet, transitionMatId, tmp3, tmp2, tmp1);
				} else {
					meshHandler->addTriangleMatTransition(n, materialIdSet, transitionMatId, tmp1, tmp2, tmp3);
				}
			} else {
				// always one iteration
				for (unsigned short matId : materialIdSet) {
					// add regular material section
					if (inverse) {
						meshHandler->addTriangleMat(n, matId, tmp3, tmp2, tmp1);
					} else {
						meshHandler->addTriangleMat(n, matId, tmp1, tmp2, tmp3);
					}
				}
			}
		}
	}

    void makeVoxelpointArray(TPointInfo(&d)[8], const int x, const int y, const int z){
        const int step = voxel_data_param.step();
        d[0] = getVoxelpoint(x, y + step, z);
        d[1] = getVoxelpoint(x, y, z);
        d[2] = getVoxelpoint(x + step, y + step, z);
        d[3] = getVoxelpoint(x + step, y, z);
        d[4] = getVoxelpoint(x, y + step, z + step);
        d[5] = getVoxelpoint(x, y, z + step);
        d[6] = getVoxelpoint(x + step, y + step, z + step);
        d[7] = getVoxelpoint(x + step, y, z + step);
    }
    
    void extractAllTransitionCell(TPointInfo(&d)[8], const int x, const int y, const int z){
        if (voxel_data_param.bGenerateLOD && !voxel_data_param.bIgnoreLodPatches) {
            if (voxel_data_param.lod > 0) {
                const int e = voxel_data.num() - voxel_data_param.step() - 1;
                if (x == 0) extractTransitionCell(0, d[1], d[0], d[5], d[4]); // X+
                if (x == e) extractTransitionCell(1, d[2], d[3], d[6], d[7]); // X-
                if (y == 0) extractTransitionCell(2, d[3], d[1], d[7], d[5]); // Y-
                if (y == e) extractTransitionCell(3, d[0], d[2], d[4], d[6]); // Y+
                if (z == 0) extractTransitionCell(4, d[3], d[2], d[1], d[0]); // Z-
                if (z == e) extractTransitionCell(5, d[6], d[7], d[4], d[5]); // Z+
            }
        }
    }
    
    //####################################################################################################################################
    // public API
    //####################################################################################################################################
    
public:
	FORCEINLINE2 void generateCell(int x, int y, int z) {
		TPointInfo d[8];
        makeVoxelpointArray(d, x, y, z);
		extractRegularCell(d);
        extractAllTransitionCell(d, x, y, z);
    }
};

typedef std::shared_ptr<VoxelMeshExtractor> VoxelMeshExtractorPtr;

//####################################################################################################################################

TMeshDataPtr polygonizeSingleCell(const TVoxelData& vd, const TVoxelDataParam& vdp, int x, int y, int z) {
	TMeshData* mesh_data = new TMeshData();
	VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));
	mesh_extractor_ptr->generateCell(x, y, z);
	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].WholeMesh;
	return TMeshDataPtr(mesh_data);
}

TMeshDataPtr polygonizeCellSubstanceCacheNoLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));

	const int n = vd.num();

	vd.forEachCacheItem(0, [=](const TSubstanceCacheItem& itm) {
		const int index = itm.index;
		const int x = index / (n * n);
		const int y = (index / n) % n;
		const int z = index % n;
		mesh_extractor_ptr->generateCell(x, y, z);
	});

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].WholeMesh;
	return TMeshDataPtr(mesh_data);
}


TMeshDataPtr polygonizeCellSubstanceCacheLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshDataPtr mesh_data_ptr = std::make_shared<TMeshData>();
	static const int max_lod = LOD_ARRAY_SIZE;

	// create mesh extractor for each LOD
	for (auto lod = 0; lod < max_lod; lod++) {
		TVoxelDataGenerationParam me_vdp = vdp;
		me_vdp.lod = lod;
		VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data_ptr->MeshSectionLodArray[lod], vd, me_vdp));
		int step = me_vdp.step();

		const int n = vd.num();
		vd.forEachCacheItem(lod, [=](const TSubstanceCacheItem& itm) {
			const int index = itm.index;
			const int x = index / (n * n);
			const int y = (index / n) % n;
			const int z = index % n;
			mesh_extractor_ptr->generateCell(x, y, z);
		});
	}

	mesh_data_ptr->CollisionMeshPtr = &mesh_data_ptr->MeshSectionLodArray[vdp.collisionLOD].WholeMesh;
	return mesh_data_ptr;
}


TMeshDataPtr polygonizeVoxelGridNoLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	VoxelMeshExtractorPtr mesh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));

    const auto n = vd.num() - 1;
	for (auto x = 0; x < n; x++) {
		for (auto y = 0; y < n; y++) {
			for (auto z = 0; z < n; z++) {
				mesh_extractor_ptr->generateCell(x, y, z);
			}
		}
	}

	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[0].WholeMesh;

	return TMeshDataPtr(mesh_data);
}

TMeshDataPtr polygonizeVoxelGridWithLOD(const TVoxelData &vd, const TVoxelDataParam &vdp) {
	TMeshData* mesh_data = new TMeshData();
	std::vector<VoxelMeshExtractorPtr> MeshExtractorLod;
	static const int max_lod = LOD_ARRAY_SIZE;

	// create mesh extractor for each LOD
	for (auto lod = 0; lod < max_lod; lod++) {
		TVoxelDataGenerationParam me_vdp = vdp;
		me_vdp.lod = lod;
		VoxelMeshExtractorPtr me_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[lod], vd, me_vdp));
		MeshExtractorLod.push_back(me_ptr);
	}

    const auto n = vd.num() - 1;
	for (auto x = 0; x < n; x++) {
		for (auto y = 0; y < n; y++) {
			for (auto z = 0; z < n; z++) {
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

    if(vdp.bZCut){
        VoxelMeshExtractorPtr mdresh_extractor_ptr = VoxelMeshExtractorPtr(new VoxelMeshExtractor(mesh_data->MeshSectionLodArray[0], vd, vdp));
    }
    
	mesh_data->CollisionMeshPtr = &mesh_data->MeshSectionLodArray[vdp.collisionLOD].WholeMesh;
	return TMeshDataPtr(mesh_data);
}

//####################################################################################################################################

TMeshDataPtr sandboxVoxelGenerateMesh(const TVoxelData &vd, const TVoxelDataParam &vdp) {
    if (vd.isSubstanceCacheValid() && !vdp.bZCut && !vdp.bForceNoCache) {
		return vdp.bGenerateLOD ? polygonizeCellSubstanceCacheLOD(vd, vdp) : polygonizeCellSubstanceCacheNoLOD(vd, vdp);
	}

	//UE_LOG(LogVt, Warning, TEXT("No voxel data cache: %f %f %f"), vd.getOrigin().X, vd.getOrigin().Y, vd.getOrigin().Z);
	return vdp.bGenerateLOD ? polygonizeVoxelGridWithLOD(vd, vdp) : polygonizeVoxelGridNoLOD(vd, vdp);
}
