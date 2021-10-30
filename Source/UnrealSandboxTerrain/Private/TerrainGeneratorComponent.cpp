// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "TerrainGeneratorComponent.h"
#include "perlin.hpp"


#define USBT_VGEN_GROUND_LEVEL_OFFSET       205.f
#define USBT_DEFAULT_GRASS_MATERIAL_ID      2



class TChunkHeightMapData {

private:

    int Size;
    float* HeightLevelArray;
    float MaxHeightLevel = -999999.f;
    float MinHeightLevel = 999999.f;

public:

    TChunkHeightMapData(int Size) {
        this->Size = Size;
        HeightLevelArray = new float[Size * Size * Size];
    }

    ~TChunkHeightMapData() {
        delete[] HeightLevelArray;
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

    UE_LOG(LogTemp, Log, TEXT("UTerrainGeneratorComponent::BeginPlay"));

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


int32 UTerrainGeneratorComponent::ZoneHash(const FVector& ZonePos) {
	int32 Hash = 7;
	Hash = Hash * 31 + (int32)ZonePos.X;
	Hash = Hash * 31 + (int32)ZonePos.Y;
	Hash = Hash * 31 + (int32)ZonePos.Z;
	return Hash;
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

int UTerrainGeneratorComponent::GetMaterialLayers(const TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const {
    static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
    float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize;
    float ZoneLow = ZoneOrigin.Z - ZoneHalfSize;
    float TerrainHigh = ChunkHeightMapData->GetMaxHeightLevel();
    float TerrainLow = ChunkHeightMapData->GetMinHeightLevel();

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

FORCEINLINE TMaterialId UTerrainGeneratorComponent::MaterialFuncion(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel) const {
    const float DeltaZ = WorldPos.Z - GroundLevel;

    TMaterialId mat = 0;
    if (DeltaZ >= -70) {
        mat = DfaultGrassMaterialId; // grass
    } else {
        const FTerrainUndergroundLayer* Layer = GetMaterialLayer(WorldPos.Z, GroundLevel);
        if (Layer != nullptr) {
            mat = Layer->MatId;
        }
    }

    return mat;
}

//======================================================================================================================================================================
// Density
//======================================================================================================================================================================

FORCEINLINE float UTerrainGeneratorComponent::GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const {
    //float scale1 = 0.0035f; // small
    const float scale1 = 0.001f; // small
    const float scale2 = 0.0004f; // medium
    const float scale3 = 0.00009f; // big

    float noise_small = Pn->noise(V.X * scale1, V.Y * scale1, 0) * 0.5f;
    float noise_medium = Pn->noise(V.X * scale2, V.Y * scale2, 0) * 5;
    float noise_big = Pn->noise(V.X * scale3, V.Y * scale3, 0) * 10;

    //float r = std::sqrt(V.X * V.X + V.Y * V.Y);
    //const float MaxR = 5000;
    //float t = 1 - exp(-pow(r, 2) / ( MaxR * 100));

    const float gl = noise_small + noise_medium + noise_big;
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


TChunkHeightMapData* UTerrainGeneratorComponent::GetChunkHeightMap(int X, int Y) {
    const std::lock_guard<std::mutex> lock(ZoneHeightMapMutex);

    TVoxelIndex Index(X, Y, 0);
    TChunkHeightMapData* ChunkHeightMapData = nullptr;

    if (ChunkDataCollection.find(Index) == ChunkDataCollection.end()) {
        ChunkHeightMapData = new TChunkHeightMapData(USBT_ZONE_DIMENSION);
        ChunkDataCollection.insert({ Index, ChunkHeightMapData });

        double Start = FPlatformTime::Seconds();
        const float step = USBT_ZONE_SIZE / (USBT_ZONE_DIMENSION - 1);
        const float s = -USBT_ZONE_SIZE / 2;

        for (int X = 0; X < USBT_ZONE_DIMENSION; X++) {
            for (int Y = 0; Y < USBT_ZONE_DIMENSION; Y++) {
                FVector a(X * step, Y * step, 0);
                FVector v(s, s, s);
                v += a;
                const FVector& LocalPos = v;
                FVector WorldPos = LocalPos + GetController()->GetZonePos(Index);
                float GroundLevel = GroundLevelFunction(Index, WorldPos);
                ChunkHeightMapData->SetHeightLevel(X, Y, GroundLevel);
            }
        }

        double End = FPlatformTime::Seconds();
        double Time = (End - Start) * 1000;
        UE_LOG(LogSandboxTerrain, Log, TEXT("generate height map  ----> %f ms --  %d %d"), Time, X, Y);
    } else {
        ChunkHeightMapData = ChunkDataCollection[Index];
    }

    return ChunkHeightMapData;
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

public:

    TVoxelData* VoxelData;
    TChunkHeightMapData* ChunkData;
    int* Processed;
    const int Total = USBT_ZONE_DIMENSION * USBT_ZONE_DIMENSION * USBT_ZONE_DIMENSION;
    int Count = 0;

    std::function<void(const TVoxelIndex&, int, TVoxelData*)> Handler = nullptr;

    TPseudoOctree() {
        Processed = new int[Total];
        for (int I = 0; I < Total; I++) {
            Processed[I] = 0x0;
        }

        Handler = [=](const TVoxelIndex& V, int Idx, TVoxelData* VoxelData) { };
    };

    ~TPseudoOctree() {
        delete Processed;
    };

    bool CheckVoxel(const TVoxelIndex& Idx, int S) {
        const int X = Idx.X;
        const int Y = Idx.Y;
        const int Z = Idx.Z;

        const FVector& Pos = VoxelData->voxelIndexToVector(X, Y, Z);
        const FVector& Pos2 = VoxelData->voxelIndexToVector(X, Y, Z + S);

        TMinMax MinMax;
        MinMax << ChunkData->GetHeightLevel(X, Y) << ChunkData->GetHeightLevel(X + S, Y) << ChunkData->GetHeightLevel(X, Y + S) << ChunkData->GetHeightLevel(X + S, Y + S);

        bool b = std::max(Pos.Z, MinMax.Min) <= std::min(Pos2.Z, MinMax.Max);

        //UE_LOG(LogSandboxTerrain, Warning, TEXT("Z1 = %f, Z2 = %f"), Pos.Z, Pos2.Z);
        //UE_LOG(LogSandboxTerrain, Warning, TEXT("Min = %f, Max = %f"), MinMax.Min, MinMax.Max);

        return b;
    };

    void PerformVoxel(const TVoxelIndex& Parent, const int LOD) {
        const int S = 1 << LOD;

        if (CheckVoxel(Parent, S)) {
            TVoxelIndex Tmp[8];
            vd::tools::makeIndexes(Tmp, Parent, S);
            vd::tools::unsafe::forceAddToCache(VoxelData, Parent.X, Parent.Y, Parent.Z, LOD);
            for (auto i = 0; i < 8; i++) {
                const TVoxelIndex& TTT = Tmp[i];
                int Idx = vd::tools::clcLinearIndex(USBT_ZONE_DIMENSION, TTT.X, TTT.Y, TTT.Z);

                if (Processed[Idx] == 0) {
                    Handler(TTT, Idx, VoxelData);
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

    void Start() {
        PerformVoxel(TVoxelIndex(0, 0, 0), 6);
    };


};

void UTerrainGeneratorComponent::GenerateLandscapeZoneSlight(const TGenerateVdTempItm& Itm) const {
    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    /* const */ TChunkHeightMapData* ChunkData = Itm.ChunkData;

    double Start2 = FPlatformTime::Seconds();

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->deinitializeMaterial(DfaultGrassMaterialId);

    TPseudoOctree Octree;
    Octree.VoxelData = VoxelData;
    Octree.ChunkData = ChunkData;
    Octree.Handler = [=] (const TVoxelIndex& V, int Idx, TVoxelData* VoxelData){
       B(V, VoxelData, ChunkData);
        //const FVector& WorldPos = std::get<1>(R);
        //const FVector& LocalPos = VoxelData->voxelIndexToVector(V.X, V.Y, V.Z);
        //const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
        //AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugPoint(GetWorld(), WorldPos, 3.f, FColor(255, 255, 255, 0), true); });
    };

    Octree.Start();
    VoxelData->setCacheToValid();

    double End2 = FPlatformTime::Seconds();
    double Time2 = (End2 - Start2) * 1000;
    float Ratio = (float)Octree.Count / (float)Octree.Total * 100.f;

    UE_LOG(LogSandboxTerrain, Warning, TEXT("Slight Generation -> %f ms -> %d points -> %.4f%%"), Time2, Octree.Count, Ratio);
}

void UTerrainGeneratorComponent::GenerateZoneVolume(const TGenerateVdTempItm& Itm) const {
    double Start = FPlatformTime::Seconds();

    const TVoxelIndex& ZoneIndex = Itm.ZoneIndex;
    TVoxelData* VoxelData = Itm.Vd;
    /* const */ TChunkHeightMapData* ChunkData = Itm.ChunkData;
    const int LOD = Itm.GenerationLOD;
    int zc = 0;
    int fc = 0;

    const int S = 1 << LOD;
    bool bContainsMoreOneMaterial = false;
    TMaterialId BaseMaterialId = 0;

    VoxelData->initCache();
    VoxelData->initializeDensity();
    VoxelData->initializeMaterial();

    for (int X = 0; X < USBT_ZONE_DIMENSION; X += S) {
        for (int Y = 0; Y < USBT_ZONE_DIMENSION; Y += S) {
            for (int Z = 0; Z < USBT_ZONE_DIMENSION; Z += S) {
                const FVector& LocalPos = VoxelData->voxelIndexToVector(X, Y, Z);
                const TVoxelIndex& Index = TVoxelIndex(X, Y, Z);

                auto R = A(ZoneIndex, Index, VoxelData, ChunkData);
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
    UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

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

ResultA UTerrainGeneratorComponent::A(const TVoxelIndex& ZoneIndex, const TVoxelIndex& VoxelIndex, TVoxelData* VoxelData, const TChunkHeightMapData* ChunkData) const {
    const FVector& LocalPos = VoxelData->voxelIndexToVector(VoxelIndex.X, VoxelIndex.Y, VoxelIndex.Z);
    const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
    const float GroundLevel = ChunkData->GetHeightLevel(VoxelIndex.X, VoxelIndex.Y);
    const float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
    const float Density2 = DensityFunctionExt(Density, ZoneIndex, WorldPos, LocalPos);
    TMaterialId MaterialId = MaterialFuncion(LocalPos, WorldPos, GroundLevel);
    VoxelData->setDensityAndMaterial(VoxelIndex, Density2, MaterialId);
    auto Result = std::make_tuple(LocalPos, WorldPos, Density2, MaterialId);
    return Result;
};

void UTerrainGeneratorComponent::B(const TVoxelIndex& Index, TVoxelData* VoxelData, const TChunkHeightMapData* ChunkData) const {
    const FVector& LocalPos = VoxelData->voxelIndexToVector(Index.X, Index.Y, Index.Z);
    const FVector& WorldPos = LocalPos + VoxelData->getOrigin();
    const float GroundLevel = ChunkData->GetHeightLevel(Index.X, Index.Y);
    const float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
    const float Density2 = DensityFunctionExt(Density, Index, WorldPos, LocalPos);
    //VoxelData->setDensity(Index, Density2);
    vd::tools::unsafe::setDensity(VoxelData, Index, Density2);
};


void UTerrainGeneratorComponent::BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& SecondPassList) {
    double Start1 = FPlatformTime::Seconds();

    for (const auto& Itm : SecondPassList) {
        if (Itm.bSlightGeneration) {
            if (Itm.Type == 2) {
                GenerateLandscapeZoneSlight(Itm);
                continue;
            }
        }

        GenerateZoneVolume(Itm);
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateVd Pass2 -> %f ms"), Time1);
}

int UTerrainGeneratorComponent::ZoneGenType(const TVoxelIndex& ZoneIndex, const TChunkHeightMapData* ChunkHeightMapData) {
    if (ForcePerformZone(ZoneIndex)) {
        return 3;
    }

    const FVector& Pos = GetController()->GetZonePos(ZoneIndex);
    static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;


    if (ChunkHeightMapData->GetMaxHeightLevel() < Pos.Z - ZoneHalfSize) {
        return 0; // air only
    }

    if (ChunkHeightMapData->GetMinHeightLevel() > Pos.Z + ZoneHalfSize) {
        TArray<FTerrainUndergroundLayer> LayerList;
        if (GetMaterialLayers(ChunkHeightMapData, Pos, &LayerList) == 1) {
            return 1; //full solid
        } 
    }

    float ZoneHigh = Pos.Z + ZoneHalfSize; // +100
    float ZoneLow = Pos.Z - ZoneHalfSize; //-10
    float TerrainHigh = ChunkHeightMapData->GetMaxHeightLevel();
    float TerrainLow = ChunkHeightMapData->GetMinHeightLevel();
    if (std::max(ZoneLow, TerrainLow) <= std::min(ZoneHigh, TerrainHigh)) {
        /*
        AsyncTask(ENamedThreads::GameThread, [=]() {
            DrawDebugBox(GetWorld(), Pos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 100), true);
        });
        */ 
        return 2; // landscape
    }

    return 3;
}

void UTerrainGeneratorComponent::GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const int Type, const TChunkHeightMapData* ChunkData) {
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

void UTerrainGeneratorComponent::BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& BatchList, TArray<TVoxelData*>& NewVdArray) {
    NewVdArray.Empty();
    NewVdArray.SetNumZeroed(BatchList.Num());
    TArray<TGenerateVdTempItm> SecondPassList;

    double Start = FPlatformTime::Seconds();
    int Idx = 0;
    for (const auto& P : BatchList) {
        const FVector Pos = GetController()->GetZonePos(P.Index);
        TVoxelData* NewVd = GetController()->NewVoxelData();
        NewVd->setOrigin(Pos);

        TChunkHeightMapData* ChunkData = GetChunkHeightMap(P.Index.X, P.Index.Y);
        int Type = ZoneGenType(P.Index, ChunkData);
        
        if (Type < 2) {
            GenerateSimpleVd(P.Index, NewVd, Type, ChunkData);
        } else {
            TGenerateVdTempItm SecondPassItm;
            SecondPassItm.Type = Type;
            SecondPassItm.Idx = Idx;
            SecondPassItm.ZoneIndex = P.Index;
            SecondPassItm.Vd = NewVd;
            SecondPassItm.ChunkData = ChunkData;

#ifdef USBT_EXPERIMENTAL_FAST_GENERATOR
            SecondPassItm.bSlightGeneration = P.bSlightGeneration;
#endif

#ifdef USBT_EXPERIMENTAL_UNGENERATED_ZONES
            if (P.TerrainLodMask > 0) {
                //SecondPassItm.GenerationLOD = USBT_VD_UNGENERATED_LOD;
            }
#endif
            SecondPassList.Add(SecondPassItm);
        }

        NewVdArray[Idx] = NewVd;
        Idx++;
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("GenerateVd Pass1 -> %f ms"), Time);

    if (SecondPassList.Num() > 0) {
        BatchGenerateComplexVd(SecondPassList);
    }

    OnBatchGenerationFinished();
}

void UTerrainGeneratorComponent::OnBatchGenerationFinished() {

}

//uint32 TDefaultTerrainGenerator::GetChunkDataMemSize() {
//    return ChunkDataCollection.size();
//}

void UTerrainGeneratorComponent::Clean() {
    ChunkDataCollection.clear();
}

void UTerrainGeneratorComponent::Clean(TVoxelIndex& Index) {

}

//======================================================================================================================================================================
// Foliage
//======================================================================================================================================================================


void UTerrainGeneratorComponent::GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    GenerateNewFoliage(Index, ZoneInstanceMeshMap);
    GenerateNewFoliageCustom(Index, Vd, ZoneInstanceMeshMap);
}

void UTerrainGeneratorComponent::GenerateNewFoliage(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    if (GetController()->FoliageMap.Num() == 0) {
        return;
    }

    FVector ZonePos = GetController()->GetZonePos(Index);

    float GroundLevel = GroundLevelFunction(Index, ZonePos); // TODO fix with zone on ground
    if (GroundLevel > ZonePos.Z + 500) {
        return;
    }

    int32 Hash = ZoneHash(ZonePos);
    FRandomStream rnd = FRandomStream();
    rnd.Initialize(Hash);
    rnd.Reset();

    static const float s = USBT_ZONE_SIZE / 2;
    static const float step = 25.f;

    for (auto x = -s; x <= s; x += step) {
        for (auto y = -s; y <= s; y += step) {

            FVector v(ZonePos);
            v += FVector(x, y, 0);

            for (auto& Elem : GetController()->FoliageMap) {
                FSandboxFoliage FoliageType = Elem.Value;

                if (FoliageType.Type == ESandboxFoliageType::Cave || FoliageType.Type == ESandboxFoliageType::Custom) {
                    continue;
                }

                int32 FoliageTypeId = Elem.Key;

                if ((int)x % (int)FoliageType.SpawnStep == 0 && (int)y % (int)FoliageType.SpawnStep == 0) {
                    float Chance = rnd.FRandRange(0.f, 1.f);

                    FSandboxFoliage FoliageType2 = FoliageExt(FoliageTypeId, FoliageType, Index, v);

                    float Probability = FoliageType2.Probability;

                    if (Chance <= Probability) {
                        float r = std::sqrt(v.X * v.X + v.Y * v.Y);
                        SpawnFoliage(FoliageTypeId, FoliageType2, v, rnd, Index, ZoneInstanceMeshMap);
                    }
                }
            }
        }
    }
}

void UTerrainGeneratorComponent::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    FVector v = Origin;

    if (FoliageType.OffsetRange > 0) {
        float ox = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) ox = -ox; v.X += ox;
        float oy = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) oy = -oy; v.Y += oy;
    }

    bool bSpawnAccurate = false;
    bool bSpawn = false;
    FVector Location(0);

    if (bSpawnAccurate) {
        const FVector start_trace(v.X, v.Y, v.Z + USBT_ZONE_SIZE / 2);
        const FVector end_trace(v.X, v.Y, v.Z - USBT_ZONE_SIZE / 2);
        FHitResult hit(ForceInit);
        GetController()->GetWorld()->LineTraceSingleByChannel(hit, start_trace, end_trace, ECC_Visibility);

        bSpawn = hit.bBlockingHit && Cast<UVoxelMeshComponent>(hit.Component.Get()); //Cast<ASandboxTerrainController>(hit.Actor.Get())
        if (bSpawn) {
            Location = hit.ImpactPoint;
        }
    } else {
        bSpawn = true;
        float GroundLevel = GroundLevelFunction(Index, FVector(v.X, v.Y, 0)) - 5.5;
        Location = FVector(v.X, v.Y, GroundLevel);
    }

    if (bSpawn) {
        float Angle = rnd.FRandRange(0.f, 360.f);
        float ScaleZ = rnd.FRandRange(FoliageType.ScaleMinZ, FoliageType.ScaleMaxZ);
        FVector Scale = FVector(1, 1, ScaleZ);
        if (OnCheckFoliageSpawn(Index, Location, Scale)) {
            FTransform Transform(FRotator(0, Angle, 0), Location, Scale);
            FTerrainInstancedMeshType MeshType;
            MeshType.MeshTypeId = FoliageTypeId;
            MeshType.Mesh = FoliageType.Mesh;
            MeshType.StartCullDistance = FoliageType.StartCullDistance;
            MeshType.EndCullDistance = FoliageType.EndCullDistance;

            auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(FoliageTypeId);
            InstanceMeshContainer.MeshType = MeshType;
            InstanceMeshContainer.TransformArray.Add(Transform);
            //Zone->SpawnInstancedMesh(MeshType, Transform);
        }
    }
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
                FTerrainInstancedMeshType MeshType;
                MeshType.MeshTypeId = FoliageTypeId;
                MeshType.Mesh = FoliageType.Mesh;
                MeshType.StartCullDistance = FoliageType.StartCullDistance;
                MeshType.EndCullDistance = FoliageType.EndCullDistance;

                auto& InstanceMeshContainer = ZoneInstanceMeshMap.FindOrAdd(FoliageTypeId);
                InstanceMeshContainer.MeshType = MeshType;
                InstanceMeshContainer.TransformArray.Add(Transform);
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

bool UTerrainGeneratorComponent::ForcePerformZone(const TVoxelIndex& ZoneIndex) {
    return false;
}