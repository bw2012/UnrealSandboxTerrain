// Copyright blackw 2015-2020

#include "TerrainGeneratorComponent.h"
#include "SandboxTerrainController.h"
#include "perlin.hpp"
#include "memstat.h"
#include <algorithm>
#include <thread>

#include "TerrainZoneComponent.h"

#define USBT_VGEN_GROUND_LEVEL_OFFSET       205.f
#define USBT_DEFAULT_GRASS_MATERIAL_ID      2


static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;


class TChunkData {

private:

    int Size;
    float* HeightLevelArray;
    float MaxHeightLevel = -999999.f;
    float MinHeightLevel = 999999.f;

public:

    TChunkData(int Size) {
        this->Size = Size;
        HeightLevelArray = new float[Size * Size * Size];
        cd_counter++;
    }

    ~TChunkData() {
        delete[] HeightLevelArray;
        cd_counter--;
    }

    float const* const GetHeightLevelArrayPtr() const {
        return HeightLevelArray;
    }

    FORCEINLINE void SetHeightLevel(const int X, const int Y, float HeightLevel) {
        if (X < Size && Y < Size) {
            int Index = X * Size + Y;
            HeightLevelArray[Index] = HeightLevel;

            if (HeightLevel > this->MaxHeightLevel) {
                this->MaxHeightLevel = HeightLevel;
            }

            if (HeightLevel < this->MinHeightLevel) {
                this->MinHeightLevel = HeightLevel;
            }
        }
    }

    FORCEINLINE float GetHeightLevel(const int X, const int Y) const {
        if (X < Size && Y < Size) {
            int Index = X * Size + Y;
            return HeightLevelArray[Index];
        } else {
            return 0;
        }
    }

    FORCEINLINE float GetMaxHeightLevel() const {
        return this->MaxHeightLevel;
    };

    FORCEINLINE float GetMinHeightLevel() const {
        return this->MinHeightLevel;
    };
};


UTerrainGeneratorComponent::UTerrainGeneratorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
    this->Pn = new TPerlinNoise();
    this->DfaultGrassMaterialId = USBT_DEFAULT_GRASS_MATERIAL_ID;
}

void UTerrainGeneratorComponent::BeginPlay() {
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("UTerrainGeneratorComponent::BeginPlay"));

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
    LastLayer.StartDepth = 9999999.f;
    LastLayer.Name = TEXT("");
    UndergroundLayersTmp.Add(LastLayer);

    PrepareMetaData();
}

void UTerrainGeneratorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    Super::EndPlay(EndPlayReason);
}

void UTerrainGeneratorComponent::FinishDestroy() {
    Super::FinishDestroy();
    delete this->Pn;
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
    FVector ZonePos = GetController()->GetZonePos(ZoneIndex);
    return ZoneHash(ZonePos);
}


float UTerrainGeneratorComponent::PerlinNoise(const float X, const float Y, const float Z) const {
	if (Pn) {
		return Pn->noise(X, Y, Z);
	}

	return 0;
};

float UTerrainGeneratorComponent::PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
	if (Pn) {
		return Pn->noise(Pos.X * PositionScale, Pos.Y * PositionScale, Pos.Z * PositionScale) * ValueScale;
	}

	return 0;
}

float UTerrainGeneratorComponent::PerlinNoise(const FVector& Pos) const {
	if (Pn) {
		return Pn->noise(Pos.X, Pos.Y, Pos.Z);
	}

	return 0;
}

//======================================================================================================================================================================
// 
//======================================================================================================================================================================

int UTerrainGeneratorComponent::GetMaterialLayers(const TChunkData* ChunkData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const {
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

FORCEINLINE TMaterialId UTerrainGeneratorComponent::MaterialFuncionExt(const TGenerateVdTempItm* GenItm, const TMaterialId MatId, const FVector& WorldPos) const {
    return  MatId;
}

//======================================================================================================================================================================
// Density
//======================================================================================================================================================================

float UTerrainGeneratorComponent::GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const {
    //float scale1 = 0.0035f; // small
    const float scale1 = 0.001f; // small
    const float scale2 = 0.0004f; // medium
    const float scale3 = 0.00009f; // big

    float noise_small = Pn->noise(V.X * scale1, V.Y * scale1, 0) * 0.5f; // 0.5
    float noise_medium = Pn->noise(V.X * scale2, V.Y * scale2, 0) * 5.f;
    float noise_big = Pn->noise(V.X * scale3, V.Y * scale3, 0) * 10.f;
    //float noise_big = Pn->noise(V.X * scale2, V.Y * scale2, 0) * 30.f; // min zone z = -2

    /*
    const float MaxR = 2000; // max radius
    const float H = 6; // height
    FVector Origin(0, 0, 0);
    FVector Tmp = V - Origin; 
    float R = std::sqrt(Tmp.X * Tmp.X + Tmp.Y * Tmp.Y);
    //float t = 1 - exp(-pow(R, 2) / ( MaxR * 100)); // hollow
    float t = exp(-pow(R, 2) / (MaxR * 100)) * H; // hill
    */


    {

        const static float test = 0.5;
        if (noise_big > -test && noise_big < test) {
            float ttt = noise_big * 100;
            FVector V2(V.X, V.Y, ttt);
            AsyncTask(ENamedThreads::GameThread, [=]() {
               // DrawDebugPoint(GetWorld(), V2, 3.f, FColor(255, 255, 255, 0), true);
            });
        }
        
    }


    const float gl = noise_small + noise_medium + noise_big;
    //const float gl = noise_big;
    return (gl * 100) + USBT_VGEN_GROUND_LEVEL_OFFSET;
}


FORCEINLINE float UTerrainGeneratorComponent::DensityFunctionExt(float Density, const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) const {
    return Density;
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
    return DensityByGroundLevel;
}

TChunkData* UTerrainGeneratorComponent::NewChunkData() {
    return new TChunkData(ZoneVoxelResolution);
}

TChunkData* UTerrainGeneratorComponent::GenerateChunkData(const TVoxelIndex& Index) {
    TChunkData* ChunkData = NewChunkData();
    double Start = FPlatformTime::Seconds();

    const float Step = USBT_ZONE_SIZE / (ZoneVoxelResolution - 1);
    const float S = -USBT_ZONE_SIZE / 2;
    for (int VX = 0; VX < ZoneVoxelResolution; VX++) {
        for (int VY = 0; VY < ZoneVoxelResolution; VY++) {
            const FVector LocalPos(S + VX * Step, S + VY * Step, S);
            FVector WorldPos = LocalPos + GetController()->GetZonePos(Index);
            float GroundLevel = GroundLevelFunction(Index, WorldPos);
            ChunkData->SetHeightLevel(VX, VY, GroundLevel);

            /*
            {
                static const float scale3 = 0.001f;
                const FVector v3(WorldPos.X * scale3, WorldPos.Y * scale3, 0); // extra cave height
                const float Noise3 = PerlinNoise(v3);
                const float BaseCaveHeight = 400;
                const float ExtraCaveHeight = 490 * Noise3;
                float CaveHeight = BaseCaveHeight + ExtraCaveHeight;
                //float CaveHeight = BaseCaveHeight;
                if (CaveHeight < 0) {
                   // CaveHeight = 0; // protection if my calculation is failed
                }

                //cave level
                const float BaseCaveLevel = 3000;
                static const float scale4 = 0.00025f;
                const FVector v4(WorldPos.X * scale4, WorldPos.Y * scale4, 10); // extra cave height
                const float Noise4 = PerlinNoise(v4);
                const float ExtraCaveLevel = 1000 * Noise4;
                const float CaveLevel = BaseCaveLevel + ExtraCaveLevel;

                {
                    FVector V(WorldPos.X, WorldPos.Y, CaveLevel);
                    AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), V, 3.f, FColor(255, 255, 255, 0), true); });
                }
            }
            */
        }
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("Generate height map  ----> %f ms --  %d %d"), Time, X, Y);

    //const static size_t SSS = sizeof(TChunkData) + sizeof(float) * ZoneVoxelResolution * ZoneVoxelResolution * ZoneVoxelResolution;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("%d"), ChunkDataCollection.size() * SSS);

    return ChunkData;
}

TChunkData* UTerrainGeneratorComponent::GetChunkData(int X, int Y) {
    const std::lock_guard<std::mutex> lock(ChunkDataMapMutex);

    TVoxelIndex Index(X, Y, 0);
    TChunkData* ChunkData = nullptr;

    if (ChunkDataCollection.find(Index) == ChunkDataCollection.end()) {
        ChunkData = GenerateChunkData(Index);
        ChunkDataCollection.insert({ Index, ChunkData });
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

                int P = Processed[Idx];
                if (Processed[Idx] == 0x0) {
                    const float D = Handler(TTT, Idx, VoxelData, LOD);
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

    std::function<float(const TVoxelIndex&, int, TVoxelData*, int)> Handler = nullptr;
    std::function<bool(const TVoxelIndex&, int, int, const TVoxelData*)> CheckVoxel = nullptr;
    std::function<bool(int)> AllowNoActiveChilds = nullptr;

    TPseudoOctree(TVoxelData* Vd) : VoxelData(Vd) {
        int N = Vd->num();
        Total = N * N * N;
        Processed = new int[Total];
        for (int I = 0; I < Total; I++) {
            Processed[I] = 0x0;
        }

        Handler = [](const TVoxelIndex& V, int Idx, TVoxelData* VoxelData, int LOD) { return 0.f; };
        CheckVoxel = [](const TVoxelIndex& V, int S, int LOD, const TVoxelData* VoxelData) { return true;  };
        AllowNoActiveChilds = [](int LOD) { return true;  };
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
    /* const */ TChunkData* ChunkData = Itm.ChunkData;

    //DrawDebugBox(GetWorld(), VoxelData->getOrigin(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true);

    double Start2 = FPlatformTime::Seconds();

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->deinitializeMaterial(DfaultGrassMaterialId);

    TPseudoOctree Octree(VoxelData);
    Octree.Handler = [=] (const TVoxelIndex& V, int Idx, TVoxelData* VoxelData, int LOD){
        const FVector& LocalPos = VoxelData->voxelIndexToVector(V.X, V.Y, V.Z);
        const FVector& WorldPos = LocalPos + VoxelData->getOrigin();

        if (LOD == 0) {
            //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), WorldPos, 3.f, FColor(255, 255, 255, 0), true); });
        }

       return B(V, VoxelData, ChunkData);
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
                //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), Pos2, 3.f, FColor(255, 100, 100, 0), true); });
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

    double End2 = FPlatformTime::Seconds();
    double Time2 = (End2 - Start2) * 1000;
    float Ratio = (float)Octree.GetCount() / (float)Octree.GetTotal() * 100.f;

    //UE_LOG(LogSandboxTerrain, Warning, TEXT("Slight Generation -> %f ms -> %d points -> %.4f%% -> %d %d %d"), Time2, Octree.GetCount(), Ratio, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
}

void UTerrainGeneratorComponent::ForceGenerateZone(TVoxelData* VoxelData, const TVoxelIndex& ZoneIndex) {
    TGenerateVdTempItm Itm;
    //Itm.Type; TODO

    Itm.Idx = 0;
    Itm.ZoneIndex = ZoneIndex;
    Itm.Vd = VoxelData;
    Itm.ChunkData = GetChunkData(ZoneIndex.X, ZoneIndex.Y);

    ExtVdGenerationData(Itm);

    GenerateZoneVolume(Itm);
}

bool IsLandscapeZone(const FVector& Pos, const TChunkData* ChunkData) {
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
    /* const */ TChunkData* ChunkData = Itm.ChunkData;
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

                float Density = 1.f;
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

                //if (Itm.bHasMaterialExt) {
                    MaterialId = MaterialFuncionExt(&Itm, MaterialId, WorldPos);
                //}

                const float Density2 = DensityFunctionExt(Density, ZoneIndex, WorldPos, LocalPos);
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
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

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

void UTerrainGeneratorComponent::GenerateZoneVolume(const TGenerateVdTempItm& Itm) const {
    double Start = FPlatformTime::Seconds();

    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    /* const */ TChunkData* ChunkData = Itm.ChunkData;
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
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

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
    const float Density2 = DensityFunctionExt(Density, ZoneIndex, WorldPos, LocalPos);
    TMaterialId MaterialId = MaterialFuncion(ZoneIndex, WorldPos, GroundLevel);

    MaterialId = MaterialFuncionExt(&Itm, MaterialId, WorldPos);

    VoxelData->setDensityAndMaterial(VoxelIndex, Density2, MaterialId);
    auto Result = std::make_tuple(LocalPos, WorldPos, Density2, MaterialId);
    return Result;
};

float UTerrainGeneratorComponent::B(const TVoxelIndex& Index, TVoxelData* VoxelData, const TChunkData* ChunkData) const {
    const FVector& LocalPos = VoxelData->voxelIndexToVector(Index.X, Index.Y, Index.Z);
    const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
    const float GroundLevel = ChunkData->GetHeightLevel(Index.X, Index.Y);
    const float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
    const float Density2 = DensityFunctionExt(Density, Index, WorldPos, LocalPos);
    vd::tools::unsafe::setDensity(VoxelData, Index, Density2);
    return Density2;
};

void UTerrainGeneratorComponent::BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& List) {
    double Start1 = FPlatformTime::Seconds();

    for (const auto& Itm : List) {
        const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;

        auto ZoneHandlerList = StructureMap[ZoneIndex];
        if (ZoneHandlerList.size() > 0) {
            //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(GetWorld(), Itm.Vd->getOrigin(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });
            GenerateZoneVolumeWithFunction(Itm, ZoneHandlerList);
            continue;
        }

        GenerateZoneVolume(Itm);
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateVd Pass2 -> %f ms"), Time1);
}

void UTerrainGeneratorComponent::BatchGenerateSlightVd(TArray<TGenerateVdTempItm>& List) {
    double Start1 = FPlatformTime::Seconds();

    for (const auto& Itm : List) {
        if (Itm.Type == TZoneGenerationType::Landscape) {
            GenerateLandscapeZoneSlight(Itm);
            continue;
        }

        // TODO handle others
        //UE_LOG(LogSandboxTerrain, Error, TEXT("BatchGenerateSlightVd: no handler - %d %d %d"), Itm.ZoneIndex.X, Itm.ZoneIndex.Y, Itm.ZoneIndex.Z);
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateVd Pass2 -> %f ms"), Time1);
}

TZoneGenerationType UTerrainGeneratorComponent::ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkData* ChunkData) {
    const FVector& Pos = GetController()->GetZonePos(ZoneIndex);
    //static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;

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

    if (IsLandscapeZone(Pos, ChunkData)) {
        return TZoneGenerationType::Landscape;
    }

    return TZoneGenerationType::Other;
}

void UTerrainGeneratorComponent::GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkData* ChunkData) {
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

    ExtVdGenerationData(VdGenerationData);
    
    return VdGenerationData;
}

void UTerrainGeneratorComponent::BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& BatchList, TArray<TGenerateZoneResult>& NewVdArray) {
    NewVdArray.Empty();
    NewVdArray.SetNumZeroed(BatchList.Num());
    TArray<TGenerateVdTempItm> ComplexList;
    TArray<TGenerateVdTempItm> FastList;

    int Idx = 0;
    for (const auto& P : BatchList) {
        const FVector Pos = GetController()->GetZonePos(P.Index);
        TVoxelData* NewVd = GetController()->NewVoxelData();
        NewVd->setOrigin(Pos);

        TGenerateVdTempItm GenItm = CollectVdGenerationData(P.Index);
        GenItm.Idx = Idx;
        GenItm.Vd = NewVd;
        NewVdArray[Idx].Vd = NewVd;
        NewVdArray[Idx].Type = GenItm.Type;
        NewVdArray[Idx].Method = GenItm.Method;

        if (GenItm.Method == TGenerationMethod::SetEmpty) {
            GenerateSimpleVd(P.Index, NewVd, GenItm.Type, GenItm.ChunkData);
        } else if (GenItm.Method == TGenerationMethod::Skip) {
            NewVd->deinitializeDensity(TVoxelDataFillState::FULL);
            NewVd->deinitializeMaterial(0);
            NewVd->setCacheToValid();
        } else if (GenItm.Method == TGenerationMethod::FastSimple) {
            FastList.Add(GenItm);
        } else if (GenItm.Method == TGenerationMethod::SlowComplex) {
            ComplexList.Add(GenItm);
        } else {
            // TODO assert unknown method
        }

        Idx++;
    }


    double Start2 = FPlatformTime::Seconds();

    //std::thread T1([&]() { BatchGenerateComplexVd(ComplexList); });
   //std::thread T2([&]() { BatchGenerateSlightVd(FastList); });

    //T1.join();
    //T2.join();

    if (ComplexList.Num() > 0) {
        BatchGenerateComplexVd(ComplexList);
    }

    if (FastList.Num() > 0) {
        BatchGenerateSlightVd(FastList);
    }

    double End2 = FPlatformTime::Seconds();
    double Time2 = (End2 - Start2) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateVd Pass2 -> %f ms"), Time2);

    OnBatchGenerationFinished();
}

void UTerrainGeneratorComponent::OnBatchGenerationFinished() {

}

void UTerrainGeneratorComponent::Clean() {
    const std::lock_guard<std::mutex> lock(ChunkDataMapMutex);
    for (auto Itm : ChunkDataCollection) {
        //delete Itm.second;

    }
    //ChunkDataCollection.clear(); //UNSAFE
}

void UTerrainGeneratorComponent::Clean(TVoxelIndex& Index) {

}

//======================================================================================================================================================================
// Foliage
//======================================================================================================================================================================


void UTerrainGeneratorComponent::GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    TChunkData* ChunkData = GetChunkData(Index.X, Index.Y);
    auto Type = ZoneGenType(Index, ChunkData);

    if (Type == TZoneGenerationType::AirOnly || Type == TZoneGenerationType::FullSolidOneMaterial || Type == TZoneGenerationType::FullSolidMultipleMaterials) {
        //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(GetWorld(), Vd->getOrigin(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });
        //return;
    }

    if (GetController()->FoliageMap.Num() != 0) {   
        if (Type == TZoneGenerationType::Landscape) {
            GenerateNewFoliageLandscape(Index, ZoneInstanceMeshMap);
        }

        GenerateNewFoliageCustom(Index, Vd, ZoneInstanceMeshMap);
    }

    PostGenerateNewInstanceObjects(Index, Type, Vd, ZoneInstanceMeshMap);
}

void UTerrainGeneratorComponent::PostGenerateNewInstanceObjects(const TVoxelIndex& ZoneIndex, const TZoneGenerationType ZoneType, const TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) const {

}

bool UTerrainGeneratorComponent::SelectRandomSpawnPoint(FRandomStream& Rnd, const TVoxelIndex& ZoneIndex, const TVoxelData* Vd, FVector& SectedLocation, FVector& SectedNormal) const {
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
    const TArray<FProcMeshVertex>& Vertexes = TmpMesh->MeshSectionLodArray[0].WholeMesh.ProcVertexBuffer;

    if (Vertexes.Num() > 0) {
        FVector AveragePos(0);
        FVector AverageNormal(0);

        for (const auto& Vertex : Vertexes) {
            FVector Position(Vertex.PositionX, Vertex.PositionY, Vertex.PositionZ);
            FVector Normal(Vertex.NormalX, Vertex.NormalY, Vertex.NormalZ);
            AveragePos += Position;
            AverageNormal += Normal;
        }

        AveragePos /= Vertexes.Num();
        AveragePos += Vd->getOrigin();

        AverageNormal /= Vertexes.Num();
        AverageNormal.Normalize(0.01f);

        SectedLocation = AveragePos;
        SectedNormal = AverageNormal;

        {
            /*
            AsyncTask(ENamedThreads::GameThread, [=]() {
                FVector TT = SectedLocation + (SectedNormal * 200);
                DrawDebugPoint(GetWorld(), SectedLocation, 6.f, FColor(255, 111, 111, 0), true);
                DrawDebugLine(GetWorld(), SectedLocation, TT, FColor(255, 111, 111, 0), true);
             });
            */
        }

        return true;
    }

    return false;
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

                    FSandboxFoliage FoliageType2 = FoliageExt(FoliageTypeId, FoliageType, Index, V);
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
                                        //UE_LOG(LogSandboxTerrain, Log, TEXT("TEST -> %d"), MeshVariantId);
                                    }

                                    const FVector NewPos = WorldLocation - ZonePos;

                                    bool bIsValidPosition = true;
                                    if (HasStructures(Index)) {
                                        auto ZoneHandlerList = StructureMap[Index];
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
                                    FTerrainInstancedMeshType MeshType;
                                    MeshType.MeshTypeId = FoliageTypeId;
                                    MeshType.MeshVariantId = MeshVariantId;
                                    MeshType.Mesh = FoliageType2.MeshVariants[MeshVariantId];
                                    MeshType.StartCullDistance = FoliageType2.StartCullDistance;
                                    MeshType.EndCullDistance = FoliageType2.EndCullDistance;

                                    auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(MeshType.GetMeshTypeCode());
                                    InstanceMeshContainer.MeshType = MeshType;
                                    InstanceMeshContainer.TransformArray.Add(Transform);

                                    Counter++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //UE_LOG(LogSandboxTerrain, Warning, TEXT("Zone: %d %d %d -> %d meshes"), Index.X, Index.Y, Index.Z, Counter);
}

void UTerrainGeneratorComponent::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {

}

void UTerrainGeneratorComponent::GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    if (GetController()->FoliageMap.Num() == 0) {
        return;
    }

    if (!UseCustomFoliage(Index)) {
        return;
    }

    FVector ZonePos = GetController()->GetZonePos(Index);
    int32 Hash = ZoneHash(ZonePos);
    FRandomStream rnd = FRandomStream();
    rnd.Initialize(Hash);
    rnd.Reset();


    for (auto& Elem : GetController()->FoliageMap) {
        FSandboxFoliage FoliageType = Elem.Value;

        if (FoliageType.Type != ESandboxFoliageType::Custom) {
            continue;
        }

        Vd->forEachCacheItem(0, [&](const TSubstanceCacheItem& itm) {
            uint32 x;
            uint32 y;
            uint32 z;

            Vd->clcVoxelIndex(itm.index, x, y, z);
            FVector WorldPos = Vd->voxelIndexToVector(x, y, z) + Vd->getOrigin();
            int32 FoliageTypeId = Elem.Key;

            FTransform Transform;

            bool bSpawn = SpawnCustomFoliage(Index, WorldPos, FoliageTypeId, FoliageType, rnd, Transform);
            if (bSpawn) {
                if (FoliageType.MeshVariants.Num() > 0) {
                    uint32 MeshVariantId = 0;
                    if (FoliageType.MeshVariants.Num() > 1) {
                        MeshVariantId = rnd.RandRange(0, FoliageType.MeshVariants.Num() - 1);
                        //UE_LOG(LogSandboxTerrain, Log, TEXT("TEST2 -> %d"), MeshVariantId);
                    }

                    FTerrainInstancedMeshType MeshType;
                    MeshType.MeshTypeId = FoliageTypeId;
                    MeshType.MeshVariantId = MeshVariantId;
                    MeshType.Mesh = FoliageType.MeshVariants[MeshVariantId];
                    MeshType.StartCullDistance = FoliageType.StartCullDistance;
                    MeshType.EndCullDistance = FoliageType.EndCullDistance;

                    auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(MeshType.GetMeshTypeCode());
                    InstanceMeshContainer.MeshType = MeshType;
                    InstanceMeshContainer.TransformArray.Add(Transform);
                }
            }

            });
    }
}


bool UTerrainGeneratorComponent::UseCustomFoliage(const TVoxelIndex& Index) {
    return false;
}

bool UTerrainGeneratorComponent::SpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform) {
    return false;
}

FSandboxFoliage UTerrainGeneratorComponent::FoliageExt(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos) {
    return FoliageType;
}

bool UTerrainGeneratorComponent::OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale) {
    return true;
}

bool UTerrainGeneratorComponent::IsOverrideGroundLevel(const TVoxelIndex& Index) {
    return false;
}

float UTerrainGeneratorComponent::GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel) {
    return GroundLevel;
}

bool UTerrainGeneratorComponent::IsForcedComplexZone(const TVoxelIndex& ZoneIndex) {
    return false;
}

bool UTerrainGeneratorComponent::HasStructures(const TVoxelIndex& ZoneIndex) const {
    return StructureMap.find(ZoneIndex) != StructureMap.end();
}

void UTerrainGeneratorComponent::PrepareMetaData() {

}

void UTerrainGeneratorComponent::AddZoneStructure(const TVoxelIndex& ZoneIndex, const TZoneStructureHandler& Structure) {
    auto& StructureList = StructureMap[ZoneIndex];
    StructureList.push_back(Structure);
}

const FString* UTerrainGeneratorComponent::GetExtZoneParam(const TVoxelIndex& ZoneIndex, FString Name) const {
    if (ZoneExtData.Contains(ZoneIndex)) {
        const TMap<FString, FString>& ExtData = ZoneExtData[ZoneIndex];
        return ExtData.Find(Name);
    }

    return nullptr;
}

bool UTerrainGeneratorComponent::CheckExtZoneParam(const TVoxelIndex& ZoneIndex, FString Name, FString Value) const{
    const FString* Param = GetExtZoneParam(ZoneIndex, Name);
    if (Param) {
        if (*Param == Value) {
            return true;
        }
    }

    return false;
}