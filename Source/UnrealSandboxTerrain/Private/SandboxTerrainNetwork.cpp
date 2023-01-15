
#include "SandboxTerrainController.h"
#include "VoxelDataInfo.hpp"
#include "TerrainZoneComponent.h"
#include "TerrainData.hpp"
#include "TerrainClientComponent.h"


bool IsGameShutdown();

void AppendDataToBuffer(TValueDataPtr Data, FBufferArchive& Buffer) {
	for (int I = 0; I < Data->size(); I++) {
		uint8 Byte = Data->at(I);
		Buffer << Byte;
	}
}

void ASandboxTerrainController::NetworkSerializeZone(FBufferArchive& Buffer, const TVoxelIndex& Index) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
	// TODO: shared lock Vd
	VdInfoPtr->Lock();

	int32 State = (int32)VdInfoPtr->DataState;
	Buffer << State;

	TValueDataPtr Data = nullptr;
	if (VdInfoPtr->DataState == TVoxelDataState::READY_TO_LOAD) {
		TVoxelData* Vd = LoadVoxelDataByIndex(Index);
		Data = SerializeVd(Vd);
		delete Vd;
	} else if (VdInfoPtr->DataState == TVoxelDataState::LOADED) {
		Data = SerializeVd(VdInfoPtr->Vd);
	} 

	int32 Size = (Data == nullptr) ? 0 : Data->size();
	Buffer << Size;
	if (Size > 0) {
		AppendDataToBuffer(Data, Buffer);
	}

	TValueDataPtr DataObj = nullptr;
	UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
	if (Zone) {
		DataObj = Zone->SerializeAndResetObjectData();
	} else {
		auto InstanceObjectMapPtr = VdInfoPtr->GetOrCreateInstanceObjectMap();
		if (InstanceObjectMapPtr) {
			DataObj = UTerrainZoneComponent::SerializeInstancedMesh(*InstanceObjectMapPtr);
		}
	}

	int32 Size2 = (DataObj == nullptr) ? 0 : DataObj->size();
	UE_LOG(LogSandboxTerrain, Warning, TEXT("Server: obj %d %d %d -> %d"), Index.X, Index.Y, Index.Z, Size2);
	Buffer << Size2;
	if (Size2 > 0) {
		AppendDataToBuffer(DataObj, Buffer);
	}
	
	VdInfoPtr->Unlock();
}

TValueDataPtr Decompress(TValueDataPtr CompressedDataPtr);

// spawn received zone on client
void ASandboxTerrainController::NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData) {
	FVector Pos = GetZonePos(Index);

	FMemoryReader BinaryData = FMemoryReader(RawVdData, true);
	BinaryData.Seek(RawVdData.Tell());

	int32 State;
	BinaryData << State;

	TVoxelDataState ServerVdState = (TVoxelDataState)State;
	if (ServerVdState == TVoxelDataState::UNGENERATED) {
		TArray<TSpawnZoneParam> SpawnList;
		TSpawnZoneParam SpawnZoneParam;
		SpawnZoneParam.Index = Index;
		SpawnZoneParam.TerrainLodMask = 0;
		SpawnList.Add(SpawnZoneParam);
		BatchSpawnZone(SpawnList);
	}

	if (ServerVdState == TVoxelDataState::READY_TO_LOAD || ServerVdState == TVoxelDataState::LOADED) {
		int32 Size;
		BinaryData << Size;

		if(Size > 0){
			TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
			VdInfoPtr->Lock();

			TValueDataPtr DataPtr = TValueDataPtr(new TValueData);
			for (int I = 0; I < Size; I++) {
				uint8 Byte;
				BinaryData << Byte;
				DataPtr->push_back(Byte);
			}

			TVoxelData* Vd = NewVoxelData();
			Vd->setOrigin(GetZonePos(Index));
			DeserializeVd(DataPtr, Vd);

			VdInfoPtr->Vd = Vd;
			VdInfoPtr->DataState = TVoxelDataState::GENERATED;
			VdInfoPtr->SetChanged();

			TMeshDataPtr MeshDataPtr = nullptr;
			if (VdInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
				MeshDataPtr = GenerateMesh(Vd);
				TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
				//ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr, 0, true);
			}

			int32 SizeObj;
			BinaryData << SizeObj;
			TInstanceMeshTypeMap ZoneInstanceMeshMap;
			UE_LOG(LogSandboxTerrain, Warning, TEXT("Client: obj %d %d %d -> %d"), Index.X, Index.Y, Index.Z, SizeObj);

			if (SizeObj > 0) {
				TValueData ObjData;
				for (int I = 0; I < SizeObj; I++) {
					uint8 Byte;
					BinaryData << Byte;
					ObjData.push_back(Byte);
				}

				DeserializeInstancedMeshes(ObjData, ZoneInstanceMeshMap);
			}

			FVector ZonePos = GetZonePos(Index);
			ASandboxTerrainController* Controller = this;

			TFunction<void()> Function = [=]() {
				if (!IsGameShutdown()) {
					if (MeshDataPtr) {
						UTerrainZoneComponent* Zone = AddTerrainZone(ZonePos);
						if (Zone) {
							Zone->ApplyTerrainMesh(MeshDataPtr, 0);
							Zone->SpawnAll(ZoneInstanceMeshMap);
						}
					}
				}
			};

			InvokeSafe(Function);

			VdInfoPtr->Unlock();
		}
	}


	/*
	TVoxelDataInfo VdInfo;
	VdInfo.Vd = NewVoxelData();
	VdInfo.Vd->setOrigin(Pos);

	FMemoryReader BinaryData = FMemoryReader(RawVdData, true);
	BinaryData.Seek(RawVdData.Tell());
	//deserializeVoxelDataFast(*VdInfo.Vd, BinaryData, true);

	VdInfo.DataState = TVoxelDataState::GENERATED;
	VdInfo.SetChanged();
	VdInfo.Vd->setCacheToValid();

	//TerrainData->RegisterVoxelData(VdInfo, Index);
	//TerrainData->RegisterVoxelData(VdInfo, Index);

	if (VdInfo.Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
		TMeshDataPtr MeshDataPtr = GenerateMesh(VdInfo.Vd);
		
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			//Zone->ApplyTerrainMesh(MeshDataPtr);
		});
		
	}
	*/

}

std::list<TChunkIndex> ReverseSpiralWalkthrough(const unsigned int r);

TArray<std::tuple<TVoxelIndex, TZoneModificationData>> ASandboxTerrainController::NetworkServerMapInfo() {
	// TODO reserve space 
	TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Result;

	for (auto Itm : ModifiedVdMap) {
		TVoxelIndex Index = Itm.Key;
		TZoneModificationData Mdata = Itm.Value;
		std::tuple<TVoxelIndex, TZoneModificationData> Entry = std::make_tuple(Index, Mdata);
		Result.Add(Entry);
	}
	
	return Result;
}

void ASandboxTerrainController::OnClientConnected() {
	FString Text = TEXT("Connected to voxel data server");

	GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Yellow, Text);
	UE_LOG(LogSandboxTerrain, Warning, TEXT("%s"), *Text);

	TerrainClientComponent->RequestMapInfo();
}

void ASandboxTerrainController::OnReceiveServerMapInfo(const TMap<TVoxelIndex, TZoneModificationData>& ServerDataMap) {
	const std::lock_guard<std::mutex> Lock(ModifiedVdMapMutex);

	TSet<TVoxelIndex> OutOfsyncZones;

	for (const auto& Itm : ServerDataMap) {
		const TVoxelIndex& Index = Itm.Key;
		const TZoneModificationData& Remote = Itm.Value;

		/*
		if (ModifiedVdMap.Contains(Index)) {
			TZoneModificationData Local = ModifiedVdMap[Index];
			if (Local.ChangeCounter != Remote.ChangeCounter) {
				OutOfsyncZones.Add(Index);
			}
		} else {
			OutOfsyncZones.Add(Index);
		}
		*/
		// TODO: client cache

		OutOfsyncZones.Add(Index);

	}

	ModifiedVdMap.Empty();
	ModifiedVdMap = ServerDataMap;

	for (const auto& Index : OutOfsyncZones) {
		TerrainClientComponent->RequestVoxelData(Index);
	}

	TVoxelIndex Index(0, 0, 0);
	BeginClientTerrainLoad(Index, OutOfsyncZones);
}