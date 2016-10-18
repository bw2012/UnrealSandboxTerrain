
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "SandboxTerrainZone.h"
#include "SandboxVoxeldata.h"
#include "SandboxAsyncHelpers.h"
#include <cmath>
#include "DrawDebugHelpers.h"

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

	//ASandboxTerrainController::instance = this;

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

	//zone generation list
	//int num = 20;
	/*
	if (!GenerateOnlySmallSpawnPoint) {
		for (int num = 0; num < 10; num++) {
			int s = num;
			for (int x = -s; x <= s; x++) {
				for (int y = -s; y <= s; y++) {
					for (int z = -s; z <= s; z++) {
						//float z = 0;
						FVector v = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
						FVector zone_index = FVector(x, y, z);
						ASandboxTerrainZone* zone = getZoneByVectorIndex(zone_index);
						if (zone == NULL) {
							// Until the end of the process some functions can be unavailable.
							//addTerrainZone(v);
							zone_queue.Add(v);
						}
					}
				}
			}
		}
	}
	*/

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

		if (zone_make_task.type == MT_GENERATE_MESH) {
			ASandboxTerrainZone* zone = getZoneByVectorIndex(getZoneIndex(zone_make_task.index));
			if (zone != NULL) {
				if (zone_make_task.mesh_data != NULL) {
					zone->applyTerrainMesh(zone_make_task.mesh_data);
					if (zone_make_task.isNew) {
						//zone->generateZoneObjects();
						zone->isLoaded = true;
					} else {
						if (!zone->isLoaded) {
							// load zone json here
							FVector v = zone->GetActorLocation();
							v /= 1000;
							//UE_LOG(LogTemp, Warning, TEXT("zone: %s -> %d objects"), *sandboxZoneJsonFullPath(v.X, v.Y, v.Z), zone->save_list.Num());
							//loadZoneJson(sandboxZoneJsonFullPath(v.X, v.Y, v.Z));
							zone->isLoaded = true;
						}
					}

					//FIXME refactor mesh_data to shared pointer
					delete zone_make_task.mesh_data;
				} else {
					zone->makeTerrain();
				}
			}
		}

		if (zone_make_task.type == MT_ADD_ZONE) {
			FVector v = FVector((float)(zone_make_task.index.X * 1000), (float)(zone_make_task.index.Y * 1000), (float)(zone_make_task.index.Z * 1000));
			ASandboxTerrainZone* zone = addTerrainZone(v);
			VoxelData* vd = sandboxGetTerrainVoxelDataByIndex(zone_make_task.index);
			zone->setVoxelData(vd);

			MeshData* md = zone->generateMesh(*vd);
			vd->resetLastMeshRegenerationTime();
			zone->applyTerrainMesh(md);

		}


	}
}

SandboxVoxelGenerator ASandboxTerrainController::newTerrainGenerator(VoxelData &voxel_data) {
	return SandboxVoxelGenerator(voxel_data);
};

//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

ASandboxTerrainController* ASandboxTerrainController::GetZoneInstance(AActor* actor) {
	ASandboxTerrainZone* zone = Cast<ASandboxTerrainZone>(actor);
	if (zone == NULL) {
		return NULL;
	}

	return zone->controller;
}

void ASandboxTerrainController::spawnInitialZone() {
	//static const int num = 4;
	const int s = InitialSpawnSize;

	UE_LOG(LogTemp, Warning, TEXT("InitialSpawnSize = %d"), s);

	for (auto x = -s; x <= s; x++) {
		for (auto y = -s; y <= s; y++) {
			for (auto z = -s; z <= s; z++) {
				FVector v = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
				VoxelData* vd = createZoneVoxeldata(v);

				if (vd->getDensityFillState() == VoxelDataFillState::MIX) {
					ASandboxTerrainZone* zone = addTerrainZone(v);
					zone->setVoxelData(vd);
					zone->makeTerrain();
				}

				//bool is_new = zone->fillZone();

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
	ASandboxTerrainZone* zone = (ASandboxTerrainZone*) GetWorld()->SpawnActor(zone_class, &transform);
	zone->controller = this;
	terrain_zone_map.Add(FVector(index.X, index.Y, index.Z), zone);

	if(ShowZoneBounds) DrawDebugBox(GetWorld(), pos, FVector(500), FColor(255, 0, 0, 100), true);

	return zone;
}

template<class H>
class FTerrainEditThread : public FRunnable {
public:
	H zone_handler;
	FVector origin;
	float radius;
	float strength;
	ASandboxTerrainController* instance;

	virtual uint32 Run() {
		instance->editTerrain(origin, radius, strength, zone_handler);
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
	} zh;

	ASandboxTerrainController::performTerrainChange(origin, r, strength, zh);
}

void ASandboxTerrainController::digTerrainCubeHole(FVector origin, float r, float strength) {
	UE_LOG(LogTemp, Warning, TEXT("digTerrainCubeHole -> %f %f %f"), origin.X, origin.Y, origin.Z);

	struct ZoneHandler {
		bool changed;
		bool not_empty = false;
		bool operator()(VoxelData* vd, FVector v, float radius, float strength) {
			changed = false;

			if (!not_empty) {
				for (int x = 0; x < vd->num(); x++) {
					for (int y = 0; y < vd->num(); y++) {
						for (int z = 0; z < vd->num(); z++) {
							FVector o = vd->voxelIndexToVector(x, y, z);
							o += vd->getOrigin();
							o -= v;
							if (o.X < radius && o.X > -radius && o.Y < radius && o.Y > -radius && o.Z < radius && o.Z > -radius) {
								vd->setDensity(x, y, z, 0);
								changed = true;
							}
						}
					}
				}
			}

			return changed;
		}
	} zh;

	ASandboxTerrainController::performTerrainChange(origin, r, strength, zh);
}

template<class H>
void ASandboxTerrainController::performTerrainChange(FVector origin, float radius, float strength, H handler) {
	FTerrainEditThread<H>* te = new FTerrainEditThread<H>();
	te->zone_handler = handler;
	te->origin = origin;
	te->radius = radius;
	te->strength = strength;
	te->instance = this;

	FString thread_name = FString::Printf(TEXT("terrain_change-thread-%d"), FPlatformTime::Seconds());
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
				VoxelData* vd = sandboxGetTerrainVoxelDataByIndex(zone_index);

				if (zone == NULL) {
					if (vd != NULL) {
						bool is_changed = handler(vd, v, radius, s);
						if (is_changed) {
							vd->setChanged();
							UE_LOG(LogTemp, Warning, TEXT("zone not found %f %f %f ---> create"), zone_index.X, zone_index.Y, zone_index.Z);

							ZoneMakeTask zone_make_task;
							zone_make_task.type = MT_ADD_ZONE;
							zone_make_task.index = zone_index;
							sandboxAsyncAddZoneMakeTask(zone_make_task);
						}
						
						continue;
					} else {
						UE_LOG(LogTemp, Warning, TEXT("zone and voxel data not found %f %f %f ---> skip"), zone_index.X, zone_index.Y, zone_index.Z);
						continue;
					}
				}

				bool is_changed = handler(vd, v, radius, s);
				if (is_changed) {
					vd->setChanged();
					MeshData* md = zone->generateMesh(*vd);
					vd->resetLastMeshRegenerationTime();

					ZoneMakeTask zone_make_task;
					zone_make_task.type = MT_GENERATE_MESH;
					zone_make_task.index = zone->GetActorLocation();
					zone_make_task.mesh_data = md;

					sandboxAsyncAddZoneMakeTask(zone_make_task);
				}

			}
		}
	}

}

VoxelData* ASandboxTerrainController::createZoneVoxeldata(FVector location) {
	double start = FPlatformTime::Seconds();
	//	if (GetWorld()->GetAuthGameMode() == NULL) {
	//		return;
	//	}

	bool isNew = false;

	VoxelData* vd = new VoxelData(50, 100 * 10);
	vd->setOrigin(location);

	FVector o = sandboxSnapToGrid(location, 1000) / 1000;
	//FString fileName = sandboxZoneBinaryFileName(o.X, o.Y, o.Z);
	//FString fileFullPath = sandboxZoneBinaryFileFullPath(o.X, o.Y, o.Z);

	//if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*fileFullPath)) {
	//	sandboxLoadVoxelData(*vd, fileName);
	//} else {

	generateTerrain(*vd);

	//	VoxelDataFillState s = vd->getDensityFillState();
	//  sandboxSaveVoxelData(*vd, fileName);
	//	isNew = true;
	//}

	vd->setChanged();
	vd->resetLastSave();

	sandboxRegisterTerrainVoxelData(vd, o);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT(" ASandboxTerrainController::createZoneVoxeldata() -> %f %f %f --> %f ms"), o.X, o.Y, o.Z, time);

	return vd;
}

void ASandboxTerrainController::generateTerrain(VoxelData &voxel_data) {
	SandboxVoxelGenerator generator = newTerrainGenerator(voxel_data);
	//SandboxVoxelGenerator generator(voxel_data);

	TSet<unsigned char> material_list;
	int zc = 0; int fc = 0;

	for (int x = 0; x < voxel_data.num(); x++) {
		for (int y = 0; y < voxel_data.num(); y++) {
			for (int z = 0; z < voxel_data.num(); z++) {
				FVector local = voxel_data.voxelIndexToVector(x, y, z);
				FVector world = local + voxel_data.getOrigin();

				float den = generator.density(local, world);
				unsigned char mat = generator.material(local, world);

				voxel_data.setDensity(x, y, z, den);
				voxel_data.setMaterial(x, y, z, mat);

				if (den == 0) zc++;
				if (den == 1) fc++;
				material_list.Add(mat);
			}
		}
	}

	int s = voxel_data.num() * voxel_data.num() * voxel_data.num();

	if (zc == s) {
		voxel_data.deinitializeDensity(VoxelDataFillState::ZERO);
	}

	if (fc == s) {
		voxel_data.deinitializeDensity(VoxelDataFillState::ALL);
	}

	if (material_list.Num() == 1) {
		unsigned char base_mat = 0;
		for (auto m : material_list) {
			base_mat = m;
			break;
		}
		voxel_data.deinitializeMaterial(base_mat);
	}

}