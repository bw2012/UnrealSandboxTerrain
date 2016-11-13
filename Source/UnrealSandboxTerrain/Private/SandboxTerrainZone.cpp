#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainZone.h"
#include "SandboxVoxeldata.h"
#include "SandboxVoxelGenerator.h"

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

	//const static ConstructorHelpers::FObjectFinder<UMaterialInterface> material(TEXT("Material'/Game/Test/M_Triplanar_Terrain.M_Triplanar_Terrain'")); \

	MainTerrainMesh = CreateDefaultSubobject<USandboxTerrainMeshComponent>(TEXT("TerrainZoneProceduralMainMesh"));
	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);
	//MainTerrainMesh->SetMaterial(0, material.Object);
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

	if (GetWorld()->GetAuthGameMode() == NULL) {
		return;
	}

	if (voxel_data == NULL) {
		return;
	}

	if (!voxel_data->isChanged()) {
		return; // skip save if not changed
	}

	// save voxel data
	FVector index = controller->getZoneIndex(GetActorLocation());
	FString fileName = controller->getZoneFileName(index.X, index.Y, index.Z);

	UE_LOG(LogTemp, Warning, TEXT("save voxeldata -> %f %f %f"), index.X, index.Y, index.Z);
	sandboxSaveVoxelData(*voxel_data, fileName);

	//TODO replace with share pointer
	delete voxel_data;
	voxel_data = NULL;
}


void ASandboxTerrainZone::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}


//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

void ASandboxTerrainZone::makeTerrain() {
	if (voxel_data == NULL) {
		voxel_data = sandboxGetTerrainVoxelDataByPos(GetActorLocation());
	}

	if (voxel_data == NULL) {
		return;
	}

	std::shared_ptr<MeshData> md_ptr = generateMesh(*voxel_data);

	if (IsInGameThread()) {
		applyTerrainMesh(md_ptr);
		voxel_data->resetLastMeshRegenerationTime();
	} else {
		UE_LOG(LogTemp, Warning, TEXT("non-game thread -> invoke async task"));
		if (controller != NULL) {
			controller->invokeZoneMeshAsync(this, md_ptr);
		}
	}
}

std::shared_ptr<MeshData> ASandboxTerrainZone::generateMesh(VoxelData &voxel_data) {
	double start = FPlatformTime::Seconds();

	if (voxel_data.getDensityFillState() == VoxelDataFillState::ZERO || voxel_data.getDensityFillState() == VoxelDataFillState::ALL) {
		return NULL;
	}


	VoxelDataParam vdp;
	vdp.dim = 0;

	MeshDataPtr md_ptr = sandboxVoxelGenerateMesh(voxel_data, vdp);

	/*
	if (bZCut) {
		vdp.z_cut = true;
		vdp.z_cut_level = ZCutLevel;
		sandboxVoxelGenerateMesh(*mesh_data_element2, voxel_data, vdp);
		mesh_data->slice_mesh = mesh_data_element2;
	}
	*/

	//int tc = (*mesh_data).triangle_count;
	//int vc = (*mesh_data).vertex_count;


	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;

	//UE_LOG(LogTemp, Warning, TEXT("Terrain mesh generated ------------------------> %d triangles %d vertexes <- volume %d"), mesh_data_element->MeshSection.ProcIndexBuffer.Num(), mesh_data_element->MeshSection.ProcVertexBuffer.Num(), voxel_data.num());
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::generateMesh -------------> %f %f %f --> %f ms"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, time);

	return md_ptr;
}

void ASandboxTerrainZone::applyTerrainMesh(std::shared_ptr<MeshData> mesh_data_ptr) {
	double start = FPlatformTime::Seconds();

	MeshData* mesh_data = mesh_data_ptr.get();

	if (mesh_data == NULL) {
		return;
	}

	FProcMeshSection& MeshSection = mesh_data->main_mesh.MeshSectionLOD[0];

	if (MeshSection.ProcVertexBuffer.Num() == 0) {
		return;
	}

	const int section = 0;

	MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	//MainTerrainMesh->CreateMeshSection(section, mesh_data->main_mesh->verts, mesh_data->main_mesh->tris, mesh_data->main_mesh->normals, mesh_data->main_mesh->uv, mesh_data->main_mesh->colors, TArray<FProcMeshTangent>(), true);
	MeshSection.bEnableCollision = true;
	MeshSection.bSectionVisible = true;
	MainTerrainMesh->SetProcMeshSection(section, MeshSection);

	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);
	MainTerrainMesh->SetVisibility(false);
	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetMaterial(0, controller->TerrainMaterial);

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



	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, time);

	//UE_LOG(LogTemp, Warning, TEXT("Terrain mesh added: %f ms"), time2, mesh_data->triangle_count, mesh_data->vertex_count);
}

