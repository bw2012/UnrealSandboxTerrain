
#include "SandboxTerrainController.h"

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

	//float CompressionRatio = (CompressedDataPtr->size() / DecompressedData.Num()) * 100.f;
	//UE_LOG(LogSandboxTerrain, Log, TEXT("CompressedData -> %d bytes -> %f%%"), DecompressedData.Num(), CompressionRatio);

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

TMeshDataPtr ASandboxTerrainController::LoadMeshDataByIndex(const TVoxelIndex& Index) {
	double Start = FPlatformTime::Seconds();
	TMeshDataPtr MeshDataPtr = nullptr;

	bool bIsLoaded = LoadDataFromKvFile(TdFile, Index, [&](TValueDataPtr DataPtr) {
		usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
		TKvFileZodeData ZoneHeader;
		Deserializer >> ZoneHeader;

		if (ZoneHeader.LenMd > 0) {
			TValueDataPtr CompressedMdPtr = std::make_shared<TValueData>();
			CompressedMdPtr->resize(ZoneHeader.LenMd);
			Deserializer.read(CompressedMdPtr->data(), ZoneHeader.LenMd);
			auto DecompressedDataPtr = Decompress(CompressedMdPtr);
			MeshDataPtr = DeserializeMeshDataFast(*DecompressedDataPtr, GetCollisionMeshSectionLodIndex());
		} 
	});

	//bIsLoaded = LoadDataFromKvFile(MdFile, Index, [&](TValueDataPtr DataPtr) {
		//auto DecompressedDataPtr = Decompress(DataPtr);
		//MeshDataPtr = DeserializeMeshDataFast(*DecompressedDataPtr, GetCollisionMeshSectionLodIndex());
	//});

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	if (bIsLoaded) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("loading mesh data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	}

	return MeshDataPtr;
}

void ASandboxTerrainController::LoadObjectDataByIndex(UTerrainZoneComponent* Zone, TInstanceMeshTypeMap& ZoneInstMeshMap) {
	double Start = FPlatformTime::Seconds();
	TVoxelIndex Index = GetZoneIndex(Zone->GetComponentLocation());

	bool bIsLoaded = LoadDataFromKvFile(TdFile, Index, [&](TValueDataPtr DataPtr) {

		usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
		TKvFileZodeData ZoneHeader;
		Deserializer >> ZoneHeader;

		auto CompressedMdPtr = std::make_shared<TValueData>();
		CompressedMdPtr->resize(ZoneHeader.LenMd);
		Deserializer.read(CompressedMdPtr->data(), ZoneHeader.LenMd);

		auto VdPtr = std::make_shared<TValueData>();
		VdPtr->resize(ZoneHeader.LenVd);
		Deserializer.read(VdPtr->data(), ZoneHeader.LenVd);

		if (ZoneHeader.LenObj > 0) {
			auto ObjDataPtr = std::make_shared<TValueData>();
			ObjDataPtr->resize(ZoneHeader.LenObj);
			Deserializer.read(ObjDataPtr->data(), ZoneHeader.LenObj);

			Zone->DeserializeInstancedMeshes(*ObjDataPtr, ZoneInstMeshMap);
		}
	});

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	if (bIsLoaded) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("loading inst-objects data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	}
}

//======================================================================================================================================================================
// load vd
//======================================================================================================================================================================

// TODO: use shared_ptr
TVoxelData* ASandboxTerrainController::LoadVoxelDataByIndex(const TVoxelIndex& Index) {
	double Start = FPlatformTime::Seconds();

	TVoxelData* Vd = NewVoxelData();
	Vd->setOrigin(GetZonePos(Index));

	TValueDataPtr DataPtr = LoadDataFromKvFile2(TdFile, Index);
	if (DataPtr) {
		usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
		TKvFileZodeData ZoneHeader;
		Deserializer >> ZoneHeader;

		auto CompressedMdPtr = std::make_shared<TValueData>();
		CompressedMdPtr->resize(ZoneHeader.LenMd);
		Deserializer.read(CompressedMdPtr->data(), ZoneHeader.LenMd);

		if (ZoneHeader.LenVd > 0) {
			auto VdPtr = std::make_shared<TValueData>();
			VdPtr->resize(ZoneHeader.LenVd);
			Deserializer.read(VdPtr->data(), ZoneHeader.LenVd);

			size_t TTT = sizeof(TVoxelDataHeader) + sizeof(uint32);
			if (VdPtr->size() > TTT) {
				auto DecompressedDataPtr = Decompress(VdPtr);
				deserializeVoxelData(Vd, *DecompressedDataPtr);
			} else {
				deserializeVoxelData(Vd, *VdPtr);
			}
		} else {
			UE_LOG(LogSandboxTerrain, Warning, TEXT("no vd"));
			return nullptr;
		}
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("loading voxel data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	return Vd;
}

//======================================================================================================================================================================
// save vd
//======================================================================================================================================================================

TValueDataPtr ASandboxTerrainController::SerializeVd(TVoxelData* Vd) {
	TValueDataPtr Data = Vd->serialize();
	size_t DataSize = Data->size();
	
	size_t TTT = sizeof(TVoxelDataHeader) + sizeof(uint32);
	if (DataSize > TTT) {
		TValueDataPtr CompressedData = Compress(Data);
		//UE_LOG(LogSandboxTerrain, Log, TEXT("SerializeVd -> %d compressed bytes "), CompressedData->size());
		return CompressedData;
	}

	return Data;
}