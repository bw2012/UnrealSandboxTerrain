// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "VoxelMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "TerrainZoneComponent.generated.h"


class ASandboxTerrainController;
struct TInstanceMeshArray;
struct FTerrainInstancedMeshType;
typedef TMap<uint64, TInstanceMeshArray> TInstanceMeshTypeMap;


class UTerrainZoneComponent;

typedef std::vector<uint8> TData;
typedef std::shared_ptr<TData> TDataPtr;


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainInstancedStaticMesh : public UHierarchicalInstancedStaticMeshComponent { //UHierarchicalInstancedStaticMeshComponent
	GENERATED_UCLASS_BODY()

	friend class UTerrainZoneComponent;

public:

	UPROPERTY()
	uint32 MeshTypeId = 0;

	UPROPERTY()
	uint32 MeshVariantId = 0;

	bool IsFoliage();

private:

	bool bIsFoliage;
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

	virtual void FinishDestroy() override;

	virtual void DestroyComponent(bool bPromoteChildren) override;

	ASandboxTerrainController* GetTerrainController();

	void ApplyTerrainMesh(std::shared_ptr<TMeshData> MeshDataPtr, bool bIgnoreCollision = false);

	std::shared_ptr<std::vector<uint8>> SerializeInstancedMeshes();

	void SpawnAll(const TInstanceMeshTypeMap& InstanceMeshMap);

	void SpawnInstancedMesh(const FTerrainInstancedMeshType& MeshType, const TInstanceMeshArray& InstMeshTransArray);

    TDataPtr SerializeAndResetObjectData();

	static TDataPtr SerializeInstancedMesh(const TInstanceMeshTypeMap& InstanceMeshMap);

private:
    
	double MeshDataTimeStamp;

	uint32 VStamp = 0;

    std::mutex TerrainMeshMutex;
    
    std::mutex InstancedMeshMutex;
};
