// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "TerrainZoneComponent.generated.h"


class ASandboxTerrainController;


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainInstancedStaticMesh : public UHierarchicalInstancedStaticMeshComponent {
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	uint32 MeshTypeId = 0;

	UPROPERTY()
	uint32 MeshVariantId = 0;

};


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainZoneComponent : public USceneComponent {
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UVoxelMeshComponent* MainTerrainMesh;

	UPROPERTY()
	TMap<uint64, UTerrainInstancedStaticMesh*> InstancedMeshMap;

public:

	ASandboxTerrainController* GetTerrainController();

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, const TTerrainLodMask TerrainLodMask = 0);

	std::shared_ptr<std::vector<uint8>> SerializeInstancedMeshes();

	void DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstanceMeshTypeMap& ZoneInstMeshMap);

	void SpawnAll(const TInstanceMeshTypeMap& InstanceMeshMap);

	void SpawnInstancedMesh(const FTerrainInstancedMeshType& MeshType, const TInstanceMeshArray& InstMeshTransArray);

	//TMeshData const * GetCachedMeshData();
    //TMeshDataPtr GetCachedMeshData();
    
    //bool HasCachedMeshData();
    
    //TValueDataPtr SerializeAndClearCachedMeshData();

	//void ClearCachedMeshData();

    TValueDataPtr SerializeAndResetObjectData();

	static TValueDataPtr SerializeInstancedMesh(const TInstanceMeshTypeMap& InstanceMeshMap);
    
	void SetNeedSave();

	bool IsNeedSave();

	TTerrainLodMask GetTerrainLodMask();
    
	volatile bool bIsSpawnFinished = false;

private:
    
	double MeshDataTimeStamp;

    std::mutex TerrainMeshMutex;
    
    std::mutex InstancedMeshMutex;
    
    TTerrainLodMask CurrentTerrainLodMask;

	//TMeshDataPtr CachedMeshDataPtr;

	volatile bool bIsObjectsNeedSave = false;
};
