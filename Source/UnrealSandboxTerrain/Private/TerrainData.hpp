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

class TTerrainData {
    
private:
    std::shared_timed_mutex VoxelDataMutex;
    std::unordered_map<TVoxelIndex, TVoxelDataInfo*> VoxelDataIndexMap;

	std::shared_timed_mutex ZoneMapMutex;
    TMap<FVector, UTerrainZoneComponent*> TerrainZoneMap;

	std::shared_timed_mutex MeshDataMutex;
	std::unordered_map<TVoxelIndex, TMeshDataPtr> MeshDataCaheIndexMap;

	std::shared_timed_mutex InstanceObjectDataMutex;
	std::unordered_map<TVoxelIndex, TInstanceMeshTypeMap> InstanceObjectDataMap;
    
public:

	//=====================================================================================
	// instance objects 
	//=====================================================================================

	TInstanceMeshTypeMap& GetInstanceObjectTypeMap(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectDataMutex);
		if (InstanceObjectDataMap.find(Index) == InstanceObjectDataMap.end()) {
			InstanceObjectDataMap.insert({ Index, TInstanceMeshTypeMap() });
		}

		return InstanceObjectDataMap[Index];
	}

	void RemoveInstanceObjectTypeMap(const TVoxelIndex& Index) {
		std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectDataMutex);
		if (InstanceObjectDataMap.find(Index) != InstanceObjectDataMap.end()) {
			InstanceObjectDataMap.erase(Index);
		}
	}

	void ForEachInstanceObjectSafeAndClear(std::function<void(const TVoxelIndex& Index, const TInstanceMeshTypeMap& InstanceObjectMap)> Function) {
		std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectDataMutex);
		for (auto& It : InstanceObjectDataMap) {
			const auto& Index = It.first;
			const TInstanceMeshTypeMap& InstanceObjectMap = InstanceObjectDataMap[It.first];
			Function(Index, InstanceObjectMap);
		}
		InstanceObjectDataMap.clear();
	}

	//=====================================================================================
	// terrain zone mesh 
	//=====================================================================================

	void PutMeshDataToCache(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr) {
		std::unique_lock<std::shared_timed_mutex> Lock(MeshDataMutex);
		auto It = MeshDataCaheIndexMap.find(Index);
		if (It != MeshDataCaheIndexMap.end()) {
			MeshDataCaheIndexMap.erase(It);
		}
		MeshDataCaheIndexMap.insert({ Index, MeshDataPtr });
	}

	void ForEachMeshDataSafeAndClear(std::function<void(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr)> Function) {
		std::unique_lock<std::shared_timed_mutex> Lock(MeshDataMutex);
		for (auto& It : MeshDataCaheIndexMap) {
			const auto& Index = It.first;
			TMeshDataPtr MeshDataPtr = MeshDataCaheIndexMap[It.first];
			Function(Index, MeshDataPtr);
		}
		MeshDataCaheIndexMap.clear();
	}

	//=====================================================================================
	// terrain zone 
	//=====================================================================================

    void AddZone(const FVector& Pos, UTerrainZoneComponent* ZoneComponent){
        std::unique_lock<std::shared_timed_mutex> Lock(ZoneMapMutex);
        TerrainZoneMap.Add(Pos, ZoneComponent);
    }
    
    UTerrainZoneComponent* GetZone(const FVector& Pos){
        std::shared_lock<std::shared_timed_mutex> Lock(ZoneMapMutex);
        if (TerrainZoneMap.Contains(Pos)) {
            return TerrainZoneMap[Pos];
        }
        return nullptr;
    }
    
    void ForEachZoneSafe(std::function<void(const FVector Pos, UTerrainZoneComponent* Zone)> Function){
        std::unique_lock<std::shared_timed_mutex> Lock(ZoneMapMutex);
        for (auto& Elem : TerrainZoneMap) {
            FVector Pos = Elem.Key;
            UTerrainZoneComponent* Zone = Elem.Value;
            Function(Pos, Zone);
        }
    }

	//=====================================================================================
	// terrina voxel data 
	//=====================================================================================
    
    void RegisterVoxelData(TVoxelDataInfo* VdInfo, TVoxelIndex Index) {
        std::unique_lock<std::shared_timed_mutex> Lock(VoxelDataMutex);
        auto It = VoxelDataIndexMap.find(Index);
        if (It != VoxelDataIndexMap.end()) {
            VoxelDataIndexMap.erase(It);
        }
        VoxelDataIndexMap.insert({ Index, VdInfo });
    }
    
    TVoxelData* GetVd(const TVoxelIndex& Index) {
        std::shared_lock<std::shared_timed_mutex> Lock(VoxelDataMutex);
        if (VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end()) {
            TVoxelDataInfo* VdInfo = VoxelDataIndexMap[Index];
            return VdInfo->Vd;
        }

        return nullptr;
    }

    bool HasVoxelData(const TVoxelIndex& Index) {
        std::shared_lock<std::shared_timed_mutex> Lock(VoxelDataMutex);
        return VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end();
    }

    TVoxelDataInfo* GetVoxelDataInfo(const TVoxelIndex& Index) {
        std::shared_lock<std::shared_timed_mutex> Lock(VoxelDataMutex);
        if (VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end()) {
            return VoxelDataIndexMap[Index];
        }

        return nullptr;
    }
    
    void ForEachVdSafe(std::function<void(const TVoxelIndex& Index, TVoxelDataInfo* VdInfo)> Function){
        std::unique_lock<std::shared_timed_mutex> Lock(VoxelDataMutex);
        for (auto& It : VoxelDataIndexMap) {
            const auto& Index = It.first;
            TVoxelDataInfo* VdInfo = VoxelDataIndexMap[It.first];
            Function(Index, VdInfo);
        }
    }

	//=====================================================================================
	// clean all 
	//=====================================================================================

    void Clean(){
		// no locking because end play only
        TerrainZoneMap.Empty();
		MeshDataCaheIndexMap.clear();
		InstanceObjectDataMap.clear();
        
        for (auto& It : VoxelDataIndexMap) {
            TVoxelDataInfo* VdInfo = It.second;
            if(VdInfo){
                VdInfo->Unload();
                delete VdInfo;
            }
        }
        VoxelDataIndexMap.clear();
    }
};

