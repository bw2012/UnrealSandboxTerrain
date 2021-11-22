// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"
#include "SandboxVoxeldata.h"
#include "VoxelIndex.h"
#include "serialization.hpp"

#include "DrawDebugHelpers.h"

TValueDataPtr SerializeMeshData(TMeshDataPtr MeshDataPtr);

UTerrainZoneComponent::UTerrainZoneComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	PrimaryComponentTick.bCanEverTick = false;
    CurrentTerrainLodMask = 0xff;
	bIsObjectsNeedSave = false;
}

void UTerrainZoneComponent::ApplyTerrainMesh(TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask) {
    const std::lock_guard<std::mutex> lock(TerrainMeshMutex);
	double start = FPlatformTime::Seconds();
	TMeshData* MeshData = MeshDataPtr.get();

	if (GetTerrainController()->bShowApplyZone) {
		DrawDebugBox(GetWorld(), GetComponentLocation(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), false, 5);
	}

	if (MeshData == nullptr) {
		return;
	}

	if (MeshDataTimeStamp > MeshDataPtr->TimeStamp) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainZone::applyTerrainMesh skip late thread -> %f"), MeshDataPtr->TimeStamp);
	}

	MeshDataTimeStamp = MeshDataPtr->TimeStamp;

	// do not reduce lod mask
    TTerrainLodMask TargetTerrainLodMask = 0;
	if(TerrainLodMask < CurrentTerrainLodMask){
        TargetTerrainLodMask = TerrainLodMask;
        CurrentTerrainLodMask = TerrainLodMask;
    } else {
        TargetTerrainLodMask = CurrentTerrainLodMask;
    }

	//MainTerrainMesh->SetMobility(EComponentMobility::Movable);
	//MainTerrainMesh->AddLocalRotation(FRotator(0.0f, 0.01, 0.0f));  // workaround
	//MainTerrainMesh->AddLocalRotation(FRotator(0.0f, -0.01, 0.0f)); // workaround

	//MainTerrainMesh->bLodFlag = GetTerrainController()->bEnableLOD;
	MainTerrainMesh->SetMeshData(MeshDataPtr, TargetTerrainLodMask);

	//MainTerrainMesh->SetMobility(EComponentMobility::Stationary);

    MainTerrainMesh->bCastShadowAsTwoSided = true;    
	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->SetVisibility(true);

	MainTerrainMesh->SetCollisionMeshData(MeshDataPtr);
	MainTerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);
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

TValueDataPtr UTerrainZoneComponent::SerializeAndResetObjectData(){
    const std::lock_guard<std::mutex> lock(InstancedMeshMutex);
    TValueDataPtr Data = SerializeInstancedMeshes();
    bIsObjectsNeedSave = false;
    return Data;
}


TValueDataPtr UTerrainZoneComponent::SerializeInstancedMesh(const TInstanceMeshTypeMap& InstanceObjectMap) {
	usbt::TFastUnsafeSerializer Serializer;
	int32 MeshCount = InstanceObjectMap.Num();
	Serializer << MeshCount;

	for (auto& Elem : InstanceObjectMap) {
		const TInstanceMeshArray& InstancedObjectArray = Elem.Value;
		int32 MeshTypeId = Elem.Key;
		int32 MeshInstanceCount = InstancedObjectArray.TransformArray.Num();

		Serializer << MeshTypeId << MeshInstanceCount;

		for (int32 InstanceIdx = 0; InstanceIdx < MeshInstanceCount; InstanceIdx++) {
			TInstantMeshData P;
			const FTransform& InstanceTransform = InstancedObjectArray.TransformArray[InstanceIdx];

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

std::shared_ptr<std::vector<uint8>> UTerrainZoneComponent::SerializeInstancedMeshes() {
	usbt::TFastUnsafeSerializer Serializer;
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
			const FVector LocalPos = InstanceTransform.GetLocation() - GetComponentLocation();

			P.X = LocalPos.X;
			P.Y = LocalPos.Y;
			P.Z = LocalPos.Z;
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

void UTerrainZoneComponent::DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstanceMeshTypeMap& ZoneInstMeshMap) {
	usbt::TFastUnsafeDeserializer Deserializer(Data.data());

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

		TInstanceMeshArray& InstMeshArray = ZoneInstMeshMap.FindOrAdd(MeshTypeId);
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

void UTerrainZoneComponent::SpawnAll(const TInstanceMeshTypeMap& InstanceMeshMap) {
	for (const auto& Elem : InstanceMeshMap) {
		const TInstanceMeshArray& InstMeshTransArray = Elem.Value;
		SpawnInstancedMesh(InstMeshTransArray.MeshType, InstMeshTransArray);
	}
}

void UTerrainZoneComponent::SpawnInstancedMesh(const FTerrainInstancedMeshType& MeshType, const TInstanceMeshArray& InstMeshTransArray) {
    const std::lock_guard<std::mutex> lock(InstancedMeshMutex);
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

	InstancedStaticMeshComponent->AddInstances(InstMeshTransArray.TransformArray, false);
}
