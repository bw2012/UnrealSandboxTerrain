// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"
#include "SandboxVoxeldata.h"
#include "VoxelIndex.h"
#include "serialization.hpp"

#include "DrawDebugHelpers.h"

UTerrainZoneComponent::UTerrainZoneComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	PrimaryComponentTick.bCanEverTick = false;
}


TMeshData const * UTerrainZoneComponent::GetCachedMeshData() {
	return (TMeshData const *) CachedMeshDataPtr.get();
}

void UTerrainZoneComponent::ClearCachedMeshData() {
	CachedMeshDataPtr = nullptr;
}

void UTerrainZoneComponent::ApplyTerrainMesh(TMeshDataPtr MeshDataPtr, bool bPutToCache) {
	double start = FPlatformTime::Seconds();

	TMeshData* MeshData = MeshDataPtr.get();

	if (MeshData == nullptr) {
		return;
	}

	if (CachedMeshDataPtr != nullptr && CachedMeshDataPtr->TimeStamp > MeshDataPtr->TimeStamp) {
		UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh skip late thread-> %f"), MeshDataPtr->TimeStamp);
	}

	if (bPutToCache) {
		CachedMeshDataPtr = MeshDataPtr;
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

	//MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	
	//MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	//MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	MainTerrainMesh->bLodFlag = GetTerrainController()->bEnableLOD;

	MainTerrainMesh->SetMeshData(MeshDataPtr);

	//MainTerrainMesh->SetMobility(EComponentMobility::Stationary);

	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetVisibility(true);

	MainTerrainMesh->SetCollisionMeshData(MeshDataPtr);
	MainTerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);
}

typedef struct TInstantMeshData {
	float X;
	float Y;
	float Z;

	float Roll;
	float Pitch;
	float Yaw;

	float ScaleX;
	float ScaleY;
	float ScaleZ;
} TInstantMeshData;


std::shared_ptr<std::vector<uint8>> UTerrainZoneComponent::SerializeInstancedMeshes() {
	FastUnsafeSerializer Serializer;
	int32 MeshCount = InstancedMeshMap.Num();
	Serializer << MeshCount;

	for (auto& Elem : InstancedMeshMap) {
		UHierarchicalInstancedStaticMeshComponent* InstancedStaticMeshComponent = Elem.Value;
		int32 MeshTypeId = Elem.Key;
		int32 MeshInstanceCount = InstancedStaticMeshComponent->GetInstanceCount();

		Serializer << MeshTypeId << MeshInstanceCount;

		for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
			TInstantMeshData P;
			FTransform InstanceTransform;
			InstancedStaticMeshComponent->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

			P.X = InstanceTransform.GetLocation().X;
			P.Y = InstanceTransform.GetLocation().Y;
			P.Z = InstanceTransform.GetLocation().Z;
			P.Roll = InstanceTransform.Rotator().Roll;
			P.Pitch = InstanceTransform.Rotator().Pitch;
			P.Yaw = InstanceTransform.Rotator().Yaw;
			P.ScaleX = InstanceTransform.GetScale3D().X;
			P.ScaleY = InstanceTransform.GetScale3D().Y;
			P.ScaleZ = InstanceTransform.GetScale3D().Z;

			Serializer << P;
		}
	}

	return Serializer.data();
}

void UTerrainZoneComponent::DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstMeshTypeMap& ZoneInstMeshMap) {
	FastUnsafeDeserializer Deserializer(Data.data());

	int32 MeshCount;
	Deserializer >> MeshCount;

	for (int Idx = 0; Idx < MeshCount; Idx++) {
		int32 MeshTypeId;
		int32 MeshInstanceCount;

		Deserializer >> MeshTypeId;
		Deserializer >> MeshInstanceCount;

		FTerrainInstancedMeshType MeshType;
		if (GetTerrainController()->FoliageMap.Contains(MeshTypeId)) {
			FSandboxFoliage FoliageType = GetTerrainController()->FoliageMap[MeshTypeId];

			MeshType.Mesh = FoliageType.Mesh;
			MeshType.MeshTypeId = MeshTypeId;
			MeshType.StartCullDistance = FoliageType.StartCullDistance;
			MeshType.EndCullDistance = FoliageType.EndCullDistance;
		}

		TInstMeshTransArray& InstMeshArray = ZoneInstMeshMap.FindOrAdd(MeshTypeId);
		InstMeshArray.MeshType = MeshType;
		InstMeshArray.TransformArray.Reserve(MeshInstanceCount);

		for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
			TInstantMeshData P;
			Deserializer >> P;
			FTransform Transform(FRotator(P.Pitch, P.Yaw, P.Roll), FVector(P.X, P.Y, P.Z), FVector(P.ScaleX, P.ScaleY, P.ScaleZ));

			if (MeshType.Mesh != nullptr) {
				InstMeshArray.TransformArray.Add(Transform);
			}
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
		InstancedStaticMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
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