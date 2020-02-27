// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "TerrainZoneComponent.generated.h"


class ASandboxTerrainController;

typedef struct TInstMeshTransArray {

	TArray<FTransform> TransformArray;

	FTerrainInstancedMeshType MeshType;

} TInstMeshTransArray;

typedef TMap<int32, TInstMeshTransArray> TInstMeshTypeMap;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainZoneComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UVoxelMeshComponent* MainTerrainMesh;

	UPROPERTY()
	TMap<uint32, UHierarchicalInstancedStaticMeshComponent*> InstancedMeshMap;

public:

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, bool bPutToCache = true);

	std::shared_ptr<std::vector<uint8>> SerializeInstancedMeshes();

	void DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstMeshTypeMap& ZoneInstMeshMap);

	void SpawnInstancedMesh(FTerrainInstancedMeshType& MeshType, FTransform& transform);

	TMeshData const * GetCachedMeshData();

	void ClearCachedMeshData();

	void SetNeedSave() {
		bIsObjectsNeedSave = true;
	}

	void ResetNeedSave() {
		bIsObjectsNeedSave = false;
	}

	bool IsNeedSave() {
		return bIsObjectsNeedSave;
	}

private:

	TMeshDataPtr CachedMeshDataPtr;

	bool bIsObjectsNeedSave = false;
};