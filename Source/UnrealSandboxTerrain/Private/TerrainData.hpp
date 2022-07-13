//
//  TerrainData.h
//  UE4VoxelTerrain
//
//  Created by blackw2012 on 19.04.2020..
//

#pragma once

#include "EngineMinimal.h"
#include "VoxelIndex.h"
#include "VoxelData.h"
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <atomic>
#include <unordered_set>

static const uint64 StorageZoneCap = 100;
static const uint64 StorageSize = (StorageZoneCap + StorageZoneCap + 1) * (StorageZoneCap + StorageZoneCap + 1) * (StorageZoneCap + StorageZoneCap + 1);

template<class T>
class TStorage {

private:
	std::array<std::shared_ptr<T>, StorageSize> PtrArray;

public:

	TStorage() {
		// initialize
		for (auto& Ptr : PtrArray) {
			Ptr = nullptr;
		}
	}

	std::shared_ptr<T> Get(uint64 Index) {
		std::shared_ptr<T> Current = std::atomic_load(&PtrArray[Index]);
		if (!Current) {
			std::shared_ptr<T> New = std::make_shared<T>();
			if (std::atomic_compare_exchange_weak(&PtrArray[Index], &Current, New)) {
				return New;
			}
		}
		return Current;
	}

	std::shared_ptr<T> operator[](uint64 Index) { 
		return Get(Index);
	}
};


class TTerrainData {
    
private:

	TStorage<TVoxelDataInfo> Storage;

	std::shared_timed_mutex StorageMapMutex;
	std::unordered_map<TVoxelIndex, std::shared_ptr<TVoxelDataInfo>> StorageMap;

	std::shared_timed_mutex SaveIndexSetpMutex;
	std::unordered_set<TVoxelIndex> SaveIndexSet;

	int64 ClcStorageLinearindex(const TVoxelIndex& Index) {
		int32 x = Index.X + StorageZoneCap;
		int32 y = Index.Y + StorageZoneCap;
		int32 z = Index.Z + StorageZoneCap;
		static const int n = (StorageZoneCap + StorageZoneCap + 1);
		int64 LinearIndex = x * n * n + y * n + z;
		return LinearIndex;
	}
    
public:

	void AddSaveIndex(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(SaveIndexSetpMutex);
		SaveIndexSet.insert(Index);
	}

	std::unordered_set<TVoxelIndex> PopSaveIndexSet() {
		std::unique_lock<std::shared_timed_mutex> Lock(SaveIndexSetpMutex);
		std::unordered_set<TVoxelIndex> Res = SaveIndexSet;
		SaveIndexSet.clear();
		return Res;
	}

	//=====================================================================================
	// instance objects 
	//=====================================================================================

	std::shared_ptr<TInstanceMeshTypeMap> GetOrCreateInstanceObjectMap(const TVoxelIndex& Index) {
		return GetVoxelDataInfo(Index)->GetOrCreateInstanceObjectMap();
	}

	//=====================================================================================
	// terrain zone mesh 
	//=====================================================================================

	void PutMeshDataToCache(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr) {
		GetVoxelDataInfo(Index)->PushMeshDataCache(MeshDataPtr);
	}

	//=====================================================================================
	// terrain zone 
	//=====================================================================================

    void AddZone(const TVoxelIndex& Index, UTerrainZoneComponent* ZoneComponent){
		GetVoxelDataInfo(Index)->AddZone(ZoneComponent);
    }
    
    UTerrainZoneComponent* GetZone(const TVoxelIndex& Index){
        return GetVoxelDataInfo(Index)->GetZone();
    }

	void RemoveZone(const TVoxelIndex& Index) {
		return GetVoxelDataInfo(Index)->RemoveZone();
	}
    
	//=====================================================================================
	// terrain voxel data 
	//=====================================================================================
    
	TVoxelDataInfoPtr GetVoxelDataInfo(const TVoxelIndex& Index) {
		int64 LinearIndex = ClcStorageLinearindex(Index);
		if (LinearIndex > 0 && LinearIndex < StorageSize) {
			return Storage.Get(LinearIndex);
		} else {
			std::unique_lock<std::shared_timed_mutex> Lock(StorageMapMutex);
			if (StorageMap.find(Index) != StorageMap.end()) {
				return StorageMap[Index];
			} else {
				TVoxelDataInfoPtr NewVdinfo = std::make_shared<TVoxelDataInfo>();
				StorageMap.insert({ Index, NewVdinfo });
				return NewVdinfo;
			}
		}
    }

	//=====================================================================================
	// clean all 
	//=====================================================================================

    void Clean(){
		// no locking because end play only
		StorageMap.clear();
    }
};

