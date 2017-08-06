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

	//if (VdInFilePtr == nullptr || !VdInFilePtr->is_open()) return;
	//VdInFilePtr->close();
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
					//SerializeMeshContainer(BinaryData, LodSection.TransitionPatchArray[i]);
				}
			}
		}

		MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[GetTerrainController()->GetCollisionMeshSectionLodIndex()].WholeMesh;
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
		BinaryData << Z;

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

		TVoxelDataInfo VdInfo;
		VdInfo.DataState = TVoxelDataState::LOADED;
		VdInfo.Vd = Vd;

		GetTerrainController()->RegisterTerrainVoxelData(VdInfo, VoxelDataIndex);
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

void UTerrainRegionComponent::SaveVoxelData2(TArray<TVoxelData*>& VoxalDataArray) {
	FString Ext = TEXT("vd2");
	Save([&](FBufferArchive& BinaryData) {

		TMap<FVector, FBufferArchive> VdSaveMap;

		for (TVoxelData* Vd : VoxalDataArray) {
			FVector Pos = Vd->getOrigin();

			// FindOrAdd works strange in UE4.16
			// created FBufferArchive is very strange and throws runtime failure while << operator
			//FBufferArchive& SaveEntry = VdSaveMap.FindOrAdd(Pos);

			FBufferArchive SaveEntry;
			serializeVoxelData(*Vd, SaveEntry);
			VdSaveMap.Add(Pos, SaveEntry);
			//UE_LOG(LogTemp, Log, TEXT("save voxel data block -> %f %f %f -> %d"), Pos.X, Pos.Y, Pos.Z, Buffer2.Num());
		}

		TArray<FVector> KeyArray;
		VdSaveMap.GetKeys(KeyArray);

		uint64 Offset = 0;
		int32 HeaderEntriesNum = KeyArray.Num();

		BinaryData << HeaderEntriesNum;

		for (FVector& Pos : KeyArray) {
			BinaryData << Pos.X;
			BinaryData << Pos.Y;
			BinaryData << Pos.Z;
			BinaryData << Offset;

			FBufferArchive& BodyDataEntry = VdSaveMap.FindOrAdd(Pos);
			int64 Size = BodyDataEntry.Num();

			BinaryData << Size;
			Offset += BodyDataEntry.Num();

			UE_LOG(LogTemp, Log, TEXT("save voxel data block -> %f %f %f -> %d"), Pos.X, Pos.Y, Pos.Z, Size);
		}

		for (FVector& Pos : KeyArray) {
			FBufferArchive& BodyDataEntry = VdSaveMap.FindOrAdd(Pos);

			for (uint8 B : BodyDataEntry) {
				BinaryData << B;
			}
		}
	}, Ext);
}


template <typename T>
void read(std::istream* is, const T& obj) {
	is->read((char*)&obj, sizeof(obj));
}

void UTerrainRegionComponent::OpenRegionVdFile() {

	FString FileExt = TEXT("vd2");

	FString SavePath = FPaths::GameSavedDir();

	ASandboxTerrainController* TC = GetTerrainController();
	if (TC == NULL) {
		UE_LOG(LogTemp, Warning, TEXT("ERROR"));
		return;
	}

	FVector Index = TC->GetRegionIndex(GetComponentLocation());

	int tx = Index.X;
	int ty = Index.Y;
	int tz = Index.Z;

	FString FileName = SavePath + TEXT("/Map/") + GetTerrainController()->MapName + TEXT("/region.") +
		FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".") + FileExt;


	VdInFilePtr = new std::ifstream(TCHAR_TO_ANSI(*FileName), std::ios::binary);

	if (!VdInFilePtr->is_open()) {
		UE_LOG(LogTemp, Warning, TEXT("error open vd2 file -> %s"), *FileName);
		return;
	}

	VdInFilePtr->seekg(0);

	int32 version = 0;
	read(VdInFilePtr, version);

	int32 HeaderEntriesNum = 0;
	read(VdInFilePtr, HeaderEntriesNum);

	for (int32 Idx = 0; Idx < HeaderEntriesNum; Idx++) {
        float X = 0;
        float Y = 0;
        float Z = 0;
		int64 Offset = 0;
		int64 Size = 0;

		read(VdInFilePtr, X);
		read(VdInFilePtr, Y);
		read(VdInFilePtr, Z);
		read(VdInFilePtr, Offset);
		read(VdInFilePtr, Size);

		TVoxelDataFileBodyPos VoxelDataFileBodyPos;
		VoxelDataFileBodyPos.Offset = Offset;
		VoxelDataFileBodyPos.Size = Size;

		FVector ZonePos(X, Y, Z);
		VdFileBodyMap.Add(ZonePos, VoxelDataFileBodyPos);

		TVoxelDataInfo VdInfo;
		VdInfo.DataState = TVoxelDataState::READY_TO_LOAD;
		VdInfo.Vd = nullptr;

		GetTerrainController()->RegisterTerrainVoxelData(VdInfo, GetTerrainController()->GetZoneIndex(ZonePos));
	}

    VdBinaryDataStart = VdInFilePtr->tellg();
	//VdBinaryDataStart = VdInFilePtr->tellg().seekpos();
}

void UTerrainRegionComponent::LoadVoxelByInnerPos(TVoxelDataFileBodyPos& BodyPos, TArray<uint8>& BinaryArray) {
	VdInFilePtr->seekg(BodyPos.Offset + VdBinaryDataStart);
	BinaryArray.Reserve(BodyPos.Size);

	for (int Idx = 0; Idx < BodyPos.Size; Idx++) {
		uint8 Byte;
		read(VdInFilePtr, Byte);
		BinaryArray.Add(Byte);
	}
}


TVoxelData* UTerrainRegionComponent::LoadVoxelDataByZoneIndex(FVector Index) {
	if (VdInFilePtr == nullptr) return nullptr;
	if (!VdInFilePtr->is_open()) return nullptr;

	FVector VoxelDataOrigin = GetTerrainController()->GetZonePos(Index);

	if (VdFileBodyMap.Contains(VoxelDataOrigin)) {
		double Start = FPlatformTime::Seconds();

		VdFileMutex.lock();

		TVoxelDataFileBodyPos& BodyPos = VdFileBodyMap[VoxelDataOrigin];
		TArray<uint8> BinaryArray;

		LoadVoxelByInnerPos(BodyPos, BinaryArray);

		VdFileMutex.unlock();

		FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
		BinaryData.Seek(0);

		TVoxelData* Vd = new TVoxelData(65, 100 * 10);
		Vd->setOrigin(VoxelDataOrigin);

		deserializeVoxelData(*Vd, BinaryData);

		TVoxelDataInfo VdInfo;
		VdInfo.DataState = TVoxelDataState::LOADED;
		VdInfo.Vd = Vd;

		GetTerrainController()->RegisterTerrainVoxelData(VdInfo, Index);

		BodyPos.bIsLoaded = true;

		double End = FPlatformTime::Seconds();
		double Time = (End - Start) * 1000;

		UE_LOG(LogTemp, Log, TEXT("loading voxel data block -> %f %f %f -> %f ms"), VoxelDataOrigin.X, VoxelDataOrigin.Y, VoxelDataOrigin.Z, Time);
		return Vd;
	}

	return nullptr;
}

void UTerrainRegionComponent::LoadAllVoxelData(TArray<TVoxelData*>& LoadedVdArray) {
	if (VdInFilePtr == nullptr) return;
	if (!VdInFilePtr->is_open()) return;

	double Start = FPlatformTime::Seconds();

	for (auto& Elem : VdFileBodyMap) {
		FVector& VoxelDataOrigin = Elem.Key;
		TVoxelDataFileBodyPos& BodyPos = Elem.Value;

		if (!BodyPos.bIsLoaded) {
			TArray<uint8> BinaryArray;
			LoadVoxelByInnerPos(BodyPos, BinaryArray);

			FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
			BinaryData.Seek(0);

			TVoxelData* Vd = new TVoxelData(65, 100 * 10);
			Vd->setOrigin(VoxelDataOrigin);

			deserializeVoxelData(*Vd, BinaryData);

			TVoxelDataInfo VdInfo;
			VdInfo.DataState = TVoxelDataState::LOADED;
			VdInfo.Vd = Vd;

			GetTerrainController()->RegisterTerrainVoxelData(VdInfo, GetTerrainController()->GetZoneIndex(VoxelDataOrigin));
			LoadedVdArray.Add(Vd);

			UE_LOG(LogTemp, Log, TEXT("loading voxel data block -> %f %f %f"), VoxelDataOrigin.X, VoxelDataOrigin.Y, VoxelDataOrigin.Z);
			BodyPos.bIsLoaded = true;
		}
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	UE_LOG(LogTemp, Log, TEXT("loading all vd -> %f ms"), Time);
}

void UTerrainRegionComponent::CloseRegionVdFile() {
	if (VdInFilePtr == nullptr || !VdInFilePtr->is_open()) return;
	VdInFilePtr->close();
}
