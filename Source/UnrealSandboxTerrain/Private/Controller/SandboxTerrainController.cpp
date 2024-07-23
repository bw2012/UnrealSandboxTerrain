#include "SandboxTerrainController.h"
#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "kvdb.hpp"

#include "TerrainZoneComponent.h"
#include "VoxelMeshComponent.h"

#include "TerrainServerComponent.h"
#include "TerrainClientComponent.h"

#include <cmath>
#include <list>

#include "Core/SandboxVoxelCore.h"
#include "serialization.hpp"
#include "Core/utils.hpp"
#include "Core/VoxelDataInfo.hpp"
#include "Core/TerrainData.hpp"
#include "Core/TerrainAreaHelper.hpp"
#include "Core/TerrainEdit.hpp"
#include "Core/ThreadPool.hpp"
#include "Core/memstat.h"

#include "Runtime/Launch/Resources/Version.h"


// ====================================
// FIXME 
// ====================================
bool bIsGameShutdown;

bool IsGameShutdown() {
	return bIsGameShutdown;
}
// ====================================

extern TAutoConsoleVariable<int32> CVarMainDistance;
extern TAutoConsoleVariable<int32> CVarDebugArea;
extern TAutoConsoleVariable<int32> CVarAutoSavePeriod;
extern TAutoConsoleVariable<int32> CVarLodRatio;

//========================================================================================
// debug only
//========================================================================================

bool bShowZoneBounds = false;

bool bShowStartSwapPos = false;

bool bShowApplyZone = false;


void ApplyTerrainMesh(UTerrainZoneComponent* Zone, std::shared_ptr<TMeshData> MeshDataPtr, bool bIgnoreCollision = false) {
	if (bShowApplyZone) {
		DrawDebugBox(Zone->GetWorld(), Zone->GetComponentLocation(), FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), false, 5);
	}

	Zone->ApplyTerrainMesh(MeshDataPtr, bIgnoreCollision);
}


//======================================================================================================================================================================
// Terrain Controller
//======================================================================================================================================================================


void ASandboxTerrainController::InitializeTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	ServerPort = 6000;
    AutoSavePeriod = 60;
    TerrainData = new TTerrainData();
    CheckAreaMap = new TCheckAreaMap();
	bSaveOnEndPlay = true;
	BeginServerTerrainLoadLocation = FVector(0);
	bSaveAfterInitialLoad = false;
	bReplicates = false;
}

void ASandboxTerrainController::BeginDestroy() {
    Super::BeginDestroy();
	bIsGameShutdown = true;
}

void ASandboxTerrainController::FinishDestroy() {
	delete TerrainData;
	delete CheckAreaMap;

	Super::FinishDestroy();
	//UE_LOG(LogVt, Warning, TEXT("vd -> %d, md -> %d, cd -> %d"), vd::tools::memory::getVdCount(), md_counter.load(), cd_counter.load());
}

ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	InitializeTerrainController();
}

ASandboxTerrainController::ASandboxTerrainController() {
	InitializeTerrainController();
}

void ASandboxTerrainController::PostLoad() {
	Super::PostLoad();
	//UE_LOG(LogVt, Log, TEXT("ASandboxTerrainController::PostLoad()"));
	bIsGameShutdown = false;
}

UTerrainGeneratorComponent* ASandboxTerrainController::NewTerrainGenerator() {
	return NewObject<UTerrainGeneratorComponent>(this, TEXT("TerrainGenerator"));
}

UTerrainGeneratorComponent* ASandboxTerrainController::GetTerrainGenerator() {
	return this->GeneratorComponent;
}

extern float LodScreenSizeArray[LOD_ARRAY_SIZE];

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();

	LoadConsoleVars();

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	UE_LOG(LogVt, Warning, TEXT("Run as PIE"));
	bGenerateOnlySmallSpawnPoint = IsDebugModeOn();
	if (bGenerateOnlySmallSpawnPoint) {
		UE_LOG(LogVt, Warning, TEXT("Debug mode ON"));
		bEnableAreaStreaming = false;
	}
#else
	UE_LOG(LogVt, Log, TEXT("Packaged project: debug features are disabled"));
	bGenerateOnlySmallSpawnPoint = false;
#endif

	UE_LOG(LogVt, Warning, TEXT("Initialize terrain parameters..."));
	UE_LOG(LogVt, Warning, TEXT("LodRatio = %f"), LodRatio);

	float ScreenSize = 1.f;
	for (auto LodIdx = 0; LodIdx < LOD_ARRAY_SIZE; LodIdx++) {
		UE_LOG(LogVt, Log, TEXT("Lod %d -> %f"), LodIdx, ScreenSize);
		LodScreenSizeArray[LodIdx] = ScreenSize;
		ScreenSize *= LodRatio;
	}

	ThreadPool = new TThreadPool(5);
	Conveyor = new TConveyour();

	GeneratorComponent = NewTerrainGenerator();
	GeneratorComponent->RegisterComponent();

	if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer) {
		FTransform Transform = GetActorTransform();
		NetProxy = (ASandboxTerrainNetProxy*)GetWorld()->SpawnActor(ASandboxTerrainNetProxy::StaticClass(), &Transform);
	}

	bIsGameShutdown = false;

	FoliageMap.Empty();
	if (FoliageDataAsset) {
		FoliageMap = FoliageDataAsset->FoliageMap;
	}

	InstMeshMap.Empty();
	MaterialMap.Empty();
	if (TerrainParameters) {
		MaterialMap = TerrainParameters->MaterialMap;

		UE_LOG(LogVt, Log, TEXT("Register terrain instance meshes:"));
		for (const auto& InstMesh : TerrainParameters->InstanceMeshes) {
			if (InstMesh.Mesh != nullptr) {
				uint64 MeshTypeCode = InstMesh.GetMeshTypeCode();
				UE_LOG(LogVt, Log, TEXT("MeshTypeCode = %lld - %s"), MeshTypeCode, *InstMesh.Mesh->GetName());
				InstMeshMap.Add(MeshTypeCode, InstMesh);
			} else {
				//TODO handle error
			}
		}
	}

	if (!GetWorld()) return;
	bIsLoadFinished = false;

	if (GetNetMode() == NM_Client) {
		UE_LOG(LogVt, Warning, TEXT("================== CLIENT =================="));
		BeginPlayClient();
	} else {
		if (GetNetMode() == NM_DedicatedServer) {
			UE_LOG(LogVt, Warning, TEXT("================== DEDICATED SERVER =================="));
		} 
		
		if (GetNetMode() == NM_ListenServer) {
			UE_LOG(LogVt, Warning, TEXT("================== LISTEN SERVER =================="));
		}

		BeginPlayServer();
	}
}

void ASandboxTerrainController::ShutdownThreads() {
	bIsWorkFinished = true;

	UE_LOG(LogVt, Warning, TEXT("Shutdown thread pool..."));
	ThreadPool->shutdownAndWait();

	UE_LOG(LogVt, Warning, TEXT("Conveyor -> %d threads. Waiting for finish..."), Conveyor->size());
	while (true) {
		std::function<void()> Function;
		if (Conveyor->pop(Function)) {
			Function();
		} else {
			break;
		}
	}
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
	UE_LOG(LogVt, Log, TEXT("Shutdown terrain controller..."));

	ShutdownThreads();

	if (bSaveOnEndPlay || GetNetMode() == NM_DedicatedServer) {
		Save();
	}

	CloseFile();
	SaveTerrainMetadata();

    TerrainData->Clean();
	GetTerrainGenerator()->Clean();

	delete ThreadPool;
	delete Conveyor;
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	LoadConsoleVars();

	double ConvTime = 0;
	while (ConvTime < ConveyorMaxTime) {
		std::function<void()> Function;
		if (Conveyor->pop(Function)) {
			double Start = FPlatformTime::Seconds();
			Function();
			double End = FPlatformTime::Seconds();
			ConvTime += (End - Start);
		} else {
			break;
		}
	}
}

//======================================================================================================================================================================
// Swapping terrain area according player position
//======================================================================================================================================================================

void ASandboxTerrainController::StartPostLoadTimers() {
	if (AutoSavePeriod > 0) {
		GetWorld()->GetTimerManager().SetTimer(TimerAutoSave, this, &ASandboxTerrainController::AutoSaveByTimer, AutoSavePeriod, true);
	}
}

void ASandboxTerrainController::StartCheckArea() {
	GetWorld()->GetTimerManager().SetTimer(TimerSwapArea, this, &ASandboxTerrainController::PerformCheckArea, 0.25, true);
}

void ASandboxTerrainController::PerformCheckArea() {
    if(!bEnableAreaStreaming){
        return;
    }
    
    double Start = FPlatformTime::Seconds();
        
	TArray<FVector> PlayerLocationList;
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
			PlayerLocationList.Add(PlayerLocation);
            const FVector PrevLocation = CheckAreaMap->PlayerStreamingPosition.FindOrAdd(PlayerId);
            const float Distance = FVector::Distance(PlayerLocation, PrevLocation);
            const float Threshold = PlayerLocationThreshold;
            if(Distance > Threshold) {
                CheckAreaMap->PlayerStreamingPosition[PlayerId] = PlayerLocation;
                TVoxelIndex LocationIndex = GetZoneIndex(PlayerLocation);
                FVector Tmp = GetZonePos(LocationIndex);
                                
                if(CheckAreaMap->PlayerStreamingHandler.Contains(PlayerId)){
                    // cancel old
                    std::shared_ptr<TTerrainLoadHelper> HandlerPtr2 = CheckAreaMap->PlayerStreamingHandler[PlayerId];
                    HandlerPtr2->Cancel();
                    CheckAreaMap->PlayerStreamingHandler.Remove(PlayerId);
                }
                
                // start new
                std::shared_ptr<TTerrainLoadHelper> HandlerPtr = std::make_shared<TTerrainLoadHelper>();
				CheckAreaMap->PlayerStreamingHandler.Add(PlayerId, HandlerPtr);

                if(bShowStartSwapPos){
                    DrawDebugBox(GetWorld(), PlayerLocation, FVector(100), FColor(255, 0, 255, 0), false, 15);
                }
                
				TTerrainAreaLoadParams Params(ActiveAreaSize, ActiveAreaDepth);
                HandlerPtr->SetParams(TEXT("player_streaming"), this, Params);
                               
                AddAsyncTask([=]() {
                    HandlerPtr->LoadArea(PlayerLocation);
                });

				bPerformSoftUnload = true;
            }
        }
    }

	if (bPerformSoftUnload || bForcePerformHardUnload) {
		CheckUnreachableZones(PlayerLocationList);		
	}
    
    double End = FPlatformTime::Seconds();
    double Time = (End - Start) * 1000;
    //UE_LOG(LogVt, Log, TEXT("PerformCheckArea -> %f ms"), Time);
}

void ASandboxTerrainController::CheckUnreachableZones(const TArray<FVector>& PlayerLocationList) {
	TArray<TVoxelIndex> UnreachableZones;

	if (PlayerLocationList.Num() == 0 && GetNetMode() == NM_DedicatedServer) {
		UE_LOG(LogVt, Warning, TEXT("DedicatedServer: No players found. Unload all zones."));
	}

	TArray<FVector> AnchorObjectList;
	GetAnchorObjectsLocation(AnchorObjectList);

	int RestoredCount = 0;

	const float RadiusByPlayerPos = ActiveAreaSize * USBT_ZONE_SIZE;
	const static float RadiusByAnchorObject = USBT_ZONE_SIZE * 1.4142; // sqrt(2)

	TArray<UTerrainZoneComponent*> Components;
	GetComponents<UTerrainZoneComponent>(Components);
	for (UTerrainZoneComponent* ZoneComponent : Components) {
		const FVector ZonePos = ZoneComponent->GetComponentLocation();
		const TVoxelIndex ZoneIndex = GetZoneIndex(ZonePos);

		bool bUnload = true;

		for (const auto& PlayerLocation : PlayerLocationList) {
			const float ZoneDistance = FVector::Distance(ZonePos, PlayerLocation);

			if (ZoneDistance < RadiusByPlayerPos * 1.5f) {
				bUnload = false;

				//AsyncTask(ENamedThreads::GameThread, [=, this]() { DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), false, 5); });

				if (ZoneDistance < RadiusByPlayerPos) {
					// restore soft unload
					TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(ZoneIndex);
					if (VoxelDataInfoPtr->IsSoftUnload()) {
						VoxelDataInfoPtr->ResetSoftUnload();
						OnRestoreZoneSoftUnload(ZoneIndex);
						RestoredCount++;
					}
				}
			}
		}

		if (bUnload) {
			for (const auto& Location : AnchorObjectList) {

				//AsyncTask(ENamedThreads::GameThread, [&]() { DrawDebugBox(GetWorld(), Location, FVector(500), FColor(0, 0, 255, 0), true); });

				if (FVector::Distance(ZonePos, Location) < RadiusByAnchorObject) {
					bUnload = false;
					//AsyncTask(ENamedThreads::GameThread, [=, this]() { DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 255, 255, 0), true); });
				}
			}
		}

		if (bUnload) {
			UnreachableZones.Add(ZoneIndex);
		}
	}
	
	if (RestoredCount > 0) {
		UE_LOG(LogVt, Log, TEXT("Soft unloaded zones restored: %d"), RestoredCount);
	}

	UE_LOG(LogVt, Log, TEXT("Found unreachable zones: %d"), UnreachableZones.Num());

	UnloadUnreachableZones(UnreachableZones);
}

void ASandboxTerrainController::ForcePerformHardUnload() {
	bForcePerformHardUnload = true;
}

void ASandboxTerrainController::ForceTerrainNetResync() {
	if (TerrainClientComponent) {
		bForceResync = true;
		TerrainClientComponent->RequestMapInfo();
	}
}

void RemoveAllChilds(UTerrainZoneComponent* ZoneComponent) {
	TArray<USceneComponent*> ChildList;
	ZoneComponent->GetChildrenComponents(true, ChildList);
	for (USceneComponent* Child : ChildList) {
		Child->DestroyComponent(true);
	}
}

void ASandboxTerrainController::UnloadUnreachableZones(const TArray<TVoxelIndex>& UnreachableZones) {
	double Start = FPlatformTime::Seconds();

	int HardCount = 0;
	int SoftCount = 0;

	for (const auto& ZoneIndex : UnreachableZones) {
		UTerrainZoneComponent* ZoneComponent = GetZoneByVectorIndex(ZoneIndex);

		ZoneSoftUnload(ZoneComponent, ZoneIndex);
		SoftCount++;

		if (bForcePerformHardUnload) {
			ZoneHardUnload(ZoneComponent, ZoneIndex);
			HardCount++;
		}
	}

	if (HardCount > 0) {
		GEngine->ForceGarbageCollection();
	}

	if (bForcePerformHardUnload) {
		bForcePerformHardUnload = false;
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogVt, Log, TEXT("Unload unreachable zones: hard = %d, soft = %d --> %f ms"), HardCount, SoftCount, Time);
}

void ASandboxTerrainController::ZoneHardUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	TVdInfoLockGuard Lock(VdInfoPtr);

	FVector ZonePos = ZoneComponent->GetComponentLocation();
	if (VdInfoPtr->IsSoftUnload() && !VdInfoPtr->IsNeedObjectsSave()) {
		if (VdInfoPtr->IsSpawnFinished()) {
			RemoveAllChilds(ZoneComponent);
			TerrainData->RemoveZone(ZoneIndex);
			ZoneComponent->DestroyComponent(true);
		} else {
			//AsyncTask(ENamedThreads::GameThread, [&, this]() { DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 0), false, 5); });
		}
	} else {
		//AsyncTask(ENamedThreads::GameThread, [&, this]() { DrawDebugBox(GetWorld(), ZonePos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 0), false, 5); });
	}
}

void ASandboxTerrainController::ZoneSoftUnload(UTerrainZoneComponent* ZoneComponent, const TVoxelIndex& ZoneIndex) {
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

bool LoadDataFromKvFile(TKvFile& KvFile, const TVoxelIndex& Index, std::function<void(TValueDataPtr)> Function);

void ASandboxTerrainController::BeginPlayServer() {
	if (!OpenFile()) {
		// TODO error message
		// return; // TODO fix UE4 create directory false positive issue
	}

	if (LoadJson()) {
		WorldSeed = MapInfo.WorldSeed;
	} else {
		BeginNewWorld();
	}

	UE_LOG(LogVt, Warning, TEXT("WorldSeed: %d"), WorldSeed);
	GeneratorComponent->ReInit();

	LoadTerrainMetadata();

	BeginServerTerrainLoad();

	if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer) {
		TerrainServerComponent = NewObject<UTerrainServerComponent>(this, TEXT("TerrainServer"));
		TerrainServerComponent->RegisterComponent();
		TerrainServerComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
	}	
}

void ASandboxTerrainController::BeginNewWorld() {

}

void ASandboxTerrainController::BeginServerTerrainLoad() {
	bEnableConveyor = false;
	SpawnInitialZone(); // spawn initial zones without conveyor
	bEnableConveyor = true;

    if (!bGenerateOnlySmallSpawnPoint) {
		TVoxelIndex B = GetZoneIndex(BeginServerTerrainLoadLocation);

        // async loading other zones
		TTerrainAreaLoadParams Params(ActiveAreaSize, ActiveAreaDepth);
        AddAsyncTask([=, this]() {            
			UE_LOG(LogVt, Warning, TEXT("Server: Begin terrain load at location: %f %f %f"), BeginServerTerrainLoadLocation.X, BeginServerTerrainLoadLocation.Y, BeginServerTerrainLoadLocation.Z);

			TTerrainLoadHelper Loader(TEXT("initial_load"), this, Params);
            Loader.LoadArea(BeginServerTerrainLoadLocation);
			bInitialLoad = false;
			UE_LOG(LogVt, Warning, TEXT("======= Finish initial terrain load ======="));

			if (!bIsWorkFinished) {
				AsyncTask(ENamedThreads::GameThread, [&] {

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 1 || ENGINE_MINOR_VERSION == 2)
					UE51MaterialIssueWorkaround();
#endif
					AddTaskToConveyor([=, this] {
						OnFinishInitialLoad(); 
						if (bSaveAfterInitialLoad) {
							SaveMapAsync();
						}
					});

					AsyncTask(ENamedThreads::GameThread, [&] {
						StartPostLoadTimers();
						StartCheckArea();
					});
				});
			}
        });
	} else {
		OnFinishInitialLoad();
	}
}

void ASandboxTerrainController::BeginPlayClient() {
	if (!OpenFile()) {
		// TODO error message
		// return; // TODO fix UE4 create directory false positive issue
	}

	LoadTerrainMetadata();
	bEnableConveyor = true;

	TerrainClientComponent = NewObject<UTerrainClientComponent>(this, TEXT("TerrainClient"));
	TerrainClientComponent->RegisterComponent();
	TerrainClientComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
}

void ASandboxTerrainController::ClientStart() {
	if (TerrainClientComponent) {
		TerrainClientComponent->Start();
	}
}

void ASandboxTerrainController::BeginClientTerrainLoad(const TVoxelIndex& ZoneIndex) {
	TTerrainAreaLoadParams Params(ActiveAreaSize, ActiveAreaDepth);

	AddAsyncTask([=, this]() {
		const TVoxelIndex Index = ZoneIndex;
		UE_LOG(LogVt, Warning, TEXT("Client: Begin terrain load at location: %f %f %f"), (float)ZoneIndex.X, (float)ZoneIndex.Y, (float)ZoneIndex.Z);
		TTerrainLoadHelper Loader(TEXT("client_initial_load"), this, Params);
		Loader.LoadArea(Index);
		bInitialLoad = false;
		UE_LOG(LogVt, Warning, TEXT("======= Finish client initial terrain load ======="));

		if (!bIsWorkFinished) {
			AsyncTask(ENamedThreads::GameThread, [&] {

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 1 || ENGINE_MINOR_VERSION == 2)
				UE51MaterialIssueWorkaround();
#endif
				AddTaskToConveyor([=, this] {
					OnFinishInitialLoad();
				});

				AsyncTask(ENamedThreads::GameThread, [&] {
					StartPostLoadTimers();
					StartCheckArea();

					GetWorld()->GetTimerManager().SetTimer(TimerPingServer, this, &ASandboxTerrainController::PingServer, 5, true);
				});
			});
		}

	});
}

void ASandboxTerrainController::OnStartBackgroundSaveTerrain() {

}

void ASandboxTerrainController::OnFinishBackgroundSaveTerrain() {

}

void ASandboxTerrainController::OnProgressBackgroundSaveTerrain(float Progress) {

}

void ASandboxTerrainController::SaveMapAsync() {
	UE_LOG(LogVt, Log, TEXT("Start save terrain async"));
	AddAsyncTask([&]() {
		std::function<void(uint32, uint32)> OnProgress = [=, this](uint32 Processed, uint32 Total) {
			if (Processed % 10 == 0) {
				float Progress = (float)Processed / (float)Total;
				AsyncTask(ENamedThreads::GameThread, [=, this]() { OnProgressBackgroundSaveTerrain(Progress); });
			}
		};

		AsyncTask(ENamedThreads::GameThread, [=, this]() { OnStartBackgroundSaveTerrain(); });
		Save(OnProgress);
		bForcePerformHardUnload = true;
		AsyncTask(ENamedThreads::GameThread, [=, this]() { OnFinishBackgroundSaveTerrain(); });

		UE_LOG(LogVt, Log, TEXT("Finish save terrain async"));
	});
}

void ASandboxTerrainController::AutoSaveByTimer() {
	if (TerrainData->IsSaveIndexEmpty()) {
		return;
	}

    UE_LOG(LogVt, Log, TEXT("Start auto save..."));
	SaveMapAsync();
}

//======================================================================================================================================================================
//  spawn zone
//======================================================================================================================================================================

uint32 ASandboxTerrainController::GetZoneVoxelResolution() {
	return (1 << (LOD_ARRAY_SIZE - 1)) + 1;
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

void ASandboxTerrainController::SpawnZone(const TVoxelIndex& Index) {
	TVoxelDataInfoPtr VdInfoPtr = GetVoxelDataInfo(Index); 
	TVdInfoLockGuard Lock(VdInfoPtr);

	if (VdInfoPtr->IsSpawnFinished()) {
		return;
	}

	auto Zone = GetZoneByVectorIndex(Index);

	// if mesh data exist in file - load, apply and return
	TMeshDataPtr MeshDataPtr = nullptr;
	TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
	LoadMeshAndObjectDataByIndex(Index, MeshDataPtr, ZoneInstanceObjectMap);
	if (MeshDataPtr && VdInfoPtr->DataState != TVoxelDataState::GENERATED) {
		if (Zone) {
			ExecGameThreadZoneApplyMesh(Index, Zone, MeshDataPtr);
		} else {
			ExecGameThreadAddZoneAndApplyMesh(Index, MeshDataPtr);
		}
	}

	VdInfoPtr->SetSpawnFinished();
}

void ASandboxTerrainController::BatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList) {
	TArray<TGenerateZoneResult> GenResultArray;
	GetTerrainGenerator()->BatchGenerateVoxelTerrain(GenerationList, GenResultArray);

	int Idx = 0;
	for (const auto& P : GenerationList) {
		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(P.Index);
		TVdInfoLockGuard Lock(VdInfoPtr);

		const auto& GenResult = GenResultArray[Idx];

		VdInfoPtr->Vd = GenResult.Vd;
		FVector v = VdInfoPtr->Vd->getOrigin();

		if (GenResult.Method == TGenerationMethod::FastSimple || GenResult.Method == Skip) {
			//AsyncTask(ENamedThreads::GameThread, [=]() { DrawDebugBox(GetWorld(), v, FVector(USBT_ZONE_SIZE / 2), FColor(0, 0, 255, 100), true); });
			VdInfoPtr->DataState = TVoxelDataState::UNGENERATED;
		} else {
			VdInfoPtr->DataState = TVoxelDataState::GENERATED;
		}

		VdInfoPtr->SetChanged();
		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(P.Index);
		GeneratorComponent->GenerateInstanceObjects(P.Index, VdInfoPtr->Vd, ZoneInstanceObjectMap, GenResult);
		Idx++;
	}
}

void ASandboxTerrainController::BatchSpawnZone(const TArray<TSpawnZoneParam>& SpawnZoneParamArray) {
	TArray<TSpawnZoneParam> GenerationList;
	TArray<TSpawnZoneParam> LoadList;
	TArray<TVoxelIndex> NoMeshList;

	for (const auto& SpawnZoneParam : SpawnZoneParamArray) {
		const TVoxelIndex Index = SpawnZoneParam.Index;

		if (TerrainData->IsOutOfSync(Index)) {
			continue; // skip network zones
		}

		if (Index.X == 0 && Index.Y == 0 && Index.Z == 1) {
			UE_LOG(LogTemp, Log, TEXT("TEST1"));
		}

		bool bIsNoMesh = false;

		//check voxel data in memory
		bool bNewVdGeneration = false;

		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
		TVdInfoLockGuard Lock(VdInfoPtr); // TODO lock order

		if (VdInfoPtr->DataState == TVoxelDataState::UNDEFINED) {
			if (TdFile.isExist(Index)) {
				TValueDataPtr DataPtr = TdFile.loadData(Index);
				usbt::TFastUnsafeDeserializer Deserializer(DataPtr->data());
				TKvFileZoneData ZoneHeader;
				Deserializer >> ZoneHeader;

				bIsNoMesh = ZoneHeader.Is(TZoneFlag::NoMesh);
				bool bIsNoVd = ZoneHeader.Is(TZoneFlag::NoVoxelData);
				if (bIsNoVd) {
					VdInfoPtr->DataState = TVoxelDataState::UNGENERATED;
				} else {
					//voxel data exist in file
					VdInfoPtr->DataState = TVoxelDataState::READY_TO_LOAD;
				}

				if (ZoneHeader.Is(TZoneFlag::InternalSolid)) {
					VdInfoPtr->SetFlagInternalFullSolid();
				}

			} else {
				// generate new voxel data
				VdInfoPtr->DataState = TVoxelDataState::GENERATION_IN_PROGRESS;
				bNewVdGeneration = true;
			}
		}

		if (bNewVdGeneration) {
			GenerationList.Add(SpawnZoneParam);
		} else {
			if (bIsNoMesh) {
				NoMeshList.Add(Index);
			} else {
				LoadList.Add(SpawnZoneParam);
			}
		}
	}

	for (const auto& P  : LoadList) {
		SpawnZone(P.Index);
	}

	if (GenerationList.Num() > 0) {
		BatchGenerateZone(GenerationList);
		PostBatchGenerateZone(GenerationList);
	}

	ExecGameThreadMoMeshZoneSpawn(NoMeshList);
}

void ASandboxTerrainController::ExecGameThreadMoMeshZoneSpawn(const TArray<TVoxelIndex>& IndexList) {
	for (const auto& Index : IndexList) {

		std::function<void()> Function = [=, this]() {
			OnFinishLoadZone(Index);
		};

		AddTaskToConveyor(Function);
	}
}

void ASandboxTerrainController::PostBatchGenerateZone(const TArray<TSpawnZoneParam>& GenerationList) {
	for (const auto& P : GenerationList) {

		TVoxelDataInfoPtr VoxelDataInfoPtr = GetVoxelDataInfo(P.Index);
		TVdInfoLockGuard Lock(VoxelDataInfoPtr);

		if (VoxelDataInfoPtr->Vd && VoxelDataInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::FULL) {
			VoxelDataInfoPtr->SetFlagInternalFullSolid();
		}

		if (VoxelDataInfoPtr->Vd && VoxelDataInfoPtr->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
			TMeshDataPtr MeshDataPtr = GenerateMesh(VoxelDataInfoPtr->Vd);
			VoxelDataInfoPtr->CleanUngenerated(); //TODO refactor
			TerrainData->PutMeshDataToCache(P.Index, MeshDataPtr);
			ExecGameThreadAddZoneAndApplyMesh(P.Index, MeshDataPtr, true);
		} else {
			VoxelDataInfoPtr->SetNeedTerrainSave();
			TerrainData->AddSaveIndex(P.Index);
		}

		VoxelDataInfoPtr->SetSpawnFinished();
	}
}

bool ASandboxTerrainController::IsWorkFinished() { 
	return bIsWorkFinished; 
};

void ASandboxTerrainController::AddInitialZone(const TVoxelIndex& ZoneIndex) {
	InitialLoadSet.insert(ZoneIndex);
}

void ASandboxTerrainController::SpawnInitialZone() {
	const int32 DebugArea = CVarDebugArea.GetValueOnGameThread();
	if (DebugArea > 0) {
		TArray<TSpawnZoneParam> SpawnList;

		if (DebugArea == 1) {
			SpawnList.Add(TSpawnZoneParam(TVoxelIndex(0, 0, 0)));
		}

		if (DebugArea == 2) {
			const int SZ = ActiveAreaDepth * USBT_ZONE_SIZE;
			const int S = 1;
			for (auto Z = SZ; Z >= -SZ; Z--) {
				for (auto X = -S; X <= S; X++) {
					for (auto Y = -S; Y <= S; Y++) {
						SpawnList.Add(TSpawnZoneParam(TVoxelIndex(X, Y, Z)));
					}
				}
			}
		}

		BatchSpawnZone(SpawnList);
		return;
	}

	TArray<TSpawnZoneParam> SpawnList;
	for (const auto& ZoneIndex : InitialLoadSet) {
		SpawnList.Add(TSpawnZoneParam(ZoneIndex));
	}

	BatchSpawnZone(SpawnList);
}

// always in game thread
UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector Pos) {
	if (!IsInGameThread()) {
		return nullptr;
	}
    
    TVoxelIndex Index = GetZoneIndex(Pos);
	auto* Zone = GetZoneByVectorIndex(Index);
	if (GetZoneByVectorIndex(Index)) {
		return Zone;
	}

    FVector IndexTmp(Index.X, Index.Y,Index.Z);
    FString ZoneName = FString::Printf(TEXT("Zone [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
    UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*ZoneName));
    if (ZoneComponent) {
		ZoneComponent->SetIsReplicated(true);
        ZoneComponent->RegisterComponent();
        //ZoneComponent->SetRelativeLocation(pos);
        ZoneComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
        ZoneComponent->SetWorldLocation(Pos);
		ZoneComponent->SetMobility(EComponentMobility::Movable);
		zone_counter++;

        FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
        UVoxelMeshComponent* TerrainMeshComp = NewObject<UVoxelMeshComponent>(this, FName(*TerrainMeshCompName));
		TerrainMeshComp->SetIsReplicated(true);
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
// basic coordinates and parameters
//======================================================================================================================================================================

TVoxelIndex ASandboxTerrainController::GetZoneIndex(const FVector& Pos) {
	const FVector Tmp = sandboxGridIndex(Pos, USBT_ZONE_SIZE);
	return TVoxelIndex(Tmp.X, Tmp.Y, Tmp.Z);
}

FVector ASandboxTerrainController::GetZonePos(const TVoxelIndex& Index) {
	return FVector((float)Index.X * USBT_ZONE_SIZE, (float)Index.Y * USBT_ZONE_SIZE, (float)Index.Z * USBT_ZONE_SIZE);
}

UTerrainZoneComponent* ASandboxTerrainController::GetZoneByVectorIndex(const TVoxelIndex& Index) {
	return TerrainData->GetZone(Index);
}

TVoxelDataInfoPtr ASandboxTerrainController::GetVoxelDataInfo(const TVoxelIndex& Index) {
    return TerrainData->GetVoxelDataInfo(Index);
}

uint32 ASandboxTerrainController::GetRegionSize() {
	return 100;
}

TVoxelIndex ASandboxTerrainController::ClcRegionByZoneIndex(const TVoxelIndex& ZoneIndex) {
	const int N = GetRegionSize() * 2 + 1;
	float X = (float)ZoneIndex.X / (float)N;
	float Y = (float)ZoneIndex.Y / (float)N;

	if (X > 0) {
		X += 0.5f;
	} else {
		X -= 0.5f;
	}
	
	if (Y > 0) {
		Y += 0.5f;
	} else {
		Y -= 0.5f;
	}

	return TVoxelIndex(X, Y, 0);
}


TVoxelIndex ASandboxTerrainController::ClcRegionOrigin(const TVoxelIndex& RegionIndex) {
	const int N = GetRegionSize() * 2 + 1;
	TVoxelIndex Tmp(RegionIndex.X, RegionIndex.Y, 0);
	Tmp =  Tmp * N;
	return Tmp;
}

//======================================================================================================================================================================
// invoke async
//======================================================================================================================================================================


void ASandboxTerrainController::InvokeSafe(std::function<void()> Function) {
    if (IsInGameThread()) {
        Function();
    } else {
        AsyncTask(ENamedThreads::GameThread, [=]() { Function(); });
    }
}

void ASandboxTerrainController::AddTaskToConveyor(std::function<void()> Function) {
	if (bEnableConveyor) {
		Conveyor->push(Function);
	} else {
		if (IsInGameThread()) {
			Function();
		} else {
			UE_LOG(LogVt, Error, TEXT("Conveyor is disabled. Attempt to run conveyor task in non game thread"));
		}
	}
}

void ASandboxTerrainController::ExecGameThreadZoneApplyMesh(const TVoxelIndex& Index, UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr) {
	ASandboxTerrainController* Controller = this;

	std::function<void()> Function = [=, this]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {		
				TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
				TVdInfoLockGuard Lock(VdInfoPtr);

				TerrainData->PutMeshDataToCache(Index, MeshDataPtr);

				ApplyTerrainMesh(Zone, MeshDataPtr);
				VdInfoPtr->SetNeedTerrainSave();
				TerrainData->AddSaveIndex(Index);
			}
		} else {
			// TODO remove
			UE_LOG(LogVt, Log, TEXT("ASandboxTerrainController::ExecGameThreadZoneApplyMesh - game shutdown"));
		}
	};

	AddTaskToConveyor(Function);
}

//TODO move to conveyor
void ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh(const TVoxelIndex& Index, TMeshDataPtr MeshDataPtr, const bool bIsNewGenerated, const bool bIsChanged) {
	FVector ZonePos = GetZonePos(Index);
	ASandboxTerrainController* Controller = this;

	std::function<void()> Function = [=, this]() {
		if (!bIsGameShutdown) {
			if (MeshDataPtr) {
				TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);
				TVdInfoLockGuard Lock(VdInfoPtr);
				//TerrainData->PutMeshDataToCache(Index, MeshDataPtr);

				UTerrainZoneComponent* Zone = AddTerrainZone(ZonePos);
				if (Zone) {
					ApplyTerrainMesh(Zone, MeshDataPtr);

					if (bIsChanged) {
						TerrainData->PutMeshDataToCache(Index, MeshDataPtr);
						VdInfoPtr->SetNeedTerrainSave();
						TerrainData->AddSaveIndex(Index);
					}

					if (bIsNewGenerated) {
						OnGenerateNewZone(Index, Zone);
					} else {
						OnLoadZone(Index, Zone);
					}
				}
			}
		} else {
			// TODO remove
			UE_LOG(LogVt, Warning, TEXT("ASandboxTerrainController::ExecGameThreadAddZoneAndApplyMesh - game shutdown"));
		}
	};

	//InvokeSafe(Function);
	AddTaskToConveyor(Function);
}

void ASandboxTerrainController::AddAsyncTask(std::function<void()> Function) {
	ThreadPool->addTask(Function);
}

//======================================================================================================================================================================
// events
//======================================================================================================================================================================

void ASandboxTerrainController::OnGenerateNewZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

    if (FoliageDataAsset) {
		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
		Zone->SpawnAll(ZoneInstanceObjectMap);
		VdInfoPtr->SetNeedObjectsSave();
    }

	OnFinishGenerateNewZone(Index);
	VdInfoPtr->ClearInstanceObjectMap();
	VdInfoPtr->SetNeedTerrainSave();
	TerrainData->AddSaveIndex(Index);
}

void ASandboxTerrainController::OnLoadZone(const TVoxelIndex& Index, UTerrainZoneComponent* Zone) {
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(Index);

	if (FoliageDataAsset) {
		TInstanceMeshTypeMap& ZoneInstanceObjectMap = *TerrainData->GetOrCreateInstanceObjectMap(Index);
		Zone->SpawnAll(ZoneInstanceObjectMap);
	}

	OnFinishLoadZone(Index);
	VdInfoPtr->ClearInstanceObjectMap();
}

void ASandboxTerrainController::OnFinishAsyncPhysicsCook(const TVoxelIndex& ZoneIndex) {

}


FORCEINLINE float ASandboxTerrainController::ClcGroundLevel(const FVector& V) {
	//return Generator->GroundLevelFuncLocal(V);
	return 0;
}

//======================================================================================================================================================================
// virtual functions
//======================================================================================================================================================================


void ASandboxTerrainController::OnOverlapActorTerrainEdit(const FOverlapResult& OverlapResult, const FVector& Pos) {

}

void ASandboxTerrainController::OnFinishGenerateNewZone(const TVoxelIndex& Index) {

}

bool ASandboxTerrainController::OnZoneSoftUnload(const TVoxelIndex& ZoneIndex) {
	return true;
}

void ASandboxTerrainController::OnRestoreZoneSoftUnload(const TVoxelIndex& ZoneIndex) {

}

bool ASandboxTerrainController::OnZoneHardUnload(const TVoxelIndex& ZoneIndex) {
	return true;
}

void ASandboxTerrainController::OnFinishLoadZone(const TVoxelIndex& Index) {

}

void ASandboxTerrainController::OnFinishInitialLoad() {

}

void ASandboxTerrainController::OnDestroyInstanceMesh(UTerrainInstancedStaticMesh* InstancedMeshComp, int32 ItemIndex) {

}

void ASandboxTerrainController::GetAnchorObjectsLocation(TArray<FVector>& List) const {

}

//======================================================================================================================================================================
// Perlin noise according seed
//======================================================================================================================================================================

// TODO use seed
float ASandboxTerrainController::PerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
	if (GeneratorComponent) {
		return GeneratorComponent->PerlinNoise(Pos, PositionScale, ValueScale);
	}

	return 0;
}

// range 0..1
float ASandboxTerrainController::NormalizedPerlinNoise(const FVector& Pos, const float PositionScale, const float ValueScale) const {
	if (GeneratorComponent) {
		return GeneratorComponent->NormalizedPerlinNoise(Pos, PositionScale, ValueScale);
	}

	return 0;
}

//======================================================================================================================================================================
// generate mesh
//======================================================================================================================================================================

std::shared_ptr<TMeshData> ASandboxTerrainController::GenerateMesh(TVoxelData* Vd) {
	double Start = FPlatformTime::Seconds();

	if (!Vd) {
		UE_LOG(LogVt, Log, TEXT("ASandboxTerrainController::GenerateMesh - NULL voxel data!"));
		return nullptr;
	}

	if (Vd->getDensityFillState() == TVoxelDataFillState::ZERO || Vd->getDensityFillState() == TVoxelDataFillState::FULL) {
		return nullptr;
	}

	TVoxelDataParam Vdp;

	if (USBT_ENABLE_LOD) {
		Vdp.bGenerateLOD = true;
		Vdp.collisionLOD = 0;
	} else {
		Vdp.bGenerateLOD = false;
		Vdp.collisionLOD = 0;
	}

	TMeshDataPtr MeshDataPtr = sandboxVoxelGenerateMesh(*Vd, Vdp);
	MeshDataPtr->BaseMaterialId = Vd->getBaseMatId();

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	MeshDataPtr->TimeStamp = End;

	//UE_LOG(LogVt, Log, TEXT("generateMesh -------------> %f %f %f --> %f ms"), Vd->getOrigin().X, Vd->getOrigin().Y, Vd->getOrigin().Z, Time);
	return MeshDataPtr;
}

FSandboxFoliage ASandboxTerrainController::GetFoliageById(uint32 FoliageId) const {
	return FoliageMap[FoliageId];
}

void ASandboxTerrainController::MarkZoneNeedsToSaveObjects(const TVoxelIndex& ZoneIndex) {
	//TODO check
	TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
	TVdInfoLockGuard Lock(VdInfoPtr);

	VdInfoPtr->SetChanged();
	VdInfoPtr->SetNeedObjectsSave();
	TerrainData->AddSaveIndex(ZoneIndex);
}

const FTerrainInstancedMeshType* ASandboxTerrainController::GetInstancedMeshType(uint32 MeshTypeId, uint32 MeshVariantId) const {
	uint64 MeshCode = FTerrainInstancedMeshType::ClcMeshTypeCode(MeshTypeId, MeshVariantId);
	const FTerrainInstancedMeshType* MeshType = InstMeshMap.Find(MeshCode);
	return MeshType;
}

float ASandboxTerrainController::GetGroundLevel(const FVector& Pos) {

	// TODO fixme on client side
	const UTerrainGeneratorComponent* Generator = GetTerrainGenerator();
	if (Generator) {
		const TVoxelIndex ZoneIndex = GetZoneIndex(Pos);
		return Generator->GroundLevelFunction(ZoneIndex, Pos);
	}

	return 0;
}

FTerrainDebugInfo ASandboxTerrainController::GetMemstat() {
	return FTerrainDebugInfo{ vd::tools::memory::getVdCount(), md_counter.load(), cd_counter.load(), (int)Conveyor->size(), ThreadPool->size(), TerrainData->SyncMapSize(), zone_counter.load()};
}

void ASandboxTerrainController::UE51MaterialIssueWorkaround() {
	if (GetNetMode() == NM_DedicatedServer) {
		return;
	}

	double Start = FPlatformTime::Seconds();

	TArray<UTerrainZoneComponent*> Components;
	GetComponents<UTerrainZoneComponent>(Components);
	for (UTerrainZoneComponent* ZoneComponent : Components) {
		FVector ZonePos = ZoneComponent->GetComponentLocation();
		const TVoxelIndex ZoneIndex = GetZoneIndex(ZonePos);

		TVoxelDataInfoPtr VdInfoPtr = TerrainData->GetVoxelDataInfo(ZoneIndex);
		VdInfoPtr->Lock();

		auto MeshDataPtr = VdInfoPtr->GetMeshDataCache();
		if (MeshDataPtr != nullptr) {
			ApplyTerrainMesh(ZoneComponent, MeshDataPtr, true);
		}

		VdInfoPtr->Unlock();
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogVt, Log, TEXT("UE51MaterialIssueWorkaround --> %f ms"), Time);
}

void ASandboxTerrainController::LoadConsoleVars() {
	const int32 MainDistanceOverride = CVarMainDistance.GetValueOnGameThread();
	if (MainDistanceOverride > 0) {
		ActiveAreaSize = MainDistanceOverride;
	}

	const int32 AutoSavePeriodOverride = CVarAutoSavePeriod.GetValueOnGameThread();
	if (AutoSavePeriodOverride > 0) {
		if (AutoSavePeriod != AutoSavePeriodOverride) {
			UE_LOG(LogVt, Warning, TEXT("Override AutoSavePeriod = %d <- %d"), AutoSavePeriod, AutoSavePeriodOverride);
		}
		AutoSavePeriod = AutoSavePeriodOverride;
	}

	const int32 LodRatioOverride = CVarLodRatio.GetValueOnGameThread();
	if (LodRatioOverride > 0) {
		if (LodRatio != LodRatioOverride) {
			UE_LOG(LogVt, Warning, TEXT("Override LodRatio = %f <- %f"), (float)LodRatio, (float)LodRatioOverride);
		}
		LodRatio = LodRatioOverride;
	}

}

bool ASandboxTerrainController::IsDebugModeOn() {
	const int32 DebugArea = CVarDebugArea.GetValueOnGameThread();
	return DebugArea > 0;
}

int ASandboxTerrainController::CheckPlayerPositionZone(const FVector& Pos) {
	TVoxelIndex ZoneIndex = GetZoneIndex(Pos);
	auto State = TerrainData->GetVoxelDataInfo(ZoneIndex)->DataState;

	if (TerrainData->GetVoxelDataInfo(ZoneIndex)->DataState == TVoxelDataState::UNDEFINED) {
		return -1;
	}

	if (TerrainData->GetVoxelDataInfo(ZoneIndex)->GetFlagInternal() == 2) {
		return -2;
	}

	return 0;
}

ASandboxTerrainNetProxy::ASandboxTerrainNetProxy(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ASandboxTerrainNetProxy::BeginPlay() {
	Super::BeginPlay();

	for (TActorIterator<ASandboxTerrainController> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
		ASandboxTerrainController* Ctrl = Cast<ASandboxTerrainController>(*ActorItr);
		if (Ctrl) {
			Controller = Ctrl;
			break;
		}
	}
}