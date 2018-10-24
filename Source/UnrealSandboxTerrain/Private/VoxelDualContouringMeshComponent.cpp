// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "VoxelDualContouringMeshComponent.h"
#include "SandboxVoxeldata.h"
#include "DrawDebugHelpers.h"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include "qef_simd.h"
#include "VoxelIndex.h"


struct EdgeInfo {
	FVector4 pos;
	FVector4 normal;
	bool winding = false;
};

using EdgeInfoMap = std::unordered_map<uint32_t, EdgeInfo>;
using VoxelIDSet = std::unordered_set<uint32_t>;
using VoxelIndexMap = std::unordered_map<uint32_t, int>;


static const TVoxelIndex4 AXIS_OFFSET[3] = {
	TVoxelIndex4(1, 0, 0, 0),
	TVoxelIndex4(0, 1, 0, 0),
	TVoxelIndex4(0, 0, 1, 0)
};

static const TVoxelIndex4 EDGE_NODE_OFFSETS[3][4] = {
	{ TVoxelIndex4(0), TVoxelIndex4(0, 0, 1, 0), TVoxelIndex4(0, 1, 0, 0), TVoxelIndex4(0, 1, 1, 0) },
	{ TVoxelIndex4(0), TVoxelIndex4(1, 0, 0, 0), TVoxelIndex4(0, 0, 1, 0), TVoxelIndex4(1, 0, 1, 0) },
	{ TVoxelIndex4(0), TVoxelIndex4(0, 1, 0, 0), TVoxelIndex4(1, 0, 0, 0), TVoxelIndex4(1, 1, 0, 0) },
};

const uint32 ENCODED_EDGE_OFFSETS[12] = {
	0x00000000,
	0x00100000,
	0x00000400,
	0x00100400,
	0x40000000,
	0x40100000,
	0x40000001,
	0x40100001,
	0x80000000,
	0x80000400,
	0x80000001,
	0x80000401,
};

const uint32_t ENCODED_EDGE_NODE_OFFSETS[12] = {
	0x00000000,
	0x00100000,
	0x00000400,
	0x00100400,
	0x00000000,
	0x00000001,
	0x00100000,
	0x00100001,
	0x00000000,
	0x00000400,
	0x00000001,
	0x00000401,
};


float Density(const TVoxelData* VoxelData, const TVoxelIndex4& Index) {
	return VoxelData->getDensity(Index.X, Index.Y, Index.Z);
}

// returns x * (1.0 - a) + y * a 
// the linear blend of x and y using the floating-point value a
FVector4 mix(const FVector4& x, const FVector4& y, float a) {
	return x * (1.f - a) + y * a;
}

FVector vertexInterpolation(FVector p1, FVector p2, float valp1, float valp2) {
	static const float isolevel = 0.5f;

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

uint32 EncodeAxisUniqueID(const int axis, const int x, const int y, const int z) {
	return (x << 0) | (y << 10) | (z << 20) | (axis << 30);
}

uint32 EncodeVoxelUniqueID(const TVoxelIndex4& idxPos) {
	return idxPos.X | (idxPos.Y << 10) | (idxPos.Z << 20);
}

TVoxelIndex4 DecodeVoxelUniqueID(const uint32_t id) {
	return TVoxelIndex4(id & 0x3ff, (id >> 10) & 0x3ff, (id >> 20) & 0x3ff, 0);
}

void FindActiveVoxels(const TVoxelData* voxelData, VoxelIDSet& activeVoxels, EdgeInfoMap& activeEdges) {
	for (int x = 0; x < voxelData->num(); x++) {
		for (int y = 0; y < voxelData->num(); y++) {
			for (int z = 0; z < voxelData->num(); z++) {
				const TVoxelIndex4 p(x, y, z, 0);

				for (int axis = 0; axis < 3; axis++) {
					const TVoxelIndex4 q = p + AXIS_OFFSET[axis];

					const float pDensity = Density(voxelData, p);
					const float qDensity = Density(voxelData, q);

					const bool zeroCrossing = (pDensity >= 0.5f && qDensity < 0.5f) || (pDensity < 0.5f && qDensity >= 0.5f);

					if (!zeroCrossing) continue;

					//UE_LOG(LogTemp, Warning, TEXT("x, y, z - pDensity, qDensity  --> %d %d %d - %f %f"), x, y, z, pDensity, qDensity);

					const FVector p1 = voxelData->voxelIndexToVector(p.X, p.Y, p.Z);
					const FVector q1 = voxelData->voxelIndexToVector(q.X, q.Y, q.Z);
					const FVector4 pos = vertexInterpolation(p1, q1, pDensity, qDensity);

					FVector4 tmp(
						Density(voxelData, p + TVoxelIndex4(1, 0, 0, 0)) - Density(voxelData, p - TVoxelIndex4(1, 0, 0, 0)),
						Density(voxelData, p + TVoxelIndex4(0, 1, 0, 0)) - Density(voxelData, p - TVoxelIndex4(0, 1, 0, 0)),
						Density(voxelData, p + TVoxelIndex4(0, 0, 1, 0)) - Density(voxelData, p - TVoxelIndex4(0, 0, 1, 0)),
						0.f);

					auto normal = -tmp.GetSafeNormal(0.000001f);

					EdgeInfo info;
					info.pos = pos;
					info.normal = normal;
					info.winding = pDensity >= 0.5f;

					const auto code = EncodeAxisUniqueID(axis, x, y, z);

					//UE_LOG(LogTemp, Warning, TEXT("code  --> %d"), code);

					activeEdges[code] = info;

					const auto edgeNodes = EDGE_NODE_OFFSETS[axis];
					for (int i = 0; i < 4; i++) {
						const auto nodeIdxPos = p - edgeNodes[i];
						const auto nodeID = EncodeVoxelUniqueID(nodeIdxPos);
						activeVoxels.insert(nodeID);
					}
				}
			}
		}
	}
}

static void GenerateVertexData(const VoxelIDSet& voxels, const EdgeInfoMap& edges, VoxelIndexMap& vertexIndices, TArray<FVector>& varray, TArray<FVector>& narray) {
	int idxCounter = 0;
	for (const auto& voxelID : voxels) {
		FVector4 p[12];
		FVector4 n[12];

		int idx = 0;
		for (int i = 0; i < 12; i++) {
			const auto edgeID = voxelID + ENCODED_EDGE_OFFSETS[i];
			const auto iter = edges.find(edgeID);

			if (iter != end(edges)) {
				const auto& info = iter->second;
				const FVector4 pos = info.pos;
				const FVector4 normal = info.normal;

				p[idx] = pos;
				n[idx] = normal;
				idx++;
			}
		}

		FVector4 nodePos;
		qef_solve_from_points_4d(&p[0].X, &n[0].X, idx, &nodePos.X);

		FVector4 nodeNormal;
		for (int i = 0; i < idx; i++) {
			nodeNormal += n[i];
		}

		nodeNormal *= (1.f / (float)idx);

		vertexIndices[voxelID] = idxCounter++;
		varray.Add(FVector(nodePos.X, nodePos.Y, nodePos.Z));
		narray.Add(FVector(nodeNormal.X, nodeNormal.Y, nodeNormal.Z));
	}

}

static void GenerateTriangles(const EdgeInfoMap& edges, const VoxelIndexMap& vertexIndices, TArray<int32>& triarray) {
	for (const auto& pair : edges) {
		const auto& edge = pair.first;
		const auto& info = pair.second;

		const TVoxelIndex4 basePos = DecodeVoxelUniqueID(edge);
		const int axis = (edge >> 30) & 0xff;

		const int nodeID = edge & ~0xc0000000;
		const uint32_t voxelIDs[4] = {
			nodeID - ENCODED_EDGE_NODE_OFFSETS[axis * 4 + 0],
			nodeID - ENCODED_EDGE_NODE_OFFSETS[axis * 4 + 1],
			nodeID - ENCODED_EDGE_NODE_OFFSETS[axis * 4 + 2],
			nodeID - ENCODED_EDGE_NODE_OFFSETS[axis * 4 + 3],
		};

		// attempt to find the 4 voxels which share this edge
		int edgeVoxels[4];
		int numFoundVoxels = 0;
		for (int i = 0; i < 4; i++) {
			const auto iter = vertexIndices.find(voxelIDs[i]);
			if (iter != end(vertexIndices)) {
				edgeVoxels[numFoundVoxels++] = iter->second;
			}
		}

		// we can only generate a quad (or two triangles) if all 4 are found
		if (numFoundVoxels < 4) {
			continue;
		}

		if (info.winding) {
			triarray.Add(edgeVoxels[0]);
			triarray.Add(edgeVoxels[1]);
			triarray.Add(edgeVoxels[3]);

			triarray.Add(edgeVoxels[0]);
			triarray.Add(edgeVoxels[3]);
			triarray.Add(edgeVoxels[2]);
		} else {
			triarray.Add(edgeVoxels[0]);
			triarray.Add(edgeVoxels[3]);
			triarray.Add(edgeVoxels[1]);

			triarray.Add(edgeVoxels[0]);
			triarray.Add(edgeVoxels[2]);
			triarray.Add(edgeVoxels[3]);
		}
	}
}

//==========================================================================================================================================================

UVoxelDualContouringMeshComponent::UVoxelDualContouringMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}



void UVoxelDualContouringMeshComponent::BeginPlay() {
	Super::BeginPlay();

	// create empty voxel data
	VoxelData = new TVoxelData(256, 500);

	// create test shape
	//=========================================================================================================================
	static const float Extend = 100.f;
	VoxelData->forEach([&](int x, int y, int z) {
		FVector Pos = VoxelData->voxelIndexToVector(x, y, z);
		if (Pos.X < Extend && Pos.X > -Extend && Pos.Y < Extend && Pos.Y > -Extend && Pos.Z < Extend && Pos.Z > -Extend) {
			VoxelData->setDensity(x, y, z, 1);
			//DrawDebugPoint(GetWorld(), Pos + GetActorLocation(), 3, FColor(255, 0, 0), true, 10000000);
		}
	});

	
	FVector Pos(100, 100, 100);
	static const float R = 50.f;
	static const float Extend2 = R * 5.f;

	VoxelData->forEach([&](int x, int y, int z) {
		float density = VoxelData->getDensity(x, y, z);
		FVector o = VoxelData->voxelIndexToVector(x, y, z);
		o -= Pos;

		float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
		if (rl < Extend2) {
			float d = density + 1 / rl * R;
			VoxelData->setDensity(x, y, z, d);
		}

	});
	

	//=========================================================================================================================

	MakeMesh();
}

void UVoxelDualContouringMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	delete VoxelData;
	VoxelData = nullptr;
}



void UVoxelDualContouringMeshComponent::MakeMesh() {
	VoxelIDSet ActiveVoxels;
	EdgeInfoMap ActiveEdges;

	FindActiveVoxels(VoxelData, ActiveVoxels, ActiveEdges);

	UE_LOG(LogTemp, Warning, TEXT("activeVoxels --> %d"), ActiveVoxels.size());
	UE_LOG(LogTemp, Warning, TEXT("activeEdges  --> %d"), ActiveEdges.size());

	TArray<FVector> VertexArray;
	TArray<FVector> NormalArray;
	TArray<int32> TriangleArray;
	VoxelIndexMap VertexIndices;

	GenerateVertexData(ActiveVoxels, ActiveEdges, VertexIndices, VertexArray, NormalArray);

	UE_LOG(LogTemp, Warning, TEXT("varray --> %d"), VertexArray.Num());
	UE_LOG(LogTemp, Warning, TEXT("narray  --> %d"), NormalArray.Num());
	UE_LOG(LogTemp, Warning, TEXT("vertexIndices  --> %d"), VertexIndices.size());

	GenerateTriangles(ActiveEdges, VertexIndices, TriangleArray);

	UE_LOG(LogTemp, Warning, TEXT("triarray  --> %d"), TriangleArray.Num());

	TMeshDataPtr MeshDataPtr(new TMeshData());
	TMeshMaterialSection& MatSection = MeshDataPtr->MeshSectionLodArray[0].RegularMeshContainer.MaterialSectionMap.FindOrAdd(0);

	for (int i = 0; i < VertexArray.Num(); i++) {
		FProcMeshVertex Vertex;
		Vertex.Position = VertexArray[i];
		Vertex.Normal = NormalArray[i];

		MatSection.MaterialMesh.AddVertex(Vertex);
		MeshDataPtr->MeshSectionLodArray[0].WholeMesh.AddVertex(Vertex);
	}

	FVector Min = MatSection.MaterialMesh.SectionLocalBox.Min;
	FVector Max = MatSection.MaterialMesh.SectionLocalBox.Max;

	UE_LOG(LogTemp, Warning, TEXT("min  --> %f %f %f"), Min.X, Min.Y, Min.Z);
	UE_LOG(LogTemp, Warning, TEXT("max  --> %f %f %f"), Max.X, Max.Y, Max.Z);

	UE_LOG(LogTemp, Warning, TEXT("activeEdges  --> %d"), ActiveEdges.size());

	for (int i = 0; i < TriangleArray.Num(); i++) {
		MatSection.MaterialMesh.ProcIndexBuffer.Add(TriangleArray[i]);
		MeshDataPtr->MeshSectionLodArray[0].WholeMesh.ProcIndexBuffer.Add(TriangleArray[i]);
	}

	MeshDataPtr->CollisionMeshPtr = &MeshDataPtr->MeshSectionLodArray[0].WholeMesh;


	//bUseComplexAsSimpleCollision = false;
	//SetCollisionProfileName(TEXT("BlockAll"));
	//SetMobility(EComponentMobility::Movable);
	//SetSimulatePhysics(true);

	if (BasicMaterial != nullptr) {
		SetMaterial(0, BasicMaterial);
	}

	if (!bUseComplexAsSimpleCollision) {
		UVoxelMeshComponent::AddCollisionConvexMesh(VertexArray);
	}


	UVoxelMeshComponent::SetMeshData(MeshDataPtr);
	UVoxelMeshComponent::SetCollisionMeshData(MeshDataPtr);
}


void UVoxelDualContouringMeshComponent::EditMeshDeleteSphere(const FVector& Origin, float Radius, float Strength) {

	VoxelData->forEach([&](int x, int y, int z) {
		FVector Pos = VoxelData->voxelIndexToVector(x, y, z);
		Pos += GetComponentLocation();
		Pos -= Origin;

		float R = std::sqrt(Pos.X * Pos.X + Pos.Y * Pos.Y + Pos.Z * Pos.Z);
		if (R < Radius) {
			VoxelData->setDensity(x, y, z, 0);
		}
	});

	MakeMesh();
}