
#include "SandboxTerrainController.h"
#include "Core/VoxelDataInfo.hpp"
#include "TerrainZoneComponent.h"
#include "Core/TerrainData.hpp"
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

	int32 VStamp = TerrainData->GetZoneVStamp(Index).VStamp;
	Buffer << VStamp;

	TValueDataPtr Data = nullptr;
	TValueDataPtr DataObj = nullptr;

	if (VdInfoPtr->DataState == TVoxelDataState::READY_TO_LOAD) {
		TVoxelData* Vd = LoadVoxelDataByIndex(Index);
		Data = SerializeVd(Vd);
		delete Vd;

		double Start = FPlatformTime::Seconds();

		TMeshDataPtr MeshDataPtr = nullptr;
		TInstanceMeshTypeMap InstMeshes;
		LoadMeshAndObjectDataByIndex(Index, MeshDataPtr, InstMeshes);
		DataObj = UTerrainZoneComponent::SerializeInstancedMesh(InstMeshes);

		double End = FPlatformTime::Seconds();
		double Time = (End - Start) * 1000;

		UE_LOG(LogVt, Log, TEXT("Loading mesh and objects data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);

	} else if (VdInfoPtr->DataState == TVoxelDataState::LOADED || VdInfoPtr->DataState == TVoxelDataState::GENERATED) {
		Data = SerializeVd(VdInfoPtr->Vd);

		UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
		if (Zone) {
			DataObj = Zone->SerializeAndResetObjectData();
		} else {
			auto InstanceObjectMapPtr = VdInfoPtr->GetOrCreateInstanceObjectMap();
			if (InstanceObjectMapPtr) {
				DataObj = UTerrainZoneComponent::SerializeInstancedMesh(*InstanceObjectMapPtr);
			}
		}
	} 

	int32 Size = (Data == nullptr) ? 0 : Data->size();
	Buffer << Size;
	if (Size > 0) {
		AppendDataToBuffer(Data, Buffer);
	}
	
	int32 Size2 = (DataObj == nullptr) ? 0 : DataObj->size();

	//UE_LOG(LogVt, Warning, TEXT("Server: vd %d %d %d -> %d"), Index.X, Index.Y, Index.Z, Size);
	//UE_LOG(LogVt, Warning, TEXT("Server: obj %d %d %d -> %d"), Index.X, Index.Y, Index.Z, Size2);

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

	//FMemoryReader BinaryData = FMemoryReader(RawVdData, true);
	//BinaryData.Seek(RawVdData.Tell());

	FArrayReader& BinaryData = RawVdData;

	int32 State;
	BinaryData << State;

	int32 VStamp;
	BinaryData << VStamp;

	UE_LOG(LogVt, Warning, TEXT("NetworkSpawnClientZone: %d %d %d VStamp = %d"), Index.X, Index.Y, Index.Z, VStamp);

	//UE_LOG(LogVt, Warning, TEXT("NetworkSpawnClientZone: %d %d %d remote state = %d"), Index.X, Index.Y, Index.Z, State);

	TVoxelDataState ServerVdState = (TVoxelDataState)State;
	if (ServerVdState == TVoxelDataState::READY_TO_LOAD || ServerVdState == TVoxelDataState::LOADED) {
		int32 Size;
		BinaryData << Size;

		//UE_LOG(LogVt, Warning, TEXT("NetworkSpawnClientZone: BinaryData %d "), Size);

		if(Size > 0) {
			TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
			VdInfoPtr->Lock();

			TerrainData->SetZoneVStamp(Index, VStamp);

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
			//UE_LOG(LogVt, Warning, TEXT("Client: obj %d %d %d -> %d"), Index.X, Index.Y, Index.Z, SizeObj);

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
			TerrainData->RemoveSyncItem(Index);
			VdInfoPtr->Unlock();
		}
	}
}

std::list<TChunkIndex> ReverseSpiralWalkthrough(const unsigned int r);

TArray<std::tuple<TVoxelIndex, TZoneModificationData>> ASandboxTerrainController::NetworkServerMapInfo() {
	// TODO reserve space 
	TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Result;

	auto Vm = TerrainData->CloneVStampMap();

	for (auto Itm : Vm) {
		TVoxelIndex Index = Itm.Key;
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

		// skip without real changes
		if (VdInfoPtr->DataState == TVoxelDataState::LOADED || VdInfoPtr->DataState == TVoxelDataState::READY_TO_LOAD) {
			TZoneModificationData Mdata = Itm.Value;
			std::tuple<TVoxelIndex, TZoneModificationData> Entry = std::make_tuple(Index, Mdata);
			Result.Add(Entry);
		}
	}
	
	return Result;
}

void ASandboxTerrainController::OnReceiveServerMapInfo(const TMap<TVoxelIndex, TZoneModificationData>& ServerDataMap) {
	UE_LOG(LogVt, Warning, TEXT("Client: OnReceiveServerMapInfo "));

	auto Vm = TerrainData->CloneVStampMap();

	TSet<TVoxelIndex> OutOfsyncZones;
	for (const auto& Itm : ServerDataMap) {
		const TVoxelIndex& Index = Itm.Key;
		const TZoneModificationData& Remote = Itm.Value;

		if (bInitialLoad) {
			OutOfsyncZones.Add(Index);
			continue;
		}

		if (Vm.Contains(Index) && Vm[Index].VStamp == Remote.VStamp) {
			continue;
		} else {
			OutOfsyncZones.Add(Index);

			if (Vm.Contains(Index)) {
				UE_LOG(LogVt, Warning, TEXT("Client: %d %d %d local = %d, remote = %d"), Index.X, Index.Y, Index.Z, Vm[Index].VStamp, Remote.VStamp);
			} else {
				UE_LOG(LogVt, Warning, TEXT("Client: %d %d %d remote = %d"), Index.X, Index.Y, Index.Z, Remote.VStamp);
			}
		}
	}

	TerrainData->AddSyncItem(OutOfsyncZones);

	//TerrainData->SwapVStampMap(ServerDataMap);

	for (const auto& Index : OutOfsyncZones) {
		UE_LOG(LogVt, Warning, TEXT("Client: RequestVoxelData %d %d %d "), Index.X, Index.Y, Index.Z);
		TerrainClientComponent->RequestVoxelData(Index);
	}

	if (bInitialLoad) {
		//FIXME
		TVoxelIndex Index(0, 0, 0);
		BeginClientTerrainLoad(Index);
	}
}

void ASandboxTerrainController::PingServer() {
	TerrainClientComponent->RequestMapInfoIfStaled();

	const auto IndexSet = TerrainData->StaledSyncItems(1);
	if (IndexSet.size() > 0) {
		UE_LOG(LogVt, Warning, TEXT("Client: staled sync items: %d"), IndexSet.size());

		for (const auto& Index : IndexSet) {
			TerrainClientComponent->RequestVoxelData(Index);
		}
	}

}