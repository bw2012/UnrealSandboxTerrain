// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainZoneComponent.h"
#include "TerrainRegionComponent.h"
#include "SandboxTerrainController.h"
#include "SandboxVoxeldata.h"
#include "VoxelIndex.h"

#include "DrawDebugHelpers.h"

UTerrainZoneComponent::UTerrainZoneComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	voxel_data = nullptr;
}


void UTerrainZoneComponent::MakeTerrain() {
	if (voxel_data == NULL) {
		voxel_data = GetTerrainController()->GetVoxelDataByPos(GetComponentLocation());
	}

	if (voxel_data == NULL) {
		return;
	}

	std::shared_ptr<TMeshData> md_ptr = GenerateMesh();

	if (IsInGameThread()) {
		ApplyTerrainMesh(md_ptr);
		voxel_data->resetLastMeshRegenerationTime();
	} else {
		UE_LOG(LogTemp, Warning, TEXT("non-game thread -> invoke async task"));
		if (GetTerrainController() != NULL) {
			GetTerrainController()->InvokeZoneMeshAsync(this, md_ptr);
		}
	}
}

std::shared_ptr<TMeshData> UTerrainZoneComponent::GenerateMesh() {
	double start = FPlatformTime::Seconds();

	if (voxel_data == NULL || 
		voxel_data->getDensityFillState() == TVoxelDataFillState::ZERO || 
		voxel_data->getDensityFillState() == TVoxelDataFillState::ALL) {
		return NULL;
	}

	TVoxelDataParam vdp;

	if (GetTerrainController()->bEnableLOD) {
		vdp.bGenerateLOD = true;
		vdp.collisionLOD = GetTerrainController()->GetCollisionMeshSectionLodIndex();
	} else {
		vdp.bGenerateLOD = false;
		vdp.collisionLOD = 0;
	}

	TMeshDataPtr md_ptr = sandboxVoxelGenerateMesh(*voxel_data, vdp);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;

	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::generateMesh -------------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);

	return md_ptr;
}

void UTerrainZoneComponent::ApplyTerrainMesh(TMeshDataPtr MeshDataPtr, bool bPutToCache) {
	double start = FPlatformTime::Seconds();

	TMeshData* MeshData = MeshDataPtr.get();

	if (MeshData == nullptr) {
		return;
	}

	UTerrainRegionComponent* Region = GetRegion();
	if (Region == nullptr) {
		return;
	}

	if (bPutToCache) {
		TVoxelIndex Index = GetTerrainController()->GetZoneIndex2(GetComponentLocation());
		FVector ZoneIndexTmp(Index.X, Index.Y, Index.Z);
		Region->PutMeshDataToCache(ZoneIndexTmp, MeshDataPtr);
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

	//##########################################
	// mat section test
	//##########################################
	/*
	TMeshLodSection& section0 = MeshData->MeshSectionLodArray[0];
	TMaterialSectionMap matSectionMap = section0.MaterialSectionMap;

	for (auto& Elem : matSectionMap) {
		short matId = Elem.Key;
		TMeshMaterialSection matSection = Elem.Value;

		UE_LOG(LogTemp, Warning, TEXT("material section -> %d - %d -> %d "), matId, matSection.MaterialId, matSection.MaterialMesh.ProcVertexBuffer.Num());
	}	
	*/
	/*
	TMaterialTransitionSectionMap& matTraSectionMap = section0.MaterialTransitionSectionMap;
	for (auto& Elem : matTraSectionMap) {
		short matId = Elem.Key;
		TMeshMaterialTransitionSection& matSection = Elem.Value;

		UE_LOG(LogTemp, Warning, TEXT("material transition section -> %d - [%s] -> %d "), matId, *matSection.TransitionName, matSection.MaterialMesh.ProcVertexBuffer.Num());

		for (auto p : matSection.MaterialMesh.ProcVertexBuffer) {
			//DrawDebugPoint(GetWorld(), p.Position, 3, FColor(255, 255, 255, 100), false, 1000000);
		}
	}
	*/
	//##########################################

	MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	MainTerrainMesh->bLodFlag = GetTerrainController()->bEnableLOD;

	MainTerrainMesh->SetMeshData(MeshDataPtr);

	MainTerrainMesh->SetMobility(EComponentMobility::Stationary);

	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetVisibility(true);

	CollisionMesh->SetMeshData(MeshDataPtr);
	CollisionMesh->SetCollisionProfileName(TEXT("BlockAll"));

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);
}

void UTerrainZoneComponent::SerializeInstancedMeshes(FBufferArchive& BinaryData) {
	int32 MeshCount = InstancedMeshMap.Num();
	BinaryData << MeshCount;

	for (auto& Elem : InstancedMeshMap) {
		UHierarchicalInstancedStaticMeshComponent* InstancedStaticMeshComponent = Elem.Value;
		int32 MeshTypeId = Elem.Key;

		int32 MeshInstanceCount = InstancedStaticMeshComponent->GetInstanceCount();

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
}

void UTerrainZoneComponent::SpawnInstancedMesh(FTerrainInstancedMeshType& MeshType, FTransform& Transform) {
	UHierarchicalInstancedStaticMeshComponent* InstancedStaticMeshComponent = nullptr;

	if (InstancedMeshMap.Contains(MeshType.MeshTypeId)) {
		InstancedStaticMeshComponent = InstancedMeshMap[MeshType.MeshTypeId];
	}

	if (InstancedStaticMeshComponent == nullptr) {
		FString InstancedStaticMeshCompName = FString::Printf(TEXT("InstancedStaticMesh - %d -> [%.0f, %.0f, %.0f]"), MeshType.MeshTypeId, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z);

		InstancedStaticMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*InstancedStaticMeshCompName));

		InstancedStaticMeshComponent->RegisterComponent();
		InstancedStaticMeshComponent->AttachTo(this);
		InstancedStaticMeshComponent->SetStaticMesh(MeshType.Mesh);
		InstancedStaticMeshComponent->SetCullDistances(MeshType.StartCullDistance, MeshType.EndCullDistance);
		InstancedStaticMeshComponent->SetMobility(EComponentMobility::Static);
		InstancedStaticMeshComponent->SetSimulatePhysics(false);

		//InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InstancedStaticMeshComponent->SetCollisionProfileName(TEXT("OverlapAll"));

		InstancedMeshMap.Add(MeshType.MeshTypeId, InstancedStaticMeshComponent);
	}

	InstancedStaticMeshComponent->AddInstanceWorldSpace(Transform);
}