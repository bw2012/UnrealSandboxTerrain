//
//  VoxelDataInfo.h
//  UE4VoxelTerrain
//
//  Created by Admin on 20.04.2020.
//

#pragma once

#include "VoxelData.h"

enum TVoxelDataState {
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

public:
    TVoxelData* Vd = nullptr;
    TVoxelDataState DataState = TVoxelDataState::UNDEFINED;
    std::shared_ptr<std::mutex> LoadVdMutexPtr;
    
    TVoxelDataInfo() {
        LoadVdMutexPtr = std::make_shared<std::mutex>();
    }
    
    ~TVoxelDataInfo() {    }

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
};
