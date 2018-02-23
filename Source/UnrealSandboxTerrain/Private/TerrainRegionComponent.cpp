// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainRegionComponent.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"

#include "DrawDebugHelpers.h"

#include <memory>

UTerrainRegionComponent::UTerrainRegionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainRegionComponent::BeginDestroy() {
	Super::BeginDestroy();
}

void SerializeMeshContainer(FBufferArchive& BinaryData, TMeshContainer& MeshContainer) {
	// save regular materials
	int32 LodSectionRegularMatNum = MeshContainer.MaterialSectionMap.Num();
	BinaryData << LodSectionRegularMatNum;
	for (auto& Elem2 : MeshContainer.MaterialSectionMap) {
		unsigned short MatId = Elem2.Key;
		TMeshMaterialSection& MaterialSection = Elem2.Value;

		BinaryData << MatId;

		FProcMeshSection& Mesh = MaterialSection.MaterialMesh;
		Mesh.SerializeMesh(BinaryData);
	}

	// save transition materials
	int32 LodSectionTransitionMatNum = MeshContainer.MaterialTransitionSectionMap.Num();
	BinaryData << LodSectionTransitionMatNum;
	for (auto& Elem2 : MeshContainer.MaterialTransitionSectionMap) {
		unsigned short MatId = Elem2.Key;
		TMeshMaterialTransitionSection& TransitionMaterialSection = Elem2.Value;

		BinaryData << MatId;

		int MatSetSize = TransitionMaterialSection.MaterialIdSet.size();
		BinaryData << MatSetSize;

		for (unsigned short MatSetElement : TransitionMaterialSection.MaterialIdSet) {
			BinaryData << MatSetElement;
		}

		FProcMeshSection& Mesh = TransitionMaterialSection.MaterialMesh;
		Mesh.SerializeMesh(BinaryData);
	}
}

void DeserializeMeshContainer(FMemoryReader& BinaryData, TMeshContainer& MeshContainer) {
	// regular materials
	int32 LodSectionRegularMatNum;
	BinaryData << LodSectionRegularMatNum;

	for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
		unsigned short MatId;
		BinaryData << MatId;

		TMeshMaterialSection& MatSection = MeshContainer.MaterialSectionMap.FindOrAdd(MatId);
		MatSection.MaterialId = MatId;

		MatSection.MaterialMesh.DeserializeMesh(BinaryData);
	}

	// transition materials
	int32 LodSectionTransitionMatNum;
	BinaryData << LodSectionTransitionMatNum;

	for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {
		unsigned short MatId;
		BinaryData << MatId;

		int MatSetSize;
		BinaryData << MatSetSize;

		std::set<unsigned short> MatSet;
		for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
			unsigned short MatSetElement;
			BinaryData << MatSetElement;

			MatSet.insert(MatSetElement);
		}

		TMeshMaterialTransitionSection& MatTransSection = MeshContainer.MaterialTransitionSectionMap.FindOrAdd(MatId);
		MatTransSection.MaterialId = MatId;
		MatTransSection.MaterialIdSet = MatSet;
		MatTransSection.TransitionName = TMeshMaterialTransitionSection::GenerateTransitionName(MatSet);

		MatTransSection.MaterialMesh.DeserializeMesh(BinaryData);
	}
}

void UTerrainRegionComponent::SerializeRegionMeshData(FBufferArchive& BinaryData) {
	int32 MeshDataCount = MeshDataCache.Num();
	BinaryData << MeshDataCount;

	for (auto& Elem : MeshDataCache) {
		TMeshDataPtr& MeshDataPtr = Elem.Value;
		FVector ZoneIndex = Elem.Key;

		BinaryData << ZoneIndex.X;
		BinaryData << ZoneIndex.Y;
		BinaryData << ZoneIndex.Z;

		TMeshData* MeshData = MeshDataPtr.get();

		int32 LodArraySize = MeshData->MeshSectionLodArray.Num();
		BinaryData << LodArraySize;

		for (int32 LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
			TMeshLodSection& LodSection = MeshData->MeshSectionLodArray[LodIdx];
			BinaryData << LodIdx;

			// save whole mesh
			LodSection.WholeMesh.SerializeMesh(BinaryData);

			SerializeMeshContainer(BinaryData, LodSection.RegularMeshContainer);

			if (LodIdx > 0) {
				for (auto i = 0; i < 6; i++) {
					SerializeMeshContainer(BinaryData, LodSection.TransitionPatchArray[i]);
				}
			}
		}
	}
}

void UTerrainRegionComponent::DeserializeRegionMeshData(FMemoryReader& BinaryData) {
	MeshDataCache.Empty();

	int32 MeshDataCount;
	BinaryData << MeshDataCount;

	for (int ZoneIdx = 0; ZoneIdx < MeshDataCount; ZoneIdx++) {
		FVector ZoneIndex;

		BinaryData << ZoneIndex.X;
		BinaryData << ZoneIndex.Y;
		BinaryData << ZoneIndex.Z;

		int32 LodArraySize;
		BinaryData << LodArraySize;

		TMeshDataPtr MeshDataPtr(new TMeshData);

		MeshDataCache.Add(ZoneIndex, MeshDataPtr);

		for (int LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
			int32 LodIndex;
			BinaryData << LodIndex;

			// whole mesh
			MeshDataPtr.get()->MeshSectionLodArray[LodIndex].WholeMesh.DeserializeMesh(BinaryData);

			DeserializeMeshContainer(BinaryData, MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer);

			if (LodIdx > 0) {
				for (auto i = 0; i < 6; i++) {
					DeserializeMeshContainer(BinaryData, MeshDataPtr.get()->MeshSectionLodArray[LodIndex].TransitionPatchArray[i]);
				}
			}
		}

		MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[GetTerrainController()->GetCollisionMeshSectionLodIndex()].WholeMesh;
	}
}

void UTerrainRegionComponent::SerializeInstancedMeshes(FBufferArchive& BinaryData, TArray<UTerrainZoneComponent*>& ZoneArray) {
	int32 ZonesCount = ZoneArray.Num();
	BinaryData << ZonesCount;

	for (UTerrainZoneComponent* Zone : ZoneArray) {
		FVector Location = Zone->GetComponentLocation();

		BinaryData << Location.X;
		BinaryData << Location.Y;
		BinaryData << Location.Z;

		Zone->SerializeInstancedMeshes(BinaryData);
	}
}


void UTerrainRegionComponent::DeserializeZoneInstancedMeshes(FMemoryReader& BinaryData, TInstMeshTypeMap& ZoneInstMeshMap) {
	int32 MeshCount;
	BinaryData << MeshCount;

	for (int Idx = 0; Idx < MeshCount; Idx++) {
		int32 MeshTypeId;
		int32 MeshInstanceCount;

		BinaryData << MeshTypeId;
		BinaryData << MeshInstanceCount;

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

			FTransform Transform(FRotator(Pitch, Yaw, Roll), FVector(X, Y, Z), FVector(ScaleX, ScaleY, ScaleZ));

			if (MeshType.Mesh != nullptr) {
				InstMeshArray.TransformArray.Add(Transform);
			}
		}
	}
}

void UTerrainRegionComponent::SpawnInstMeshFromLoadCache(UTerrainZoneComponent* Zone) {
	TVoxelIndex ZoneIndex = GetTerrainController()->GetZoneIndex(Zone->GetComponentLocation());
	FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
	if (InstancedMeshLoadCache.Contains(ZoneIndexTmp)) {
		TInstMeshTypeMap& ZoneInstMeshMap = InstancedMeshLoadCache[ZoneIndexTmp];

		for (auto& Elem : ZoneInstMeshMap) {
			TInstMeshTransArray& InstMeshTransArray = Elem.Value;

			for (FTransform& Transform : InstMeshTransArray.TransformArray) {
				Zone->SpawnInstancedMesh(InstMeshTransArray.MeshType, Transform);
			}

		}

		InstancedMeshLoadCache.Remove(ZoneIndexTmp);
	}

}

void UTerrainRegionComponent::DeserializeRegionInstancedMeshes(FMemoryReader& BinaryData) {
	int32 ZonesCount;
	BinaryData << ZonesCount;

	for (int Idx = 0; Idx < ZonesCount; Idx++) {
		FVector ZoneLocation;

		BinaryData << ZoneLocation.X;
		BinaryData << ZoneLocation.Y;
		BinaryData << ZoneLocation.Z;

		TVoxelIndex ZoneIndex = GetTerrainController()->GetZoneIndex(ZoneLocation);
		FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

		TInstMeshTypeMap& ZoneInstMeshMap = InstancedMeshLoadCache.FindOrAdd(ZoneIndexTmp);

		DeserializeZoneInstancedMeshes(BinaryData, ZoneInstMeshMap);
	}
}

void UTerrainRegionComponent::Save(std::function<void(FBufferArchive& BinaryData)> SaveFunction, FString& FileExt) {
	double Start = FPlatformTime::Seconds();

	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->GetRegionIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") +
		GetTerrainController()->MapName + TEXT("/region.") +
		FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".") + FileExt;

	FBufferArchive BinaryData;

	int32 version = 1;
	BinaryData << version;

	SaveFunction(BinaryData);

	bool bIsSaved = FFileHelper::SaveArrayToFile(BinaryData, *FileName);

	BinaryData.FlushCache();
	BinaryData.Empty();

	double End = FPlatformTime::Seconds();
	double LogTime = (End - Start) * 1000;

	if(bIsSaved){
		UE_LOG(LogSandboxTerrain, Log, TEXT("Save region '%s' file -------------> %f %f %f --> %f ms"), *FileExt, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, LogTime);
	} else {
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Failed to save region '%s' file ----> %f %f %f"), *FileExt, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z);
	}

}

void UTerrainRegionComponent::Load(std::function<void(FMemoryReader& BinaryData)> LoadFunction, FString& FileExt) {
	double Start = FPlatformTime::Seconds();

	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->GetRegionIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/region.") +
		FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".") + FileExt;

	TArray<uint8> BinaryArray;

	if (!FPaths::FileExists(FileName)) {
		return;
	}

	if (!FFileHelper::LoadFileToArray(BinaryArray, *FileName)) {
		//UE_LOG(LogTemp, Warning, TEXT("File not found -> %s"), *FileName);
		return;
	}

	if (BinaryArray.Num() <= 0) return;

	FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
	BinaryData.Seek(0);

	int32 version = 1;
	BinaryData << version;

	UE_LOG(LogTemp, Warning, TEXT("File format version -> %d"), version);

	LoadFunction(BinaryData);

	BinaryData.FlushCache();
	BinaryArray.Empty();
	BinaryData.Close();

	double End = FPlatformTime::Seconds();
	double LogTime = (End - Start) * 1000;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Load region %s file -------------> %f %f %f --> %f ms"), *FileExt, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, LogTime);
}

void UTerrainRegionComponent::SaveFile(TArray<UTerrainZoneComponent*>& ZoneArray) {
	FString Ext = TEXT("dat");
	Save([&](FBufferArchive& BinaryData) { 
		SerializeRegionMeshData(BinaryData); 
		SerializeInstancedMeshes(BinaryData, ZoneArray);
	}, Ext);
}

void UTerrainRegionComponent::LoadFile() {
	FString Ext = TEXT("dat");
	Load([&](FMemoryReader& BinaryData) { 
		DeserializeRegionMeshData(BinaryData);
		DeserializeRegionInstancedMeshes(BinaryData); }, Ext);
}