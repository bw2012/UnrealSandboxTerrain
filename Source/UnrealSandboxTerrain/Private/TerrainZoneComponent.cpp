// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"

#include "DrawDebugHelpers.h"

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

	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::generateMesh -------------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);

	return md_ptr;
}

void UTerrainZoneComponent::applyTerrainMesh(std::shared_ptr<MeshData> mesh_data_ptr) {
	double start = FPlatformTime::Seconds();

	MeshData* mesh_data = mesh_data_ptr.get();

	if (mesh_data == NULL) {
		return;
	}

	//##########################################
	// draw debug points
	//##########################################
	/*
	MeshLodSection& section6 = mesh_data->MeshSectionLodArray[4];
	for (auto p : section6.DebugPointList) {
		DrawDebugPoint(GetWorld(), p, 5, FColor(0, 0, 255, 100), false, 1000000);
		UE_LOG(LogTemp, Warning, TEXT("DebugPointList ---> %f %f %f "), p.X, p.Y, p.Z);
	}
	*/
	//##########################################

	MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	MainTerrainMesh->bLodFlag = GetTerrainController()->bEnableLOD;

	MainTerrainMesh->SetMeshData(mesh_data_ptr);

	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);

	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetMaterial(0, GetTerrainController()->TerrainMaterial);
	MainTerrainMesh->SetVisibility(true);

	CollisionMesh->SetMeshData(mesh_data_ptr);
	CollisionMesh->SetCollisionProfileName(TEXT("BlockAll"));

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);

	if (voxel_data->isNew()) {
		voxel_data->bIsNew = false;
		GetTerrainController()->OnGenerateNewZone(this);
	}
}

void UTerrainZoneComponent::SaveInstancedMeshesToFile() {
	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->getZoneIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/zone_inst_mesh.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".dat");

	FBufferArchive BinaryData;

	SerializeInstancedMeshes(BinaryData);

	if (FFileHelper::SaveArrayToFile(BinaryData, *FileName)) {
		BinaryData.FlushCache();
		BinaryData.Empty();
	}

}

void UTerrainZoneComponent::SerializeInstancedMeshes(FBufferArchive& BinaryData) {

	int32 MeshCount = 1;

	int32 MeshId = 0;
	int32 MeshInstanceCount = InstancedStaticMeshComponent->GetInstanceCount();

	BinaryData << MeshCount;

	BinaryData << MeshId;
	BinaryData << MeshInstanceCount;

	for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
		FTransform InstanceTransform;
		InstancedStaticMeshComponent->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

		float X = InstanceTransform.GetLocation().X;
		float Y = InstanceTransform.GetLocation().Y;
		float Z = InstanceTransform.GetLocation().Z;

		float RotationX = InstanceTransform.GetRotation().X;
		float RotationY = InstanceTransform.GetRotation().Y;
		float RotationZ = InstanceTransform.GetRotation().Z;

		float ScaleX = InstanceTransform.GetScale3D().X;
		float ScaleY = InstanceTransform.GetScale3D().Y;
		float ScaleZ = InstanceTransform.GetScale3D().Z;

		BinaryData << X;
		BinaryData << Y;
		BinaryData << Z;

		BinaryData << RotationX;
		BinaryData << RotationY;
		BinaryData << RotationZ;

		BinaryData << ScaleX;
		BinaryData << ScaleY;
		BinaryData << ScaleZ;
	}
}