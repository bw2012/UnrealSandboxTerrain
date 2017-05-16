// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "SandboxVoxeldata.h"
#include "SandboxTerrainController.h"
#include <memory>
#include <fstream>
#include "TerrainRegionComponent.generated.h"


class ASandboxTerrainController;

typedef struct TInstMeshTransArray {

	TArray<FTransform> TransformArray;

	FTerrainInstancedMeshType MeshType;

} TInstMeshTransArray;


typedef TMap<int32, TInstMeshTransArray> TInstMeshTypeMap;

typedef TMap<FVector, TInstMeshTypeMap> TInstMeshZoneTemp;


typedef struct TVoxelDataFileBodyPos {

	int64 Offset;

	int64 Size;

} TVoxelDataFileBodyPos;


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

	virtual void BeginDestroy();

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

	void SetChanged() {	bIsChanged = true; }

	bool IsChanged() { return bIsChanged; }
	
	void SerializeRegionMeshData(FBufferArchive& BinaryData);

	void SerializeRegionVoxelData(FBufferArchive& BinaryData, TArray<TVoxelData*>& VoxalDataArray);

	void DeserializeRegionMeshData(FMemoryReader& BinaryData);

	void DeserializeRegionVoxelData(FMemoryReader& BinaryData);


	void SerializeInstancedMeshes(FBufferArchive& BinaryData, TArray<UTerrainZoneComponent*>& ZoneArray);

	void DeserializeZoneInstancedMeshes(FMemoryReader& BinaryData, TInstMeshTypeMap& ZoneInstMeshMap);

	void DeserializeRegionInstancedMeshes(FMemoryReader& BinaryData);


	void SaveFile(TArray<UTerrainZoneComponent*>& ZoneArray);

	void LoadFile();

	void SaveVoxelData(TArray<TVoxelData*>& VoxalDataArray);

	void SaveVoxelData2(TArray<TVoxelData*>& VoxalDataArray);

	void LoadVoxelData();

	void LoadVoxelData2();

	TVoxelData* LoadVoxelData3(FVector Index);

	void SpawnInstMeshFromLoadCache(UTerrainZoneComponent* Zone);

private:

	void Save(std::function<void(FBufferArchive& BinaryData)> SaveFunction, FString& FileExt);

	void Load(std::function<void(FMemoryReader& BinaryData)> LoadFunction, FString& FileExt);

	TMap<FVector, TMeshDataPtr> MeshDataCache;

	TInstMeshZoneTemp InstancedMeshLoadCache;

	bool bIsChanged = false;

	//========================================================================================
	// lazy voxel data loading
	//========================================================================================

	std::ifstream* VdInFilePtr = nullptr;

	int64 VdBinaryDataStart;

	TMap<FVector, TVoxelDataFileBodyPos> VdFileBodyMap;

	std::mutex VdFileMutex;
};