// Copyright blackw 2015-2020

#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"
#include "VoxelIndex.h"
#include "serialization.hpp"
#include "DrawDebugHelpers.h"
#include "Core/memstat.h"


TValueDataPtr SerializeMeshData(TMeshDataPtr MeshDataPtr);

UTerrainInstancedStaticMesh::UTerrainInstancedStaticMesh(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	//zone_counter++;
}

UTerrainZoneComponent::UTerrainZoneComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	PrimaryComponentTick.bCanEverTick = false;
	//zone_counter++;
}

void UTerrainZoneComponent::FinishDestroy() {
	Super::FinishDestroy();
}

void UTerrainZoneComponent::DestroyComponent(bool bPromoteChildren) {
	Super::DestroyComponent(bPromoteChildren);
	zone_counter--;
}

void UTerrainZoneComponent::ApplyTerrainMesh(TMeshDataPtr MeshDataPtr, bool bIgnoreCollision) {
    const std::lock_guard<std::mutex> lock(TerrainMeshMutex);
	double start = FPlatformTime::Seconds();
	TMeshData* MeshData = MeshDataPtr.get();

	if (MeshData == nullptr) {
		return;
	}
	
	if (MeshDataTimeStamp > MeshDataPtr->TimeStamp) {
		//UE_LOG(LogVt, Warning, TEXT("Zone: %f %f %f - trying to apply stale mesh -> new mesh = %f < existing mesh = %f"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, MeshDataPtr->TimeStamp, VStamp);
		//return;
	}

	if (VStamp > MeshDataPtr->VStamp) {
		UE_LOG(LogVt, Warning, TEXT("Zone: %f %f %f - trying to apply stale mesh -> new mesh = %d < existing mesh = %d"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, MeshDataPtr->VStamp, VStamp);
		return;
	}

	MeshDataTimeStamp = MeshDataPtr->TimeStamp;
	VStamp = MeshDataPtr->VStamp;

	for (auto TTT : MeshDataPtr->MeshSectionLodArray) {
		for (auto P : TTT.DebugPointList) {
			DrawDebugPoint(GetWorld(), P, 5.f, FColor(255, 255, 255, 0), true);
		}
	}
		
	MainTerrainMesh->SetMeshData(MeshDataPtr);

	if (!bIgnoreCollision) {
		MainTerrainMesh->SetCollisionMeshData(MeshDataPtr);
	}

    MainTerrainMesh->bCastShadowAsTwoSided = true;    
	MainTerrainMesh->SetCastShadow(true);
	MainTerrainMesh->bCastHiddenShadow = true;
	MainTerrainMesh->bAffectDistanceFieldLighting = false;
	MainTerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogVt, Log, TEXT("ASandboxTerrainZone::applyTerrainMesh ---------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);
}

TValueDataPtr UTerrainZoneComponent::SerializeAndResetObjectData(){
    const std::lock_guard<std::mutex> lock(InstancedMeshMutex);
    TValueDataPtr Data = SerializeInstancedMeshes();
    return Data;
}

TValueDataPtr UTerrainZoneComponent::SerializeInstancedMesh(const TInstanceMeshTypeMap& InstanceObjectMap) {
	usbt::TFastUnsafeSerializer Serializer;
	int32 MeshCount = InstanceObjectMap.Num();
	Serializer << MeshCount;

	for (auto& Elem : InstanceObjectMap) {
		const TInstanceMeshArray& InstancedObjectArray = Elem.Value;
		union {
			uint32 A[2];
			uint64 B;
		};

		B = Elem.Key;
		uint32 MeshTypeId = A[0];
		uint32 MeshVariantId = A[1];
		int32 MeshInstanceCount = InstancedObjectArray.TransformArray.Num();

		Serializer << MeshTypeId << MeshVariantId << MeshInstanceCount;

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
		UTerrainInstancedStaticMesh* InstancedStaticMeshComponent = Elem.Value;

		union {
			uint32 A[2];
			uint64 B;
		};

		B = Elem.Key;
		uint32 MeshTypeId = A[0];
		uint32 MeshVariantId = A[1];
		int32 MeshInstanceCount = InstancedStaticMeshComponent->GetInstanceCount();

		Serializer << MeshTypeId << MeshVariantId << MeshInstanceCount;

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

void UTerrainZoneComponent::SpawnAll(const TInstanceMeshTypeMap& InstanceMeshMap) {
	const std::lock_guard<std::mutex> lock(InstancedMeshMutex);
	for (const auto& Elem : InstanceMeshMap) {
		const TInstanceMeshArray& InstMeshTransArray = Elem.Value;
		SpawnInstancedMesh(InstMeshTransArray.MeshType, InstMeshTransArray);
	}
}

void SetCollisionTree(UTerrainInstancedStaticMesh* InstancedStaticMeshComponent) {
	InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void SetCollisionGrass(UTerrainInstancedStaticMesh* InstancedStaticMeshComponent) {
	
	InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	//InstancedStaticMeshComponent->SetCollisionProfileName(TEXT("OverlapAll"));
	
	/*
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Vehicle, ECollisionResponse::ECR_Ignore);
	InstancedStaticMeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Destructible, ECollisionResponse::ECR_Ignore);

	InstancedStaticMeshComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	InstancedStaticMeshComponent->SetGenerateOverlapEvents(false);
	*/

	InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // workaround bad UE5 performance
	InstancedStaticMeshComponent->SetCastShadow(false);
}

void UTerrainZoneComponent::SpawnInstancedMesh(const FTerrainInstancedMeshType& MeshType, const TInstanceMeshArray& InstMeshTransArray) {
	UTerrainInstancedStaticMesh* InstancedStaticMeshComponent = nullptr;

	uint64 MeshCode = MeshType.GetMeshTypeCode();
	if (InstancedMeshMap.Contains(MeshCode)) {
		InstancedStaticMeshComponent = InstancedMeshMap[MeshCode];
	}

	bool bIsFoliage = GetTerrainController()->FoliageMap.Contains(MeshType.MeshTypeId);
	if (InstancedStaticMeshComponent == nullptr) {
		//UE_LOG(LogVt, Log, TEXT("SpawnInstancedMesh -> %d %d"), MeshType.MeshVariantId, MeshType.MeshTypeId);
		FString InstancedStaticMeshCompName = FString::Printf(TEXT("InstancedStaticMesh - [%d, %d]-> [%.0f, %.0f, %.0f]"), MeshType.MeshTypeId, MeshType.MeshVariantId, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z);

		InstancedStaticMeshComponent = NewObject<UTerrainInstancedStaticMesh>(this, FName(*InstancedStaticMeshCompName));
		InstancedStaticMeshComponent->bIsFoliage = bIsFoliage;
		InstancedStaticMeshComponent->RegisterComponent();
		InstancedStaticMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
		InstancedStaticMeshComponent->SetStaticMesh(MeshType.Mesh);
		InstancedStaticMeshComponent->SetIsReplicated(true);

		int32 StartCullDistance = MeshType.StartCullDistance;
		int32 EndCullDistance = MeshType.EndCullDistance;
		if (GetWorld()->WorldType == EWorldType::PIE || GetWorld()->WorldType == EWorldType::Editor) {
			//StartCullDistance /= 10;
			//EndCullDistance /= 10;
		}

		InstancedStaticMeshComponent->SetCullDistances(StartCullDistance, EndCullDistance);
		InstancedStaticMeshComponent->SetMobility(EComponentMobility::Movable);
		InstancedStaticMeshComponent->SetSimulatePhysics(false);

		if (bIsFoliage) {
			auto FoliageType = GetTerrainController()->FoliageMap[MeshType.MeshTypeId];
			if (FoliageType.Type == ESandboxFoliageType::Tree) {
				SetCollisionTree(InstancedStaticMeshComponent);
			} else {
				SetCollisionGrass(InstancedStaticMeshComponent);
			}
		} else {
			InstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		InstancedStaticMeshComponent->bNavigationRelevant = false;
		InstancedStaticMeshComponent->MeshTypeId = MeshType.MeshTypeId;
		InstancedStaticMeshComponent->MeshVariantId = MeshType.MeshVariantId;

		InstancedMeshMap.Add(MeshCode, InstancedStaticMeshComponent);
	}

	InstancedStaticMeshComponent->AddInstances(InstMeshTransArray.TransformArray, false);
	//UE_LOG(LogVt, Warning, TEXT("AddInstances -> %d"), InstMeshTransArray.TransformArray.Num());
}

ASandboxTerrainController* UTerrainZoneComponent::GetTerrainController() {
	return (ASandboxTerrainController*)GetAttachmentRootActor();
};

bool UTerrainInstancedStaticMesh::IsFoliage() {
	return bIsFoliage;
}