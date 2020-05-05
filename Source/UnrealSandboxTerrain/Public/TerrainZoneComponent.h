// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "TerrainZoneComponent.generated.h"


class ASandboxTerrainController;

typedef struct TInstanceMeshArray {

	TArray<FTransform> TransformArray;

	FTerrainInstancedMeshType MeshType;

} TInstanceMeshArray;

typedef TMap<int32, TInstanceMeshArray> TInstanceMeshTypeMap;

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

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0);

	std::shared_ptr<std::vector<uint8>> SerializeInstancedMeshes();

	void DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstanceMeshTypeMap& ZoneInstMeshMap);

	void SpawnAll(const TInstanceMeshTypeMap& InstanceMeshMap);

	void SpawnInstancedMesh(const FTerrainInstancedMeshType& MeshType, const FTransform& transform);

	//TMeshData const * GetCachedMeshData();
    //TMeshDataPtr GetCachedMeshData();
    
    //bool HasCachedMeshData();
    
    //TValueDataPtr SerializeAndClearCachedMeshData();

	//void ClearCachedMeshData();

    TValueDataPtr SerializeAndResetObjectData();

	static TValueDataPtr SerializeInstancedMesh(const TInstanceMeshTypeMap& InstanceMeshMap);
    
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
    
	double MeshDataTimeStamp;

    std::mutex TerrainMeshMutex;
    
    std::mutex InstancedMeshMutex;
    
    TTerrainLodMask CurrentTerrainLodMask;

	//TMeshDataPtr CachedMeshDataPtr;

	bool bIsObjectsNeedSave = false;
};
