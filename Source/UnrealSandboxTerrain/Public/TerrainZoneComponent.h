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

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, bool bPutToCache = true, const TTerrainLodMask TerrainLodMask = 0);

	std::shared_ptr<std::vector<uint8>> SerializeInstancedMeshes();

	void DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstMeshTypeMap& ZoneInstMeshMap);

	void SpawnInstancedMesh(FTerrainInstancedMeshType& MeshType, FTransform& transform);

	//TMeshData const * GetCachedMeshData();
    //TMeshDataPtr GetCachedMeshData();
    
    bool HasCachedMeshData();
    
    TValueDataPtr SerializeAndClearCachedMeshData();

	void ClearCachedMeshData();

    TValueDataPtr SerializeAndResetObjectData();
    
	void SetNeedSave() {
		bIsObjectsNeedSave = true;
	}

	bool IsNeedSave() {
		return bIsObjectsNeedSave;
	}

    TTerrainLodMask GetTerrainLodMask(){
        return CurrentTerrainLodMask;
    }
    
private:
    
    std::mutex TerrainMeshMutex;
    
    std::mutex InstancedMeshMutex;
    
    TTerrainLodMask CurrentTerrainLodMask;

	TMeshDataPtr CachedMeshDataPtr;

	bool bIsObjectsNeedSave = false;
};
