// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "SandboxVoxeldata.h"
#include "SandboxTerrainController.h"
#include <memory>
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

	void PutMeshDataToCache(FVector& ZoneIndex, TMeshDataPtr MeshDataPtr) {
		MeshDataCache.Add(ZoneIndex, MeshDataPtr);
	}

	TMeshDataPtr GetMeshData(FVector& ZoneIndex) {
		if (MeshDataCache.Contains(ZoneIndex)) {
			return MeshDataCache[ZoneIndex];
		}

		return nullptr;
	}

	void CleanMeshDataCache() {
		MeshDataCache.Empty();
	}

	void ForEachMeshData(std::function<void(FVector&, TMeshDataPtr&)> Function) {
		for (auto& Elem : MeshDataCache) {
			Function(Elem.Key, Elem.Value);
		}
	}
	
	void SerializeRegionMeshData(FBufferArchive& BinaryData);

	void SerializeRegionVoxelData(FBufferArchive& BinaryData, TArray<TVoxelData*>& VoxalDataArray);

	void DeserializeRegionMeshData(FMemoryReader& BinaryData);

	void DeserializeRegionVoxelData(FMemoryReader& BinaryData);

	void SaveFile();

	void LoadFile();

	void SaveVoxelData(TArray<TVoxelData*>& VoxalDataArray);

	void LoadVoxelData();

private:

	void Save(std::function<void(FBufferArchive& BinaryData)> SaveFunction, FString& FileExt);

	void Load(std::function<void(FMemoryReader& BinaryData)> LoadFunction, FString& FileExt);

	TMap<FVector, TMeshDataPtr> MeshDataCache;

};