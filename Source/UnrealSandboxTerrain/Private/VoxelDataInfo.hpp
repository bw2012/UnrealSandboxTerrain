//
//  VoxelDataInfo.h
//  UE4VoxelTerrain
//
//  Created by Admin on 20.04.2020.
//

#pragma once

#include "VoxelData.h"
#include <atomic>

enum TVoxelDataState : uint32 {
    UNDEFINED = 0,
    GENERATED = 1,
    LOADED = 2,
    READY_TO_LOAD = 3
};

class TVoxelDataInfo {

private:
    volatile double LastChange;
    volatile double LastSave;
    volatile double LastMeshGeneration;
    volatile double LastCacheCheck;

	TMeshDataPtr MeshDataCachePtr = nullptr;

	std::shared_timed_mutex ZoneMutex;
	std::atomic<UTerrainZoneComponent*> ZoneComponentAtomicPtr = nullptr;

	std::shared_timed_mutex InstanceObjectMapMutex;
	std::shared_ptr<TInstanceMeshTypeMap> InstanceMeshTypeMapPtr = nullptr;

public:
    TVoxelData* Vd = nullptr;
    TVoxelDataState DataState = TVoxelDataState::UNDEFINED;
    std::shared_ptr<std::mutex> VdMutexPtr;
    
    TVoxelDataInfo() {
		VdMutexPtr = std::make_shared<std::mutex>();
    }
    
    ~TVoxelDataInfo() { 
		//UE_LOG(LogSandboxTerrain, Warning, TEXT("~TVoxelDataInfo()"));
		if (Vd != nullptr) {
			delete Vd;
			Vd = nullptr;
		}
	}

    bool IsNewGenerated() const {
        return DataState == TVoxelDataState::GENERATED;
    }
    
    bool IsNewLoaded() const {
        return DataState == TVoxelDataState::LOADED;
    }
    
    void SetChanged() {
        LastChange = FPlatformTime::Seconds();
    }
    
    bool IsChanged() {
        return LastChange > LastSave;
    }
    
    void ResetLastSave() {
        LastSave = FPlatformTime::Seconds();
    }
    
    bool IsNeedToRegenerateMesh() {
        return LastChange > LastMeshGeneration;
    }
    
    void ResetLastMeshRegenerationTime() {
        LastMeshGeneration = FPlatformTime::Seconds();
    }

    void Unload(){
        if (Vd != nullptr) {
            delete Vd;
            Vd = nullptr;
        }
        DataState = TVoxelDataState::READY_TO_LOAD;
    }

	void PushMeshDataCache(TMeshDataPtr MeshDataPtr) {
		std::atomic_store(&MeshDataCachePtr, MeshDataPtr);
	}

	TMeshDataPtr PopMeshDataCache() {
		TMeshDataPtr NullPtr = nullptr;
		TMeshDataPtr MeshDataPtr = std::atomic_exchange(&MeshDataCachePtr, NullPtr);
		return MeshDataPtr;
	}

	void AddZone(UTerrainZoneComponent* ZoneComponent) {
		ZoneComponentAtomicPtr.store(ZoneComponent);
	}

	UTerrainZoneComponent* GetZone() {
		UTerrainZoneComponent* ZoneComponent = ZoneComponentAtomicPtr.load();
		return ZoneComponent;
	}

	std::shared_ptr<TInstanceMeshTypeMap> GetOrCreateInstanceObjectMap() {
		std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectMapMutex);
		if (!InstanceMeshTypeMapPtr) {
			InstanceMeshTypeMapPtr = std::make_shared<TInstanceMeshTypeMap>();
		}
		return InstanceMeshTypeMapPtr;
	}

	std::shared_ptr<TInstanceMeshTypeMap> PopInstanceObjectMap() {
		std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectMapMutex);
		auto Res = InstanceMeshTypeMapPtr;
		InstanceMeshTypeMapPtr = nullptr;
		return Res;
	}

};


typedef std::shared_ptr<TVoxelDataInfo> TVoxelDataInfoPtr;