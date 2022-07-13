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
    READY_TO_LOAD = 3, 
    GENERATION_IN_PROGRESS = 4,
    UNGENERATED = 5
};


class TSpinlock {
private:
    std::atomic_flag atomic_flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (atomic_flag.test_and_set(std::memory_order_acquire)) { }
    }

    void unlock() {
        atomic_flag.clear(std::memory_order_release);
    }
};

class TVoxelDataInfo {

private:
    volatile double LastChange;
    volatile double LastSave;
    volatile double LastMeshGeneration;
    volatile double LastCacheCheck;

	TMeshDataPtr MeshDataCachePtr = nullptr;

	//std::shared_timed_mutex ZoneMutex;
	std::atomic<UTerrainZoneComponent*> ZoneComponentAtomicPtr = nullptr;

	std::shared_timed_mutex InstanceObjectMapMutex;
	std::shared_ptr<TInstanceMeshTypeMap> InstanceMeshTypeMapPtr = nullptr;

    bool bSoftUnload = false;
    //std::shared_ptr<std::mutex> VdMutexPtr;
    //std::mutex VdMutex;
    //TSpinlock VdMutex;
    FCriticalSection VdMutex;

public:

    TVoxelData* Vd = nullptr;
    volatile TVoxelDataState DataState = TVoxelDataState::UNDEFINED;
    
    TVoxelDataInfo() {
		//VdMutexPtr = std::make_shared<std::mutex>();
		LastChange = 0;
		LastSave = 0;
		LastMeshGeneration = 0;
		LastCacheCheck = 0;
    }
    
    ~TVoxelDataInfo() { 
		//UE_LOG(LogSandboxTerrain, Log, TEXT("~TVoxelDataInfo()"));
		if (Vd != nullptr) {
			delete Vd;
			Vd = nullptr;
		}
	}

    void Lock() {
        VdMutex.Lock();

        /*
        try {
            VdMutex.lock();
        } catch (std::exception e) {
            FString ExceptionString(e.what());
            UE_LOG(LogSandboxTerrain, Warning, TEXT("Exception: %s"), *ExceptionString);
        }
        */
    }

    void Unlock() {
        VdMutex.Unlock();
    }

    bool CanSave() const {
        return DataState == TVoxelDataState::GENERATED || DataState == TVoxelDataState::LOADED;
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
            DataState = TVoxelDataState::READY_TO_LOAD;
        }
    }

    void CleanUngenerated() {
        if (DataState == TVoxelDataState::UNGENERATED) {
            if (Vd != nullptr) {
                delete Vd;
                Vd = nullptr;
            }
        }
    }

    void SetSoftUnload() {
        bSoftUnload = true;
    }

    void ResetSoftUnload() {
        bSoftUnload = false;
    }

    bool IsSoftUnload() {
        return bSoftUnload;
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

     void RemoveZone() {
        ZoneComponentAtomicPtr.store(nullptr);
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

    void ClearInstanceObjectMap() {
        std::unique_lock<std::shared_timed_mutex> Lock(InstanceObjectMapMutex);
        InstanceMeshTypeMapPtr = nullptr;
    }
};


typedef std::shared_ptr<TVoxelDataInfo> TVoxelDataInfoPtr;