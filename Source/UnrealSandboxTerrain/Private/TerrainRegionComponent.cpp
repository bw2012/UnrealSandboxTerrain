// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "TerrainRegionComponent.h"
#include "SandboxTerrainController.h"

#include "DrawDebugHelpers.h"

UTerrainRegionComponent::UTerrainRegionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void SerializeMesh(FBufferArchive& BinaryData, const FProcMeshSection& Mesh) {
	// vertexes
	int32 VertexNum = Mesh.ProcVertexBuffer.Num();
	BinaryData << VertexNum;
	for (auto& Vertex : Mesh.ProcVertexBuffer) {

		float PosX = Vertex.Position.X;
		float PosY = Vertex.Position.Y;
		float PosZ = Vertex.Position.Z;

		BinaryData << PosX;
		BinaryData << PosY;
		BinaryData << PosZ;

		float NormalX = Vertex.Normal.X;
		float NormalY = Vertex.Normal.Y;
		float NormalZ = Vertex.Normal.Z;

		BinaryData << NormalX;
		BinaryData << NormalY;
		BinaryData << NormalZ;

		uint8 ColorR = Vertex.Color.R;
		uint8 ColorG = Vertex.Color.G;
		uint8 ColorB = Vertex.Color.B;
		uint8 ColorA = Vertex.Color.A;

		BinaryData << ColorR;
		BinaryData << ColorG;
		BinaryData << ColorB;
		BinaryData << ColorA;
	}

	// indexes
	int32 IndexNum = Mesh.ProcIndexBuffer.Num();
	BinaryData << IndexNum;
	for (int32 Index : Mesh.ProcIndexBuffer) {
		BinaryData << Index;
	}
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

			// save regular materials
			int32 LodSectionRegularMatNum = LodSection.MaterialSectionMap.Num();
			BinaryData << LodSectionRegularMatNum;
			for (auto& Elem2 : LodSection.MaterialSectionMap) {
				unsigned short MatId = Elem2.Key;
				TMeshMaterialSection& MaterialSection = Elem2.Value;

				BinaryData << MatId;

				FProcMeshSection& Mesh = MaterialSection.MaterialMesh;
				SerializeMesh(BinaryData, Mesh);
			}

			// save transition materials
			int32 LodSectionTransitionMatNum = LodSection.MaterialTransitionSectionMap.Num();
			BinaryData << LodSectionTransitionMatNum;
			for (auto& Elem2 : LodSection.MaterialTransitionSectionMap) {
				unsigned short MatId = Elem2.Key;
				TMeshMaterialTransitionSection& TransitionMaterialSection = Elem2.Value;

				int MatSetSize = TransitionMaterialSection.MaterialIdSet.size();
				BinaryData << MatSetSize;

				for (unsigned short MatSetElement : TransitionMaterialSection.MaterialIdSet) {
					BinaryData << MatSetElement;
				}

				BinaryData << MatId;

				FProcMeshSection& Mesh = TransitionMaterialSection.MaterialMesh;
				SerializeMesh(BinaryData, Mesh);
			}

		}



	}

}


void UTerrainRegionComponent::SaveRegionToFile() {
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


void DeserializeMesh(FMemoryReader& BinaryData, FProcMeshSection& Mesh) {

	int32 VertexNum; 
	BinaryData << VertexNum;

	UE_LOG(LogTemp, Warning, TEXT("VertexNum -> %d"), VertexNum);

	for (int Idx = 0; Idx < VertexNum; Idx++) {

		float PosX;
		float PosY;
		float PosZ;

		BinaryData << PosX;
		BinaryData << PosY;
		BinaryData << PosZ;

		float NormalX;
		float NormalY;
		float NormalZ;

		BinaryData << NormalX;
		BinaryData << NormalY;
		BinaryData << NormalZ;

		uint8 ColorR;
		uint8 ColorG;
		uint8 ColorB;
		uint8 ColorA;

		BinaryData << ColorR;
		BinaryData << ColorG;
		BinaryData << ColorB;
		BinaryData << ColorA;
	}

	int32 IndexNum;
	BinaryData << IndexNum;
	UE_LOG(LogTemp, Warning, TEXT("IndexNum -> %d"), IndexNum);

	for (int Idx = 0; Idx < IndexNum; Idx++) {
		int32 Index;
		BinaryData << Index;
	}
}

void UTerrainRegionComponent::LoadRegionFromFile() {
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

		for (int LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
			int32 LodIndex;
			BinaryData << LodIndex;

			// regular materials
			int32 LodSectionRegularMatNum;
			BinaryData << LodSectionRegularMatNum;

			UE_LOG(LogTemp, Warning, TEXT("LodIndex -> %d"), LodIndex);
			UE_LOG(LogTemp, Warning, TEXT("LodSectionRegularMatNum -> %d"), LodSectionRegularMatNum);

			for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
				unsigned short MatId;
				BinaryData << MatId;

				UE_LOG(LogTemp, Warning, TEXT("MatId -> %d"), MatId);

				FProcMeshSection Mesh;
				DeserializeMesh(BinaryData, Mesh);
			}

			// transition materials
			int32 LodSectionTransitionMatNum;
			BinaryData << LodSectionTransitionMatNum;

			UE_LOG(LogTemp, Warning, TEXT("LodSectionTransitionMatNum -> %d"), LodSectionTransitionMatNum);

			for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {

				int MatSetSize;
				BinaryData << MatSetSize;

				UE_LOG(LogTemp, Warning, TEXT("MatSetSize -> %d"), MatSetSize);
				for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
					unsigned short MatSetElement;
					BinaryData << MatSetElement;

					UE_LOG(LogTemp, Warning, TEXT("MatSetElement -> %d"), MatSetElement);
				}

				unsigned short MatId;
				BinaryData << MatId;

				UE_LOG(LogTemp, Warning, TEXT("MatId -> %d"), MatId);

				FProcMeshSection Mesh;
				DeserializeMesh(BinaryData, Mesh);
			}
			
		}
	}


	//=============================



	BinaryData.FlushCache();
	BinaryArray.Empty();
	BinaryData.Close();
}