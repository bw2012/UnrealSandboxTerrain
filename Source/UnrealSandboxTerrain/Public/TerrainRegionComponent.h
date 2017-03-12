// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "SandboxVoxeldata.h"
#include "SandboxTerrainController.h"
#include "TerrainRegionComponent.generated.h"


class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainRegionComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UHierarchicalInstancedStaticMeshComponent* InstancedStaticMeshComponent;

public:

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	void PutMeshDataToCache(FVector ZoneIndex, TMeshDataPtr MeshDataPtr) {
		MeshDataCache.Add(ZoneIndex, MeshDataPtr);
	}

	TMeshDataPtr GetMeshData(FVector ZoneIndex) {
		if (MeshDataCache.Contains(ZoneIndex)) {
			return MeshDataCache[ZoneIndex];
		}

		return nullptr;
	}

	void CleanMeshDataCache() {
		MeshDataCache.Empty();
	}

	void SerializeRegionMeshData(FBufferArchive& BinaryData);

	void DeserializeRegionMeshData(FMemoryReader& BinaryData);

	void SaveFile();

	void LoadFile();

private:
	TMap<FVector, TMeshDataPtr> MeshDataCache;

};