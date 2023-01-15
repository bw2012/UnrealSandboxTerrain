
#include "SandboxTerrainController.h"
#include "VoxelMeshData.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "TerrainZoneComponent.h"
#include "Json.h"
#include "JsonObjectConverter.h"
#include "VoxelDataInfo.hpp"
#include "TerrainData.hpp"


//======================================================================================================================================================================
// mesh data de/serealization
//======================================================================================================================================================================

void SerializeMeshContainer(const TMeshContainer& MeshContainer, usbt::TFastUnsafeSerializer& Serializer) {
	// save regular materials
	int32 LodSectionRegularMatNum = MeshContainer.MaterialSectionMap.Num();
	Serializer << LodSectionRegularMatNum;
	for (auto& Elem : MeshContainer.MaterialSectionMap) {
		unsigned short MatId = Elem.Key;
		const TMeshMaterialSection& MaterialSection = Elem.Value;

		Serializer << MatId;

		const FProcMeshSection& Mesh = MaterialSection.MaterialMesh;
		Mesh.SerializeMesh(Serializer);
	}

	// save transition materials
	int32 LodSectionTransitionMatNum = MeshContainer.MaterialTransitionSectionMap.Num();
	Serializer << LodSectionTransitionMatNum;
	for (auto& Elem : MeshContainer.MaterialTransitionSectionMap) {
		unsigned short MatId = Elem.Key;
		const TMeshMaterialTransitionSection& TransitionMaterialSection = Elem.Value;

		Serializer << MatId;

		int MatSetSize = TransitionMaterialSection.MaterialIdSet.size();
		Serializer << MatSetSize;

		for (unsigned short MatSetElement : TransitionMaterialSection.MaterialIdSet) {
			Serializer << MatSetElement;
		}

		const FProcMeshSection& Mesh = TransitionMaterialSection.MaterialMesh;
		Mesh.SerializeMesh(Serializer);
	}
}

TValueDataPtr Compress(TValueDataPtr CompressedDataPtr) {
	TValueDataPtr Result = std::make_shared<TValueData>();
	TArray<uint8> BinaryArray;
	BinaryArray.SetNum(CompressedDataPtr->size());
	FMemory::Memcpy(BinaryArray.GetData(), CompressedDataPtr->data(), CompressedDataPtr->size());

	TArray<uint8> CompressedData;
	FArchiveSaveCompressedProxy Compressor = FArchiveSaveCompressedProxy(CompressedData, NAME_Zlib);
	Compressor << BinaryArray;
	Compressor.Flush();

	Result->resize(CompressedData.Num());
	FMemory::Memcpy(Result->data(), CompressedData.GetData(), CompressedData.Num());
	CompressedData.Empty();

	return Result;
}

TValueDataPtr SerializeMeshData(TMeshDataPtr MeshDataPtr) {
	usbt::TFastUnsafeSerializer Serializer;

	int32 LodArraySize = MeshDataPtr->MeshSectionLodArray.Num();
	Serializer << LodArraySize;

	for (int32 LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
		const TMeshLodSection& LodSection = MeshDataPtr->MeshSectionLodArray[LodIdx];
		Serializer << LodIdx;

		// save whole mesh
		LodSection.WholeMesh.SerializeMesh(Serializer);

		SerializeMeshContainer(LodSection.RegularMeshContainer, Serializer);

		if (LodIdx > 0) {
			for (auto i = 0; i < 6; i++) {
				SerializeMeshContainer(LodSection.TransitionPatchArray[i], Serializer);
			}
		}
	}

	TValueDataPtr Result = Compress(Serializer.data());
	return Result;
}

void DeserializeMeshContainerFast(TMeshContainer& MeshContainer, usbt::TFastUnsafeDeserializer& Deserializer) {
	// regular materials
	int32 LodSectionRegularMatNum;
	Deserializer >> LodSectionRegularMatNum;

	for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
		unsigned short MatId;
		Deserializer >> MatId;

		TMeshMaterialSection& MatSection = MeshContainer.MaterialSectionMap.FindOrAdd(MatId);
		MatSection.MaterialId = MatId;

		MatSection.MaterialMesh.DeserializeMeshFast(Deserializer);
	}

	// transition materials
	int32 LodSectionTransitionMatNum;
	Deserializer >> LodSectionTransitionMatNum;

	for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {
		unsigned short MatId;
		Deserializer >> MatId;

		int MatSetSize;
		Deserializer >> MatSetSize;

		std::set<unsigned short> MatSet;
		for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
			unsigned short MatSetElement;
			Deserializer >> MatSetElement;

			MatSet.insert(MatSetElement);
		}

		TMeshMaterialTransitionSection& MatTransSection = MeshContainer.MaterialTransitionSectionMap.FindOrAdd(MatId);
		MatTransSection.MaterialId = MatId;
		MatTransSection.MaterialIdSet = MatSet;

		MatTransSection.MaterialMesh.DeserializeMeshFast(Deserializer);
	}
}

TMeshDataPtr DeserializeMeshDataFast(const std::vector<uint8>& Data, uint32 CollisionMeshSectionLodIndex) {
	TMeshDataPtr MeshDataPtr(new TMeshData);
	usbt::TFastUnsafeDeserializer Deserializer(Data.data());

	int32 LodArraySize;
	Deserializer >> LodArraySize;

	for (int LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
		int32 LodIndex;
		Deserializer >> LodIndex;

		// whole mesh
		MeshDataPtr.get()->MeshSectionLodArray[LodIndex].WholeMesh.DeserializeMeshFast(Deserializer);
		DeserializeMeshContainerFast(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer, Deserializer);

		if (LodIdx > 0) {
			for (auto i = 0; i < 6; i++) {
				DeserializeMeshContainerFast(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].TransitionPatchArray[i], Deserializer);
			}
		}
	}

	MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[CollisionMeshSectionLodIndex].WholeMesh;
	return MeshDataPtr;
}

bool LoadDataFromKvFile(TKvFile& KvFile, const TVoxelIndex& Index, std::function<void(TValueDataPtr)> Function) {
	TValueDataPtr DataPtr = KvFile.loadData(Index);
	if (DataPtr == nullptr || DataPtr->size() == 0) {
		return false; 
	}
	Function(DataPtr);
	return true;
}

TValueDataPtr LoadDataFromKvFile2(TKvFile& KvFile, const TVoxelIndex& Index) {
	TValueDataPtr DataPtr = KvFile.loadData(Index);
	if (DataPtr == nullptr || DataPtr->size() == 0) {
		return nullptr;
	}
	return DataPtr;
}

TValueDataPtr Decompress(TValueDataPtr CompressedDataPtr) {
	TValueDataPtr Result = std::make_shared<TValueData>();
	TArray<uint8> BinaryArray;
	BinaryArray.SetNum(CompressedDataPtr->size());
	FMemory::Memcpy(BinaryArray.GetData(), CompressedDataPtr->data(), CompressedDataPtr->size());

	FArchiveLoadCompressedProxy Decompressor = FArchiveLoadCompressedProxy(BinaryArray, NAME_Zlib);
	if (Decompressor.GetError()) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("FArchiveLoadCompressedProxy -> ERROR : File was not compressed"));
		return Result;
	}

	TArray<uint8> DecompressedData;
	Decompressor << DecompressedData;

	float CompressionRatio = ((float)CompressedDataPtr->size() / (float)DecompressedData.Num()) * 100.f;
	//UE_LOG(LogSandboxTerrain, Log, TEXT("DecompressedData -> %d bytes ==> %d bytes -> %f%%"), DecompressedData.Num(), CompressedDataPtr->size(), CompressionRatio);

	Result->resize(DecompressedData.Num());
	FMemory::Memcpy(Result->data(), DecompressedData.GetData(), DecompressedData.Num());
	Decompressor.FlushCache();
	DecompressedData.Empty();

	return Result;
}

//======================================================================================================================================================================
// 
//======================================================================================================================================================================

bool ASandboxTerrainController::LoadMeshAndObjectDataByIndex(const TVoxelIndex& Index, TMeshDataPtr& MeshData, TInstanceMeshTypeMap& ZoneInstMeshMap) const {
	double Start = FPlatformTime::Seconds();

	TValueDataPtr DataPtr = LoadDataFromKvFile2(TdFile, Index);
	if (DataPtr) {
		usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
		TKvFileZodeData ZoneHeader;
		Deserializer >> ZoneHeader;

		if (ZoneHeader.LenMd > 0) {
			TValueDataPtr CompressedMdPtr = std::make_shared<TValueData>();
			CompressedMdPtr->resize(ZoneHeader.LenMd);
			Deserializer.read(CompressedMdPtr->data(), ZoneHeader.LenMd);
			auto DecompressedDataPtr = Decompress(CompressedMdPtr);
			MeshData = DeserializeMeshDataFast(*DecompressedDataPtr, 0);
		}

		TValueDataPtr ObjDataPtr = LoadDataFromKvFile2(ObjFile, Index);
		if (ObjDataPtr) {
			DeserializeInstancedMeshes(*ObjDataPtr, ZoneInstMeshMap);
		}
	}

	return true;
}

//======================================================================================================================================================================
// load vd
//======================================================================================================================================================================

// TODO: use shared_ptr
TVoxelData* ASandboxTerrainController::LoadVoxelDataByIndex(const TVoxelIndex& Index) {
	double Start = FPlatformTime::Seconds();

	TVoxelData* Vd = NewVoxelData();
	Vd->setOrigin(GetZonePos(Index));

	TValueDataPtr DataPtr = LoadDataFromKvFile2(VdFile, Index);
	if (DataPtr) {
		DeserializeVd(DataPtr, Vd);
	} else {
		UE_LOG(LogSandboxTerrain, Warning, TEXT("LoadVoxelDataByIndex error: no vd found in file"));
		return nullptr;
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Loading voxel data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	return Vd;
}

//======================================================================================================================================================================
// serialize vd
//======================================================================================================================================================================

TValueDataPtr ASandboxTerrainController::SerializeVd(TVoxelData* Vd) const {
	TValueDataPtr Data = Vd->serialize();
	size_t DataSize = Data->size();
	
	size_t TTT = sizeof(TVoxelDataHeader) + sizeof(uint32);
	if (DataSize > TTT) {
		TValueDataPtr CompressedData = Compress(Data);
		return CompressedData;
	}

	return Data;
}

void ASandboxTerrainController::DeserializeVd(TValueDataPtr Data, TVoxelData* Vd) const {
	size_t TTT = sizeof(TVoxelDataHeader) + sizeof(uint32);
	if (Data->size() > TTT) {
		auto DecompressedDataPtr = Decompress(Data);
		deserializeVoxelData(Vd, *DecompressedDataPtr);
	} else {
		deserializeVoxelData(Vd, *Data);
	}
}

//======================================================================================================================================================================
// kv file
//======================================================================================================================================================================

bool OpenKvFile(kvdb::KvFile<TVoxelIndex, TValueData>& KvFile, const FString& FileName, const FString& SaveDir) {
	FString FullPath = SaveDir + FileName;
	std::string FilePathString = std::string(TCHAR_TO_UTF8(*FullPath));

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath)) {
		kvdb::KvFile<TVoxelIndex, TValueData>::create(FilePathString, std::unordered_map<TVoxelIndex, TValueData>());// create new empty file
	}

	if (!KvFile.open(FilePathString)) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("Unable to open file: %s"), *FullPath);
		return false;
	}

	return true;
}

bool CheckSaveDir(FString SaveDir) {
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDir)) {
		PlatformFile.CreateDirectory(*SaveDir);
		if (!PlatformFile.DirectoryExists(*SaveDir)) {
			UE_LOG(LogSandboxTerrain, Log, TEXT("Unable to create save directory -> %s"), *SaveDir);
			return false;
		}
	}

	return true;
}

FString ASandboxTerrainController::GetSaveDir() {
	FString SaveDir = FPaths::ProjectSavedDir() + TEXT("/Map/") + MapName + TEXT("/");
	if (GetNetMode() == NM_Client) {
		SaveDir = SaveDir + TEXT("/ClientCache/");
	}

	return SaveDir;
}

bool ASandboxTerrainController::OpenFile() {
	// open vd file 	
	FString FileNameTd = TEXT("terrain.dat");
	FString FileNameVd = TEXT("terrain_voxeldata.dat");
	FString FileNameObj = TEXT("terrain_objects.dat");


	FString SaveDir = GetSaveDir();
	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *SaveDir);

	if (!CheckSaveDir(SaveDir)) {
		return false;
	}

	if (!OpenKvFile(TdFile, FileNameTd, SaveDir)) {
		return false;
	}

	if (!OpenKvFile(VdFile, FileNameVd, SaveDir)) {
		return false;
	}

	if (!OpenKvFile(ObjFile, FileNameObj, SaveDir)) {
		return false;
	}

	return true;
}

void ASandboxTerrainController::CloseFile() {
	TdFile.close();
	VdFile.close();
	ObjFile.close();
}

//======================================================================================================================================================================
// save
//======================================================================================================================================================================

uint32 SaveZoneToFile(TKvFile& TerrainDataFile, TKvFile& VoxelDataFile, TKvFile& ObjDataFile, const TVoxelIndex& Index, const TValueDataPtr DataVd, const TValueDataPtr DataMd, const TValueDataPtr DataObj) {
	TKvFileZodeData ZoneHeader;
	if (!DataVd) {
		ZoneHeader.SetFlag((int)TZoneFlag::NoVoxelData);
	}

	if (DataMd) {
		ZoneHeader.LenMd = DataMd->size();
	} else {
		ZoneHeader.SetFlag((int)TZoneFlag::NoMesh);
	}

	usbt::TFastUnsafeSerializer ZoneSerializer;
	ZoneSerializer << ZoneHeader;

	if (DataMd) {
		ZoneSerializer.write(DataMd->data(), DataMd->size());
	}

	auto DataPtr = ZoneSerializer.data();

	uint32 CRC = 0;
	//uint32 CRC = CRC32__(DataPtr->data(), DataPtr->size());

	TerrainDataFile.save(Index, *DataPtr);

	if (DataVd) {
		VoxelDataFile.save(Index, *DataVd);
	}

	if (DataObj) {
		ObjDataFile.save(Index, *DataObj);
	}

	return CRC;
}

// TODO finish. use it in level generator 
void ASandboxTerrainController::ForceSave(const TVoxelIndex& ZoneIndex, TVoxelData* Vd, TMeshDataPtr MeshDataPtr, const TInstanceMeshTypeMap& InstanceObjectMap) {
	TValueDataPtr DataVd = nullptr;
	TValueDataPtr DataMd = nullptr;
	TValueDataPtr DataObj = nullptr;

	if (Vd) {
		DataVd = SerializeVd(Vd);
	}

	if (MeshDataPtr) {
		DataMd = SerializeMeshData(MeshDataPtr);
	}

	if (InstanceObjectMap.Num() > 0) {
		DataObj = UTerrainZoneComponent::SerializeInstancedMesh(InstanceObjectMap);
	}

	//SaveZoneToFile(TdFile, ZoneIndex, DataVd, DataMd, DataObj);
}

void ASandboxTerrainController::Save(std::function<void(uint32, uint32)> OnProgress, std::function<void(uint32)> OnFinish) {
	const std::lock_guard<std::mutex> lock(SaveMutex);

	if (!TdFile.isOpen() || !VdFile.isOpen() || !ObjFile.isOpen()) {
		// TODO error message
		return;
	}

	double Start = FPlatformTime::Seconds();

	uint32 SavedCount = 0;
	std::unordered_set<TVoxelIndex> SaveIndexSet = TerrainData->PopSaveIndexSet();
	uint32 Total = (uint32)SaveIndexSet.size();
	for (const TVoxelIndex& Index : SaveIndexSet) {
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

		TValueDataPtr DataVd = nullptr;
		TValueDataPtr DataMd = nullptr;
		TValueDataPtr DataObj = nullptr;

		VdInfoPtr->Lock();
		bool bSave = false;

		if (VdInfoPtr->IsNeedTerrainSave()) {
			if (VdInfoPtr->Vd && VdInfoPtr->CanSaveVd()) {
				DataVd = SerializeVd(VdInfoPtr->Vd);
			}

			auto MeshDataPtr = VdInfoPtr->PopMeshDataCache();
			if (MeshDataPtr) {
				DataMd = SerializeMeshData(MeshDataPtr);
			} else {
				if (VdInfoPtr->Vd && VdInfoPtr->Vd->getDensityFillState() == MIXED)
					UE_LOG(LogSandboxTerrain, Error, TEXT("PopMeshDataCache fail -> %d %d %d"), Index.X, Index.Y, Index.Z);
			}

			if (FoliageDataAsset) {
				UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
				if (Zone) {
					// IsNeedTerrainSave means zone was changed or generated therefore we not need to load mesh data 
					DataObj = Zone->SerializeAndResetObjectData();
				}
			}

			VdInfoPtr->ResetNeedTerrainSave();
			VdInfoPtr->ResetNeedObjectsSave();
			bSave = true;
		} else if (VdInfoPtr->IsNeedObjectsSave()) {
			if (FoliageDataAsset) {
				UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
				if (Zone) {
					DataObj = Zone->SerializeAndResetObjectData();
					ObjFile.save(Index, *DataObj); // save objects only
				} 
				// legacy
				/*else {
					auto InstanceObjectMapPtr = VdInfoPtr->GetOrCreateInstanceObjectMap();
					if (InstanceObjectMapPtr) {
						DataObj = UTerrainZoneComponent::SerializeInstancedMesh(*InstanceObjectMapPtr);
					}
				}*/
			}

			VdInfoPtr->ResetNeedObjectsSave();
		}

		if (bSave) {
			uint32 CRC = SaveZoneToFile(TdFile, VdFile, ObjFile, Index, DataVd, DataMd, DataObj);
		}

		SavedCount++;
		VdInfoPtr->ResetLastSave();

		if (OnProgress) {
			OnProgress(SavedCount, Total);
		}

		VdInfoPtr->Unload();
		VdInfoPtr->Unlock();
	}

	SaveJson();

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Save terrain data: %d zones saved -> %f ms "), SavedCount, Time);

	if (OnFinish) {
		OnFinish(SavedCount);
	}
}

//======================================================================================================================================================================
// json
//======================================================================================================================================================================

void ASandboxTerrainController::SaveJson() {
	MapInfo.SaveTimestamp = FPlatformTime::Seconds();
	FString JsonStr;

	FString FileName = TEXT("terrain.json");
	FString SaveDir = GetSaveDir();
	FString FullPath = SaveDir + TEXT("/") + FileName;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Save terrain json..."));
	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *FullPath);

	FJsonObjectConverter::UStructToJsonObjectString(MapInfo, JsonStr);
	FFileHelper::SaveStringToFile(*JsonStr, *FullPath);
}

bool ASandboxTerrainController::LoadJson() {
	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Load terrain json..."));
	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *FullPath);

	FString JsonRaw;
	if (!FFileHelper::LoadFileToString(JsonRaw, *FullPath, FFileHelper::EHashOptions::None)) {
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Error loading json file"));
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonRaw, &MapInfo, 0, 0)) {
		UE_LOG(LogSandboxTerrain, Error, TEXT("Error parsing json file"));
		return false;
	}

	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *JsonRaw);
	return true;
}

//======================================================================================================================================================================
// metadata
//======================================================================================================================================================================

void ASandboxTerrainController::SaveTerrainMetadata() {
	FString FileName = TEXT("terrain_meta.dat");
	FString SaveDir = GetSaveDir();
	FString FullPath = SaveDir + TEXT("/") + FileName;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Save terrain metadata..."));
	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *FullPath);

	FBufferArchive Buffer;

	static int32 Version = 0;
	int32 Size = ModifiedVdMap.Num();

	Buffer << Version;
	Buffer << Size;

	for (auto& Elem : ModifiedVdMap) {
		TVoxelIndex Index = Elem.Key;
		TZoneModificationData M = Elem.Value;

		Buffer << Index.X;
		Buffer << Index.Y;
		Buffer << Index.Z;
		Buffer << M.ChangeCounter;
	}

	if (FFileHelper::SaveArrayToFile(Buffer, *FullPath)) {
		// Free Binary Array 	
		Buffer.FlushCache();
		Buffer.Empty();
	}
}

void ASandboxTerrainController::LoadTerrainMetadata() {
	FString FileName = TEXT("terrain_meta.dat");
	FString SaveDir = GetSaveDir();
	FString FullPath = SaveDir + TEXT("/") + FileName;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Load terrain metadata..."));
	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *FullPath);

	ModifiedVdMap.Empty();

	TArray<uint8> Data;
	if (FFileHelper::LoadFileToArray(Data, *FullPath)) {
		if (Data.Num() > 0) {
			FMemoryReader Buffer = FMemoryReader(Data, true); //true, free data after done
			Buffer.Seek(0);

			int32 Version = 0;
			int32 Size = 0;

			Buffer << Version;
			Buffer << Size;

			for (int32 I = 0; I < Size; I++) {
				TVoxelIndex Index;
				TZoneModificationData M;

				Buffer << Index.X;
				Buffer << Index.Y;
				Buffer << Index.Z;
				Buffer << M.ChangeCounter;

				ModifiedVdMap.Add(Index, M);
			}

			Buffer.FlushCache();
			Data.Empty();
			Buffer.Close();

			return;
		}
	}

	UE_LOG(LogSandboxTerrain, Warning, TEXT("Unable to load metadata!"));
}

//======================================================================================================================================================================
// inst. meshes
//======================================================================================================================================================================

void ASandboxTerrainController::DeserializeInstancedMeshes(std::vector<uint8>& Data, TInstanceMeshTypeMap& ZoneInstMeshMap) const {
	usbt::TFastUnsafeDeserializer Deserializer(Data.data());

	int32 MeshCount;
	Deserializer >> MeshCount;

	for (int Idx = 0; Idx < MeshCount; Idx++) {
		uint32 MeshTypeId;
		uint32 MeshVariantId;
		int32 MeshInstanceCount;

		Deserializer >> MeshTypeId;
		Deserializer >> MeshVariantId;
		Deserializer >> MeshInstanceCount;

		FTerrainInstancedMeshType MeshType;
		if (FoliageMap.Contains(MeshTypeId)) {
			FSandboxFoliage FoliageType = FoliageMap[MeshTypeId];
			if ((uint32)FoliageType.MeshVariants.Num() > MeshVariantId) {
				MeshType.Mesh = FoliageType.MeshVariants[MeshVariantId];
				MeshType.MeshTypeId = MeshTypeId;
				MeshType.MeshVariantId = MeshVariantId;
				MeshType.StartCullDistance = FoliageType.StartCullDistance;
				MeshType.EndCullDistance = FoliageType.EndCullDistance;
			}
		} else {
			const FTerrainInstancedMeshType* MeshTypePtr = GetInstancedMeshType(MeshTypeId, MeshVariantId);
			if (MeshTypePtr) {
				MeshType = *MeshTypePtr;
			}
		}

		TInstanceMeshArray& InstMeshArray = ZoneInstMeshMap.FindOrAdd(MeshType.GetMeshTypeCode());
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