
#include "SandboxTerrainController.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "Async/Async.h"
#include "Json.h"
#include "JsonObjectConverter.h"
#include "DrawDebugHelpers.h"
#include "kvdb.hpp"

#include "TerrainZoneComponent.h"
#include "VoxelMeshComponent.h"

#include <cmath>
#include <list>

#include "serialization.hpp"
#include "utils.hpp"
#include "VoxelDataInfo.hpp"
#include "TerrainData.hpp"
#include "TerrainAreaPipeline.hpp"
#include "TerrainEdit.hpp"

#include "memstat.h"



//# define USBT_DEBUG_ZONE_CRC 1


TValueDataPtr SerializeMeshData(TMeshDataPtr MeshDataPtr);
//bool CheckSaveDir(FString SaveDir);

//FIXME 
bool bIsGameShutdown;


//======================================================================================================================================================================
// Terrain Controller
//======================================================================================================================================================================


void ASandboxTerrainController::InitializeTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	SaveGeneratedZones = 1000;
	ServerPort = 6000;
    AutoSavePeriod = 20;
    TerrainData = new TTerrainData();
    CheckAreaMap = new TCheckAreaMap();
	bSaveOnEndPlay = true;
	BeginTerrainLoadLocation = FVector(0);
	bSaveAfterInitialLoad = false;

    //FString SaveDir = FPaths::ProjectSavedDir() + TEXT("/Map/") + MapName + TEXT("/");
    //CheckSaveDir(SaveDir);
}

void ASandboxTerrainController::BeginDestroy() {
    Super::BeginDestroy();
	bIsGameShutdown = true;
}

void ASandboxTerrainController::FinishDestroy() {
	Super::FinishDestroy();
	UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::FinishDestroy()"));
	delete TerrainData;
	delete CheckAreaMap;
	UE_LOG(LogSandboxTerrain, Warning, TEXT("vd -> %d, md -> %d, cd -> %d"), vd::tools::memory::getVdCount(), md_counter.load(), cd_counter.load());
}

ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	InitializeTerrainController();
}

ASandboxTerrainController::ASandboxTerrainController() {
	InitializeTerrainController();
}

void ASandboxTerrainController::PostLoad() {
	Super::PostLoad();
	UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::PostLoad()"));

	bIsGameShutdown = false;

#if WITH_EDITOR
	//spawnInitialZone();
#endif

}

UTerrainGeneratorComponent* ASandboxTerrainController::NewTerrainGenerator() {
	return NewObject<UTerrainGeneratorComponent>(this, TEXT("TerrainGenerator"));
}

UTerrainGeneratorComponent* ASandboxTerrainController::GetTerrainGenerator() {
	return this->GeneratorComponent;
}

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();
	UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::BeginPlay()"));

	bIsGameShutdown = false;
    
    GlobalTerrainZoneLOD[0] = 0;
    GlobalTerrainZoneLOD[1] = LodDistance.Distance1;
    GlobalTerrainZoneLOD[2] = LodDistance.Distance2;
    GlobalTerrainZoneLOD[3] = LodDistance.Distance3;
    GlobalTerrainZoneLOD[4] = LodDistance.Distance4;
    GlobalTerrainZoneLOD[5] = LodDistance.Distance5;
    GlobalTerrainZoneLOD[6] = LodDistance.Distance6;

	FoliageMap.Empty();
	if (FoliageDataAsset) {
		FoliageMap = FoliageDataAsset->FoliageMap;
	}

	InstMeshMap.Empty();
	MaterialMap.Empty();
	if (TerrainParameters) {
		MaterialMap = TerrainParameters->MaterialMap;

		for (const auto& InstMesh : TerrainParameters->InstanceMeshes) {
			uint64 MeshTypeCode = InstMesh.GetMeshTypeCode();

			UE_LOG(LogSandboxTerrain, Log, TEXT("TEST -> %lld"), MeshTypeCode);

			InstMeshMap.Add(MeshTypeCode, InstMesh);
		}
	}

	if (!GetWorld()) return;
	bIsLoadFinished = false;

	if (GetWorld()->IsServer()) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("SERVER"));
		BeginPlayServer();
	} else {
		UE_LOG(LogSandboxTerrain, Log, TEXT("CLIENT"));
		BeginClient();
	}
 
	StartCheckArea();
}

void ASandboxTerrainController::ShutdownThreads() {
	bIsWorkFinished = true;

	std::unique_lock<std::shared_timed_mutex> Lock(ThreadListMutex);
	UE_LOG(LogSandboxTerrain, Warning, TEXT("TerrainControllerEventList -> %d threads. Waiting for finish..."), TerrainControllerEventList.Num());
	for (auto& TerrainControllerEvent : TerrainControllerEventList) {
		while (!TerrainControllerEvent->IsComplete()) {};
	}
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	UE_LOG(LogSandboxTerrain, Warning, TEXT("ASandboxTerrainController::EndPlay"));

	ShutdownThreads();

	if (bSaveOnEndPlay) {
		Save();
	}

	CloseFile();
    TerrainData->Clean();
	GetTerrainGenerator()->Clean();
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	const std::lock_guard<std::mutex> Lock(ConveyorMutex);
	for (auto Idx = 0; Idx < MaxConveyorTasks; Idx++) {
		if (ConveyorList.size() > 0) {
			const auto& Function = ConveyorList.front();
			Function();
			ConveyorList.pop_front();
		}
	}


	//if (bIsGeneratingTerrain) {
	//	OnProgressBuildTerrain(GeneratingProgress);
	//}
}

//======================================================================================================================================================================
// Swapping terrain area according player position
//======================================================================================================================================================================

void ASandboxTerrainController::StartPostLoadTimers() {
	GetWorld()->GetTimerManager().SetTimer(TimerAutoSave, this, &ASandboxTerrainController::AutoSaveByTimer, AutoSavePeriod, true);
}

void ASandboxTerrainController::StartCheckArea() {
    AsyncTask(ENamedThreads::GameThread, [=]() {
        for (auto Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator) {
            APlayerController* PlayerController = Iterator->Get();
            if (PlayerController){
                const auto PlayerId = PlayerController->GetUniqueID();
                const auto Pawn = PlayerController->GetCharacter();
				if (Pawn) {
					const FVector Location = Pawn->GetActorLocation();
				}
            }
        }
        GetWorld()->GetTimerManager().SetTimer(TimerSwapArea, this, &ASandboxTerrainController::PerformCheckArea, 0.25, true);
    });
}

void ASandboxTerrainController::PerformCheckArea() {
    if(!bEnableAreaSwapping){
        return;
    }
    
    double Start = FPlatformTime::Seconds();
        
	bool bPerformSoftUnload = false;
    for (auto Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator) {
        APlayerController* PlayerController = Iterator->Get();
        if (PlayerController){
            const auto PlayerId = PlayerController->GetUniqueID();
            const auto Pawn = PlayerController->GetPawn();
			if (!Pawn) {
				continue;
			}

            const FVector PlayerLocation = Pawn->GetActorLocation();
            const FVector PrevLocation = CheckAreaMap->PlayerStreamingPosition.FindOrAdd(PlayerId);
            const float Distance = FVector::Distance(PlayerLocation, PrevLocation);
            const float Threshold = PlayerLocationThreshold;
            if(Distance > Threshold) {
                CheckAreaMap->PlayerStreamingPosition[PlayerId] = PlayerLocation;
                TVoxelIndex LocationIndex = GetZoneIndex(PlayerLocation);
                FVector Tmp = GetZonePos(LocationIndex);
                                
                if(CheckAreaMap->PlayerStreamingHandler.Contains(PlayerId)){
                    // cancel old
                    std::shared_ptr<TTerrainLoadPipeline> HandlerPtr2 = CheckAreaMap->PlayerStreamingHandler[PlayerId];
                    HandlerPtr2->Cancel();
                    CheckAreaMap->PlayerStreamingHandler.Remove(PlayerId);
                }
                
                // start new
                std::shared_ptr<TTerrainLoadPipeline> HandlerPtr = std::make_shared<TTerrainLoadPipeline>();
				CheckAreaMap->PlayerStreamingHandler.Add(PlayerId, HandlerPtr);

                if(bShowStartSwapPos){
                    DrawDebugBox(GetWorld(), PlayerLocation, FVector(100), FColor(255, 0, 255, 0), false, 15);
                    static const float Len = 1000;
                    DrawDebugCylinder(GetWorld(), FVector(Tmp.X, Tmp.Y, Len), FVector(Tmp.X, Tmp.Y, -Len), DynamicLoadArea.Radius, 128, FColor(255, 0, 255, 128), false, 30);
                }
                
				TTerrainAreaPipelineParams Params;
                Params.FullLodDistance = DynamicLoadArea.FullLodDistance;
                Params.Radius = DynamicLoadArea.Radius;
                Params.TerrainSizeMinZ = LocationIndex.Z + DynamicLoadArea.TerrainSizeMinZ;
                Params.TerrainSizeMaxZ = LocationIndex.Z + DynamicLoadArea.TerrainSizeMaxZ;
                HandlerPtr->SetParams(TEXT("Player_Swap_Terrain_Task"), this, Params);
                               
                RunThread([=]() {
                    HandlerPtr->LoadArea(PlayerLocation);
                });

				bPerformSoftUnload = true;
            }

			if (bPerformSoftUnload || bForcePerformHardUnload) {
				UnloadFarZones(PlayerLocation, DynamicLoadArea.Radius);
			}
        }
    }
    
    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogSandboxTerrain, Log, TEXT("PerformCheckArea -> %f ms"), Time);
}

void ASandboxTerrainController::ForcePerformHardUnload() {
	bForcePerformHardUnload = true;
}

void RemoveAllChilds(UTerrainZoneComponent* ZoneComponent) {
	TArray<USceneComponent*> ChildList;
	ZoneComponent->GetChildrenComponents(true, ChildList);
	for (USceneComponent* Child : ChildList) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *Child->GetName());
		//Child->SetVisibility(false, false);
		Child->DestroyComponent(true);
	}
}

void ASandboxTerrainController::UnloadFarZones(FVector PlayerLocation, float Radius) {
	UE_LOG(LogSandboxTerrain, Log, TEXT("UnloadFarZones"));

	double Start = FPlatformTime::Seconds();

	// hard unload far zones
	TArray<UTerrainZoneComponent*> Components;
	GetComponents<UTerrainZoneComponent>(Components);
	for (UTerrainZoneComponent* ZoneComponent : Components) {
		FVector ZonePos = ZoneComponent->GetComponentLocation();
		const TVoxelIndex ZoneIndex = GetZoneIndex(ZonePos);
		float ZoneDistance = FVector::Distance(ZonePos, PlayerLocation);
		if (ZoneDistance > Radius * 1.5f) {
			//DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true);
			ZoneSoftUnload(ZoneComponent, ZoneIndex);
			if (bForcePerformHardUnload) {
				ZoneHardUnload(ZoneComponent, ZoneIndex);
			}
		} else {
			if (ZoneDistance < Radius) {
				// restore soft unload
				TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(ZoneIndex);
				if (VoxelDataInfoPtr->IsSoftUnload()) {
					//DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 0), true);
					VoxelDataInfoPtr->ResetSoftUnload();
					OnRestoreZoneSoftUnload(ZoneIndex);
				}
			}
		}
	}

	if (bForcePerformHardUnload) {
		bForcePerformHardUnload = false;
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("UnloadFarZones --> %f ms"), Time);
}

void ASandboxTerrainController::ZoneHardUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex) {
	UE_LOG(LogSandboxTerrain, Log, TEXT("ZoneHardUnload"));
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	VdInfoPtr->Lock();
	if (VdInfoPtr->IsSoftUnload()) {
		if (!ZoneComponent->IsNeedSave() && ZoneComponent->bIsSpawnFinished) {
			RemoveAllChilds(ZoneComponent);
			TerrainData->RemoveZone(ZoneIndex);
			ZoneComponent->DestroyComponent(true);
		}
	}
	VdInfoPtr->Unlock();
}

void ASandboxTerrainController::ZoneSoftUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex) {
	UE_LOG(LogSandboxTerrain, Log, TEXT("ZoneSoftUnload"));
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	if (!VdInfoPtr->IsSoftUnload()) {
		// TODO: lock zone + double check locking
		bool bCanUnload = OnZoneSoftUnload(ZoneIndex);
		if (bCanUnload) {
			// soft unload
			VdInfoPtr->SetSoftUnload();
		}
	} 
}

//======================================================================================================================================================================
// begin play
//======================================================================================================================================================================

void ASandboxTerrainController::BeginPlayServer() {
	GeneratorComponent = NewTerrainGenerator();
	GeneratorComponent->RegisterComponent();

	if (!OpenFile()) {
		return;
	}

	LoadJson();

	if (bShowInitialArea) {
		static const float Len = 5000;
		DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance, 128, FColor(255, 255, 255, 0), true);
		DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance + LodDistance.Distance2, 128, FColor(255, 255, 255, 0), true);
		DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance + LodDistance.Distance5, 128, FColor(255, 255, 255, 0), true);
	}

	BeginTerrainLoad();
}

void ASandboxTerrainController::BeginTerrainLoad() {
	SpawnInitialZone();
    
    if (!bGenerateOnlySmallSpawnPoint) {
        // async loading other zones
        RunThread([&]() {
			TTerrainAreaPipelineParams Params;
            Params.FullLodDistance = InitialLoadArea.FullLodDistance;
            Params.Radius = InitialLoadArea.Radius;
            Params.TerrainSizeMinZ = InitialLoadArea.TerrainSizeMinZ;
            Params.TerrainSizeMaxZ = InitialLoadArea.TerrainSizeMaxZ;
            
			UE_LOG(LogSandboxTerrain, Warning, TEXT("Begin terrain load at location: %f %f %f"), BeginTerrainLoadLocation.X, BeginTerrainLoadLocation.Y, BeginTerrainLoadLocation.Z);

			TTerrainLoadPipeline Loader(TEXT("Initial_Load_Task"), this, Params);
            Loader.LoadArea(BeginTerrainLoadLocation);
			UE_LOG(LogSandboxTerrain, Warning, TEXT("======= Finish initial terrain load ======="));

			//GetTerrainGenerator()->Clean();

			UE_LOG(LogSandboxTerrain, Warning, TEXT("vd -> %d, md -> %d, cd -> %d"), vd::tools::memory::getVdCount(), md_counter.load(), cd_counter.load());

			if (!bIsWorkFinished) {
				if (bSaveAfterInitialLoad) {
					SaveMapAsync();
				}

				AsyncTask(ENamedThreads::GameThread, [&] {
					StartPostLoadTimers();
				});
			}
        });
    }
}

void ASandboxTerrainController::BeginClient() {
	//UVdClientComponent* VdClientComponent = NewObject<UVdClientComponent>(this, TEXT("VdClient"));
	//VdClientComponent->RegisterComponent();
	//VdClientComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
}

void ASandboxTerrainController::NetworkSerializeVd(FBufferArchive& Buffer, const TVoxelIndex& VoxelIndex) {
	/*
	TVoxelDataInfo* VoxelDataInfo = GetVoxelDataInfo(VoxelIndex);
	// TODO: shared lock Vd
	if (VoxelDataInfo) {
		if (VoxelDataInfo->DataState == TVoxelDataState::READY_TO_LOAD) {
			TVoxelData* Vd = LoadVoxelDataByIndex(VoxelIndex);
			//serializeVoxelData(*Vd, Buffer);
			delete Vd;
		} else if (VoxelDataInfo->DataState == TVoxelDataState::LOADED)  {
			//serializeVoxelData(*VoxelDataInfo->Vd, Buffer);
		}
	}
	*/
}

//======================================================================================================================================================================
// save
//======================================================================================================================================================================

uint32 SaveZoneToFile(TKvFile& File, const TVoxelIndex& Index, const TValueDataPtr DataVd, const TValueDataPtr DataMd, const TValueDataPtr DataObj) {
	TKvFileZodeData ZoneHeader;
	if (DataVd) {
		ZoneHeader.LenVd = DataVd->size();
	} else {
		ZoneHeader.SetFlag((int)TZoneFlag::NoVoxelData);
	}

	if (DataMd) {
		ZoneHeader.LenMd = DataMd->size();
	} else {
		ZoneHeader.SetFlag((int)TZoneFlag::NoMesh);
	}

	if (DataObj) {
		ZoneHeader.LenObj = DataObj->size();
	}

	usbt::TFastUnsafeSerializer ZoneSerializer;
	ZoneSerializer << ZoneHeader;

	if (DataMd) {
		ZoneSerializer.write(DataMd->data(), DataMd->size());
	}

	if (DataVd) {
		ZoneSerializer.write(DataVd->data(), DataVd->size());
	}

	if (DataObj) {
		ZoneSerializer.write(DataObj->data(), DataObj->size());
	}

	auto DataPtr = ZoneSerializer.data();

#ifdef USBT_DEBUG_ZONE_CRC
	uint32 CRC = kvdb::CRC32(DataPtr->data(), DataPtr->size());
	UE_LOG(LogSandboxTerrain, Log, TEXT("Save zone -> %d %d %d -> CRC32 = 0x%x "), Index.X, Index.Y, Index.Z, CRC);
#else
	uint32 CRC = 0;
#endif

	File.save(Index, *DataPtr);
	return CRC;
}


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

	SaveZoneToFile(TdFile, ZoneIndex, DataVd, DataMd, DataObj);
}

void ASandboxTerrainController::Save(std::function<void(uint32, uint32)> OnProgress, std::function<void(uint32)> OnFinish) {
	const std::lock_guard<std::mutex> lock(SaveMutex);

	if (!TdFile.isOpen()) {
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
		if (VdInfoPtr->IsChanged()) {

			if (VdInfoPtr->Vd && VdInfoPtr->CanSave()) {
				DataVd = SerializeVd(VdInfoPtr->Vd);
			}

			auto MeshDataPtr = VdInfoPtr->PopMeshDataCache();
			if (MeshDataPtr) {
				DataMd = SerializeMeshData(MeshDataPtr);
			}

			if (FoliageDataAsset) {
				UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
				if (Zone && Zone->IsNeedSave()) {
					DataObj = Zone->SerializeAndResetObjectData();
				} else {
					auto InstanceObjectMapPtr = VdInfoPtr->GetOrCreateInstanceObjectMap();
					if (InstanceObjectMapPtr) {
						DataObj = UTerrainZoneComponent::SerializeInstancedMesh(*InstanceObjectMapPtr);
					}
				}
			}

			uint32 CRC = SaveZoneToFile(TdFile, Index, DataVd, DataMd, DataObj);

			SavedCount++;
			VdInfoPtr->ResetLastSave();

			if (OnProgress) {
				OnProgress(SavedCount, Total);
			}
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

void ASandboxTerrainController::OnStartBackgroundSaveTerrain() {

}

void ASandboxTerrainController::OnFinishBackgroundSaveTerrain() {

}

void ASandboxTerrainController::OnProgressBackgroundSaveTerrain(float Progress) {

}

void ASandboxTerrainController::SaveMapAsync() {
	UE_LOG(LogSandboxTerrain, Log, TEXT("Start save terrain async"));
	RunThread([&]() {

		std::function<void(uint32, uint32)> OnProgress = [=](uint32 Processed, uint32 Total) {
			if (Processed % 10 == 0) {
				float Progress = (float)Processed / (float)Total;
				//UE_LOG(LogSandboxTerrain, Log, TEXT("Save terrain: %d / %d - %f%%"), Processed, Total, Progress * 100);
				OnProgressBackgroundSaveTerrain(Progress);
			}
		};

		OnStartBackgroundSaveTerrain();
		Save(OnProgress);
		bForcePerformHardUnload = true;
		OnFinishBackgroundSaveTerrain();

		UE_LOG(LogSandboxTerrain, Log, TEXT("Finish save terrain async"));
		UE_LOG(LogSandboxTerrain, Warning, TEXT("vd -> %d, md -> %d, cd -> %d"), vd::tools::memory::getVdCount(), md_counter.load(), cd_counter.load());
	});
}

void ASandboxTerrainController::AutoSaveByTimer() {
    UE_LOG(LogSandboxTerrain, Log, TEXT("Start auto save..."));
	SaveMapAsync();
}

void ASandboxTerrainController::SaveJson() {
    UE_LOG(LogSandboxTerrain, Log, TEXT("Save terrain json"));
    
	MapInfo.SaveTimestamp = FPlatformTime::Seconds();
	FString JsonStr;
    
	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	FJsonObjectConverter::UStructToJsonObjectString(MapInfo, JsonStr);
	//UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *JsonStr);
	FFileHelper::SaveStringToFile(*JsonStr, *FullPath);
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

bool CheckSaveDir(FString SaveDir){
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

bool ASandboxTerrainController::OpenFile() {
	// open vd file 	
	FString FileNameTd = TEXT("terrain.dat");
	FString FileNameVd = TEXT("terrain_voxeldata.dat");
	FString FileNameMd = TEXT("terrain_mesh.dat");
	FString FileNameObj = TEXT("terrain_objects.dat");

	FString SavePath = FPaths::ProjectSavedDir();
	FString SaveDir = SavePath + TEXT("/Map/") + MapName + TEXT("/");

	UE_LOG(LogSandboxTerrain, Log, TEXT("%s"), *SaveDir);

	if (!GetWorld()->IsServer()) {
		SaveDir = SaveDir + TEXT("/ClientCache/");
	}

    if(!CheckSaveDir(SaveDir)){
        return false;
    }

	if (!OpenKvFile(TdFile, FileNameTd, SaveDir)) {
		return false;
	}

	return true;
}

void ASandboxTerrainController::CloseFile() {
	TdFile.close();
}



//======================================================================================================================================================================
// load
//======================================================================================================================================================================

bool ASandboxTerrainController::LoadJson() {
	UE_LOG(LogSandboxTerrain, Log, TEXT("Load terrain json"));

	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

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
//  spawn zone
//======================================================================================================================================================================

// spawn received zone on client
void ASandboxTerrainController::NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData) {
	FVector Pos = GetZonePos(Index);

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
		/*
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			//Zone->ApplyTerrainMesh(MeshDataPtr);
		});
		*/
	}
}

uint32 ASandboxTerrainController::GetZoneVoxelResolution() {
	int L = LOD_ARRAY_SIZE - 1;
	int R = 1 << L;
	int RRR = R + 1;
	return RRR;
}

float ASandboxTerrainController::GetZoneSize() {
	return USBT_ZONE_SIZE;
}

TVoxelData* ASandboxTerrainController::NewVoxelData() {
	return new TVoxelData(GetZoneVoxelResolution(), GetZoneSize());
}

std::list<TChunkIndex> ASandboxTerrainController::MakeChunkListByAreaSize(const uint32 AreaRadius) {
	return ReverseSpiralWalkthrough(AreaRadius);
}

//TODO: is it used?
bool ASandboxTerrainController::IsVdExistsInFile(const TVoxelIndex& ZoneIndex) {
	if (TdFile.isOpen()) {
		return TdFile.isExist(ZoneIndex);
	}

	return false;
}

void ASandboxTerrainController::SpawnZone(const TVoxelIndex& Index, const TTerrainLodMask TerrainLodMask) {
	//UE_LOG(LogSandboxTerrain, Log, TEXT("SpawnZone -> %d %d %d "), Index.X, Index.Y, Index.Z);

	// voxel data must exist in this point
	TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(Index);

	bool bMeshExist = false;
	auto ExistingZone = GetZoneByVectorIndex(Index);
	if (ExistingZone) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("ExistingZone -> %d %d %d "), Index.X, Index.Y, Index.Z);
		if (ExistingZone->GetTerrainLodMask() <= TerrainLodMask) {
			return;
		} else {
			bMeshExist = true;
		}
	}

	// if mesh data exist in file - load, apply and return
	TMeshDataPtr MeshDataPtr = LoadMeshDataByIndex(Index);
	if (MeshDataPtr && VoxelDataInfoPtr->DataState != TVoxelDataState::GENERATED) {
		if (bMeshExist) {
			// just change lod mask
			TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
			ExecGameThreadZoneApplyMesh(ExistingZone, MeshDataPtr, TerrainLodMask);
			return;
		} else {
			// spawn new zone with mesh
			TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
			ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr, TerrainLodMask);
			return;
		}
	}


	// mesh data not found, but vd exists
	/*
	if (VoxelDataInfoPtr->DataState == TVoxelDataState::READY_TO_LOAD) {
		VoxelDataInfoPtr->VdMutexPtr->lock();
		if (VoxelDataInfoPtr->DataState == TVoxelDataState::READY_TO_LOAD) {
			VoxelDataInfoPtr->Vd = LoadVoxelDataByIndex(Index);
			VoxelDataInfoPtr->DataState = TVoxelDataState::LOADED;
		}
		VoxelDataInfoPtr->VdMutexPtr->unlock();
	}
	*/

	// if no mesh data in file - generate mesh from voxel data
	if (VoxelDataInfoPtr->Vd && VoxelDataInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
		VoxelDataInfoPtr->Lock();
		MeshDataPtr = GenerateMesh(VoxelDataInfoPtr->Vd);
		VoxelDataInfoPtr->CleanUngenerated(); //TODO refactor
		VoxelDataInfoPtr->Unlock();

		if (ExistingZone) {
			// just change lod mask
			ExecGameThreadZoneApplyMesh(ExistingZone, MeshDataPtr, TerrainLodMask);
			return;
		}

		bool bIsNewGenerated = (VoxelDataInfoPtr->DataState == TVoxelDataState::GENERATED);
		TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
		ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr, TerrainLodMask, bIsNewGenerated);
	}
}

void ASandboxTerrainController::BatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList) {
	TArray<TGenerateZoneResult> NewVdArray;
	GetTerrainGenerator()->BatchGenerateVoxelTerrain(GenerationList, NewVdArray);

	int Idx = 0;
	for (const auto& P : GenerationList) {
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(P.Index);
		VdInfoPtr->Lock();

		VdInfoPtr->Vd = NewVdArray[Idx].Vd;

		FVector v = VdInfoPtr->Vd->getOrigin();

		if (NewVdArray[Idx].Method == TGenerationMethod::FastSimple || NewVdArray[Idx].Method == Skip) {
			//AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(GetWorld(), v, FVector(USBT_ZONE_SIZE / 2), FColor(0, 0, 255, 100), true); });
			VdInfoPtr->DataState = TVoxelDataState::UNGENERATED;
		} else {
			VdInfoPtr->DataState = TVoxelDataState::GENERATED;
		}
/*
#ifdef USBT_EXPERIMENTAL_UNGENERATED_ZONES 
		if (P.TerrainLodMask == 0) {
			VdInfoPtr->DataState = TVoxelDataState::GENERATED;
		} else {
			VdInfoPtr->DataState = TVoxelDataState::UNGENERATED;
		}
#else
		VdInfoPtr->DataState = TVoxelDataState::GENERATED;
#endif
*/

		VdInfoPtr->SetChanged();

		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(P.Index);
		GeneratorComponent->GenerateInstanceObjects(P.Index, VdInfoPtr->Vd, ZoneInstanceObjectMap);

		//TerrainData->AddSaveIndex(P.Index);

		VdInfoPtr->Unlock();

		Idx++;
	}
}

void ASandboxTerrainController::BatchSpawnZone(const TArray<TSpawnZoneParam>& SpawnZoneParamArray) {
	TArray<TSpawnZoneParam> GenerationList;
	TArray<TSpawnZoneParam> LoadList;

	for (const auto& SpawnZoneParam : SpawnZoneParamArray) {
		const TVoxelIndex Index = SpawnZoneParam.Index;
		const TTerrainLodMask TerrainLodMask = SpawnZoneParam.TerrainLodMask;

		bool bIsNoMesh = false;

		//check voxel data in memory
		bool bNewVdGeneration = false;
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
		VdInfoPtr->Lock();
		if (VdInfoPtr->DataState == TVoxelDataState::UNDEFINED) {
			if (TdFile.isExist(Index)) {
				// TODO: refactor kvdb
				TValueDataPtr DataPtr = TdFile.loadData(Index);
				usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
				TKvFileZodeData ZoneHeader;
				Deserializer >> ZoneHeader;

				bIsNoMesh = ZoneHeader.Is(TZoneFlag::NoMesh);
				bool bIsNoVd = ZoneHeader.Is(TZoneFlag::NoVoxelData);
				if (bIsNoVd) {
					//UE_LOG(LogSandboxTerrain, Log, TEXT("NoVoxelData"));
					VdInfoPtr->DataState = TVoxelDataState::UNGENERATED;
				} else {
					//voxel data exist in file
					VdInfoPtr->DataState = TVoxelDataState::READY_TO_LOAD;
				}
			} else {
				// generate new voxel data
				VdInfoPtr->DataState = TVoxelDataState::GENERATION_IN_PROGRESS;
				bNewVdGeneration = true;
			}
		}

		/*
		if (VdInfoPtr->DataState == TVoxelDataState::UNGENERATED && TerrainLodMask == 0) {
			VdInfoPtr->DataState = TVoxelDataState::GENERATION_IN_PROGRESS;
			UE_LOG(LogSandboxTerrain, Log, TEXT("bNewVdGeneration"))
			bNewVdGeneration = true;
		}
		*/

		VdInfoPtr->Unlock();

		if (bNewVdGeneration) {
			GenerationList.Add(SpawnZoneParam);
		} else {
			if (!bIsNoMesh) {
				LoadList.Add(SpawnZoneParam);
			}
		}
	}

	for (const auto& P  : LoadList) {
		SpawnZone(P.Index, P.TerrainLodMask);
	}

	if (GenerationList.Num() > 0) {
		BatchGenerateZone(GenerationList);
	}

	for (const auto& P : GenerationList) {
		//SpawnZone(P.Index, P.TerrainLodMask);
		TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(P.Index);

		if (VoxelDataInfoPtr->Vd && VoxelDataInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
			VoxelDataInfoPtr->Lock();
			TMeshDataPtr MeshDataPtr = GenerateMesh(VoxelDataInfoPtr->Vd);
			VoxelDataInfoPtr->CleanUngenerated(); //TODO refactor
			VoxelDataInfoPtr->Unlock();
			TerrainData->PutMeshDataToCache(P.Index, MeshDataPtr);
			ExecGameThreadAddZoneAndApplyMesh(P.Index, MeshDataPtr, P.TerrainLodMask, true);
		}
	}
}

bool ASandboxTerrainController::IsWorkFinished() { 
	return bIsWorkFinished; 
};

void ASandboxTerrainController::AddInitialZone(const TVoxelIndex& ZoneIndex) {
	InitialLoadSet.insert(ZoneIndex);
}

void ASandboxTerrainController::SpawnInitialZone() {
	const int s = static_cast<int>(TerrainInitialArea);

	if (s > 0) {
		//UE_LOG(LogTemp, Warning, TEXT("Zone Z range: %d -> %d"), InitialLoadArea.TerrainSizeMaxZ, -InitialLoadArea.TerrainSizeMinZ);
		for (auto z = InitialLoadArea.TerrainSizeMaxZ; z >= InitialLoadArea.TerrainSizeMinZ; z--) {
			for (auto x = -s; x <= s; x++) {
				for (auto y = -s; y <= s; y++) {
					AddInitialZone(TVoxelIndex(x, y, z));
				}
			}
		}
	} else {
		AddInitialZone(TVoxelIndex(0, 0, 0));
	}

	TArray<TSpawnZoneParam> SpawnList;
	for (const auto& ZoneIndex : InitialLoadSet) {
		TSpawnZoneParam SpawnZoneParam;
		SpawnZoneParam.Index = ZoneIndex;
		SpawnZoneParam.TerrainLodMask = 0;
		SpawnList.Add(SpawnZoneParam);
	}

	BatchSpawnZone(SpawnList);
}

// always in game thread
UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector Pos) {
	if (!IsInGameThread()) {
		return nullptr;
	}
    
    TVoxelIndex Index = GetZoneIndex(Pos);
	if (GetZoneByVectorIndex(Index)) {
		return nullptr; // no duplicate
	}

    FVector IndexTmp(Index.X, Index.Y,Index.Z);
    FString ZoneName = FString::Printf(TEXT("Zone [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
    UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*ZoneName));
    if (ZoneComponent) {
        ZoneComponent->RegisterComponent();
        //ZoneComponent->SetRelativeLocation(pos);
        ZoneComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
        ZoneComponent->SetWorldLocation(Pos);
		ZoneComponent->SetMobility(EComponentMobility::Movable);

        FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
        UVoxelMeshComponent* TerrainMeshComp = NewObject<UVoxelMeshComponent>(this, FName(*TerrainMeshCompName));
        TerrainMeshComp->RegisterComponent();
        TerrainMeshComp->SetMobility(EComponentMobility::Movable);
        TerrainMeshComp->SetCanEverAffectNavigation(true);
        TerrainMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
        TerrainMeshComp->AttachToComponent(ZoneComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
        TerrainMeshComp->ZoneIndex = Index;

        ZoneComponent->MainTerrainMesh = TerrainMeshComp;
    }

    TerrainData->AddZone(Index, ZoneComponent);

	if (bShowZoneBounds) {
		DrawDebugBox(GetWorld(), Pos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 100), true);
	}

    return ZoneComponent;
}

//======================================================================================================================================================================
//
//======================================================================================================================================================================

TVoxelIndex ASandboxTerrainController::GetZoneIndex(const FVector& Pos) {
	FVector Tmp = sandboxGridIndex(Pos, USBT_ZONE_SIZE);
	return TVoxelIndex(Tmp.X, Tmp.Y, Tmp.Z);
}

FVector ASandboxTerrainController::GetZonePos(const TVoxelIndex& Index) {
	return FVector((float)Index.X * USBT_ZONE_SIZE, (float)Index.Y * USBT_ZONE_SIZE, (float)Index.Z * USBT_ZONE_SIZE);
}

UTerrainZoneComponent* ASandboxTerrainController::GetZoneByVectorIndex(const TVoxelIndex& Index) {
	return TerrainData->GetZone(Index);
}

/*
TVoxelData* ASandboxTerrainController::GetVoxelDataByPos(const FVector& Pos) {
    return GetVoxelDataByIndex(GetZoneIndex(Pos));
}
*/

TVoxelDataInfoPtr ASandboxTerrainController::GetVoxelDataInfo(const TVoxelIndex& Index) {
    return TerrainData->GetVoxelDataInfo(Index);
}

//======================================================================================================================================================================
// invoke async
//======================================================================================================================================================================

/*
void ASandboxTerrainController::InvokeSafe(std::function<void()> Function) {
    if (IsInGameThread()) {
        Function();
    } else {
        AsyncTask(ENamedThreads::GameThread, [=]() { Function(); });
    }
}
*/

void ASandboxTerrainController::AddTaskToConveyor(std::function<void()> Function) {
	const std::lock_guard<std::mutex> Lock(ConveyorMutex);
	ConveyorList.push_back(Function);
}

void ASandboxTerrainController::ExecGameThreadZoneApplyMesh(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr,const TTerrainLodMask TerrainLodMask) {
	ASandboxTerrainController* Controller = this;

	TFunction<void()> Function = [=]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {
				Zone->ApplyTerrainMesh(MeshDataPtr, TerrainLodMask);
			}
		} else {
			UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::ExecGameThreadZoneApplyMesh - game shutdown"));
		}
	};

	if (IsInGameThread()) {
		Function();
	} else {
		AsyncTask(ENamedThreads::GameThread, Function);
	}
}

void ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask, const bool bIsNewGenerated) {
	FVector ZonePos = GetZonePos(Index);
	ASandboxTerrainController* Controller = this;

	TFunction<void()> Function = [=]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {
				UTerrainZoneComponent* Zone = AddTerrainZone(ZonePos);
				if (Zone) {
					Zone->ApplyTerrainMesh(MeshDataPtr, TerrainLodMask);

					if (bIsNewGenerated) {
						AddTaskToConveyor([=]() {
							OnGenerateNewZone(Index, Zone);
						});
					} else {
						AddTaskToConveyor([=]() {
							OnLoadZone(Index, Zone);
						});
					}
				} 
			}
		} else {
			UE_LOG(LogSandboxTerrain, Warning, TEXT("ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh - game shutdown"));
		}
	};

	if (IsInGameThread()) {
		Function();
	} else {
		AsyncTask(ENamedThreads::GameThread, Function);
	}
}

void ASandboxTerrainController::RunThread(TUniqueFunction<void()> Function) {
    std::unique_lock<std::shared_timed_mutex> Lock(ThreadListMutex);
    TerrainControllerEventList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(Function), TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask));
}

//======================================================================================================================================================================
// events
//======================================================================================================================================================================

void ASandboxTerrainController::OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
	VdInfoPtr->Lock();

    if (FoliageDataAsset) {
		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
		Zone->SpawnAll(ZoneInstanceObjectMap);
		Zone->SetNeedSave();
		TerrainData->AddSaveIndex(Index);
    }

	OnFinishGenerateNewZone(Index);
	VdInfoPtr->ClearInstanceObjectMap();
	Zone->bIsSpawnFinished = true;
	VdInfoPtr->Unlock();
}

void ASandboxTerrainController::OnLoadZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
	VdInfoPtr->Lock();
	if (FoliageDataAsset) {
		LoadFoliage(Zone);
	}

	OnFinishLoadZone(Index);
	Zone->bIsSpawnFinished = true;
	VdInfoPtr->Unlock();
}

void ASandboxTerrainController::OnFinishAsyncPhysicsCook(const TVoxelIndex& ZoneIndex) {
    /*
	InvokeSafe([=]() {
		UTerrainZoneComponent* Zone = GetZoneByVectorIndex(ZoneIndex);
		if (Zone) {
			TVoxelDataInfo* VoxelDataInfo = GetVoxelDataInfo(ZoneIndex);
			if (VoxelDataInfo->IsNewGenerated()) {
				if (TerrainGeneratorComponent && FoliageDataAsset) {
					TerrainGeneratorComponent->GenerateNewFoliage(Zone);
				}
				OnGenerateNewZone(Zone);
			}
		}
	});
     */
}


FORCEINLINE float ASandboxTerrainController::ClcGroundLevel(const FVector& V) {
	//return Generator->GroundLevelFuncLocal(V);
	return 0;
}

//======================================================================================================================================================================
// virtual functions
//======================================================================================================================================================================



FORCEINLINE void ASandboxTerrainController::OnOverlapActorDuringTerrainEdit(const FHitResult& OverlapResult, const FVector& Pos) {

}

FORCEINLINE void ASandboxTerrainController::OnFinishGenerateNewZone(const TVoxelIndex& Index) {

}

//======================================================================================================================================================================
// Perlin noise according seed
//======================================================================================================================================================================

// TODO use seed
float ASandboxTerrainController::PerlinNoise(const FVector& Pos) const {
	//return Generator->PerlinNoise(Pos.X, Pos.Y, Pos.Z);
	return 0;
}

// range 0..1
float ASandboxTerrainController::NormalizedPerlinNoise(const FVector& Pos) const {
	//float NormalizedPerlin = (Generator->PerlinNoise(Pos.X, Pos.Y, Pos.Z) + 0.87) / 1.73;
	//return NormalizedPerlin;
	return 0;
}

//======================================================================================================================================================================
// Sandbox Foliage
//======================================================================================================================================================================

void ASandboxTerrainController::LoadFoliage(UTerrainZoneComponent* Zone) {
	TInstanceMeshTypeMap ZoneInstanceMeshMap;
	LoadObjectDataByIndex(Zone, ZoneInstanceMeshMap);
	Zone->SpawnAll(ZoneInstanceMeshMap);
}

//======================================================================================================================================================================
// generate mesh
//======================================================================================================================================================================

std::shared_ptr<TMeshData> ASandboxTerrainController::GenerateMesh(TVoxelData* Vd) {
	double Start = FPlatformTime::Seconds();

	if (!Vd) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::GenerateMesh - NULL voxel data!"));
		return nullptr;
	}

	if (Vd->getDensityFillState() == TVoxelDataFillState::ZERO || Vd->getDensityFillState() == TVoxelDataFillState::FULL) {
		return nullptr;
	}

	TVoxelDataParam Vdp;
    
	// test
    //Vdp.bZCut = true;
    //Vdp.ZCutLevel = -100;

	if (USBT_ENABLE_LOD) {
		Vdp.bGenerateLOD = true;
		Vdp.collisionLOD = GetCollisionMeshSectionLodIndex();
	} else {
		Vdp.bGenerateLOD = false;
		Vdp.collisionLOD = 0;
	}

	//Vdp.bForceNoCache = true;

	TMeshDataPtr MeshDataPtr = sandboxVoxelGenerateMesh(*Vd, Vdp);

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	MeshDataPtr->TimeStamp = End;

	//UE_LOG(LogSandboxTerrain, Log, TEXT("generateMesh -------------> %f %f %f --> %f ms"), Vd->getOrigin().X, Vd->getOrigin().Y, Vd->getOrigin().Z, Time);
	return MeshDataPtr;
}


int ASandboxTerrainController::GetCollisionMeshSectionLodIndex() {
	if (CollisionSection > 6)
		return 6;

	return CollisionSection;

	//return 0; // nolod
}

const FSandboxFoliage& ASandboxTerrainController::GetFoliageById(uint32 FoliageId) const {
	return FoliageMap[FoliageId];
}

void ASandboxTerrainController::MarkZoneNeedsToSave(TVoxelIndex ZoneIndex) {
	TerrainData->AddSaveIndex(ZoneIndex);
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	VdInfoPtr->Lock();
	VdInfoPtr->SetChanged();
	VdInfoPtr->Unlock();
	//UE_LOG(LogSandboxTerrain, Log, TEXT("MarkZoneNeedsToSave -> %d %d %d"), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);
}

/*
void ASandboxTerrainController::ExecGameThreadRestoreSoftUnload(const TVoxelIndex& ZoneIndex) {
	ASandboxTerrainController* Controller = this;

	TFunction<void()> Function = [=]() {
		if (!bIsGameShutdown) {
			TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
			UTerrainZoneComponent* ZoneComponent = VdInfoPtr->GetZone();
			if (ZoneComponent) {
				VdInfoPtr->SetSoftUnload();
				ZoneComponent->SetVisibility(true, true);
			}
		} else {
			UE_LOG(LogSandboxTerrain, Log, TEXT("ASandboxTerrainController::ExecGameThreadRestoreSoftUnload - game shutdown"));
		}
	};

	if (IsInGameThread()) {
		Function();
	} else {
		AsyncTask(ENamedThreads::GameThread, Function);
	}
}
*/

bool ASandboxTerrainController::OnZoneSoftUnload(const TVoxelIndex& ZoneIndex) {
	return true;
}

void ASandboxTerrainController::OnRestoreZoneSoftUnload(const TVoxelIndex& ZoneIndex) {

}

/*
void ASandboxTerrainController::ZoneHardUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex) {
	//TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	//VdInfoPtr->RemoveZone();

	TArray<USceneComponent*> Children;
	ZoneComponent->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children) {
		Child->DestroyComponent(true);
	}

	ZoneComponent->DestroyComponent(true);
}*/


bool ASandboxTerrainController::OnZoneHardUnload(const TVoxelIndex& ZoneIndex) {
	return true;
}

void ASandboxTerrainController::OnFinishLoadZone(const TVoxelIndex& Index) {

}

const FTerrainInstancedMeshType* ASandboxTerrainController::GetInstancedMeshType(uint32 MeshTypeId, uint32 MeshVariantId) const {
	uint64 MeshCode = FTerrainInstancedMeshType::ClcMeshTypeCode(MeshTypeId, MeshVariantId);
	const FTerrainInstancedMeshType* MeshType = InstMeshMap.Find(MeshCode);
	return MeshType;
}