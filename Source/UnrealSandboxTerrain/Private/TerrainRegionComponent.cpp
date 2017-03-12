// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainRegionComponent.h"
#include "SandboxTerrainController.h"

#include "DrawDebugHelpers.h"

UTerrainRegionComponent::UTerrainRegionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainRegionComponent::SerializeRegionMeshData(FBufferArchive& BinaryData) {

	int32 MeshDataCount = MeshDataCache.Num();
	BinaryData << MeshDataCount;

	UE_LOG(LogTemp, Warning, TEXT("region total zone meshes -> %d"), MeshDataCount);

	for (auto& Elem : MeshDataCache) {
		TMeshDataPtr& MeshDataPtr = Elem.Value;
		FVector ZoneIndex = Elem.Key;

		UE_LOG(LogTemp, Warning, TEXT("save region zone mesh -> %f %f %f"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

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


void UTerrainRegionComponent::SaveFile() {
	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->GetRegionIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/region.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".dat");

	UE_LOG(LogTemp, Warning, TEXT("save region -> %s"), *FileName);

	FBufferArchive BinaryData;

	SerializeRegionMeshData(BinaryData);

	if (FFileHelper::SaveArrayToFile(BinaryData, *FileName)) {
		BinaryData.FlushCache();
		BinaryData.Empty();
	}
}

void UTerrainRegionComponent::DeserializeRegionMeshData(FMemoryReader& BinaryData) {
	MeshDataCache.Empty();

	int32 MeshDataCount;
	BinaryData << MeshDataCount;

	UE_LOG(LogTemp, Warning, TEXT("MeshDataCount -> %d"), MeshDataCount);

	for (int ZoneIdx = 0; ZoneIdx < MeshDataCount; ZoneIdx++) {
		FVector ZoneIndex;

		BinaryData << ZoneIndex.X;
		BinaryData << ZoneIndex.Y;
		BinaryData << ZoneIndex.Z;

		UE_LOG(LogTemp, Warning, TEXT("region zone mesh -> %f %f %f"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

		int32 LodArraySize;
		BinaryData << LodArraySize;

		UE_LOG(LogTemp, Warning, TEXT("LodArraySize -> %d"), LodArraySize);

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

			UE_LOG(LogTemp, Warning, TEXT("LodIndex -> %d"), LodIndex);
			UE_LOG(LogTemp, Warning, TEXT("LodSectionRegularMatNum -> %d"), LodSectionRegularMatNum);

			for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
				unsigned short MatId;
				BinaryData << MatId;

				UE_LOG(LogTemp, Warning, TEXT("MatId -> %d"), MatId);

				TMeshMaterialSection& MatSection = MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer.MaterialSectionMap.FindOrAdd(MatId);
				MatSection.MaterialId = MatId;

				MatSection.MaterialMesh.DeserializeMesh(BinaryData);
			}

			// transition materials
			int32 LodSectionTransitionMatNum;
			BinaryData << LodSectionTransitionMatNum;

			UE_LOG(LogTemp, Warning, TEXT("LodSectionTransitionMatNum -> %d"), LodSectionTransitionMatNum);

			for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {
				unsigned short MatId;
				BinaryData << MatId;
				UE_LOG(LogTemp, Warning, TEXT("MatId -> %d"), MatId);

				int MatSetSize;
				BinaryData << MatSetSize;

				UE_LOG(LogTemp, Warning, TEXT("MatSetSize -> %d"), MatSetSize);
				std::set<unsigned short> MatSet;
				for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
					unsigned short MatSetElement;
					BinaryData << MatSetElement;

					MatSet.insert(MatSetElement);

					UE_LOG(LogTemp, Warning, TEXT("MatSetElement -> %d"), MatSetElement);
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

void UTerrainRegionComponent::LoadFile() {
	FString SavePath = FPaths::GameSavedDir();
	FVector Index = GetTerrainController()->getZoneIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/region.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".dat");

	TArray<uint8> BinaryArray;
	if (!FFileHelper::LoadFileToArray(BinaryArray, *FileName)) {
		UE_LOG(LogTemp, Warning, TEXT("file not found -> %s"), *FileName);
		return;
	}

	if (BinaryArray.Num() <= 0) return;

	FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
	BinaryData.Seek(0);

	//=============================
	DeserializeRegionMeshData(BinaryData);
	
	//=============================

	BinaryData.FlushCache();
	BinaryArray.Empty();
	BinaryData.Close();
}