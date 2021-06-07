
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainGenerator.h"
#include "perlin.hpp"


TBaseTerrainGenerator::TBaseTerrainGenerator(ASandboxTerrainController* Controller) {
	this->Controller = Controller;
	this->Pn = new TPerlinNoise();
}

TBaseTerrainGenerator::~TBaseTerrainGenerator() {
	delete this->Pn;
}

ASandboxTerrainController* TBaseTerrainGenerator::GetController() const {
    return this->Controller;
}

int32 TBaseTerrainGenerator::ZoneHash(const FVector& ZonePos) {
	int32 Hash = 7;
	Hash = Hash * 31 + (int32)ZonePos.X;
	Hash = Hash * 31 + (int32)ZonePos.Y;
	Hash = Hash * 31 + (int32)ZonePos.Z;
	return Hash;
}


float TBaseTerrainGenerator::PerlinNoise(const float X, const float Y, const float Z) const {
	if (Pn) {
		return Pn->noise(X, Y, Z);
	}

	return 0;
};

float TBaseTerrainGenerator::PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
	if (Pn) {
		return Pn->noise(Pos.X * PositionScale, Pos.Y * PositionScale, Pos.Z * PositionScale) * ValueScale;
	}

	return 0;
}

float TBaseTerrainGenerator::PerlinNoise(const FVector& Pos) const {
    if (Pn) {
        return Pn->noise(Pos.X, Pos.Y, Pos.Z);
    }

    return 0;
}


//======================================================================================================================================================================
// 
//======================================================================================================================================================================

//#define USBT_VGEN_GRADIENT_THRESHOLD        400
//#define USBT_VD_MIN_DENSITY                 0.003    // minimal density = 1f/255
#define USBT_VGEN_GROUND_LEVEL_OFFSET       205.f


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
        }
        else {
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



void TDefaultTerrainGenerator::OnBeginPlay() {
    UndergroundLayersTmp.Empty();

    if (this->Controller->TerrainParameters && this->Controller->TerrainParameters->UndergroundLayers.Num() > 0) {
        this->UndergroundLayersTmp = this->Controller->TerrainParameters->UndergroundLayers;
    }
    else {
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


//======================================================================================================================================================================
// 
//======================================================================================================================================================================

int TDefaultTerrainGenerator::GetMaterialLayersCount(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) const {
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

const FTerrainUndergroundLayer* TDefaultTerrainGenerator::GetMaterialLayer(float Z, float RealGroundLevel)  const {
    for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
        const FTerrainUndergroundLayer& Layer = UndergroundLayersTmp[Idx];
        if (Z <= RealGroundLevel - Layer.StartDepth && Z > RealGroundLevel - UndergroundLayersTmp[Idx + 1].StartDepth) {
            return &Layer;
        }
    }
    return nullptr;
}

FORCEINLINE TMaterialId TDefaultTerrainGenerator::MaterialFuncion(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel)  const {
    const float DeltaZ = WorldPos.Z - GroundLevel;

    TMaterialId mat = 0;
    if (DeltaZ >= -70) {
        mat = 2; // grass
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

FORCEINLINE float TDefaultTerrainGenerator::GroundLevelFunction(const TVoxelIndex& Index, const FVector& V) const {
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


FORCEINLINE float TDefaultTerrainGenerator::DensityFunctionExt(float Density, const TVoxelIndex& ZoneIndex, const FVector& WorldPos, const FVector& LocalPos) const {
    return Density;
}

FORCEINLINE float TDefaultTerrainGenerator::ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const {
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


TChunkHeightMapData* TDefaultTerrainGenerator::GetChunkHeightMap(int X, int Y) {
    const std::lock_guard<std::mutex> lock(ZoneHeightMapMutex);

    TVoxelIndex Index(X, Y, 0);
    TChunkHeightMapData* ChunkHeightMapData = nullptr;

    if (ChunkDataCollection.find(Index) == ChunkDataCollection.end()) {
        ChunkHeightMapData = new TChunkHeightMapData(USBT_ZONE_DIMENSION);
        ChunkDataCollection.insert({ Index, ChunkHeightMapData });

        double Start = FPlatformTime::Seconds();

        for (int X = 0; X < USBT_ZONE_DIMENSION; X++) {
            for (int Y = 0; Y < USBT_ZONE_DIMENSION; Y++) {

                const float step = USBT_ZONE_SIZE / (USBT_ZONE_DIMENSION - 1);
                const float s = -USBT_ZONE_SIZE / 2;
                FVector v(s, s, s);
                FVector a(X * step, Y * step, 0);
                v += a;

                const FVector& LocalPos = v;
                FVector WorldPos = LocalPos + Controller->GetZonePos(Index);
                float GroundLevel = GroundLevelFunction(Index, WorldPos);
                ChunkHeightMapData->SetHeightLevel(X, Y, GroundLevel);
            }
        }

        double End = FPlatformTime::Seconds();
        double Time = (End - Start) * 1000;
        //UE_LOG(LogTemp, Warning, TEXT("generate height map  ----> %f ms --  %d %d"), Time, X, Y);
    } else {
        ChunkHeightMapData = ChunkDataCollection[Index];
    }

    return ChunkHeightMapData;
};

bool TDefaultTerrainGenerator::IsZoneOverGroundLevel(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin) const {
    static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
    return ChunkHeightMapData->GetMaxHeightLevel() < ZoneOrigin.Z - ZoneHalfSize;
}

FORCEINLINE bool TDefaultTerrainGenerator::IsZoneOnGroundLevel(TChunkHeightMapData* ChunkHeightMapData, const FVector& ZoneOrigin) const {
    static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
    float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize + 500;
    float ZoneLow = ZoneOrigin.Z - ZoneHalfSize - 10;
    float TerrainHigh = ChunkHeightMapData->GetMaxHeightLevel();
    float TerrainLow = ChunkHeightMapData->GetMinHeightLevel();
    return std::max(ZoneLow, TerrainLow) <= std::min(ZoneHigh, TerrainHigh);
}
//======================================================================================================================================================================
// Generator
//======================================================================================================================================================================

void TDefaultTerrainGenerator::GenerateZoneVolume(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, const TChunkHeightMapData* ChunkHeightMapData) const {
    int zc = 0;
    int fc = 0;

    double Start = FPlatformTime::Seconds();

    bool bContainsMoreOneMaterial = false;
    unsigned char BaseMaterialId = 0;

    VoxelData->initCache();

    for (int X = 0; X < USBT_ZONE_DIMENSION; X++) {
        for (int Y = 0; Y < USBT_ZONE_DIMENSION; Y++) {
            for (int Z = 0; Z < USBT_ZONE_DIMENSION; Z++) {
                const FVector& LocalPos = VoxelData->voxelIndexToVector(X, Y, Z);
                const TVoxelIndex& Index = TVoxelIndex(X, Y, Z);

                FVector WorldPos = LocalPos + VoxelData->getOrigin();
                float GroundLevel = ChunkHeightMapData->GetHeightLevel(Index.X, Index.Y);

                float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
                Density = DensityFunctionExt(Density, ZoneIndex, WorldPos, LocalPos);
                unsigned char MaterialId = MaterialFuncion(LocalPos, WorldPos, GroundLevel);

                VoxelData->setDensity(Index.X, Index.Y, Index.Z, Density);
                VoxelData->setMaterial(Index.X, Index.Y, Index.Z, MaterialId);

                VoxelData->performSubstanceCacheLOD(Index.X, Index.Y, Index.Z);

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

                BaseMaterialId = MaterialId;
            }
        }
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogTemp, Warning, TEXT("TDefaultTerrainGenerator::GenerateZoneVolume -> %f ms - %d %d %d"), Time, ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

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
}

void TDefaultTerrainGenerator::BatchGenerateComplexVd(TArray<TGenerateVdTempItm>& GenPass2List) {
    double Start1 = FPlatformTime::Seconds();

    for (const auto& Pass2Itm : GenPass2List) {
        GenerateZoneVolume(Pass2Itm.ZoneIndex, Pass2Itm.Vd, Pass2Itm.ChunkData);
    }

    double End1 = FPlatformTime::Seconds();
    double Time1 = (End1 - Start1) * 1000;
    //UE_LOG(LogTemp, Warning, TEXT("GenerateVd Pass2 -> %f ms"), Time1);
}

int TDefaultTerrainGenerator::GenerateSimpleVd(const TVoxelIndex& ZoneIndex, TVoxelData* VoxelData, TChunkHeightMapData** ChunkDataPtr) {
    FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
    // get heightmap data
    TChunkHeightMapData* ChunkHeightMapData = GetChunkHeightMap(ZoneIndex.X, ZoneIndex.Y);
    *ChunkDataPtr = ChunkHeightMapData;

    static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
    const FVector Origin = VoxelData->getOrigin();

    bool bForcePerformZone = ForcePerformZone(ZoneIndex);
    if (bForcePerformZone) {
        //GenerateZoneVolume(ZoneIndex, VoxelData, ChunkHeightMapData);
        return 2;
    } else {
        if (!IsZoneOverGroundLevel(ChunkHeightMapData, Origin)) {
            TArray<FTerrainUndergroundLayer> LayerList;
            int LayersCount = GetMaterialLayersCount(ChunkHeightMapData, Origin, &LayerList);
            bool bIsZoneOnGround = IsZoneOnGroundLevel(ChunkHeightMapData, Origin);
            if (LayersCount == 1 && !bIsZoneOnGround) {
                // only one material
                VoxelData->deinitializeDensity(TVoxelDataFillState::FULL);
                VoxelData->deinitializeMaterial(LayerList[0].MatId);
                return 1;
            } else {
                //GenerateZoneVolume(ZoneIndex, VoxelData, ChunkHeightMapData);
                return 2;
            }
        } else {
            // air only
            VoxelData->deinitializeDensity(TVoxelDataFillState::ZERO);
            VoxelData->deinitializeMaterial(0);
            return 0;
        }
    }
}

void TDefaultTerrainGenerator::BatchGenerateVoxelTerrain(const TArray<TSpawnZoneParam>& BatchList, TArray<TVoxelData*>& NewVdArray) {
    NewVdArray.Empty();
    NewVdArray.SetNumZeroed(BatchList.Num());

    TArray<TGenerateVdTempItm> GenPass2List;

    double Start = FPlatformTime::Seconds();

    int Idx = 0;
    for (const auto& P : BatchList) {
        const FVector Pos = GetController()->GetZonePos(P.Index);
        TVoxelData* NewVd = GetController()->NewVoxelData();
        NewVd->setOrigin(Pos);

        TChunkHeightMapData* ChunkDataPtr = nullptr;
        const int Res = GenerateSimpleVd(P.Index, NewVd, &ChunkDataPtr);

        if (Res == 2) {
            TGenerateVdTempItm Pass2Itm;
            Pass2Itm.Idx = Idx;
            Pass2Itm.ZoneIndex = P.Index;
            Pass2Itm.Vd = NewVd;
            Pass2Itm.ChunkData = ChunkDataPtr;
            GenPass2List.Add(Pass2Itm);
        } else {
            NewVd->setCacheToValid();
        }

        NewVdArray[Idx] = NewVd;
        Idx++;
    }

    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogTemp, Warning, TEXT("GenerateVd Pass1 -> %f ms"), Time);

    if (GenPass2List.Num() > 0) {
        BatchGenerateComplexVd(GenPass2List);
    }

    OnBatchGenerationFinished();
}

void TDefaultTerrainGenerator::OnBatchGenerationFinished() {

}

//uint32 TDefaultTerrainGenerator::GetChunkDataMemSize() {
//    return ChunkDataCollection.size();
//}

void TDefaultTerrainGenerator::Clean() {
    ChunkDataCollection.clear();
}

void TDefaultTerrainGenerator::Clean(TVoxelIndex& Index) {

}

//======================================================================================================================================================================
// Foliage
//======================================================================================================================================================================


void TDefaultTerrainGenerator::GenerateInstanceObjects(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    GenerateNewFoliage(Index, ZoneInstanceMeshMap);
    GenerateNewFoliageCustom(Index, Vd, ZoneInstanceMeshMap);
}

void TDefaultTerrainGenerator::GenerateNewFoliage(const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    if (Controller->FoliageMap.Num() == 0) {
        return;
    }

    FVector ZonePos = Controller->GetZonePos(Index);

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

            for (auto& Elem : Controller->FoliageMap) {
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

void TDefaultTerrainGenerator::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, const FVector& Origin, FRandomStream& rnd, const TVoxelIndex& Index, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
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
        Controller->GetWorld()->LineTraceSingleByChannel(hit, start_trace, end_trace, ECC_Visibility);

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

void TDefaultTerrainGenerator::GenerateNewFoliageCustom(const TVoxelIndex& Index, TVoxelData* Vd, TInstanceMeshTypeMap& ZoneInstanceMeshMap) {
    if (Controller->FoliageMap.Num() == 0) {
        return;
    }

    if (!UseCustomFoliage(Index)) {
        return;
    }

    FVector ZonePos = Controller->GetZonePos(Index);
    int32 Hash = ZoneHash(ZonePos);
    FRandomStream rnd = FRandomStream();
    rnd.Initialize(Hash);
    rnd.Reset();


    for (auto& Elem : Controller->FoliageMap) {
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