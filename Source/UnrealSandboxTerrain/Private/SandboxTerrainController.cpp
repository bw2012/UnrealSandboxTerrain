#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "SandboxVoxeldata.h"

ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

ASandboxTerrainController::ASandboxTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	zone_queue.Empty(); zone_queue_pos = 0;
}

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController ---> BeginPlay"));

	ASandboxTerrainController::instance = this;


	UWorld* const world = GetWorld();
	if (!world) {
		return;
	}

	if (GetWorld()->GetAuthGameMode() != NULL) {
		UE_LOG(LogTemp, Warning, TEXT("SERVER"));
	} else {
		UE_LOG(LogTemp, Warning, TEXT("CLIENT"));
		//return;
	}

	spawnInitialZone();

	zone_queue.Empty(); zone_queue_pos = 0;
	UE_LOG(LogTemp, Warning, TEXT("queue %d %d"), zone_queue.Num(), zone_queue_pos);

	//zone generation list
	//int num = 20;
	if (!GenerateOnlySmallSpawnPoint) {
		for (int num = 0; num < 10; num++) {
			int s = num;
			for (int x = -s; x <= s; x++) {
				for (int y = -s; y <= s; y++) {
					for (int z = -s; z <= s; z++) {
						//float z = 0;
						FVector v = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
						FVector zone_index = FVector(x, y, z);
						//ATerrainZone* zone = getZoneByVectorIndex(zone_index);
						//if (zone == NULL) {
							// Until the end of the process some functions can be unavailable.
							//addTerrainZone(v);
						//	zone_queue.Add(v);
						//}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("zone queue %d"), zone_queue.Num());
	//FLoadAllZonesThread* zone_loader = new FLoadAllZonesThread();

	//zone_loader->lc = this;
	//FRunnableThread* zone_loader_thread = FRunnableThread::Create(zone_loader, TEXT("THREAD_TEST"));
	//FIXME delete thread after finish
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

ASandboxTerrainController* ASandboxTerrainController::instance;
TMap<FVector, ASandboxTerrainZone*> ASandboxTerrainController::terrain_zone_map;

void ASandboxTerrainController::spawnInitialZone() {
	//static const int num = 4;
	static const int s = 1;

	for (auto x = -s; x <= s; x++) {
		for (auto y = -s; y <= s; y++) {
			for (auto z = -s; z <= s; z++) {
				FVector v = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
				ASandboxTerrainZone* zone = addTerrainZone(v);
				bool is_new = zone->fillZone();
				zone->makeTerrain();

				/*
				if (is_new) {
					//zone->generateZoneObjects();
				} else {
					if (!zone->isLoaded) {
						//todo
					}
					FVector v = zone->GetActorLocation();
					v /= 1000;
					//UE_LOG(LogTemp, Warning, TEXT("zone: %s -> %d objects"), *sandboxZoneJsonFullPath(v.X, v.Y, v.Z), zone->save_list.Num());
					//loadZoneJson(sandboxZoneJsonFullPath(v.X, v.Y, v.Z));
					zone->isLoaded = true;
				}
				*/
			}
		}
	}
}

FVector ASandboxTerrainController::getZoneIndex(FVector v) {
	FVector tmp = sandboxSnapToGrid(v, 1000) / 1000;
	return FVector((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
}

ASandboxTerrainZone* ASandboxTerrainController::addTerrainZone(FVector pos) {
	FVector index = getZoneIndex(pos);

	FTransform transform(FRotator(0, 0, 0), FVector(pos.X, pos.Y, pos.Z));
	UClass *zone_class = ASandboxTerrainZone::StaticClass();
	ASandboxTerrainZone* zone = (ASandboxTerrainZone*)GetWorld()->SpawnActor(zone_class, &transform);
	ASandboxTerrainController::terrain_zone_map.Add(FVector(index.X, index.Y, index.Z), zone);

	return zone;
}