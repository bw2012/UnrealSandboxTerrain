// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"

UTerrainZoneComponent::UTerrainZoneComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}


void UTerrainZoneComponent::makeTerrain() {
	if (voxel_data == NULL) {
		voxel_data = GetTerrainController()->GetTerrainVoxelDataByPos(GetComponentLocation());
	}

	if (voxel_data == NULL) {
		return;
	}

	std::shared_ptr<MeshData> md_ptr = generateMesh();

	if (IsInGameThread()) {
		applyTerrainMesh(md_ptr);
		voxel_data->resetLastMeshRegenerationTime();
	} else {
		UE_LOG(LogTemp, Warning, TEXT("non-game thread -> invoke async task"));
		if (GetTerrainController() != NULL) {
			GetTerrainController()->invokeZoneMeshAsync(this, md_ptr);
		}
	}
}

std::shared_ptr<MeshData> UTerrainZoneComponent::generateMesh() {
	double start = FPlatformTime::Seconds();

	if (voxel_data->getDensityFillState() == VoxelDataFillState::ZERO || voxel_data->getDensityFillState() == VoxelDataFillState::ALL) {
		return NULL;
	}


	bool enableLOD = GetTerrainController()->bEnableLOD;

	VoxelDataParam vdp;

	if (enableLOD) {
		vdp.bGenerateLOD = true;
		vdp.collisionLOD = 1;
	} else {
		vdp.bGenerateLOD = false;
		vdp.collisionLOD = 0;
	}

	MeshDataPtr md_ptr = sandboxVoxelGenerateMesh(*voxel_data, vdp);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;

	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::generateMesh -------------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);

	return md_ptr;
}

void UTerrainZoneComponent::applyTerrainMesh(std::shared_ptr<MeshData> mesh_data_ptr) {
	double start = FPlatformTime::Seconds();

	MeshData* mesh_data = mesh_data_ptr.get();

	if (mesh_data == NULL) {
		return;
	}

	MeshDataSection& MeshDataSection = mesh_data->MeshDataSectionLOD[0];
	FProcMeshSection& MeshSection = MeshDataSection.MainMesh;

	if (MeshSection.ProcVertexBuffer.Num() == 0) {
		//return;
	}

	const int section = 0;

	MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	MainTerrainMesh->SetMeshData(mesh_data_ptr);

	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);
	MainTerrainMesh->SetVisibility(false);
	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetMaterial(0, GetTerrainController()->TerrainMaterial);

	CollisionMesh->SetMeshData(mesh_data_ptr);


	MainTerrainMesh->SetVisibility(true);
	MainTerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));



	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);
}