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
	} else {
		GetTerrainController()->OnLoadZone(this);
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

void UTerrainZoneComponent::LoadInstancedMeshesFromFile() {
	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->getZoneIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/zone_inst_mesh.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".dat");

	TArray<uint8> BinaryArray;
	if (!FFileHelper::LoadFileToArray(BinaryArray, *FileName)) {
		UE_LOG(LogTemp, Warning, TEXT("file not found -> %s"), *FileName);
		return;
	}

	if (BinaryArray.Num() <= 0) return;

	FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
	BinaryData.Seek(0);

	// ==============================
	int32 MeshCount;
	int32 MeshTypeId;
	int32 MeshInstanceCount;

	BinaryData << MeshCount;
	BinaryData << MeshTypeId;
	BinaryData << MeshInstanceCount;

	UE_LOG(LogTemp, Warning, TEXT("MeshCount -> %d"), MeshCount);
	UE_LOG(LogTemp, Warning, TEXT("MeshTypeId -> %d"), MeshTypeId);
	UE_LOG(LogTemp, Warning, TEXT("MeshInstanceCount -> %d"), MeshInstanceCount);

	FTerrainInstancedMeshType MeshType;
	MeshType.Mesh = GetTerrainController()->GetInstancedMesh(0);

	for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
		float X;
		float Y;
		float Z;

		float Roll;
		float Pitch;
		float Yaw;

		float ScaleX;
		float ScaleY;
		float ScaleZ;

		BinaryData << X;
		BinaryData << Y;
		BinaryData << Z;

		BinaryData << Roll;
		BinaryData << Pitch;
		BinaryData << Yaw;

		BinaryData << ScaleX;
		BinaryData << ScaleY;
		BinaryData << ScaleZ;

		FRotator Rotator(Pitch, Yaw, Roll);
		FTransform Transform(Rotator, FVector(X, Y, Z), FVector(ScaleX, ScaleY, ScaleZ));

		SpawnInstancedMesh(MeshType, Transform);
	}

	// ==============================

	BinaryData.FlushCache();
	BinaryArray.Empty();
	BinaryData.Close();
}

void UTerrainZoneComponent::SerializeInstancedMeshes(FBufferArchive& BinaryData) {
	if (InstancedStaticMeshComponent == NULL) {
		return;
	}

	int32 MeshCount = 1;

	int32 MeshTypeId = 0;
	int32 MeshInstanceCount = InstancedStaticMeshComponent->GetInstanceCount();

	BinaryData << MeshCount;

	BinaryData << MeshTypeId;
	BinaryData << MeshInstanceCount;

	for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
		FTransform InstanceTransform;
		InstancedStaticMeshComponent->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

		float X = InstanceTransform.GetLocation().X;
		float Y = InstanceTransform.GetLocation().Y;
		float Z = InstanceTransform.GetLocation().Z;

		float Roll = InstanceTransform.Rotator().Roll;
		float Pitch = InstanceTransform.Rotator().Pitch;
		float Yaw = InstanceTransform.Rotator().Yaw;

		float ScaleX = InstanceTransform.GetScale3D().X;
		float ScaleY = InstanceTransform.GetScale3D().Y;
		float ScaleZ = InstanceTransform.GetScale3D().Z;

		BinaryData << X;
		BinaryData << Y;
		BinaryData << Z;

		BinaryData << Roll;
		BinaryData << Pitch;
		BinaryData << Yaw;

		BinaryData << ScaleX;
		BinaryData << ScaleY;
		BinaryData << ScaleZ;
	}
}

void UTerrainZoneComponent::SpawnInstancedMesh(FTerrainInstancedMeshType& MeshType, FTransform& Transform) {
	if (InstancedStaticMeshComponent == nullptr) {
		FString InstancedStaticMeshCompName = FString::Printf(TEXT("InstancedStaticMesh -> [%.0f, %.0f, %.0f]"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z);

		InstancedStaticMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*InstancedStaticMeshCompName));

		InstancedStaticMeshComponent->RegisterComponent();
		InstancedStaticMeshComponent->AttachTo(this);
		InstancedStaticMeshComponent->SetStaticMesh(MeshType.Mesh);
		InstancedStaticMeshComponent->SetCullDistances(100, 500);
		InstancedStaticMeshComponent->SetMobility(EComponentMobility::Static);
		InstancedStaticMeshComponent->SetSimulatePhysics(false);

		//InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InstancedStaticMeshComponent->SetCollisionProfileName(TEXT("OverlapAll"));
	}

	InstancedStaticMeshComponent->AddInstanceWorldSpace(Transform);
}