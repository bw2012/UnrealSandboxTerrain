// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainGeneratorComponent.h"
#include "SandboxVoxeldata.h"
#include "SandboxPerlinNoise.h"
#include "SandboxTerrainController.h"


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

	for (int x = 0; x < VoxelData.num(); x++) {
		for (int y = 0; y < VoxelData.num(); y++) {
			for (int z = 0; z < VoxelData.num(); z++) {
				FVector LocalPos = VoxelData.voxelIndexToVector(x, y, z);
				FVector WorldPos = LocalPos + VoxelData.getOrigin();

				float GroundLevel = ZoneHeightMapData->GetHeightLevel(TVoxelIndex(x, y, 0));

				float den = ClcDensityByGroundLevel(WorldPos, GroundLevel);
				//float den = DensityFunc(ZoneIndex, LocalPos, WorldPos);

				unsigned char mat = MaterialFunc(LocalPos, WorldPos);

				VoxelData.setDensity(x, y, z, den);
				VoxelData.setMaterial(x, y, z, mat);

				VoxelData.performSubstanceCacheLOD(x, y, z);

				if (den == 0) zc++;
				if (den == 1) fc++;
				material_list.Add(mat);
			}
		}
	}

	int s = VoxelData.num() * VoxelData.num() * VoxelData.num();

	if (zc == s) {
		VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
	}

	if (fc == s) {
		VoxelData.deinitializeDensity(TVoxelDataFillState::ALL);
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

	FVector ZoneIndex = GetTerrainController()->GetZoneIndex(VoxelData.getOrigin());

	// get heightmap data
	//======================================
	TVoxelIndex Index2((int)ZoneIndex.X, (int)ZoneIndex.Y, 0);
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

	if (!(ZoneHeightMapData->GetMaxHeightLevel() < VoxelData.getOrigin().Z - 500.f)) {
		GenerateZoneVolume(VoxelData, ZoneHeightMapData);
	} else {
		VoxelData.deinitializeDensity(TVoxelDataFillState::ZERO);
		VoxelData.deinitializeMaterial(0);
	}
		
	VoxelData.setCacheToValid();

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::generateTerrain ----> %f %f %f --> %f ms"), VoxelData.getOrigin().X, VoxelData.getOrigin().Y, VoxelData.getOrigin().Z, time);
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

	gl = gl * 100;

	return gl;
}

float UTerrainGeneratorComponent::ClcDensityByGroundLevel(const FVector& v, const float gl) const {
	float val = 1;

	if (v.Z > gl + 400) {
		val = 0;
	}
	else if (v.Z > gl) {
		float d = (1 / (v.Z - gl)) * 100;
		val = d;
	}

	if (val > 1) {
		val = 1;
	}

	if (val < 0.003) { // minimal density = 1f/255
		val = 0;
	}

	return val;
}

float UTerrainGeneratorComponent::ClcDensityByGroundLevel(FVector v) {
	float gl = GroundLevelFunc(v);
	float val = 1;

	if (v.Z > gl + 400) {
		val = 0;
	}
	else if (v.Z > gl) {
		float d = (1 / (v.Z - gl)) * 100;
		val = d;
	}

	if (val > 1) {
		val = 1;
	}

	if (val < 0.003) { // minimal density = 1f/255
		val = 0;
	}

	return val;
}

float UTerrainGeneratorComponent::DensityFunc(const FVector& ZoneIndex, const FVector& LocalPos, const FVector& WorldPos) {
	float den = ClcDensityByGroundLevel(WorldPos);

	// ==============================================================
	// cavern
	// ==============================================================
	//if (this->cavern) {
	//	den = funcGenerateCavern(den, local);
	//}
	// ==============================================================

	return den;
}

unsigned char UTerrainGeneratorComponent::MaterialFunc(const FVector& LocalPos, const FVector& WorldPos) {
	FVector test = FVector(WorldPos);
	test.Z += 30;

	float densityUpper = ClcDensityByGroundLevel(test);

	unsigned char mat = 0;

	if (densityUpper < 0.5) {
		mat = 2; // grass
	} else {
		if (WorldPos.Z < -1100) {
			mat = 99; // obsidian
		} else {
			if (WorldPos.Z < -350) {
				mat = 4; // basalt
			} else {
				mat = 1; // dirt
			}
		}
	}


	return mat;
}
