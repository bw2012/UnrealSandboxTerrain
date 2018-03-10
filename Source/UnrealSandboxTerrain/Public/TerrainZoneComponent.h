// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "SandboxTerrainMeshComponent.h"
#include "SandboxTerrainCollisionComponent.h"
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
	USandboxTerrainMeshComponent* MainTerrainMesh;

	UPROPERTY()
	USandboxTerrainCollisionComponent* CollisionMesh;

	UPROPERTY()
	TMap<uint32, UHierarchicalInstancedStaticMeshComponent*> InstancedMeshMap;

public:

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, bool bPutToCache = true);

	void SerializeInstancedMeshes(FBufferArchive& binaryData);

	void DeserializeInstancedMeshes(FMemoryReader& BinaryData, TInstMeshTypeMap& ZoneInstMeshMap);

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