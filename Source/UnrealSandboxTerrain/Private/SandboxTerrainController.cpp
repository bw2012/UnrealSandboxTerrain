
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "SandboxTerrainZone.h"
#include "SandboxVoxeldata.h"
#include "SandboxAsyncHelpers.h"
#include <cmath>

ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	zone_queue.Empty(); zone_queue_pos = 0;
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

	if (sandboxAsyncIsNextZoneMakeTask()) {
		ZoneMakeTask zone_make_task = sandboxAsyncGetZoneMakeTask();
		ASandboxTerrainZone* zone = getZoneByVectorIndex(getZoneIndex(zone_make_task.origin));
		if (zone != NULL) {
			if (zone_make_task.mesh_data != NULL) {
				zone->applyTerrainMesh(zone_make_task.mesh_data);

				if (zone_make_task.isNew) {
					//zone->generateZoneObjects();
					zone->isLoaded = true;
				}
				else {
					if (!zone->isLoaded) {
						// load zone json here
						FVector v = zone->GetActorLocation();
						v /= 1000;
						//UE_LOG(LogTemp, Warning, TEXT("zone: %s -> %d objects"), *sandboxZoneJsonFullPath(v.X, v.Y, v.Z), zone->save_list.Num());
						//loadZoneJson(sandboxZoneJsonFullPath(v.X, v.Y, v.Z));
						zone->isLoaded = true;
					}
				}

				//delete zone_make_task.mesh_data;

				//FIXME delete mesh data after finish
			}
			else {
				zone->makeTerrain();
			}
		}
	}
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

ASandboxTerrainZone* ASandboxTerrainController::getZoneByVectorIndex(FVector v) {
	if (ASandboxTerrainController::terrain_zone_map.Contains(v)) {
		return ASandboxTerrainController::terrain_zone_map[v];
	}

	return NULL;
}

ASandboxTerrainZone* ASandboxTerrainController::addTerrainZone(FVector pos) {
	FVector index = getZoneIndex(pos);

	FTransform transform(FRotator(0, 0, 0), FVector(pos.X, pos.Y, pos.Z));
	UClass *zone_class = ASandboxTerrainZone::StaticClass();
	ASandboxTerrainZone* zone = (ASandboxTerrainZone*)GetWorld()->SpawnActor(zone_class, &transform);
	ASandboxTerrainController::terrain_zone_map.Add(FVector(index.X, index.Y, index.Z), zone);

	return zone;
}

template<class H>
class FTerrainEditThread : public FRunnable {
public:
	H zone_handler;
	FVector origin;
	float radius;
	float strength;

	virtual uint32 Run() {
		ASandboxTerrainController::instance->editTerrain(origin, radius, strength, zone_handler);
		return 0;
	}
};

void ASandboxTerrainController::digTerrainRoundHole(FVector origin, float r, float strength) {
	UE_LOG(LogTemp, Warning, TEXT("digTerrainRoundHole -> %f %f %f"), origin.X, origin.Y, origin.Z);

	//if (GetWorld() == NULL) return;

	struct ZoneHandler {
		bool changed;
		bool operator()(VoxelData* vd, FVector v, float radius, float strength) {
			changed = false;
			//VoxelData* vd = zone->getVoxelData();
			for (int x = 0; x < vd->num(); x++) {
				for (int y = 0; y < vd->num(); y++) {
					for (int z = 0; z < vd->num(); z++) {
						float density = vd->getDensity(x, y, z);
						FVector o = vd->voxelIndexToVector(x, y, z);
						o += vd->getOrigin();
						o -= v;

						float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
						if (rl < radius) {
							float d = density - 1 / rl * strength;
							vd->setDensity(x, y, z, d);
							changed = true;
						}
					}
				}
			}

			return changed;
		}
	};

	struct ZoneHandler zh;
	FTerrainEditThread<ZoneHandler>* te = new FTerrainEditThread<ZoneHandler>();
	te->zone_handler = zh;
	te->origin = origin;
	te->radius = r;
	te->strength = strength;

	FString thread_name = FString::Printf(TEXT("thread"));
	FRunnableThread* thread = FRunnableThread::Create(te, *thread_name);
	//FIXME delete thread after finish

	/*
	FVector ttt(origin);
	ttt.Z -= 10;
	TArray<struct FHitResult> OutHits;
	bool overlap = GetWorld()->SweepMultiByChannel(OutHits, origin, ttt, FQuat(), ECC_EngineTraceChannel1, FCollisionShape::MakeSphere(r));
	if (overlap) {
		for (auto item : OutHits) {
			AActor* actor = item.GetActor();
			ASandboxObject* obj = Cast<ASandboxObject>(actor);
			if (obj != NULL) {
				UE_LOG(LogTemp, Warning, TEXT("overlap %s -> %d"), *obj->GetName(), item.Item);
				obj->informTerrainChange(item.Item);
			}
		}
	}
	*/
}

template<class H>
void ASandboxTerrainController::editTerrain(FVector v, float radius, float s, H handler) {
	FVector base_zone_index = getZoneIndex(v);
	
	static const float vvv[3] = { -1, 0, 1 };
	for (float x : vvv) {
		for (float y : vvv) {
			for (float z : vvv) {
				FVector zone_index(x, y, z);
				zone_index += base_zone_index;

				ASandboxTerrainZone* zone = getZoneByVectorIndex(zone_index);

				if (zone == NULL) {
					UE_LOG(LogTemp, Warning, TEXT("zone not found %f %f %f"), zone_index.X, zone_index.Y, zone_index.Z);
					continue;
				}

				VoxelData* vd = zone->getVoxelData();

				if (vd == NULL) {
					UE_LOG(LogTemp, Warning, TEXT("voxel data not found %f %f %f"), zone_index.X, zone_index.Y, zone_index.Z);
					continue;
				}


				bool is_changed = handler(vd, v, radius, s);
				if (is_changed) {
					vd->setChanged();
					MeshData* md = zone->generateMesh(*vd);
					vd->resetLastMeshRegenerationTime();

					ZoneMakeTask zone_make_task;
					zone_make_task.origin = zone->GetActorLocation();
					zone_make_task.mesh_data = md;

					sandboxAsyncAddZoneMakeTask(zone_make_task);
				}

			}
		}
	}

}