// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainRegionComponent.h"
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

	SaveFunction(BinaryData);

	if (FFileHelper::SaveArrayToFile(BinaryData, *FileName)) {
		BinaryData.FlushCache();
		BinaryData.Empty();
	}

	double End = FPlatformTime::Seconds();
	double LogTime = (End - Start) * 1000;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Save region '%s' file -------------> %f %f %f --> %f ms"), *FileExt, GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, LogTime);
}

void UTerrainRegionComponent::Load(std::function<void(FMemoryReader& BinaryData)> LoadFunction, FString& FileExt) {
	double Start = FPlatformTime::Seconds();

	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->GetZoneIndex(GetComponentLocation());

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
	Load([&](FMemoryReader& BinaryData) { DeserializeRegionVoxelData(BinaryData); }, Ext);
}

void UTerrainRegionComponent::SaveFile() {
	FString Ext = TEXT("dat");
	Save([&](FBufferArchive& BinaryData) { SerializeRegionMeshData(BinaryData); }, Ext);
}

void UTerrainRegionComponent::LoadFile() {
	FString Ext = TEXT("dat");
	Load([&](FMemoryReader& BinaryData) { DeserializeRegionMeshData(BinaryData); }, Ext);
}