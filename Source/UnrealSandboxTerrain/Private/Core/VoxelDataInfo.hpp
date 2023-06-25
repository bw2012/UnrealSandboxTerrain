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

#ifdef __cpp_lib_atomic_shared_ptr                      
    std::atomic<TMeshDataPtr> MeshDataCachePtr = nullptr;
#else
    TMeshDataPtr MeshDataCachePtr = nullptr;
#endif

    std::atomic<UTerrainZoneComponent*> ZoneComponentAtomicPtr = nullptr;

    std::atomic<bool> bNeedTerrainSave { false };
    std::atomic<bool> bNeedObjectsSave { false };
    std::atomic<bool> bSpawnFinished { false };

    std::atomic<int> FlagInternal { 0 };

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
		LastChange = 0;
		LastSave = 0;
		LastMeshGeneration = 0;
		LastCacheCheck = 0;
    }
    
    ~TVoxelDataInfo() { 
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
            UE_LOG(LogVt, Warning, TEXT("Exception: %s"), *ExceptionString);
        }
        */
    }

    void Unlock() {
        VdMutex.Unlock();
    }

    int GetFlagInternal() {
        return FlagInternal;
    }

    void SetFlagInternalFullSolid() {
        FlagInternal = 2;
    }

    void SetSpawnFinished() {
        bSpawnFinished = true;
    }

    void ResetSpawnFinished() {
        bSpawnFinished = false;
    }

    bool IsSpawnFinished() {
        return bSpawnFinished;
    }

    bool IsNeedObjectsSave() {
        return bNeedObjectsSave;
    }

    void SetNeedObjectsSave() {
        bNeedObjectsSave = true;
    }

    void ResetNeedObjectsSave() {
        bNeedObjectsSave = false;
    }

    bool IsNeedTerrainSave() {
        return bNeedTerrainSave;
    }

    void SetNeedTerrainSave() {
        bNeedTerrainSave = true;;
    }

    void ResetNeedTerrainSave() {
        bNeedTerrainSave = false;;
    }

    bool CanSaveVd() const {
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
#ifdef __cpp_lib_atomic_shared_ptr                      
        MeshDataCachePtr.store(MeshDataPtr);
#else
        std::atomic_store(&MeshDataCachePtr, MeshDataPtr);
#endif

        FlagInternal = 1;
	}

    TMeshDataPtr GetMeshDataCache() {
#ifdef __cpp_lib_atomic_shared_ptr                      
        return MeshDataCachePtr.load();
#else
        return std::atomic_load(&MeshDataCachePtr);
#endif
    }

	TMeshDataPtr PopMeshDataCache() {
#ifdef __cpp_lib_atomic_shared_ptr                      
        TMeshDataPtr MeshDataPtr = MeshDataCachePtr.exchange(nullptr);
        return MeshDataPtr;
#else
        TMeshDataPtr nullPtr = nullptr;
        TMeshDataPtr MeshDataPtr = std::atomic_exchange(&MeshDataCachePtr, nullPtr);
        return MeshDataPtr;
#endif
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


class TVdInfoLockGuard {

private:

    TVoxelDataInfoPtr VdInfoPtr = nullptr;

public:

    TVdInfoLockGuard(const TVdInfoLockGuard&) = delete;

    TVdInfoLockGuard& operator=(TVdInfoLockGuard&) = delete;

    TVdInfoLockGuard(TVoxelDataInfoPtr VdiPtr) {
        VdiPtr->Lock();
        VdInfoPtr = VdiPtr;
    }

    ~TVdInfoLockGuard() {
        VdInfoPtr->Unlock();
    }
};
