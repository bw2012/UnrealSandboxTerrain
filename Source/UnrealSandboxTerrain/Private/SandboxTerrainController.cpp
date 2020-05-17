
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "Async.h"
#include "Json.h"
#include "JsonObjectConverter.h"
#include "DrawDebugHelpers.h"

#include "TerrainZoneComponent.h"
#include "VdServerComponent.h"
#include "VdClientComponent.h"
#include "VoxelMeshComponent.h"

#include <cmath>
#include <list>

#include "serialization.hpp"
#include "utils.hpp"
#include "VoxelDataInfo.hpp"
#include "TerrainGenerator.hpp"
#include "TerrainData.hpp"
#include "TerrainAreaPipeline.hpp"
#include "TerrainEdit.hpp"


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
	bEnableLOD = false;
	SaveGeneratedZones = 1000;
	ServerPort = 6000;
    AutoSavePeriod = 20;
    TerrainData = new TTerrainData();
    CheckAreaMap = new TCheckAreaMap();
    Generator = new TTerrainGenerator(this);
    
    //FString SaveDir = FPaths::ProjectSavedDir() + TEXT("/Map/") + MapName + TEXT("/");
    //CheckSaveDir(SaveDir);
}

void ASandboxTerrainController::BeginDestroy() {
    Super::BeginDestroy();

	bIsGameShutdown = true;

    //delete TerrainData;
    //delete CheckAreaMap;
    //delete Generator;
}

void ASandboxTerrainController::FinishDestroy() {
	Super::FinishDestroy();
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::FinishDestroy()"));
	delete TerrainData;
	delete CheckAreaMap;
	delete Generator;
}

ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	InitializeTerrainController();
}

ASandboxTerrainController::ASandboxTerrainController() {
	InitializeTerrainController();
}

void ASandboxTerrainController::PostLoad() {
	Super::PostLoad();
	UE_LOG(LogTemp, Log, TEXT("ASandboxTerrainController::PostLoad()"));

	bIsGameShutdown = false;

#if WITH_EDITOR
	//spawnInitialZone();
#endif

}

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::BeginPlay()"));
    
    Generator->OnBeginPlay();
    
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

	MaterialMap.Empty();
	if (TerrainParameters) {
		MaterialMap = TerrainParameters->MaterialMap;
	}

	if (!GetWorld()) return;
	bIsLoadFinished = false;

	if (GetWorld()->IsServer()) {
		UE_LOG(LogTemp, Warning, TEXT("SERVER"));
		BeginPlayServer();
	} else {
		UE_LOG(LogTemp, Warning, TEXT("CLIENT"));
		BeginClient();
	}
 
	StartCheckArea();
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	bIsWorkFinished = true;

	{
		std::unique_lock<std::shared_timed_mutex> Lock(ThreadListMutex);
		UE_LOG(LogTemp, Warning, TEXT("TerrainControllerEventList -> %d threads. Waiting for finish..."), TerrainControllerEventList.Num());
		for (auto& TerrainControllerEvent : TerrainControllerEventList) {
			while (!TerrainControllerEvent->IsComplete()) {};
		}
	}

	Save();
	CloseFile();
    TerrainData->Clean();
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

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
					CheckAreaMap->PlayerSwapPosition.Add(PlayerId, Location);
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
        
    for (auto Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator) {
        APlayerController* PlayerController = Iterator->Get();
        if (PlayerController){
            const auto PlayerId = PlayerController->GetUniqueID();
            const auto Pawn = PlayerController->GetCharacter();
            const FVector Location = Pawn->GetActorLocation();
            const FVector PrevLocation = CheckAreaMap->PlayerSwapPosition.FindOrAdd(PlayerId);
            const float Distance = FVector::Distance(Location, PrevLocation);
            const float Threshold = PlayerLocationThreshold;
            if(Distance > Threshold) {
                CheckAreaMap->PlayerSwapPosition[PlayerId] = Location;
                TVoxelIndex LocationIndex = GetZoneIndex(Location);
                FVector Tmp = GetZonePos(LocationIndex);
                                
                if(CheckAreaMap->PlayerSwapHandler.Contains(PlayerId)){
                    // cancel old
                    std::shared_ptr<TTerrainLoadPipeline> HandlerPtr2 = CheckAreaMap->PlayerSwapHandler[PlayerId];
                    HandlerPtr2->Cancel();
                    CheckAreaMap->PlayerSwapHandler.Remove(PlayerId);
                }
                
                // start new
                std::shared_ptr<TTerrainLoadPipeline> HandlerPtr = std::make_shared<TTerrainLoadPipeline>();
				CheckAreaMap->PlayerSwapHandler.Add(PlayerId, HandlerPtr);

                if(bShowStartSwapPos){
                    DrawDebugBox(GetWorld(), Location, FVector(100), FColor(255, 0, 255, 0), false, 15);
                    static const float Len = 1000;
                    DrawDebugCylinder(GetWorld(), FVector(Tmp.X, Tmp.Y, Len), FVector(Tmp.X, Tmp.Y, -Len), DynamicLoadArea.Radius, 128, FColor(255, 0, 255, 128), false, 30);
                }
                
				TTerrainAreaPipelineParams Params;
                Params.FullLodDistance = DynamicLoadArea.FullLodDistance;
                Params.Radius = DynamicLoadArea.Radius;
                Params.TerrainSizeMinZ = DynamicLoadArea.TerrainSizeMinZ;
                Params.TerrainSizeMaxZ = DynamicLoadArea.TerrainSizeMaxZ;
                HandlerPtr->SetParams(TEXT("Player_Swap_Terrain_Task"), this, Params);
                
                //TSharedPtr<ASandboxTerrainController> ControllerPtr = MakeShareable(this);
                //TSharedRef<ASandboxTerrainController> NewReference = this;
                
                RunThread([=]() {
                    //TTerrainLoadPipeline& Handler2 = CheckAreaMap->Get(PlayerId);
                    HandlerPtr->LoadArea(Location);
                });
            }
        }
    }
    
    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogSandboxTerrain, Warning, TEXT("PerformCheckArea -> %f ms"), Time);
}

//======================================================================================================================================================================
// 
//======================================================================================================================================================================


void ASandboxTerrainController::RunGenerateTerrainPipeline(std::function<void()> OnFinish, std::function<void(uint32, uint32)> OnProgress) {
	RunThread([=]() {

		TTerrainAreaPipelineParams Params;
		Params.FullLodDistance = InitialLoadArea.FullLodDistance;
		Params.Radius = InitialLoadArea.Radius;
		Params.TerrainSizeMinZ = InitialLoadArea.TerrainSizeMinZ;
		Params.TerrainSizeMaxZ = InitialLoadArea.TerrainSizeMaxZ;
		Params.OnProgress = OnProgress;

		TTerrainGeneratorPipeline GeneratorPipeline(TEXT("Generate_Terrain_Pipeline"), this, Params);
		GeneratorPipeline.LoadArea(FVector(0));

		if (OnFinish && !bIsWorkFinished) {
			OnFinish();
		}
	});
}

//======================================================================================================================================================================
// begin play
//======================================================================================================================================================================

void ASandboxTerrainController::BeginPlayServer() {
	if (!OpenFile()) {
		return;
	}

	BeginTerrainLoad();

	UVdServerComponent* VdServerComponent = NewObject<UVdServerComponent>(this, TEXT("VdServer"));
	VdServerComponent->RegisterComponent();
	VdServerComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
}

void ASandboxTerrainController::BeginTerrainLoad() {
	LoadJson();

	SpawnInitialZone();
    
    if(bShowInitialArea){
        static const float Len = 5000;
        DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance, 128, FColor(255, 255, 255, 0), true);
        DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance + LodDistance.Distance2, 128, FColor(255, 255, 255, 0), true);
        DrawDebugCylinder(GetWorld(), FVector(0, 0, Len), FVector(0, 0, -Len), InitialLoadArea.FullLodDistance + LodDistance.Distance5, 128, FColor(255, 255, 255, 0), true);
    }

    if (!bGenerateOnlySmallSpawnPoint) {
        // async loading other zones
        RunThread([&]() {
			TTerrainAreaPipelineParams Params;
            Params.FullLodDistance = InitialLoadArea.FullLodDistance;
            Params.Radius = InitialLoadArea.Radius;
            Params.TerrainSizeMinZ = InitialLoadArea.TerrainSizeMinZ;
            Params.TerrainSizeMaxZ = InitialLoadArea.TerrainSizeMaxZ;
            
			TTerrainLoadPipeline Loader(TEXT("Initial_Load_Task"), this, Params);
            Loader.LoadArea(FVector(0));
			UE_LOG(LogTemp, Warning, TEXT("Finish initial terrain load"));

			if (!bIsWorkFinished) {
				//Generator->Clean;
				AsyncTask(ENamedThreads::GameThread, [&] {
					StartPostLoadTimers();
				});
			}
        });
    }
}

void ASandboxTerrainController::BeginClient() {
	UVdClientComponent* VdClientComponent = NewObject<UVdClientComponent>(this, TEXT("VdClient"));
	VdClientComponent->RegisterComponent();
	VdClientComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
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

void ASandboxTerrainController::FastSave() {
    const std::lock_guard<std::mutex> lock(FastSaveMutex);
	Save();
}

void ASandboxTerrainController::Save() {
	if (!VdFile.isOpen() || !MdFile.isOpen() || !ObjFile.isOpen()) {
		return;
	}

	double Start = FPlatformTime::Seconds();

	uint32 SavedVd = 0;
	uint32 SavedMd = 0;
	uint32 SavedObj = 0;

	std::unordered_set<TVoxelIndex> SaveIndexSet = TerrainData->PopSaveIndexSet();
	for (const TVoxelIndex& Index : SaveIndexSet) {
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

		//save voxel data
		if (VdInfoPtr->Vd) {
			VdInfoPtr->VdMutexPtr->lock();
			if (VdInfoPtr->IsChanged()) {
				//TVoxelIndex Index = GetZoneIndex(VdInfo.Vd->getOrigin());
				UE_LOG(LogSandboxTerrain, Warning, TEXT("save vd: %d %d %d"), Index.X, Index.Y, Index.Z);
				auto Data = VdInfoPtr->Vd->serialize();
				VdFile.save(Index, *Data);
				VdInfoPtr->ResetLastSave();
				SavedVd++;
			}
			VdInfoPtr->Unload();
			VdInfoPtr->VdMutexPtr->unlock();
		}

		//save mesh data
		auto MeshDataPtr = VdInfoPtr->PopMeshDataCache();
		if (MeshDataPtr) {
			TValueDataPtr DataPtr = SerializeMeshData(MeshDataPtr);
			if (DataPtr) {
				MdFile.save(Index, *DataPtr);
				SavedMd++;
			}
		}

		if (FoliageDataAsset) {
			auto InstanceObjectMapPtr = VdInfoPtr->PopInstanceObjectMap();
			if (InstanceObjectMapPtr) {
				TValueDataPtr DataPtr = UTerrainZoneComponent::SerializeInstancedMesh(*InstanceObjectMapPtr);
				if (DataPtr) {
					ObjFile.save(Index, *DataPtr);
					SavedObj++;
				}
			}

			UTerrainZoneComponent* Zone = VdInfoPtr->GetZone();
			if (Zone && Zone->IsNeedSave()) {
				auto Data = Zone->SerializeAndResetObjectData();
				ObjFile.save(Index, *Data);
				SavedObj++;
			}
		}

	}

	SaveJson();

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
    UE_LOG(LogSandboxTerrain, Warning, TEXT("Terrain saved: vd/md/obj -> %d/%d/%d  -> %f ms "), SavedVd, SavedMd, SavedObj, Time);
}

void ASandboxTerrainController::SaveMapAsync() {
	UE_LOG(LogSandboxTerrain, Log, TEXT("Start save terrain async"));
	RunThread([&]() {
		FastSave();
	});
}

void ASandboxTerrainController::AutoSaveByTimer() {
    UE_LOG(LogSandboxTerrain, Log, TEXT("Start auto save..."));
    FastSave();
}

void ASandboxTerrainController::SaveJson() {
    UE_LOG(LogTemp, Log, TEXT("Save terrain json"));
    
	MapInfo.SaveTimestamp = FPlatformTime::Seconds();
	FString JsonStr;
    
	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	FJsonObjectConverter::UStructToJsonObjectString(MapInfo, JsonStr);
	//UE_LOG(LogSandboxTerrain, Warning, TEXT("%s"), *JsonStr);
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
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Unable to open file: %s"), *FullPath);
		return false;
	}

	return true;
}

bool CheckSaveDir(FString SaveDir){
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDir)) {
        PlatformFile.CreateDirectory(*SaveDir);
        if (!PlatformFile.DirectoryExists(*SaveDir)) {
            UE_LOG(LogTemp, Warning, TEXT("Unable to create save directory -> %s"), *SaveDir);
            return false;
        }
    }
    
    return true;
}

bool ASandboxTerrainController::OpenFile() {
	// open vd file 	
	FString FileNameVd = TEXT("terrain_voxeldata.dat");
	FString FileNameMd = TEXT("terrain_mesh.dat");
	FString FileNameObj = TEXT("terrain_objects.dat");

	FString SavePath = FPaths::ProjectSavedDir();
	FString SaveDir = SavePath + TEXT("/Map/") + MapName + TEXT("/");

	if (!GetWorld()->IsServer()) {
		SaveDir = SaveDir + TEXT("/ClientCache/");
	}

    if(!CheckSaveDir(SaveDir)){
        return false;
    }

	if (!OpenKvFile(VdFile, FileNameVd, SaveDir)) {
		return false;
	}

	if (!OpenKvFile(MdFile, FileNameMd, SaveDir)) {
		return false;
	}

	if (!OpenKvFile(ObjFile, FileNameObj, SaveDir)) {
		return false;
	}

	return true;
}

void ASandboxTerrainController::CloseFile() {
	VdFile.close();
	MdFile.close();
	ObjFile.close();
}



//======================================================================================================================================================================
// load
//======================================================================================================================================================================

bool ASandboxTerrainController::LoadJson() {
	UE_LOG(LogTemp, Warning, TEXT("Load terrain json"));

	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	FString JsonRaw;
	if (!FFileHelper::LoadFileToString(JsonRaw, *FullPath, FFileHelper::EHashOptions::None)) {
		UE_LOG(LogTemp, Error, TEXT("Error loading json file"));
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonRaw, &MapInfo, 0, 0)) {
		UE_LOG(LogTemp, Error, TEXT("Error parsing json file"));
		return false;
	}

	UE_LOG(LogSandboxTerrain, Warning, TEXT("%s"), *JsonRaw);
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

TVoxelData* ASandboxTerrainController::NewVoxelData() {
	return new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
}

int ASandboxTerrainController::GeneratePipeline(const TVoxelIndex& Index) {
	if (!VdFile.isExist(Index)) {
		FVector Pos = GetZonePos(Index);
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

		// generate new voxel data
		GenerateNewZoneVd(VdInfoPtr, Index);

		if (VdInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
			TMeshDataPtr MeshDataPtr = GenerateMesh(VdInfoPtr->Vd);
			TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
		}

		TerrainData->AddSaveIndex(Index);
		return TZoneSpawnResult::GeneratedNewVd;
	}

	return TZoneSpawnResult::None;
}

void ASandboxTerrainController::GenerateNewZoneVd(TVoxelDataInfoPtr VdInfoPtr, const TVoxelIndex& Index) {
	// generate new voxel data
	const FVector Pos = GetZonePos(Index);
	VdInfoPtr->Vd = NewVoxelData();
	VdInfoPtr->Vd->setOrigin(Pos);
	Generator->GenerateVoxelTerrain(*VdInfoPtr->Vd);
	VdInfoPtr->DataState = TVoxelDataState::GENERATED;
	VdInfoPtr->SetChanged();
	VdInfoPtr->Vd->setCacheToValid();

	TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
	Generator->GenerateNewFoliageLandscape(Index, ZoneInstanceObjectMap);
	Generator->GenerateNewFoliageCustom(Index, VdInfoPtr->Vd, ZoneInstanceObjectMap);
}


// load or generate new zone voxel data and mesh
int ASandboxTerrainController::SpawnZonePipeline(const TVoxelIndex& Index, const TTerrainLodMask TerrainLodMask) {
	//UE_LOG(LogTemp, Log, TEXT("SpawnZone -> %d %d %d "), Index.X, Index.Y, Index.Z);
	
    bool bMeshExist = false;
    auto ExistingZone = GetZoneByVectorIndex(Index);
    if(ExistingZone){
		//UE_LOG(LogTemp, Log, TEXT("ExistingZone -> %d %d %d "), Index.X, Index.Y, Index.Z);
        if(ExistingZone->GetTerrainLodMask() <= TerrainLodMask){
            return TZoneSpawnResult::None;
        } else {
            bMeshExist = true;
        }
    }
    
    //if no voxel data in memory
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
	bool bNewVdGenerated = false;
	VdInfoPtr->VdMutexPtr->lock();
	if (VdInfoPtr->DataState == TVoxelDataState::UNDEFINED) {
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
		// if voxel data exist in file
		if (VdFile.isExist(Index)) {
			VdInfoPtr->DataState = TVoxelDataState::READY_TO_LOAD;
		} else {
			// generate new voxel data
			GenerateNewZoneVd(VdInfoPtr, Index);
			bNewVdGenerated = true;
			TerrainData->AddSaveIndex(Index);
		}
	}
	VdInfoPtr->VdMutexPtr->unlock();

	// voxel data must exist in this point
	TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(Index);

	// if mesh data exist in file - load, apply and return
	TMeshDataPtr MeshDataPtr = LoadMeshDataByIndex(Index);
	if (MeshDataPtr) {
        if(bMeshExist){
            // just change lod mask
			ExecGameThreadZoneApplyMesh(ExistingZone, MeshDataPtr, TerrainLodMask);
            return (int)TZoneSpawnResult::ChangeLodMask;
        } else {
            // spawn new zone with mesh
			ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr, TerrainLodMask, TVoxelDataState::LOADED);
            return (int)TZoneSpawnResult::SpawnMesh;
        }
    } 
    
	// if no mesh data in file - generate mesh from voxel data
	if (VoxelDataInfoPtr->Vd && VoxelDataInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
		VoxelDataInfoPtr->VdMutexPtr->lock();
		MeshDataPtr = GenerateMesh(VoxelDataInfoPtr->Vd);
		VoxelDataInfoPtr->VdMutexPtr->unlock();

		if (ExistingZone) {
			// just change lod mask
			ExecGameThreadZoneApplyMesh(ExistingZone, MeshDataPtr, TerrainLodMask);
		}

        TVoxelDataState State = VoxelDataInfoPtr->DataState;
		TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
		ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr, TerrainLodMask, State);
	}

	//UE_LOG(LogTemp, Log, TEXT("None -> %d %d %d "), Index.X, Index.Y, Index.Z);
	return bNewVdGenerated ? TZoneSpawnResult::GeneratedNewVd : TZoneSpawnResult::None;
}

void ASandboxTerrainController::SpawnInitialZone() {
	const int s = static_cast<int>(TerrainInitialArea);
	TSet<FVector> InitialZoneSet;

	if (s > 0) {
		for (auto z = -5; z <= 5; z++) {
			for (auto x = -s; x <= s; x++) {
				for (auto y = -s; y <= s; y++) {
					SpawnZonePipeline(TVoxelIndex(x, y, z));
				}
			}
			//TerrainGeneratorComponent->Clean();
		}
	} else {
		FVector Pos = FVector(0);
		SpawnZonePipeline(TVoxelIndex(0, 0, 0));
		InitialZoneSet.Add(Pos);
	}
}

// always in game thread
UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector Pos) {
    if (!IsInGameThread()) return nullptr;
    
    TVoxelIndex Index = GetZoneIndex(Pos);
    if(GetZoneByVectorIndex(Index)) return nullptr; // no duplicate

    FVector IndexTmp(Index.X, Index.Y,Index.Z);
    FString ZoneName = FString::Printf(TEXT("Zone -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
    UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*ZoneName));
    if (ZoneComponent) {
        ZoneComponent->RegisterComponent();
        //ZoneComponent->SetRelativeLocation(pos);

        ZoneComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
        ZoneComponent->SetWorldLocation(Pos);

        FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
        UVoxelMeshComponent* TerrainMeshComp = NewObject<UVoxelMeshComponent>(this, FName(*TerrainMeshCompName));
        TerrainMeshComp->RegisterComponent();
        TerrainMeshComp->SetMobility(EComponentMobility::Stationary);
        TerrainMeshComp->SetCanEverAffectNavigation(true);
        TerrainMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
        TerrainMeshComp->AttachToComponent(ZoneComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
        TerrainMeshComp->ZoneIndex = Index;

        ZoneComponent->MainTerrainMesh = TerrainMeshComp;
    }

    TerrainData->AddZone(Index, ZoneComponent);
    if(bShowZoneBounds) DrawDebugBox(GetWorld(), Pos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 100), true);
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

void ASandboxTerrainController::ExecGameThreadZoneApplyMesh(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr,const TTerrainLodMask TerrainLodMask) {
	ASandboxTerrainController* Controller = this;

	TFunction<void()> Function = [=]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {
				Zone->ApplyTerrainMesh(MeshDataPtr, TerrainLodMask);
			}
		} else {
			UE_LOG(LogTemp, Log, TEXT("ASandboxTerrainController::ExecGameThreadZoneApplyMesh - game shutdown"));
		}
	};

	if (IsInGameThread()) {
		Function();
	} else {
		AsyncTask(ENamedThreads::GameThread, Function);
	}
}

void ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const TTerrainLodMask TerrainLodMask, const uint32 State) {
	FVector ZonePos = GetZonePos(Index);
	ASandboxTerrainController* Controller = this;

	TFunction<void()> Function = [=]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {
				UTerrainZoneComponent* Zone = AddTerrainZone(ZonePos);
				if (Zone) {
					Zone->ApplyTerrainMesh(MeshDataPtr, TerrainLodMask);

					if (State == TVoxelDataState::GENERATED) {
						OnGenerateNewZone(Index, Zone);
					}

					if (State == TVoxelDataState::LOADED) {
						OnLoadZone(Zone);
					}
				}
			}
		} else {
			UE_LOG(LogTemp, Log, TEXT("ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh - game shutdown"));
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
    if (FoliageDataAsset) {
		//TInstanceMeshTypeMap ZoneInstanceMeshMap;
        //Generator->GenerateNewFoliage(Index, ZoneInstanceMeshMap);

		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
		Zone->SpawnAll(ZoneInstanceObjectMap);
		Zone->SetNeedSave();
		TerrainData->AddSaveIndex(Index);

		OnFinishGenerateNewZone(Index);
    }
}

void ASandboxTerrainController::OnLoadZone(UTerrainZoneComponent* Zone) {
	if (FoliageDataAsset) {
		LoadFoliage(Zone);
	}
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

//======================================================================================================================================================================
// virtual functions
//======================================================================================================================================================================

FORCEINLINE bool ASandboxTerrainController::OnCheckFoliageSpawn(const TVoxelIndex& ZoneIndex, const FVector& FoliagePos, FVector& Scale) {
    return true;
}

FORCEINLINE float ASandboxTerrainController::GeneratorDensityFunc(const TVoxelDensityFunctionData& FunctionData) {
    return FunctionData.Density;
}

FORCEINLINE bool ASandboxTerrainController::IsOverrideGroundLevel(const TVoxelIndex& Index) {
	return false;
}

FORCEINLINE float ASandboxTerrainController::GeneratorGroundLevelFunc(const TVoxelIndex& Index, const FVector& Pos, float GroundLevel) {
	return GroundLevel;
}

FORCEINLINE bool ASandboxTerrainController::GeneratorForcePerformZone(const TVoxelIndex& ZoneIndex) {
    return false;
}

FORCEINLINE void ASandboxTerrainController::OnOverlapActorDuringTerrainEdit(const FHitResult& OverlapResult, const FVector& Pos) {

}

FORCEINLINE FSandboxFoliage ASandboxTerrainController::GeneratorFoliageOverride(const int32 FoliageTypeId, const FSandboxFoliage& FoliageType, const TVoxelIndex& ZoneIndex, const FVector& WorldPos) {
	return FoliageType;
}

FORCEINLINE void ASandboxTerrainController::OnFinishGenerateNewZone(const TVoxelIndex& Index) {

}

FORCEINLINE bool  ASandboxTerrainController::GeneratorUseCustomFoliage(const TVoxelIndex& Index) {
	return false;
}

FORCEINLINE bool ASandboxTerrainController::GeneratorSpawnCustomFoliage(const TVoxelIndex& Index, const FVector& WorldPos, int32 FoliageTypeId, FSandboxFoliage FoliageType, FRandomStream& Rnd, FTransform& Transform) {
	return false;
}

//======================================================================================================================================================================
// Perlin noise according seed
//======================================================================================================================================================================

// TODO use seed
float ASandboxTerrainController::PerlinNoise(const FVector& Pos) const {
	return Generator->PerlinNoise(Pos.X, Pos.Y, Pos.Z);
}

// range 0..1
float ASandboxTerrainController::NormalizedPerlinNoise(const FVector& Pos) const {
	float NormalizedPerlin = (Generator->PerlinNoise(Pos.X, Pos.Y, Pos.Z) + 0.87) / 1.73;
	return NormalizedPerlin;
}

//======================================================================================================================================================================
// Sandbox Foliage
//======================================================================================================================================================================

void ASandboxTerrainController::LoadFoliage(UTerrainZoneComponent* Zone) {
	TInstanceMeshTypeMap ZoneInstanceMeshMap;
	LoadObjectDataByIndex(Zone, ZoneInstanceMeshMap);
	Zone->SpawnAll(ZoneInstanceMeshMap);

	/*
	for (auto& Elem : ZoneInstMeshMap) {
		TInstanceMeshArray& InstMeshTransArray = Elem.Value;

		for (FTransform& Transform : InstMeshTransArray.TransformArray) {
			Zone->SpawnInstancedMesh(InstMeshTransArray.MeshType, Transform);
		}
	}
	*/
}

//======================================================================================================================================================================
// generate mesh
//======================================================================================================================================================================

std::shared_ptr<TMeshData> ASandboxTerrainController::GenerateMesh(TVoxelData* Vd) {
	double Start = FPlatformTime::Seconds();

	if (!Vd) {
		UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::GenerateMesh - NULL voxel data!"));
		return nullptr;
	}

	if (Vd->getDensityFillState() == TVoxelDataFillState::ZERO || Vd->getDensityFillState() == TVoxelDataFillState::FULL) {
		return nullptr;
	}

	TVoxelDataParam Vdp;
    
    Vdp.bZCut = true;
    Vdp.ZCutLevel = -100;

	if (bEnableLOD) {
		Vdp.bGenerateLOD = true;
		Vdp.collisionLOD = GetCollisionMeshSectionLodIndex();
	} else {
		Vdp.bGenerateLOD = false;
		Vdp.collisionLOD = 0;
	}

	TMeshDataPtr MeshDataPtr = sandboxVoxelGenerateMesh(*Vd, Vdp);

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	MeshDataPtr->TimeStamp = End;

	//UE_LOG(LogTemp, Warning, TEXT("generateMesh -------------> %f %f %f --> %f ms"), Vd->getOrigin().X, Vd->getOrigin().Y, Vd->getOrigin().Z, Time);
	return MeshDataPtr;
}


