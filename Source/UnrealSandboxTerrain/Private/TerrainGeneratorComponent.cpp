// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainGeneratorComponent.h"
#include "SandboxVoxeldata.h"
#include "SandboxPerlinNoise.h"
#include "SandboxTerrainController.h"
#include "TerrainZoneComponent.h"
#include <algorithm>


#define USBT_VGEN_GRADIENT_THRESHOLD	400
#define USBT_VD_MIN_DENSITY				0.003	// minimal density = 1f/255
#define USBT_VGEN_GROUND_LEVEL_OFFSET	205.f	


TZoneHeightMapData::TZoneHeightMapData(int Size){
    this->Size = Size;
    HeightLevelArray = new float[Size * Size * Size];
}

TZoneHeightMapData::~TZoneHeightMapData(){
    delete[] HeightLevelArray;
}

FORCEINLINE void TZoneHeightMapData::SetHeightLevel(TVoxelIndex VoxelIndex, float HeightLevel){
    if(VoxelIndex.X < Size && VoxelIndex.Y < Size && VoxelIndex.Z < Size){
        int Index = VoxelIndex.X * Size * Size + VoxelIndex.Y * Size + VoxelIndex.Z;
        HeightLevelArray[Index] = HeightLevel;
        
        if(HeightLevel > this->MaxHeightLevel){
            this->MaxHeightLevel = HeightLevel;
        }
        
        if(HeightLevel < this->MinHeightLevel){
            this->MinHeightLevel = HeightLevel;
        }
    }
}

FORCEINLINE float TZoneHeightMapData::GetHeightLevel(TVoxelIndex VoxelIndex) const {
    if(VoxelIndex.X < Size && VoxelIndex.Y < Size && VoxelIndex.Z < Size){
        int Index = VoxelIndex.X * Size * Size + VoxelIndex.Y * Size + VoxelIndex.Z;
        return HeightLevelArray[Index];
    } else {
        return 0;
    }
}



usand::PerlinNoise Pn;

UTerrainGeneratorComponent::UTerrainGeneratorComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	FTerrainUndergroundLayer DefaultLayer;
	DefaultLayer.MatId = 1;
	DefaultLayer.StartDepth = 0;
	DefaultLayer.Name = TEXT("Dirt");

	UndergroundLayers.Add(DefaultLayer);
}

void UTerrainGeneratorComponent::BeginPlay() {
	FTerrainUndergroundLayer LastLayer;
	LastLayer.MatId = 0;
	LastLayer.StartDepth = 9999999.f;
	LastLayer.Name = TEXT("");

	this->UndergroundLayersTmp = this->UndergroundLayers;
	UndergroundLayersTmp.Add(LastLayer);
}

void UTerrainGeneratorComponent::BeginDestroy() {
	Super::BeginDestroy();

	for (std::unordered_map<TVoxelIndex, TZoneHeightMapData*>::iterator It = ZoneHeightMapCollection.begin(); It != ZoneHeightMapCollection.end(); ++It) {
		delete It->second;
	}

	ZoneHeightMapCollection.clear();
}

void UTerrainGeneratorComponent::GenerateZoneVolume(TVoxelData &VoxelData, const TZoneHeightMapData* ZoneHeightMapData) {
	TSet<unsigned char> material_list;
	int zc = 0; int fc = 0;

	bool bIsBounds = !MaxTerrainBounds.IsZero();

	for (int x = 0; x < VoxelData.num(); x++) {
		for (int y = 0; y < VoxelData.num(); y++) {
			for (int z = 0; z < VoxelData.num(); z++) {
				FVector LocalPos = VoxelData.voxelIndexToVector(x, y, z);
				FVector WorldPos = LocalPos + VoxelData.getOrigin();

				float GroundLevel = ZoneHeightMapData->GetHeightLevel(TVoxelIndex(x, y, 0));

				float Density = ClcDensityByGroundLevel(WorldPos, GroundLevel);
				//float den = DensityFunc(ZoneIndex, LocalPos, WorldPos);

				unsigned char MaterialId = MaterialFunc(LocalPos, WorldPos, GroundLevel);

				if (bIsBounds){
					if (WorldPos.X > MaxTerrainBounds.X || WorldPos.X < -MaxTerrainBounds.X){
						Density = 0;
					}

					if (WorldPos.Y > MaxTerrainBounds.Y || WorldPos.Y < -MaxTerrainBounds.Y) {
						Density = 0;
					}
				}

				VoxelData.setDensity(x, y, z, Density);
				VoxelData.setMaterial(x, y, z, MaterialId);

				VoxelData.performSubstanceCacheLOD(x, y, z);

				if (Density == 0) zc++;
				if (Density == 1) fc++;
				material_list.Add(MaterialId);
			}
		}
	}

	int s = VoxelData.num() * VoxelData.num() * VoxelData.num();

	if (zc == s) {
		VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
	}

	if (fc == s) {
		VoxelData.deinitializeDensity(TVoxelDataFillState::FULL);
	}

	if (material_list.Num() == 1) {
		unsigned char base_mat = 0;
		for (auto m : material_list) {
			base_mat = m;
			break;
		}
		VoxelData.deinitializeMaterial(base_mat);
	}

}

void UTerrainGeneratorComponent::GenerateVoxelTerrain(TVoxelData &VoxelData) {
	double start = FPlatformTime::Seconds();

	TVoxelIndex ZoneIndex = GetTerrainController()->GetZoneIndex(VoxelData.getOrigin());
	FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

	// get heightmap data
	//======================================
	TVoxelIndex Index2((int)ZoneIndexTmp.X, (int)ZoneIndexTmp.Y, 0);
	TZoneHeightMapData* ZoneHeightMapData = nullptr;

	if (ZoneHeightMapCollection.find(Index2) == ZoneHeightMapCollection.end()) {
		ZoneHeightMapData = new TZoneHeightMapData(VoxelData.num()); 
		ZoneHeightMapCollection.insert({ Index2, ZoneHeightMapData });

		for (int X = 0; X < VoxelData.num(); X++) {
			for (int Y = 0; Y < VoxelData.num(); Y++) {
				FVector LocalPos = VoxelData.voxelIndexToVector(X, Y, 0);
				FVector WorldPos = LocalPos + VoxelData.getOrigin();

				float GroundLevel = GroundLevelFunc(WorldPos);
				ZoneHeightMapData->SetHeightLevel(TVoxelIndex(X, Y, 0), GroundLevel);
			}
		}

	} else {
		ZoneHeightMapData = ZoneHeightMapCollection[Index2];
	}

	//======================================

	static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
	const FVector Origin = VoxelData.getOrigin();
	if (!IsZoneOverGroundLevel(ZoneHeightMapData, Origin)) {
		TArray<FTerrainUndergroundLayer> LayerList;
		int LayersCount = GetAllUndergroundMaterialLayers(ZoneHeightMapData, Origin, &LayerList);
		bool bIsZoneOnGround = IsZoneOnGroundLevel(ZoneHeightMapData, Origin);
		if (LayersCount == 1 && !bIsZoneOnGround) {
			// only one material
			VoxelData.deinitializeDensity(TVoxelDataFillState::FULL);
			VoxelData.deinitializeMaterial(LayerList[0].MatId);
		} else {
			GenerateZoneVolume(VoxelData, ZoneHeightMapData);
		}
	} else {
		// air only
		VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
		VoxelData.deinitializeMaterial(0);
	}
		
	VoxelData.setCacheToValid();

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::generateTerrain ----> %f %f %f --> %f ms"), VoxelData.getOrigin().X, VoxelData.getOrigin().Y, VoxelData.getOrigin().Z, time);
}

bool UTerrainGeneratorComponent::IsZoneOverGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin) {
	static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
	return ZoneHeightMapData->GetMaxHeightLevel() < ZoneOrigin.Z - ZoneHalfSize;
}

bool UTerrainGeneratorComponent::IsZoneOnGroundLevel(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin) {
	static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
	float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize;
	float ZoneLow = ZoneOrigin.Z - ZoneHalfSize;
	float TerrainHigh = ZoneHeightMapData->GetMaxHeightLevel();
	float TerrainLow = ZoneHeightMapData->GetMinHeightLevel();

	return std::max(ZoneLow, TerrainLow) < std::min(ZoneHigh, TerrainHigh);
}

float UTerrainGeneratorComponent::GroundLevelFunc(FVector v) {
	//float scale1 = 0.0035f; // small
	float scale1 = 0.0015f; // small
	float scale2 = 0.0004f; // medium
	float scale3 = 0.00009f; // big

	float noise_small = Pn.noise(v.X * scale1, v.Y * scale1, 0);
	float noise_medium = Pn.noise(v.X * scale2, v.Y * scale2, 0) * 5;
	float noise_big = Pn.noise(v.X * scale3, v.Y * scale3, 0) * 15;
	float gl = noise_medium + noise_small + noise_big;
	return (gl * 100) + USBT_VGEN_GROUND_LEVEL_OFFSET;
}

float UTerrainGeneratorComponent::ClcDensityByGroundLevel(const FVector& V, const float GroundLevel) const {
	float val = 1;
	float gl = GroundLevel - USBT_VGEN_GROUND_LEVEL_OFFSET;

	if (V.Z > gl + USBT_VGEN_GRADIENT_THRESHOLD) {
		return 0;
	} else if (V.Z > gl) {
		float d = (1 / (V.Z - gl)) * 100;
		val = d;
	}

	if (val > 1) {
		return 1;
	}

	if (val < USBT_VD_MIN_DENSITY) { // minimal density = 1f/255
		return 0;
	}

	return val;
}

float UTerrainGeneratorComponent::DensityFunc(const FVector& ZoneIndex, const FVector& LocalPos, const FVector& WorldPos) {
	//float den = ClcDensityByGroundLevel(WorldPos);

	// ==============================================================
	// cavern
	// ==============================================================
	//if (this->cavern) {
	//	den = funcGenerateCavern(den, local);
	//}
	// ==============================================================

	return 0;
}

FTerrainUndergroundLayer* UTerrainGeneratorComponent::GetUndergroundMaterialLayer(const float Z, float RealGroundLevel) {
	for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
		FTerrainUndergroundLayer& Layer = UndergroundLayersTmp[Idx];
		if (Z <= RealGroundLevel - Layer.StartDepth && Z > RealGroundLevel - UndergroundLayersTmp[Idx + 1].StartDepth) {
			return &Layer;
		}
	}
	return nullptr;
}

int UTerrainGeneratorComponent::GetAllUndergroundMaterialLayers(TZoneHeightMapData* ZoneHeightMapData, const FVector& ZoneOrigin, TArray<FTerrainUndergroundLayer>* LayerList) {
	static const float ZoneHalfSize = USBT_ZONE_SIZE / 2;
	float ZoneHigh = ZoneOrigin.Z + ZoneHalfSize;
	float ZoneLow = ZoneOrigin.Z - ZoneHalfSize;
	float TerrainHigh = ZoneHeightMapData->GetMaxHeightLevel();
	float TerrainLow = ZoneHeightMapData->GetMinHeightLevel();

	int Cnt = 0;
	for (int Idx = 0; Idx < UndergroundLayersTmp.Num() - 1; Idx++) {
		FTerrainUndergroundLayer& Layer1 = UndergroundLayersTmp[Idx];
		FTerrainUndergroundLayer& Layer2 = UndergroundLayersTmp[Idx + 1];

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

unsigned char UTerrainGeneratorComponent::MaterialFunc(const FVector& LocalPos, const FVector& WorldPos, float GroundLevel) {
	FVector test = FVector(WorldPos);
	test.Z += 30;

	float densityUpper = ClcDensityByGroundLevel(test, GroundLevel);

	unsigned char mat = 0;

	if (densityUpper < 0.5) {
		mat = 2; // grass
	} else {
		FTerrainUndergroundLayer* Layer = GetUndergroundMaterialLayer(WorldPos.Z, GroundLevel);
		if (Layer != nullptr) {
			mat = Layer->MatId;
		}
	}

	return mat;
}


//======================================================================================================================================================================
// Foliage
//======================================================================================================================================================================

void UTerrainGeneratorComponent::GenerateNewFoliage(UTerrainZoneComponent* Zone) {
	if (GetTerrainController()->FoliageMap.Num() == 0) return;
	if (GroundLevelFunc(Zone->GetComponentLocation()) > Zone->GetComponentLocation().Z + 500) return;

	int32 Hash = 7;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().X;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().Y;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().Z;

	FRandomStream rnd = FRandomStream();
	rnd.Initialize(Hash);
	rnd.Reset();

	static const float s = USBT_ZONE_SIZE / 2;
	static const float step = 25.f;

	for (auto x = -s; x <= s; x += step) {
		for (auto y = -s; y <= s; y += step) {

			FVector v(Zone->GetComponentLocation());
			v += FVector(x, y, 0);

			for (auto& Elem : GetTerrainController()->FoliageMap) {
				FSandboxFoliage FoliageType = Elem.Value;
				int32 FoliageTypeId = Elem.Key;

				if ((int)x % (int)FoliageType.SpawnStep == 0 && (int)y % (int)FoliageType.SpawnStep == 0) {
					//UE_LOG(LogTemp, Warning, TEXT("%d - %d"), (int)x, (int)y);
					float Chance = rnd.FRandRange(0.f, 1.f);
					if (Chance <= FoliageType.Probability) {
						float r = std::sqrt(v.X * v.X + v.Y * v.Y);
						SpawnFoliage(FoliageTypeId, FoliageType, v, rnd, Zone);
					}
				}
			}


		}
	}

	Zone->SetNeedSave();
}

void UTerrainGeneratorComponent::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone) {

	if (FoliageType.OffsetRange > 0) {
		float ox = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) ox = -ox; v.X += ox;
		float oy = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) oy = -oy; v.Y += oy;
	}

	const FVector start_trace(v.X, v.Y, v.Z + USBT_ZONE_SIZE / 2);
	const FVector end_trace(v.X, v.Y, v.Z - USBT_ZONE_SIZE / 2);

	FHitResult hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(hit, start_trace, end_trace, ECC_WorldStatic);

	if (hit.bBlockingHit) {
		if (Cast<ASandboxTerrainController>(hit.Actor.Get()) != NULL) {
			if (Cast<USandboxTerrainMeshComponent>(hit.Component.Get()) != NULL) {

				float angle = rnd.FRandRange(0.f, 360.f);
				float ScaleZ = rnd.FRandRange(FoliageType.ScaleMinZ, FoliageType.ScaleMaxZ);
				FTransform Transform(FRotator(0, angle, 0), hit.ImpactPoint, FVector(1, 1, ScaleZ));

				FTerrainInstancedMeshType MeshType;
				MeshType.MeshTypeId = FoliageTypeId;
				MeshType.Mesh = FoliageType.Mesh;
				MeshType.StartCullDistance = FoliageType.StartCullDistance;
				MeshType.EndCullDistance = FoliageType.EndCullDistance;

				Zone->SpawnInstancedMesh(MeshType, Transform);
			}
		}
	}
}