// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "VoxelDcMeshComponent.h"
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

#define VOXEL_GRID_SIZE			16
#define VOXEL_GRID_OFFSET		(float)VOXEL_GRID_SIZE / 2.f 


static const FVector4 AXIS_OFFSET[3] = {
	FVector4(1.f, 0.f, 0.f, 0.f),
	FVector4(0.f, 1.f, 0.f, 0.f),
	FVector4(0.f, 0.f, 1.f, 0.f)
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


float Density(const FVector4& p) {
	const float Extend = 3.f;

	if (p.X < Extend && p.X > -Extend && p.Y < Extend && p.Y > -Extend && p.Z < Extend && p.Z > -Extend) {
		return 1.f;
	}

	return -1.f;
}

// returns x * (1.0 - a) + y * a 
// the linear blend of x and y using the floating-point value a
FVector4 mix(const FVector4& x, const FVector4& y, float a) {
	return x * (1.f - a) + y * a;
}

float FindIntersection(const FVector4& p0, const FVector4& p1) {
	const int FIND_EDGE_INFO_STEPS = 16;
	const float FIND_EDGE_INFO_INCREMENT = 1.f / FIND_EDGE_INFO_STEPS;

	float minValue = FLT_MAX;
	float currentT = 0.f;
	float t = 0.f;
	for (int i = 0; i < FIND_EDGE_INFO_STEPS; i++) {
		const FVector4 p = mix(p0, p1, currentT);
		const float	d = std::abs(Density(p));

		if (d < minValue) {
			t = currentT;
			minValue = d;
		}

		currentT += FIND_EDGE_INFO_INCREMENT;
	}

	return t;
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
	for (int x = 0; x < VOXEL_GRID_SIZE; x++) {
		for (int y = 0; y < VOXEL_GRID_SIZE; y++) {
			for (int z = 0; z < VOXEL_GRID_SIZE; z++) {

				const TVoxelIndex4 idxPos(x, y, z, 0);
				const FVector4 p(x - VOXEL_GRID_OFFSET, y - VOXEL_GRID_OFFSET, z - VOXEL_GRID_OFFSET, 1.f);

				for (int axis = 0; axis < 3; axis++) {

					const FVector4 q = p + AXIS_OFFSET[axis];

					const float pDensity = Density(p);
					const float qDensity = Density(q);

					const bool zeroCrossing = (pDensity >= 0.f && qDensity < 0.f) || (pDensity < 0.f && qDensity >= 0.f);

					if (!zeroCrossing) continue;

					//UE_LOG(LogTemp, Warning, TEXT("x, y, z - pDensity, qDensity  --> %d %d %d - %f %f"), x, y, z, pDensity, qDensity);

					const float t = FindIntersection(p, q);
					const FVector4 pos(mix(p, q, t), 1.f);

					const float H = 0.001f;

					FVector4 tmp(
						Density(pos + FVector4(H, 0.f, 0.f, 0.f)) - Density(pos - FVector4(H, 0.f, 0.f, 0.f)),
						Density(pos + FVector4(0.f, H, 0.f, 0.f)) - Density(pos - FVector4(0.f, H, 0.f, 0.f)),
						Density(pos + FVector4(0.f, 0.f, H, 0.f)) - Density(pos - FVector4(0.f, 0.f, H, 0.f)),
						0.f);

					auto normal = tmp.GetSafeNormal(0.000001f);

					EdgeInfo info;
					info.pos = pos;
					info.normal = normal;
					info.winding = pDensity >= 0.f;

					const auto code = EncodeAxisUniqueID(axis, x, y, z);

					//UE_LOG(LogTemp, Warning, TEXT("code  --> %d"), code);

					activeEdges[code] = info;

					const auto edgeNodes = EDGE_NODE_OFFSETS[axis];
					for (int i = 0; i < 4; i++) {
						const auto nodeIdxPos = idxPos - edgeNodes[i];
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


UVoxelDcMeshComponent::UVoxelDcMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}



void UVoxelDcMeshComponent::BeginPlay() {
	Super::BeginPlay();

	VoxelData = new TVoxelData(128, 100.f);
	static const float Extend = 50.f;

	VoxelData->forEach([&](int x, int y, int z) {
		FVector Pos = VoxelData->voxelIndexToVector(x, y, z);
		if (Pos.X < Extend && Pos.X > -Extend && Pos.Y < Extend && Pos.Y > -Extend && Pos.Z < Extend && Pos.Z > -Extend) {
			VoxelData->setDensity(x, y, z, 1);
		}
	});

	VoxelIDSet activeVoxels;
	EdgeInfoMap activeEdges;

	FindActiveVoxels(VoxelData, activeVoxels, activeEdges);

	UE_LOG(LogTemp, Warning, TEXT("activeVoxels --> %d"), activeVoxels.size());
	UE_LOG(LogTemp, Warning, TEXT("activeEdges  --> %d"), activeEdges.size());
	
	TArray<FVector> varray;
	TArray<FVector> narray;
	TArray<int32> triarray;
	VoxelIndexMap vertexIndices;

	GenerateVertexData(activeVoxels, activeEdges, vertexIndices, varray, narray);

	UE_LOG(LogTemp, Warning, TEXT("varray --> %d"), varray.Num());
	UE_LOG(LogTemp, Warning, TEXT("narray  --> %d"), narray.Num());
	UE_LOG(LogTemp, Warning, TEXT("vertexIndices  --> %d"), vertexIndices.size());

	GenerateTriangles(activeEdges, vertexIndices, triarray);

	UE_LOG(LogTemp, Warning, TEXT("triarray  --> %d"), triarray.Num());

	TArray<FVector2D> UV0;
	TArray<FLinearColor> vertexColors;
	TArray<FProcMeshTangent> tangents;

	TMeshDataPtr MeshDataPtr(new TMeshData());
	TMeshMaterialSection& MatSection = MeshDataPtr->MeshSectionLodArray[0].RegularMeshContainer.MaterialSectionMap.FindOrAdd(0);

	for (int i = 0; i < varray.Num(); i++) {
		FProcMeshVertex vertex;
		vertex.Position = varray[i] * 100;
		vertex.Normal = narray[i];

		MatSection.MaterialMesh.ProcVertexBuffer.Add(vertex);
		//DrawDebugPoint(GetWorld(), FVector(pos.X * 100, pos.Y * 100, pos.Z * 100), 5, FColor(255, 0, 0), true, 10000000);
	}

	for (int i = 0; i < triarray.Num(); i++) {
		MatSection.MaterialMesh.ProcIndexBuffer.Add(triarray[i]);
	}

	USandboxTerrainMeshComponent::SetMeshData(MeshDataPtr);

}