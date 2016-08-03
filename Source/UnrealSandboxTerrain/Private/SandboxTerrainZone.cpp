#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainZone.h"
#include "SandboxVoxeldata.h"
#include "SandboxPerlinNoise.h"

ASandboxTerrainZone::ASandboxTerrainZone(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	init();
}

ASandboxTerrainZone::ASandboxTerrainZone() {
	init();
}

void ASandboxTerrainZone::init() {
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	const static ConstructorHelpers::FObjectFinder<UMaterialInterface> material(TEXT("Material'/Game/Test/M_Triplanar_Terrain.M_Triplanar_Terrain'")); \

	MainTerrainMesh = CreateDefaultSubobject<USandboxTerrainMeshComponent>(TEXT("TerrainZoneProceduralMainMesh"));
	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);
	MainTerrainMesh->SetMaterial(0, material.Object);
	MainTerrainMesh->SetCanEverAffectNavigation(true);
	MainTerrainMesh->SetCollisionProfileName(TEXT("InvisibleWall"));
	SetRootComponent(MainTerrainMesh);

	/*
	SliceTerrainMesh = CreateDefaultSubobject<USandboxTerrainSliceMeshComponent>(TEXT("TerrainZoneProceduralSliceMesh"));
	SliceTerrainMesh->SetMobility(EComponentMobility::Stationary);
	SliceTerrainMesh->SetMaterial(0, material.Object);
	SliceTerrainMesh->SetCanEverAffectNavigation(false);

	SliceTerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SliceTerrainMesh->SetCollisionProfileName(TEXT("UI"));
	*/

	SetRootComponent(MainTerrainMesh);

	//SliceTerrainMesh->AttachTo(MainTerrainMesh);
	//AddOwnedComponent(SliceTerrainMesh);

	SetActorEnableCollision(true);

	//isLoaded = false;
	voxel_data = NULL;
}

void ASandboxTerrainZone::BeginPlay() {
	Super::BeginPlay();

	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone ---> BeginPlay"));
}

void ASandboxTerrainZone::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void ASandboxTerrainZone::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}


//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

void sandboxTerrainGenerate(VoxelData &voxel_data);

bool ASandboxTerrainZone::fillZone() {
	//	if (GetWorld()->GetAuthGameMode() == NULL) {
	//		return;
	//	}

	bool isNew = false;

	VoxelData* vd = new VoxelData(50, 100 * 10);
	vd->setOrigin(GetActorLocation());

	FVector o = sandboxSnapToGrid(GetActorLocation(), 1000) / 1000;
	//FString fileName = sandboxZoneBinaryFileName(o.X, o.Y, o.Z);
	//FString fileFullPath = sandboxZoneBinaryFileFullPath(o.X, o.Y, o.Z);

	//if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*fileFullPath)) {
	//	sandboxLoadVoxelData(*vd, fileName);
	//} else {


	double start = FPlatformTime::Seconds();
	sandboxTerrainGenerate(*vd);
	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("Terrain volume is generated: %f %f %f --> %f ms"), o.X, o.Y, o.Z, time);

	//	VoxelDataFillState s = vd->getDensityFillState();
	//  sandboxSaveVoxelData(*vd, fileName);
	//	isNew = true;
	//}

	vd->setChanged();
	vd->resetLastSave();

	sandboxRegisterTerrainVoxelData(vd, o);

	voxel_data = vd;

	return isNew;
}

void ASandboxTerrainZone::makeTerrain() {
	if (voxel_data == NULL) {
		voxel_data = sandboxGetTerrainVoxelDataByPos(GetActorLocation());
	}

	if (voxel_data == NULL) {
		return;
	}

	MeshData* md = generateMesh(*voxel_data);
	applyTerrainMesh(md);
	voxel_data->resetLastMeshRegenerationTime();
}

MeshData* ASandboxTerrainZone::generateMesh(VoxelData &voxel_data) {
	if (voxel_data.getDensityFillState() == VoxelDataFillState::ZERO || voxel_data.getDensityFillState() == VoxelDataFillState::ALL) {
		return NULL;
	}

	double start = FPlatformTime::Seconds();

	MeshData* mesh_data = new MeshData();
	MeshDataElement* mesh_data_element = new MeshDataElement();
	MeshDataElement* mesh_data_element2 = new MeshDataElement();

	VoxelDataParam vdp;
	vdp.lod = 1;

	sandboxVoxelGenerateMesh(*mesh_data_element, voxel_data, vdp);
	mesh_data->main_mesh = mesh_data_element;

	/*
	if (bZCut) {
		vdp.z_cut = true;
		vdp.z_cut_level = ZCutLevel;
		sandboxVoxelGenerateMesh(*mesh_data_element2, voxel_data, vdp);
		mesh_data->slice_mesh = mesh_data_element2;
	}
	*/

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;

	//int tc = (*mesh_data).triangle_count;
	//int vc = (*mesh_data).vertex_count;
	//UE_LOG(LogTemp, Warning, TEXT("Terrain mesh generated: %f ms -> %d triangles %d vertexes <- volume %d"), time, tc, vc, voxel_data.num());

	return mesh_data;
}

void ASandboxTerrainZone::applyTerrainMesh(MeshData* mesh_data) {
	if (mesh_data == NULL) {
		return;
	}

	if (mesh_data->main_mesh->verts.Num() == 0) {
		return;
	}

	const int section = 0;
	double start2 = FPlatformTime::Seconds();

	MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround
	MainTerrainMesh->CreateMeshSection(section, mesh_data->main_mesh->verts, mesh_data->main_mesh->tris, mesh_data->main_mesh->normals, mesh_data->main_mesh->uv, mesh_data->main_mesh->colors, TArray<FProcMeshTangent>(), true);
	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);
	MainTerrainMesh->SetVisibility(false);
	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;

	/*
	if (bZCut && mesh_data->slice_mesh != NULL) {
		SliceTerrainMesh->CreateMeshSection(section, mesh_data->slice_mesh->verts, mesh_data->slice_mesh->tris, mesh_data->slice_mesh->normals, mesh_data->slice_mesh->uv, mesh_data->slice_mesh->colors, TArray<FProcMeshTangent>(), true);
		SliceTerrainMesh->SetVisibility(true);
		SliceTerrainMesh->SetCastShadow(false);
		SliceTerrainMesh->SetCollisionProfileName(TEXT("UI"));

		MainTerrainMesh->SetVisibility(false);
		MainTerrainMesh->SetCollisionProfileName(TEXT("InvisibleWall"));
	}
	else {
		SliceTerrainMesh->SetVisibility(false);
		SliceTerrainMesh->SetCollisionProfileName(TEXT("NoCollision"));
		*/

		MainTerrainMesh->SetVisibility(true);
		MainTerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	/* } */



	double end2 = FPlatformTime::Seconds();
	double time2 = (end2 - start2) * 1000;

	//UE_LOG(LogTemp, Warning, TEXT("Terrain mesh added: %f ms"), time2, mesh_data->triangle_count, mesh_data->vertex_count);
}

// ================================================================================================
// terrain generator
// ================================================================================================

PerlinNoise perlin_noise;

float groundLevel(FVector v) {
	//float scale1 = 0.0035f; // small
	float scale1 = 0.0015f; // small
	float scale2 = 0.0004f; // medium
	float scale3 = 0.00009f; // big

	float noise_small = perlin_noise.noise(v.X * scale1, v.Y * scale1, 0);
	float noise_medium = perlin_noise.noise(v.X * scale2, v.Y * scale2, 0) * 5;
	float noise_big = perlin_noise.noise(v.X * scale3, v.Y * scale3, 0) * 15;
	float gl = noise_medium + noise_small + noise_big;

	gl = gl * 100;

	return gl;
}

float densityByGroundLevel(FVector v) {
	float gl = groundLevel(v);
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


float funcGenerateCavern(float density, FVector v) {
	float den = density;
	float r = FMath::Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
	if (r < 300) {
		float scale1 = 0.01f; // small
		float scale2 = 0.005f; // medium
		float noise_medium = FMath::Abs(perlin_noise.noise(v.X * scale2, v.Y * scale2, v.Z * scale2));
		float noise_small = FMath::Abs(perlin_noise.noise(v.X * scale1, v.Y * scale1, v.Z * scale1));

		//den -= noise_medium;
		den -= noise_small * 0.15 + noise_medium * 0.3 + (1 / r) * 100;
		if (den < 0) {
			den = 0;
		}
	}

	return den;
}

FORCEINLINE unsigned long vectorHash(FVector v) {
	return ((int)v.X * 73856093) ^ ((int)v.Y * 19349663) ^ ((int)v.Z * 83492791);
}

float clcGroundLevelDelta(FVector v) {
	return groundLevel(v) - v.Z;
}

void sandboxTerrainGenerate(VoxelData &voxel_data) {
	double start = FPlatformTime::Seconds();

	int32 zone_seed = vectorHash(voxel_data.getOrigin());

	FRandomStream rnd = FRandomStream();
	rnd.Initialize(zone_seed);
	rnd.Reset();

	bool cavern = false;

	float gl_delta = clcGroundLevelDelta(voxel_data.getOrigin());
	if (rnd.FRandRange(0.f, 1.f) > 0.95 || (voxel_data.getOrigin().X == 0 && voxel_data.getOrigin().Y == 0)) {
		if (gl_delta > 500 && gl_delta < 2000) {
			cavern = true;
		}
	}

	TSet<unsigned char> material_list;
	int zc = 0; int fc = 0;

	for (int x = 0; x < voxel_data.num(); x++) {
		for (int y = 0; y < voxel_data.num(); y++) {
			for (int z = 0; z < voxel_data.num(); z++) {
				FVector v = voxel_data.voxelIndexToVector(x, y, z);
				FVector tmp = v + voxel_data.getOrigin();

				float den = densityByGroundLevel(tmp);

				// ==============================================================
				// cavern
				// ==============================================================
				if (cavern) {
					den = funcGenerateCavern(den, v);
				}
				// ==============================================================


				voxel_data.setDensity(x, y, z, den);

				if (den == 0) zc++;
				if (den == 1) fc++;

				FVector test2 = FVector(tmp);
				test2.Z += 30;

				float den2 = densityByGroundLevel(test2);


				unsigned char mat = 0;
				if (den2 < 0.5) {
					mat = 2;
				}
				else {
					mat = 1;
				}

				voxel_data.setMaterial(x, y, z, mat);
				material_list.Add(mat);

			}
		}
	}

	int s = voxel_data.num() * voxel_data.num() * voxel_data.num();

	if (zc == s) {
		voxel_data.deinitializeDensity(VoxelDataFillState::ZERO);
	}

	if (fc == s) {
		voxel_data.deinitializeDensity(VoxelDataFillState::ALL);
	}

	if (material_list.Num() == 1) {
		unsigned char base_mat = 0;
		for (auto m : material_list) {
			base_mat = m;
			break;
		}
		voxel_data.deinitializeMaterial(base_mat);
	}

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("Terrain volume generated: %f ms"), time);
}

