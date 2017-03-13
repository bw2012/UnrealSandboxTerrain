// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "SandboxTerrainMeshComponent.h"
#include "SandboxTerrainCollisionComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "TerrainZoneComponent.generated.h"


class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainZoneComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	USandboxTerrainMeshComponent* MainTerrainMesh;

	UPROPERTY()
	USandboxTerrainCollisionComponent* CollisionMesh;

	UPROPERTY()
	TMap<uint32, UHierarchicalInstancedStaticMeshComponent*> InstancedMeshMap;

public:

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	void MakeTerrain();

	TVoxelData* getVoxelData() { 
		return voxel_data; 
	};

	void SetVoxelData(TVoxelData* vd) {
		this->voxel_data = vd; 
	};

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> mesh_data_ptr, bool bPutToCache = true);

	std::shared_ptr<TMeshData> GenerateMesh();

	void SerializeInstancedMeshes(FBufferArchive& binaryData);

	void SaveInstancedMeshesToFile();

	void LoadInstancedMeshesFromFile();

	void SpawnInstancedMesh(FTerrainInstancedMeshType& MeshType, FTransform& transform);

	UTerrainRegionComponent* GetRegion() {
		return Cast<UTerrainRegionComponent>(GetAttachParent());
	}

private:
	TVoxelData* voxel_data;
};