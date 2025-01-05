// Copyright blackw 2015-2020

#include "TerrainGeneratorComponent.h"
#include "SandboxTerrainController.h"
#include "Core/perlin.hpp"
#include "Core/memstat.h"
#include <algorithm>
#include <thread>
#include <atomic>
#include "Math/UnrealMathUtility.h"
#include "TerrainZoneComponent.h"
#include "../Core/SandboxVoxelCore.h"


#define USBT_VGEN_GROUND_LEVEL_OFFSET       205.f
#define USBT_DEFAULT_GRASS_MATERIAL_ID      2


static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;

extern TAutoConsoleVariable<int32> CVarGeneratorDebugMode;



TChunkFloatMatrix::TChunkFloatMatrix(int Size) {
    this->Size = Size;
    FloatArray = new float[Size * Size * Size];
    Max = -MAX_FLT;
    Min = MAX_FLT;
}


TChunkFloatMatrix::~TChunkFloatMatrix() {
    delete[] FloatArray;
}

void TChunkFloatMatrix::SetVal(const int X, const int Y, float Val) {
    if (X < Size && Y < Size) {
        int Index = X * Size + Y;
        FloatArray[Index] = Val;

        if (Val > this->Max) {
            this->Max = Val;
        }

        if (Val < this->Min) {
            this->Min = Val;
        }
    }
}

float TChunkFloatMatrix::GetVal(const int X, const int Y) const {
    if (X < Size && Y < Size) {
        int Index = X * Size + Y;
        return FloatArray[Index];
    } else {
        return 0;
    }
}

float TChunkFloatMatrix::GetMax() const {
    return this->Max;
}

float TChunkFloatMatrix::GetMin() const {
    return this->Min;
}

TChunkData::TChunkData(int Size) {
    Height = new TChunkFloatMatrix(Size);
    cd_counter++;
}

TChunkData::~TChunkData() {
    delete Height;
    cd_counter--;
}

float const* const TChunkData::GetHeightLevelArrayPtr() const {
    return Height->GetArrayPtr();
}

FORCEINLINE void TChunkData::SetHeightLevel(const int X, const int Y, float HeightLevel) {
    Height->SetVal(X, Y, HeightLevel);
}

FORCEINLINE float TChunkData::GetHeightLevel(const int X, const int Y) const {
    return Height->GetVal(X, Y);
}

FORCEINLINE float TChunkData::GetMaxHeightLevel() const {
    return Height->GetMax();
}

FORCEINLINE float TChunkData::GetMinHeightLevel() const {
    return Height->GetMin();
}

//======================================================================================================================================================================
// 
//======================================================================================================================================================================


TStructuresGenerator* UTerrainGeneratorComponent::NewStructuresGenerator() {
    auto* Gen = new TStructuresGenerator();
    Gen->MasterGenerator = this;
    return Gen;
}

UTerrainGeneratorComponent::UTerrainGeneratorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
    this->Pn = new TPerlinNoise();
    this->DfaultGrassMaterialId = USBT_DEFAULT_GRASS_MATERIAL_ID;
    this->StructuresGenerator = NewStructuresGenerator();
}

void UTerrainGeneratorComponent::BeginPlay() {
    Super::BeginPlay();

    ZoneVoxelResolution = GetController()->GetZoneVoxelResolution();

    UndergroundLayersTmp.Empty();

    if (GetController()->TerrainParameters && GetController()->TerrainParameters->UndergroundLayers.Num() > 0) {
        this->UndergroundLayersTmp = GetController()->TerrainParameters->UndergroundLayers;
    } else {
        FTerrainUndergroundLayer DefaultLayer;
        DefaultLayer.MatId = 1;
        DefaultLayer.StartDepth = 0;
        DefaultLayer.Name = TEXT("Dirt");
        UndergroundLayersTmp.Add(DefaultLayer);
    }

    FTerrainUndergroundLayer LastLayer;
    LastLayer.MatId = 0;
    LastLayer.StartDepth = MAX_FLT;
    LastLayer.Name = TEXT("");
    UndergroundLayersTmp.Add(LastLayer);

    PrepareMetaData();
}

void UTerrainGeneratorComponent::ReInit() {
    UE_LOG(LogTemp, Warning, TEXT("UTerrainGeneratorComponent::BeginPlay() WorldSeed: %d"), GetController()->WorldSeed);

    if (GetController()->WorldSeed != 0) {
        this->Pn->reinit(GetController()->WorldSeed);
    }
}

TStructuresGenerator* UTerrainGeneratorComponent::GetStructuresGenerator() {
    return this->StructuresGenerator;
}

void UTerrainGeneratorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    Super::EndPlay(EndPlayReason);
}

void UTerrainGeneratorComponent::FinishDestroy() {
    Super::FinishDestroy();
    delete this->Pn;
    delete this->StructuresGenerator;
}

ASandboxTerrainController* UTerrainGeneratorComponent::GetController() const {
    return (ASandboxTerrainController*)GetOwner();
}

int32 UTerrainGeneratorComponent::ZoneHash(const FVector& ZonePos) const {
	int32 Hash = 7;
	Hash = Hash * 31 + (int32)ZonePos.X;
	Hash = Hash * 31 + (int32)ZonePos.Y;
	Hash = Hash * 31 + (int32)ZonePos.Z;
	return Hash;
}

int32 UTerrainGeneratorComponent::ZoneHash(const TVoxelIndex& ZoneIndex) const {
    const FVector ZonePos = GetController()->GetZonePos(ZoneIndex);
    return ZoneHash(ZonePos);
}


float UTerrainGeneratorComponent::PerlinNoise(const float X, const float Y, const float Z) const {
	return Pn ? Pn->noise(X, Y, Z) : 0;
};

float UTerrainGeneratorComponent::PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
	return Pn ? Pn->noise(Pos.X * PositionScale, Pos.Y * PositionScale, Pos.Z * PositionScale) * ValueScale : 0;
}

float UTerrainGeneratorComponent::PerlinNoise(const FVector& Pos) const {
	return Pn ? Pn->noise(Pos.X, Pos.Y, Pos.Z) : 0;
}

// range 0..1
float UTerrainGeneratorComponent::NormalizedPerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
    return (PerlinNoise(Pos, PositionScale, 1.f) + 0.87f) / 1.73f * ValueScale;
}


//======================================================================================================================================================================
// 
//======================================================================================================================================================================

int UTerrainGeneratorComponent::GetMaterialLayers(const TChunkDataPtr ChunkData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const {
    float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize;
    float ZoneLow = ZoneOrigin.Z - ZoneHalfSize;
    float TerrainHigh = ChunkData->GetMaxHeightLevel();
    float TerrainLow = ChunkData->GetMinHeightLevel();

    int Cnt = 0;
    for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
        const FTerrainUndergroundLayer& Layer1 = UndergroundLayersTmp[Idx];
        const FTerrainUndergroundLayer& Layer2 = UndergroundLayersTmp[Idx + 1];

        float LayerHigh = TerrainHigh - Layer1.StartDepth;
        float LayerLow = TerrainLow - Layer2.StartDepth;

        if (std::max(ZoneLow, LayerLow) < std::min(ZoneHigh, LayerHigh)) {
            Cnt++;
            if (LayerList) {
                LayerList->Add(Layer1);
            }
        }
    }

    return Cnt;
}

const FTerrainUndergroundLayer* UTerrainGeneratorComponent::GetMaterialLayer(float Z, float RealGroundLevel)  const {
    for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
        const FTerrainUndergroundLayer& Layer = UndergroundLayersTmp[Idx];
        if (Z <= RealGroundLevel - Layer.StartDepth && Z > RealGroundLevel - UndergroundLayersTmp[Idx + 1].StartDepth) {
            return &Layer;
        }
    }
    return nullptr;
}

FORCEINLINE TMaterialId UTerrainGeneratorComponent::MaterialFuncion(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float GroundLevel) const {
    const float DeltaZ = WorldPos.Z - GroundLevel;

    TMaterialId MatId = 0;
    if (DeltaZ >= -70) {
        MatId = DfaultGrassMaterialId; // grass
    } else {
        const FTerrainUndergroundLayer* Layer = GetMaterialLayer(WorldPos.Z, GroundLevel);
        if (Layer != nullptr) {
            MatId = Layer->MatId;
        }
    }

    return MatId;
}

FORCEINLINE TMaterialId UTerrainGeneratorComponent::MaterialFuncionExt(const TGenerateVdTempItm* GenItm, const TMaterialId MatId, const FVector& WorldPos, const TVoxelIndex VoxelIndex) const {
    return  MatId;
}

//======================================================================================================================================================================
// Density
//======================================================================================================================================================================

float UTerrainGeneratorComponent::GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const {
    static const float scale1 = 0.001f; // small
    static const float scale2 = 0.0004f; // medium
    static const float scale3 = 0.00009f; // big

    const float noise_small = Pn->noise(V.X * scale1, V.Y * scale1, 0) * 0.5f; 
    const float noise_medium = Pn->noise(V.X * scale2, V.Y * scale2, 0) * 5.f;
    const float noise_big = Pn->noise(V.X * scale3, V.Y * scale3, 0) * 10.f;
    const float gl = noise_small + noise_medium + noise_big;

    return (gl * 100) + USBT_VGEN_GROUND_LEVEL_OFFSET;
}


FORCEINLINE float UTerrainGeneratorComponent::DensityFunctionExt(float InDensity, const TFunctionIn& In) const {
    return InDensity;
}

FORCEINLINE float UTerrainGeneratorComponent::ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const {
    const float Z = V.Z;
    const float D = Z - GroundLevel;

    if (D > 500) {
        return 0.f;
    }

    if (D < -500) {
        return 1.f;
    }

    float DensityByGroundLevel = 1 - (1 / (1 + exp(-(Z - GroundLevel) / 20)));

    if (DensityByGroundLevel > 1.f) {
        DensityByGroundLevel = 1.f;
    }

    if (DensityByGroundLevel < 0.f) {
        DensityByGroundLevel = 0.f;
    }

    return DensityByGroundLevel;
}

TChunkDataPtr UTerrainGeneratorComponent::NewChunkData() {
    return TChunkDataPtr(new TChunkData(ZoneVoxelResolution));
}

TChunkDataPtr UTerrainGeneratorComponent::GenerateChunkData(const TVoxelIndex& Index) {
    TChunkDataPtr ChunkData = NewChunkData();
    double Start = FPlatformTime::Seconds();

    const float Step = USBT_ZONE_SIZE / (ZoneVoxelResolution - 1);
    const float S = -USBT_ZONE_SIZE / 2;
    for (int VX = 0; VX < ZoneVoxelResolution; VX++) {
        for (int VY = 0; VY < ZoneVoxelResolution; VY++) {
            const FVector LocalPos(S + VX * Step, S + VY * Step, S);
            FVector WorldPos = LocalPos + GetController()->GetZonePos(Index);
            float GroundLevel = GroundLevelFunction(Index, WorldPos);
            ChunkData->SetHeightLevel(VX, VY, GroundLevel);

            GenerateChunkDataExt(ChunkData, Index, VX, VY, WorldPos);
        }
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogVt, Log, TEXT("Generate height map  ----> %f ms --  %d %d"), Time, X, Y);
    return ChunkData;
}

void UTerrainGeneratorComponent::GenerateChunkDataExt(TChunkDataPtr ChunkData, const TVoxelIndex& Index, int X, int Y, const FVector& WorldPos) const {

}

TChunkDataPtr UTerrainGeneratorComponent::GetChunkData(int X, int Y) {
    const std::lock_guard<std::mutex> lock(ChunkDataMapMutex);

    TVoxelIndex Index(X, Y, 0);
    TChunkDataPtr ChunkData = nullptr;

    if (ChunkDataCollection.find(Index) == ChunkDataCollection.end()) {
        ChunkData = GenerateChunkData(Index);

#ifdef __cpp_lib_atomic_shared_ptr                      
        ChunkDataCollection[Index] = ChunkData; // TODO linux
#else
        auto& Ptr = ChunkDataCollection[Index];
        std::atomic_store(&Ptr, ChunkData);
#endif
    } else {
        ChunkData = ChunkDataCollection[Index];
    }

    return ChunkData;
};

//======================================================================================================================================================================
// Generator
//======================================================================================================================================================================

struct TMinMax {
    float Min = 999;
    float Max = -999;

    TMinMax() {};

    TMinMax(float Val) : Min(Val), Max(Val) {};

    friend TMinMax& operator << (TMinMax& In, float Val) {
        if (Val < In.Min) {
            In.Min = Val;
        }

        if (Val > In.Max) {
            In.Max = Val;
        }

        return In;
    }
};

FORCEINLINE int ClcZ(float Gl, float ZonePosZ, float Step, int S, int D) {
    const float H = Gl - ZonePosZ;
    int Z = (int)(H / Step) + (D / 2) - 1;
    if (Z < 0) {
        Z = 0;
    }

    Z *= S;

    return Z;
};

class TPseudoOctree {

private:

    TVoxelData* VoxelData;
    int Total;
    int* Processed;
    int Count = 0;

    void PerformVoxel(const TVoxelIndex& Parent, const int LOD) {
        const int S = 1 << LOD;

        if (CheckVoxel(Parent, S, LOD, VoxelData)) {
            TVoxelIndex Tmp[8];
            vd::tools::makeIndexes(Tmp, Parent, S);
            vd::tools::unsafe::forceAddToCache(VoxelData, Parent.X, Parent.Y, Parent.Z, LOD);

            for (auto i = 0; i < 8; i++) {
                const TVoxelIndex& TTT = Tmp[i];
                int Idx = vd::tools::clcLinearIndex(VoxelData->num(), TTT.X, TTT.Y, TTT.Z);
                if (Processed[Idx] == 0x0) {
                    Handler(TTT, Idx, VoxelData, LOD); 
                    Count++;
                    Processed[Idx] = 0xff;
                }
            }

            if (LOD != 0) {
                const int ChildLOD = LOD - 1;
                const int ChildS = 1 << ChildLOD;
                TVoxelIndex Child[8];
                vd::tools::makeIndexes(Child, Parent, ChildS);
                for (auto I = 0; I < 8; I++) {
                    PerformVoxel(Child[I], ChildLOD);
                }
            }

        }
    };

public:

    std::function<void(const TVoxelIndex&, int, TVoxelData*, int)> Handler = nullptr;
    std::function<bool(const TVoxelIndex&, int, int, const TVoxelData*)> CheckVoxel = nullptr;

    TPseudoOctree(TVoxelData* Vd) : VoxelData(Vd) {
        int N = Vd->num();
        Total = N * N * N;
        Processed = new int[Total];
        for (int I = 0; I < Total; I++) {
            Processed[I] = 0x0;
        }

        Handler = [](const TVoxelIndex& V, int Idx, TVoxelData* VoxelData_, int LOD) {  };
        CheckVoxel = [](const TVoxelIndex& V, int S, int LOD, const TVoxelData* VoxelData_) { return true;  };
    };

    ~TPseudoOctree() {
        delete Processed;
    };

    void Start() {
        PerformVoxel(TVoxelIndex(0, 0, 0), LOD_ARRAY_SIZE - 1);
    };

    int GetCount() {
        return Count;
    }

    int GetTotal() {
        return Total;
    }
};

void UTerrainGeneratorComponent::GenerateLandscapeZoneSlight(const TGenerateVdTempItm& Itm) const {
    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    TConstChunkData ChunkData = Itm.ChunkData;

    double Start2 = FPlatformTime::Seconds();

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->deinitializeMaterial(DfaultGrassMaterialId);

    TPseudoOctree Octree(VoxelData);
    Octree.Handler = [=, this] (const TVoxelIndex& V, int Idx, TVoxelData* VoxelData, int LOD) {
        if (LOD == 0) {
            const FVector& LocalPos = VoxelData->voxelIndexToVector(V.X, V.Y, V.Z);
            const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
        }
       B(ZoneIndex, V, VoxelData, ChunkData);
    };

    Octree.CheckVoxel = [=](const TVoxelIndex& V, int S, int LOD, const TVoxelData* Vd) {
        if (LOD > 3) {
            return true;
        }

        const int X = V.X;
        const int Y = V.Y;
        const int Z = V.Z;

        const FVector& Pos = Vd->voxelIndexToVector(X, Y, Z) + Vd->getOrigin();
        const FVector& Pos2 = Vd->voxelIndexToVector(X, Y, Z + S) + Vd->getOrigin();

        {
            // rare ugly crackholes workaround
            const float T = ChunkData->GetHeightLevel(X, Y) - Pos2.Z;
            const static float F = 5.f;
            if (T < F && T > -F) {
                return true;
            }
        }

        TMinMax MinMax;
        MinMax << ChunkData->GetHeightLevel(X, Y) << ChunkData->GetHeightLevel(X + S, Y) << ChunkData->GetHeightLevel(X, Y + S) << ChunkData->GetHeightLevel(X + S, Y + S);
        const static float F = 5.f;
        bool R = std::max((float)Pos.Z, MinMax.Min - F) < std::min((float)Pos2.Z, MinMax.Max + F);
        return R;
    };

    Octree.Start();
    VoxelData->setCacheToValid();

    const int LOD = Itm.GenerationLOD;
    const int S = 1 << LOD;

    for (int X = 0; X < ZoneVoxelResolution; X += S) {
        for (int Y = 0; Y < ZoneVoxelResolution; Y += S) {
            B(ZoneIndex, TVoxelIndex(X, Y, 0), VoxelData, ChunkData);
            B(ZoneIndex, TVoxelIndex(X, Y, ZoneVoxelResolution - 1), VoxelData, ChunkData);
        }
    }

    for (int X = 0; X < ZoneVoxelResolution; X += S) {
        for (int Z = 0; Z < ZoneVoxelResolution; Z += S) {
            B(ZoneIndex, TVoxelIndex(X, 0, Z), VoxelData, ChunkData);
            B(ZoneIndex, TVoxelIndex(X, ZoneVoxelResolution - 1, Z), VoxelData, ChunkData);
        }
    }

    for (int Y = 0; Y < ZoneVoxelResolution; Y += S) {
        for (int Z = 0; Z < ZoneVoxelResolution; Z += S) {
            B(ZoneIndex, TVoxelIndex(0, Y, Z), VoxelData, ChunkData);
            B(ZoneIndex, TVoxelIndex(ZoneVoxelResolution - 1, Y, Z), VoxelData, ChunkData);
        }
    }

    double End2 = FPlatformTime::Seconds();
    double Time2 = (End2 - Start2) * 1000;
    float Ratio = (float)Octree.GetCount() / (float)Octree.GetTotal() * 100.f;
}

void UTerrainGeneratorComponent::ForceGenerateZone(TVoxelData* VoxelData, const TVoxelIndex& ZoneIndex) {

    HandleRegionByZoneIndex(ZoneIndex.X, ZoneIndex.Y);

    TGenerateVdTempItm Itm;
    Itm.Idx = 0;
    Itm.ZoneIndex = ZoneIndex;
    Itm.Vd = VoxelData;
    Itm.ChunkData = GetChunkData(ZoneIndex.X, ZoneIndex.Y);

    ExtVdGenerationData(Itm);

    GenerateZoneVolume(Itm);
}

bool IsLandscapeZone(const FVector& Pos, TConstChunkData ChunkData) {
    float ZoneHigh = Pos.Z + ZoneHalfSize;
    float ZoneLow = Pos.Z - ZoneHalfSize;
    float TerrainHigh = ChunkData->GetMaxHeightLevel();
    float TerrainLow = ChunkData->GetMinHeightLevel();
    return std::max(ZoneLow, TerrainLow) <= std::min(ZoneHigh, TerrainHigh);
}

void UTerrainGeneratorComponent::GenerateZoneVolumeWithFunction(const TGenerateVdTempItm& Itm, const std::vector<TZoneStructureHandler>& StructureList) const {
    double Start = FPlatformTime::Seconds();

    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    TConstChunkData ChunkData = Itm.ChunkData;
    const int LOD = Itm.GenerationLOD;
    int zc = 0;
    int fc = 0;

    const int S = 1 << LOD;
    bool bContainsMoreOneMaterial = false;
    TMaterialId BaseMaterialId = 0;

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->initializeMaterial();

    bool bIsLandscape = IsLandscapeZone(VoxelData->getOrigin(), ChunkData);

    for (int X = 0; X < ZoneVoxelResolution; X += S) {
        for (int Y = 0; Y < ZoneVoxelResolution; Y += S) {
            for (int Z = 0; Z < ZoneVoxelResolution; Z += S) {
                const TVoxelIndex& Index = TVoxelIndex(X, Y, Z);
                const FVector& LocalPos = VoxelData->voxelIndexToVector(X, Y, Z);
                const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
                const float GroundLevel = ChunkData->GetHeightLevel(X, Y);

                float Density = (Itm.Type == TZoneGenerationType::AirOnly) ? 0. : 1.f;

                if (Itm.Type == TZoneGenerationType::Other) {
                    const FVector& Pos = GetController()->GetZonePos(ZoneIndex);
                    if (ChunkData->GetMaxHeightLevel() < Pos.Z - ZoneHalfSize){
                        Density = 0.f;
                    }
                }

                TMaterialId MaterialId = MaterialFuncion(ZoneIndex, WorldPos, GroundLevel);

                if (bIsLandscape) {
                    Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
                }

                for (const auto& StructureHandler : StructureList) {
                    if (StructureHandler.Function) {
                        auto R = StructureHandler.Function(Density, MaterialId, Index, LocalPos, WorldPos);
                        Density = std::get<0>(R);
                        MaterialId = std::get<1>(R);
                    }
                }

                MaterialId = MaterialFuncionExt(&Itm, MaterialId, WorldPos, Index);

                const float Density2 = DensityFunctionExt(Density, std::make_tuple(ZoneIndex, Index, WorldPos, LocalPos, ChunkData));

                VoxelData->setDensityAndMaterial(Index, Density2, MaterialId);
                VoxelData->performSubstanceCacheLOD(Index.X, Index.Y, Index.Z, LOD);

                if (Density == 0) {
                    zc++;
                }

                if (Density == 1) {
                    fc++;
                }

                if (!BaseMaterialId) {
                    BaseMaterialId = MaterialId;
                } else {
                    if (BaseMaterialId != MaterialId) {
                        bContainsMoreOneMaterial = true;
                    }
                }
            }
        }
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogVt, Log, TEXT("GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

    int n = VoxelData->num();
    int s = n * n * n;

    if (zc == s) {
        VoxelData->deinitializeDensity(TVoxelDataFillState::ZERO);
    }

    if (fc == s) {
        VoxelData->deinitializeDensity(TVoxelDataFillState::FULL);
    }

    if (!bContainsMoreOneMaterial) {
        VoxelData->deinitializeMaterial(BaseMaterialId);
    } else {
        VoxelData->setBaseMatId(BaseMaterialId);
    }

    VoxelData->setCacheToValid();
}

void UTerrainGeneratorComponent::GenerateZoneVolume(const TGenerateVdTempItm& Itm) const {
    double Start = FPlatformTime::Seconds();

    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    TConstChunkData ChunkData = Itm.ChunkData;
    const int LOD = Itm.GenerationLOD;
    int zc = 0;
    int fc = 0;

    const int S = 1 << LOD;
    bool bContainsMoreOneMaterial = false;
    TMaterialId BaseMaterialId = 0;

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->initializeMaterial();

    for (int X = 0; X < ZoneVoxelResolution; X += S) {
        for (int Y = 0; Y < ZoneVoxelResolution; Y += S) {
            for (int Z = 0; Z < ZoneVoxelResolution; Z += S) {
                const FVector& LocalPos = VoxelData->voxelIndexToVector(X, Y, Z);
                const TVoxelIndex& Index = TVoxelIndex(X, Y, Z);

                auto R = A(ZoneIndex, Index, VoxelData, Itm);
                float Density = std::get<2>(R);
                TMaterialId MaterialId = std::get<3>(R);

                if (LOD > 0) {
                   // MaterialId = DfaultGrassMaterialId; // FIXME
                    VoxelData->setMaterial(Index.X, Index.Y, Index.Z, DfaultGrassMaterialId);
                }

                VoxelData->performSubstanceCacheLOD(Index.X, Index.Y, Index.Z, LOD);

                if (Density == 0) {
                    zc++;
                }

                if (Density == 1) {
                    fc++;
                }

                if (!BaseMaterialId) {
                    BaseMaterialId = MaterialId;
                } else {
                    if (BaseMaterialId != MaterialId) {
                        bContainsMoreOneMaterial = true;
                    }
                }
            }
        }
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogVt, Log, TEXT("GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

    int n = VoxelData->num();
    int s = n * n * n;

    if (zc == s) {
        VoxelData->deinitializeDensity(TVoxelDataFillState::ZERO);
    }

    if (fc == s) {
        VoxelData->deinitializeDensity(TVoxelDataFillState::FULL);
    }

    if (!bContainsMoreOneMaterial) {
        VoxelData->deinitializeMaterial(BaseMaterialId);
    }

    VoxelData->setCacheToValid();
}

ResultA UTerrainGeneratorComponent::A(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, const TGenerateVdTempItm& Itm) const {
    const FVector& LocalPos = VoxelData->voxelIndexToVector(VoxelIndex.X, VoxelIndex.Y, VoxelIndex.Z);
    const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
    const float GroundLevel = Itm.ChunkData->GetHeightLevel(VoxelIndex.X, VoxelIndex.Y);
    const float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
    const float Density2 = DensityFunctionExt(Density, std::make_tuple(ZoneIndex, VoxelIndex, WorldPos, LocalPos, Itm.ChunkData));
    TMaterialId MaterialId = MaterialFuncion(ZoneIndex, WorldPos, GroundLevel);

    MaterialId = MaterialFuncionExt(&Itm, MaterialId, WorldPos, VoxelIndex);

    VoxelData->setDensityAndMaterial(VoxelIndex, Density2, MaterialId);
    auto Result = std::make_tuple(LocalPos, WorldPos, Density2, MaterialId);
    return Result;
};

float UTerrainGeneratorComponent::B(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, TConstChunkData ChunkData) const {
    const FVector& LocalPos = VoxelData->voxelIndexToVector(VoxelIndex.X, VoxelIndex.Y, VoxelIndex.Z);
    const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
    const float GroundLevel = ChunkData->GetHeightLevel(VoxelIndex.X, VoxelIndex.Y);
    const float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
    const float Density2 = DensityFunctionExt(Density, std::make_tuple(ZoneIndex, VoxelIndex, WorldPos, LocalPos, ChunkData)); 
    vd::tools::unsafe::setDensity(VoxelData, VoxelIndex, Density2);
    return Density2;
};

void UTerrainGeneratorComponent::BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& List) {
    double Start1 = FPlatformTime::Seconds();
    int32 DebugMode = CVarGeneratorDebugMode.GetValueOnAnyThread();

    for (const auto& Itm : List) {
        const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;

        auto ZoneHandlerList = StructuresGenerator->StructureMap[ZoneIndex];
        if (ZoneHandlerList.size() > 0) {
            GenerateZoneVolumeWithFunction(Itm, ZoneHandlerList);
            continue;
        }

        GenerateZoneVolume(Itm);
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;

    if (DebugMode > 0) {
        UE_LOG(LogVt, Warning, TEXT("BatchGenerateComplexVd -> %f ms"), Time1);
    }
}

void UTerrainGeneratorComponent::BatchGenerateSlightVd(TArray<TGenerateVdTempItm>& List) {
    double Start1 = FPlatformTime::Seconds();
    int32 DebugMode = CVarGeneratorDebugMode.GetValueOnAnyThread();

    for (const auto& Itm : List) {
        if (Itm.Type == TZoneGenerationType::Landscape) {
            GenerateLandscapeZoneSlight(Itm);
            continue;
        }

        // TODO handle others
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;

    if (DebugMode > 0) {
        UE_LOG(LogVt, Warning, TEXT("BatchGenerateSlightVd -> %f ms"), Time1);
    }
}

TZoneGenerationType UTerrainGeneratorComponent::ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkDataPtr ChunkData) {
    const FVector& Pos = GetController()->GetZonePos(ZoneIndex);

    if (IsLandscapeZone(Pos, ChunkData)) {
        return TZoneGenerationType::Landscape;
    }

    if (HasStructures(ZoneIndex)) {
        return TZoneGenerationType::Other;
    }

    if (IsForcedComplexZone(ZoneIndex)) {
        return TZoneGenerationType::Other;
    }

    if (ChunkData->GetMaxHeightLevel() < Pos.Z - ZoneHalfSize) {
        return TZoneGenerationType::AirOnly; // air only
    }

    if (ChunkData->GetMinHeightLevel() > Pos.Z + ZoneHalfSize) {
        TArray<FTerrainUndergroundLayer> LayerList;
        if (GetMaterialLayers(ChunkData, Pos, &LayerList) == 1) {
            return TZoneGenerationType::FullSolidOneMaterial; //full solid
        } else {
            return TZoneGenerationType::FullSolidMultipleMaterials;
        }
    }

    return TZoneGenerationType::Other;
}

void UTerrainGeneratorComponent::GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkDataPtr ChunkData) {
    if (Type == 0) {
        // air only
        VoxelData->deinitializeDensity(TVoxelDataFillState::ZERO);
        VoxelData->deinitializeMaterial(0);
    }

    if (Type == 1) {
        TArray<FTerrainUndergroundLayer> LayerList;
        GetMaterialLayers(ChunkData, VoxelData->getOrigin(), &LayerList);
        VoxelData->deinitializeDensity(TVoxelDataFillState::FULL);
        VoxelData->deinitializeMaterial(LayerList[0].MatId);
    }

    VoxelData->setCacheToValid();
}

void UTerrainGeneratorComponent::ExtVdGenerationData(TGenerateVdTempItm& VdGenerationData) {

}

TGenerateVdTempItm UTerrainGeneratorComponent::CollectVdGenerationData(const TVoxelIndex& ZoneIndex) {
    TGenerateVdTempItm VdGenerationData;

    HandleRegionByZoneIndex(ZoneIndex.X, ZoneIndex.Y);

    VdGenerationData.ZoneIndex = ZoneIndex;
    VdGenerationData.ChunkData = GetChunkData(ZoneIndex.X, ZoneIndex.Y);
    VdGenerationData.Type = ZoneGenType(ZoneIndex, VdGenerationData.ChunkData);
    VdGenerationData.bHasStructures = HasStructures(ZoneIndex);
    VdGenerationData.GenerationLOD = 0; // not used
    VdGenerationData.Method = TGenerationMethod::NotDefined;

    auto& Type = VdGenerationData.Type;
    auto& bHasStructures = VdGenerationData.bHasStructures;

    if ((Type == TZoneGenerationType::AirOnly || Type == TZoneGenerationType::FullSolidOneMaterial) && !bHasStructures) {
        VdGenerationData.Method = TGenerationMethod::SetEmpty;
    } else if (Type == TZoneGenerationType::FullSolidMultipleMaterials && !bHasStructures) {
        VdGenerationData.Method = TGenerationMethod::Skip;
    } else {
        bool bIsComplex = (Type == TZoneGenerationType::Other) || bHasStructures;
        if (bIsComplex) {
            VdGenerationData.Method = TGenerationMethod::SlowComplex;
        } else {
            VdGenerationData.Method = TGenerationMethod::FastSimple;
        }
    }
    
    return VdGenerationData;
}

void UTerrainGeneratorComponent::BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& BatchList, TArray<TGenerateZoneResult>& NewVdArray) {
    NewVdArray.Empty();
    NewVdArray.SetNumZeroed(BatchList.Num());
    TArray<TGenerateVdTempItm> ComplexList;
    TArray<TGenerateVdTempItm> FastList;
    int32 DebugMode = CVarGeneratorDebugMode.GetValueOnAnyThread();

    int Idx = 0;
    for (const auto& P : BatchList) {
        const FVector Pos = GetController()->GetZonePos(P.Index);
        TVoxelData* NewVd = GetController()->NewVoxelData();
        NewVd->setOrigin(Pos);

        TGenerateVdTempItm GenItm = CollectVdGenerationData(P.Index);
        GenItm.Idx = Idx;
        GenItm.Vd = NewVd;

        ExtVdGenerationData(GenItm);

        NewVdArray[Idx].Vd = NewVd;
        NewVdArray[Idx].Type = GenItm.Type;
        NewVdArray[Idx].Method = GenItm.Method;
        NewVdArray[Idx].OreData = GenItm.OreData;

        if (GenItm.Method == TGenerationMethod::SetEmpty) {
            GenerateSimpleVd(P.Index, NewVd, GenItm.Type, GenItm.ChunkData);
        } else if (GenItm.Method == TGenerationMethod::Skip) {
            NewVd->deinitializeDensity(TVoxelDataFillState::FULL);
            NewVd->deinitializeMaterial(0);
            NewVd->setCacheToValid();
        } else if (GenItm.Method == TGenerationMethod::FastSimple) {
            if (DebugMode == 2) {
                ComplexList.Add(GenItm);
            } else {
                FastList.Add(GenItm);
            }
        } else if (GenItm.Method == TGenerationMethod::SlowComplex) {
            ComplexList.Add(GenItm);
        } else {
            // TODO assert unknown method
        }

        Idx++;
    }

    double Start2 = FPlatformTime::Seconds();

    if (ComplexList.Num() > 0) {
        BatchGenerateComplexVd(ComplexList);
    }

    if (FastList.Num() > 0) {
        BatchGenerateSlightVd(FastList);
    }

    double End2 = FPlatformTime::Seconds();
    double Time2 = (End2 - Start2) * 1000;

    if (DebugMode > 0) {
        UE_LOG(LogVt, Warning, TEXT("BatchGenerateVoxelTerrain -> %f ms"), Time2);
    }

    OnBatchGenerationFinished();
}

void UTerrainGeneratorComponent::OnBatchGenerationFinished() {

}

void UTerrainGeneratorComponent::Clean() {
    const std::lock_guard<std::mutex> lock(ChunkDataMapMutex);
    //TODO linux ChunkDataCollection.clear(); 
}

void UTerrainGeneratorComponent::Clean(const TVoxelIndex& Index) {
    const std::lock_guard<std::mutex> lock(ChunkDataMapMutex);
    //TOOD linux ChunkDataCollection.erase(Index);
}

//======================================================================================================================================================================
// Foliage
//======================================================================================================================================================================

void UTerrainGeneratorComponent::GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap, const TGenerateZoneResult& GenResult) {
    TChunkDataPtr ChunkData = GetChunkData(Index.X, Index.Y);
    auto Type = GenResult.Type; // ZoneGenType(Index, ChunkData);

    if (Type == TZoneGenerationType::AirOnly || Type == TZoneGenerationType::FullSolidOneMaterial || Type == TZoneGenerationType::FullSolidMultipleMaterials) {
        //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(GetWorld(), Vd->getOrigin(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });
        //return;
    }

    if (GetController()->FoliageMap.Num() != 0) {   
        if (Type == TZoneGenerationType::Landscape) {
            GenerateNewFoliageLandscape(Index, ZoneInstanceMeshMap);
        }
    }

    // generate minerals as inst. meshes
    if (GenResult.OreData != nullptr && GenResult.OreData->MeshTypeId > 0) {
        if (Type == TZoneGenerationType::Other) {
            const FVector ZonePos = Vd->getOrigin();

            //AsyncTask(ENamedThreads::GameThread, [=, this]() { DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });

            FRandomStream Rnd = MakeNewRandomStream(ZonePos);
            GenerateRandomInstMesh(ZoneInstanceMeshMap, GenResult.OreData->MeshTypeId, Rnd, Index, Vd, 3, 5);
        } 

        //TODO: FullSolid
        //const TZoneOreData* ZoneOreData = GenItm->OreData.get();
    }

    PostGenerateNewInstanceObjects(Index, Type, Vd, ZoneInstanceMeshMap);
}

FRandomStream UTerrainGeneratorComponent::MakeNewRandomStream(const FVector& ZonePos) const {
    int32 Hash = ZoneHash(ZonePos);
    FRandomStream Rnd = FRandomStream();
    Rnd.Initialize(Hash);
    Rnd.Reset();
    return Rnd;
}

void UTerrainGeneratorComponent::PostGenerateNewInstanceObjects(const TVoxelIndex& ZoneIndex, const TZoneGenerationType ZoneType, const TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const {

}

void UTerrainGeneratorComponent::GenerateRandomInstMesh(TInstanceMeshTypeMap& ZoneInstanceMeshMap, uint32 MeshTypeId, FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, int Min, int Max, const TInstanceMeshSpawnParams& Params) const {
    FVector WorldPos(0);
    FVector Normal(0);

    FVector ZonePos = Vd->getOrigin();

    int Num = (Min != Max) ? Rnd.RandRange(Min, Max) : 1;

    for (int I = 0; I < Num; I++) {
        if (SelectRandomSpawnPoint(Rnd, ZoneIndex, Vd, WorldPos, Normal)) {
            const FVector LocalPos = WorldPos - ZonePos;
            const FVector Scale = FVector(1, 1, 1);
            FRotator Rotation = Normal.Rotation();
            Rotation.Pitch -= 90;
            const FQuat Q = FQuat(Normal, (Rnd.FRandRange(0.f, 360.f) * PI / 180));
            FTransform T(Q, LocalPos, Scale);

            //AsyncTask(ENamedThreads::GameThread, [=, this] () { DrawDebugLine(GetWorld(), WorldPos, WorldPos + (Normal * 100 ), FColor(255, 255, 255, 0), true); });

            const FTerrainInstancedMeshType* MeshType = GetController()->GetInstancedMeshType(MeshTypeId, 0);
            if (MeshType) {
                auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(MeshType->GetMeshTypeCode());
                InstanceMeshContainer.MeshType = *MeshType;
                InstanceMeshContainer.TransformArray.Add(T);
            }
        }
    }
}

bool UTerrainGeneratorComponent::SelectRandomSpawnPoint(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, FVector& SectedLocation, FVector& SectedNormal, const TInstanceMeshSpawnParams& Params) const {
    int VoxelArraySize = vd::tools::getCacheSize(Vd, 0);

    if (VoxelArraySize == 0) {
        return false;
    }

    int RandomNumber = Rnd.FRandRange(0, VoxelArraySize);
    const TSubstanceCacheItem& CacheItem = vd::tools::getCacheItmByNumber(Vd, 0, RandomNumber);
    int LinearIndex = CacheItem.index;

    uint32 X = 0;
    uint32 Y = 0;
    uint32 Z = 0;
    Vd->clcVoxelIndex(LinearIndex, X, Y, Z);
    FVector Pos = Vd->voxelIndexToVector(X, Y, Z) + Vd->getOrigin();

    TVoxelDataParam Param;
    TMeshDataPtr TmpMesh = polygonizeSingleCell(*Vd, Param, X, Y, Z);
    const TArray<TMeshVertex>& Vertexes = TmpMesh->MeshSectionLodArray[0].WholeMesh.ProcVertexBuffer;

    if (Vertexes.Num() > 0) {
        FVector AveragePos(0);
        FVector AverageNormal(0);

        for (const auto& Vertex : Vertexes) {
            AveragePos += Vertex.Pos;
            AverageNormal += Vertex.Normal;
        }

        AveragePos /= Vertexes.Num();
        AveragePos += Vd->getOrigin();

        AverageNormal /= Vertexes.Num();
        AverageNormal.Normalize(0.01f);

        SectedLocation = AveragePos;
        SectedNormal = AverageNormal;
        return true;
    }

    return false;
}

void UTerrainGeneratorComponent::SpawnFoliageAsInstanceMesh(const FTransform& Transform, uint32 MeshTypeId, uint32 MeshVariantId, const FSandboxFoliage& FoliageType, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const {
    FTerrainInstancedMeshType MeshType;
    MeshType.MeshTypeId = MeshTypeId;
    MeshType.MeshVariantId = MeshVariantId;
    MeshType.Mesh = FoliageType.MeshVariants[MeshVariantId];
    MeshType.StartCullDistance = FoliageType.StartCullDistance;
    MeshType.EndCullDistance = FoliageType.EndCullDistance;

    auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(MeshType.GetMeshTypeCode());
    InstanceMeshContainer.MeshType = MeshType;
    InstanceMeshContainer.TransformArray.Add(Transform);
}

void UTerrainGeneratorComponent::GenerateNewFoliageLandscape(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    FVector ZonePos = GetController()->GetZonePos(Index);
    int32 Hash = ZoneHash(ZonePos);
    FRandomStream rnd = FRandomStream();
    rnd.Initialize(Hash);
    rnd.Reset();

    int Counter = 0;

    static const float S = USBT_ZONE_SIZE / 2;
    static const float Step = 25.f;

    for (auto X = -S; X <= S; X += Step) {
        for (auto Y = -S; Y <= S; Y += Step) {
            FVector V(ZonePos);
            FVector LocalPos(X, Y, 0);
            V += LocalPos;

            for (auto& Elem : GetController()->FoliageMap) {
                FSandboxFoliage FoliageType = Elem.Value;

                if (FoliageType.Type == ESandboxFoliageType::Cave || FoliageType.Type == ESandboxFoliageType::Custom) {
                    continue;
                }

                int32 FoliageTypeId = Elem.Key;

                if ((int)X % (int)FoliageType.SpawnStep == 0 && (int)Y % (int)FoliageType.SpawnStep == 0) {
                    float Chance = rnd.FRandRange(0.f, 1.f);

                    const FSandboxFoliage FoliageType2 = FoliageExt(FoliageTypeId, FoliageType, Index, V);
                    float Probability = FoliageType2.Probability;

                    if (Chance <= Probability) {
                        float r = std::sqrt(V.X * V.X + V.Y * V.Y);

                        if (FoliageType2.OffsetRange > 0) {
                            float ox = rnd.FRandRange(0.f, FoliageType2.OffsetRange); 
                            if (rnd.GetFraction() > 0.5) {
                                ox = -ox;
                            }

                            float oy = rnd.FRandRange(0.f, FoliageType2.OffsetRange);
                            if (rnd.GetFraction() > 0.5) {
                                oy = -oy;
                            }

                            V.X += ox;
                            V.Y += oy;
                        }

                        float GroundLevel = GroundLevelFunction(Index, FVector(V.X, V.Y, 0)) - 5.5;
                        FVector WorldLocation(V.X, V.Y, GroundLevel);

                        FVector Min(-ZoneHalfSize, -ZoneHalfSize, -ZoneHalfSize);
                        FVector Max(ZoneHalfSize, ZoneHalfSize, ZoneHalfSize);
                        FBox Box(Min, Max);
                        Box = Box.MoveTo(ZonePos);

                        if (FMath::PointBoxIntersection(WorldLocation, Box)) {
                            float Angle = rnd.FRandRange(0.f, 360.f);
                            float ScaleZ = rnd.FRandRange(FoliageType2.ScaleMinZ, FoliageType2.ScaleMaxZ);
                            FVector Scale = FVector(1, 1, ScaleZ);
                            if (OnCheckFoliageSpawn(Index, WorldLocation, Scale)) {
                                if (FoliageType2.MeshVariants.Num() > 0) {
                                    uint32 MeshVariantId = 0;
                                    if (FoliageType2.MeshVariants.Num() > 1) {
                                        MeshVariantId = rnd.RandRange(0, FoliageType.MeshVariants.Num() - 1);
                                    }

                                    const FVector NewPos = WorldLocation - ZonePos;

                                    bool bIsValidPosition = true;
                                    if (HasStructures(Index)) {
                                        auto ZoneHandlerList = StructuresGenerator->StructureMap[Index];
                                        if (ZoneHandlerList.size() > 0) {
                                            for (const auto& ZoneHandler : ZoneHandlerList) {
                                                if (ZoneHandler.LandscapeFoliageFilter) {
                                                    if (!ZoneHandler.LandscapeFoliageFilter(Index, WorldLocation, NewPos)) {
                                                        bIsValidPosition = false;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    if (!bIsValidPosition) {
                                        continue;
                                    }
   
                                    FTransform Transform(FRotator(0, Angle, 0), NewPos, Scale);

                                    /*
                                    FTerrainInstancedMeshType MeshType;
                                    MeshType.MeshTypeId = FoliageTypeId;
                                    MeshType.MeshVariantId = MeshVariantId;
                                    MeshType.Mesh = FoliageType2.MeshVariants[MeshVariantId];
                                    MeshType.StartCullDistance = FoliageType2.StartCullDistance;
                                    MeshType.EndCullDistance = FoliageType2.EndCullDistance;

                                    auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(MeshType.GetMeshTypeCode());
                                    InstanceMeshContainer.MeshType = MeshType;
                                    InstanceMeshContainer.TransformArray.Add(Transform);
                                    */

                                    SpawnFoliageAsInstanceMesh(Transform, FoliageTypeId, MeshVariantId, FoliageType2, ZoneInstanceMeshMap);

                                    Counter++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

FSandboxFoliage UTerrainGeneratorComponent::FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos) {
    return FoliageType;
}

bool UTerrainGeneratorComponent::OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale) {
    return true;
}

bool UTerrainGeneratorComponent::IsForcedComplexZone(const TVoxelIndex& ZoneIndex) {
    return false;
}

bool UTerrainGeneratorComponent::HasStructures(const TVoxelIndex& ZoneIndex) const {
    return this->StructuresGenerator->HasStructures(ZoneIndex);
}

void UTerrainGeneratorComponent::PrepareMetaData() {

}

void UTerrainGeneratorComponent::AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure) {
    this->GetStructuresGenerator()->AddZoneStructure(ZoneIndex, Structure);
}

const FString* UTerrainGeneratorComponent::GetZoneTag(const TVoxelIndex& ZoneIndex, FString Name) const {
    if (ZoneTagData.Contains(ZoneIndex)) {
        const TMap<FString, FString>& ExtData = ZoneTagData[ZoneIndex];
        return ExtData.Find(Name);
    }

    return nullptr;
}

bool UTerrainGeneratorComponent::CheckZoneTagExists(const TVoxelIndex& ZoneIndex, FString Name) const {
    const FString* Param = GetZoneTag(ZoneIndex, Name);
    if (Param) {
        return true;
    }

    TVoxelIndex ChunkIndex(ZoneIndex.X, ZoneIndex.Y, 0);
    if (ChunkTagData.Contains(ChunkIndex)) {
        const TMap<FString, FString>& ExtData = ChunkTagData[ChunkIndex];
        const FString* Param2 = ExtData.Find(Name);

        if (Param2) {
            return true;
        }
    }

    return false;
}

bool UTerrainGeneratorComponent::CheckZoneTag(const TVoxelIndex& ZoneIndex, FString Name, FString Value) const {
    const FString* Param = GetZoneTag(ZoneIndex, Name);
    if (Param && *Param == Value) {
        return true;
    }

    TVoxelIndex ChunkIndex(ZoneIndex.X, ZoneIndex.Y, 0);
    if (ChunkTagData.Contains(ChunkIndex)) {
        const TMap<FString, FString>& ExtData = ChunkTagData[ChunkIndex];
        const FString* Param2 = ExtData.Find(Name);

        if (Param2 && *Param2 == Value) {
            return true;
        }
    }

    return false;
}

float UTerrainGeneratorComponent::PerformLandscapeZone(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float Lvl) const {
    return StructuresGenerator->PerformLandscapeZone(ZoneIndex, WorldPos, Lvl);
}

void UTerrainGeneratorComponent::SetZoneTag(const TVoxelIndex& ZoneIndex, FString Name, FString Value) {
    ZoneTagData.FindOrAdd(ZoneIndex).Add(Name, Value);
}

void UTerrainGeneratorComponent::SetChunkTag(const TVoxelIndex& ChunkIndex, FString Name, FString Value) {
    ChunkTagData.FindOrAdd(TVoxelIndex(ChunkIndex.X, ChunkIndex.Y, 0)).Add(Name, Value);
}

void UTerrainGeneratorComponent::HandleRegionByZoneIndex(int X, int Y) {
    auto RegionIndex = GetController()->ClcRegionByZoneIndex(TVoxelIndex(X, Y, 0));
    auto& Region = RegionMap.FindOrAdd(RegionIndex);
    if (!Region.bGenerated) {
        Region.X = RegionIndex.X;
        Region.Y = RegionIndex.Y;

        UE_LOG(LogTemp, Warning, TEXT("GenerateRegion: %d %d"), Region.X, Region.Y);
        GenerateRegion(Region);

        Region.bGenerated = true;
    }
}

void UTerrainGeneratorComponent::GenerateRegion(TTerrainRegion& Region) {

}

//======================================================================================================================================================================
// Structures 
//======================================================================================================================================================================

bool TStructuresGenerator::HasStructures(const TVoxelIndex& ZoneIndex) const {
    return StructureMap.find(ZoneIndex) != StructureMap.end();
}

void TStructuresGenerator::AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure) {
    auto& StructureList = StructureMap[ZoneIndex];
    StructureList.push_back(Structure);
}

ASandboxTerrainController* TStructuresGenerator::GetController() {
    return MasterGenerator->GetController();
}

UTerrainGeneratorComponent* TStructuresGenerator::GetGeneratorComponent() {
    return MasterGenerator;
}

void TStructuresGenerator::AddLandscapeStructure(const TLandscapeZoneHandler& Structure) {
    auto& StructureList = LandscapeStructureMap[Structure.ZoneIndex];
    StructureList.push_back(Structure);
}

float TStructuresGenerator::PerformLandscapeZone(const TVoxelIndex& ZoneIndex, const FVector& WorldPos, float Lvl) const {
    float L = Lvl;
    if (LandscapeStructureMap.find(ZoneIndex) != LandscapeStructureMap.end()) {
        const auto& StructureList = LandscapeStructureMap.at(ZoneIndex);
        if (StructureList.size() > 0) {
            for (const auto& LandscapeHandler : StructureList) {
                if (LandscapeHandler.Function) {
                    L = LandscapeHandler.Function(L, ZoneIndex, WorldPos);
                }
            }
        }
    }

    return L;
}

void TStructuresGenerator::SetZoneTag(const TVoxelIndex& ZoneIndex, FString Name, FString Value) {
    GetGeneratorComponent()->SetZoneTag(ZoneIndex, Name, Value);
}

void TStructuresGenerator::SetChunkTag(const TVoxelIndex& ChunkIndex, FString Name, FString Value) {
    GetGeneratorComponent()->SetChunkTag(ChunkIndex, Name, Value);
}

