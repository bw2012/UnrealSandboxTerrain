// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainRegionComponent.h"
#include "TerrainZoneComponent.h"
#include "SandboxTerrainController.h"

#include "DrawDebugHelpers.h"

#include <memory>

UTerrainRegionComponent::UTerrainRegionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

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
			LodSection.RegularMeshContainer.WholeMesh.SerializeMesh(BinaryData);

			// save regular materials
			int32 LodSectionRegularMatNum = LodSection.RegularMeshContainer.MaterialSectionMap.Num();
			BinaryData << LodSectionRegularMatNum;
			for (auto& Elem2 : LodSection.RegularMeshContainer.MaterialSectionMap) {
				unsigned short MatId = Elem2.Key;
				TMeshMaterialSection& MaterialSection = Elem2.Value;

				BinaryData << MatId;

				FProcMeshSection& Mesh = MaterialSection.MaterialMesh;
				Mesh.SerializeMesh(BinaryData);
			}

			// save transition materials
			int32 LodSectionTransitionMatNum = LodSection.RegularMeshContainer.MaterialTransitionSectionMap.Num();
			BinaryData << LodSectionTransitionMatNum;
			for (auto& Elem2 : LodSection.RegularMeshContainer.MaterialTransitionSectionMap) {
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
			MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer.WholeMesh.DeserializeMesh(BinaryData);

			// regular materials
			int32 LodSectionRegularMatNum;
			BinaryData << LodSectionRegularMatNum;

			for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
				unsigned short MatId;
				BinaryData << MatId;

				TMeshMaterialSection& MatSection = MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer.MaterialSectionMap.FindOrAdd(MatId);
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

				TMeshMaterialTransitionSection& MatTransSection = MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer.MaterialTransitionSectionMap.FindOrAdd(MatId);
				MatTransSection.MaterialId = MatId;
				MatTransSection.MaterialIdSet = MatSet;
				MatTransSection.TransitionName = TMeshMaterialTransitionSection::GenerateTransitionName(MatSet);

				MatTransSection.MaterialMesh.DeserializeMesh(BinaryData);
			}
		}

		MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[0].RegularMeshContainer.WholeMesh;
	}
}

void UTerrainRegionComponent::SerializeRegionVoxelData(FBufferArchive& BinaryData, TArray<TVoxelData*>& VoxalDataArray) {
	int32 VoxelDataCount = VoxalDataArray.Num();
	BinaryData << VoxelDataCount;

	for (TVoxelData* VoxelData : VoxalDataArray) {

		float X = VoxelData->getOrigin().X;
		float Y = VoxelData->getOrigin().Y;
		float Z = VoxelData->getOrigin().Z;

		BinaryData << X;
		BinaryData << Y;
		BinaryData <<  Z;

		serializeVoxelData(*VoxelData, BinaryData);
	}
}

void UTerrainRegionComponent::DeserializeRegionVoxelData(FMemoryReader& BinaryData) {
	int32 VoxelDataCount;
	BinaryData << VoxelDataCount;

	for (int Idx = 0; Idx < VoxelDataCount; Idx++) {
		FVector VoxelDataOrigin;

		BinaryData << VoxelDataOrigin.X;
		BinaryData << VoxelDataOrigin.Y;
		BinaryData << VoxelDataOrigin.Z;

		TVoxelData* Vd = new TVoxelData(65, 100 * 10);
		FVector VoxelDataIndex = GetTerrainController()->GetZoneIndex(VoxelDataOrigin);

		Vd->setOrigin(VoxelDataOrigin);

		deserializeVoxelData(*Vd, BinaryData);

		GetTerrainController()->RegisterTerrainVoxelData(Vd, VoxelDataIndex);
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
	FVector ZoneIndex = GetTerrainController()->GetZoneIndex(Zone->GetComponentLocation());
	if (InstancedMeshLoadCache.Contains(ZoneIndex)) {
		TInstMeshTypeMap& ZoneInstMeshMap = InstancedMeshLoadCache[ZoneIndex];

		for (auto& Elem : ZoneInstMeshMap) {
			TInstMeshTransArray& InstMeshTransArray = Elem.Value;

			for (FTransform& Transform : InstMeshTransArray.TransformArray) {
				Zone->SpawnInstancedMesh(InstMeshTransArray.MeshType, Transform);
			}

		}

		InstancedMeshLoadCache.Remove(ZoneIndex);
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

		FVector ZoneIndex = GetTerrainController()->GetZoneIndex(ZoneLocation);

		TInstMeshTypeMap& ZoneInstMeshMap = InstancedMeshLoadCache.FindOrAdd(ZoneIndex);

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
	if (!FFileHelper::LoadFileToArray(BinaryArray, *FileName)) {
		UE_LOG(LogTemp, Warning, TEXT("File not found -> %s"), *FileName);
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

void UTerrainRegionComponent::SaveVoxelData(TArray<TVoxelData*>& VoxalDataArray) {
	FString Ext = TEXT("vd");
	Save([&](FBufferArchive& BinaryData) { SerializeRegionVoxelData(BinaryData, VoxalDataArray); }, Ext);
}

void UTerrainRegionComponent::LoadVoxelData() {
	FString Ext = TEXT("vd");
	Load([&](FMemoryReader& BinaryData) { 
		DeserializeRegionVoxelData(BinaryData); 
	}, Ext);
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