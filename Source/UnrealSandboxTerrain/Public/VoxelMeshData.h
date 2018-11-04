#pragma once

#include "EngineMinimal.h"
#include "VoxelData.h"
#include "ProcMeshData.h"

#include <list>
#include <array>
#include <memory>
#include <set>
#include <mutex>
#include <functional>


// mesh per one material
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

	double TimeStamp = 0;

	TMeshData() {
		MeshSectionLodArray.SetNum(LOD_ARRAY_SIZE); // 64
		CollisionMeshPtr = nullptr;
	}

	~TMeshData() {
		// for memory leaks checking
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