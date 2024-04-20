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


struct TSyncItem {

	double Timestamp = 0;

	int Cnt = 0;
};

class TTerrainData {
    
private:


	std::shared_timed_mutex StorageMapMutex;
	std::unordered_map<TVoxelIndex, std::shared_ptr<TVoxelDataInfo>> StorageMap;

	std::shared_timed_mutex SaveIndexSetMutex;
	std::unordered_set<TVoxelIndex> SaveIndexSet;

	std::shared_timed_mutex SyncMapMutex;
	std::unordered_map<TVoxelIndex, TSyncItem> SyncMap;

	TMap<TVoxelIndex, TZoneModificationData> ModifiedVdMap;
	std::atomic<int32> MapVerHash = 0;
	std::mutex ModifiedVdMapMutex;

	std::atomic<int32> ZonesCount = 0;
    
public:

	int32 GetMapVStamp() {
		return MapVerHash;
	}

	void SetZoneVStamp(const TVoxelIndex& ZoneIndex, const int32 VStamp) {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		ModifiedVdMap.FindOrAdd(ZoneIndex).VStamp = VStamp;
	}

	TZoneModificationData GetZoneVStamp(const TVoxelIndex& ZoneIndex) {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		return ModifiedVdMap.FindOrAdd(ZoneIndex);
	}

	void SwapVStampMap(const TMap<TVoxelIndex, TZoneModificationData>& NewDataMap) {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		ModifiedVdMap.Empty();
		ModifiedVdMap = NewDataMap;
	}

	void AddUnsafe(const TVoxelIndex& ZoneIndex, const TZoneModificationData& Data) {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		ModifiedVdMap.Add(ZoneIndex, Data);
	}

	TMap<TVoxelIndex, TZoneModificationData> CloneVStampMap() {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		return ModifiedVdMap;
	}

	void IncreaseVStamp(const TVoxelIndex& ZoneIndex) {
		const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);
		TZoneModificationData& Data = ModifiedVdMap.FindOrAdd(ZoneIndex);
		Data.VStamp++;
		MapVerHash++;
	}

	bool IsSaveIndexEmpty() {
		std::unique_lock<std::shared_timed_mutex> Lock(SaveIndexSetMutex);
		return SaveIndexSet.size() == 0;
	}

	void AddSaveIndex(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(SaveIndexSetMutex);
		SaveIndexSet.insert(Index);
	}

	std::unordered_set<TVoxelIndex> PopSaveIndexSet() {
		std::unique_lock<std::shared_timed_mutex> Lock(SaveIndexSetMutex);
		std::unordered_set<TVoxelIndex> Res = SaveIndexSet;
		SaveIndexSet.clear();
		return Res;
	}

	void AddSyncItem(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		SyncMap[Index].Timestamp = FPlatformTime::Seconds();
		SyncMap[Index].Cnt++;
	}

	void AddSyncItem(const TSet<TVoxelIndex>& IndexSet) {
		std::unique_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		for (const auto& Index : IndexSet) {
			SyncMap[Index].Timestamp = FPlatformTime::Seconds();
			SyncMap[Index].Cnt++;
		}
	}

	void RemoveSyncItem(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		SyncMap.erase(Index);
	}

	bool IsOutOfSync(const TVoxelIndex& Index) {
		std::shared_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		return SyncMap.find(Index) != SyncMap.end();
	}

	int SyncMapSize() {
		std::shared_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		return (int)SyncMap.size();
	}

	std::unordered_set<TVoxelIndex> StaledSyncItems(const double Timeout) {
		std::shared_lock<std::shared_timed_mutex> Lock(SyncMapMutex);
		std::unordered_set<TVoxelIndex> result;

		const auto Timestamp = FPlatformTime::Seconds();

		for (const auto& P : SyncMap) {
			const auto& Index = P.first;
			const auto& Itm = P.second;
			
			if (Timestamp - Itm.Timestamp > Timeout) {
				result.insert(Index);
			}
		}

		return result;
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
		ZonesCount++;
    }
    
    UTerrainZoneComponent* GetZone(const TVoxelIndex& Index){
        return GetVoxelDataInfo(Index)->GetZone();
    }

	void RemoveZone(const TVoxelIndex& Index) {
		auto Ptr = GetVoxelDataInfo(Index);
		Ptr->ResetSpawnFinished();
		Ptr->RemoveZone();
		ZonesCount--;
	}
    
	//=====================================================================================
	// terrain voxel data 
	//=====================================================================================
    
	TVoxelDataInfoPtr GetVoxelDataInfo(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(StorageMapMutex);
		if (StorageMap.find(Index) != StorageMap.end()) {
			return StorageMap[Index];
		} else {
			TVoxelDataInfoPtr NewVdinfo = std::make_shared<TVoxelDataInfo>();
			StorageMap.insert({ Index, NewVdinfo });
			return NewVdinfo;
		}
    }

	//=====================================================================================
	// clean all 
	//=====================================================================================

    void Clean(){
		// no locking because end play only
		StorageMap.clear();
		ModifiedVdMap.Empty();
    }
};

