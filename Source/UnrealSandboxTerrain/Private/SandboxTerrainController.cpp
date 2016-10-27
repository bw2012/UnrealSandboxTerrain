
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "SandboxTerrainZone.h"
#include "SandboxVoxeldata.h"
#include "SandboxAsyncHelpers.h"
#include <cmath>
#include "DrawDebugHelpers.h"
#include "Async.h"


class FLoadInitialZonesThread : public FRunnable {

private:
	 
	volatile int state = TH_STATE_NEW;

	FRunnableThread* thread;

public:

	FLoadInitialZonesThread() {
		thread = NULL;
	}

	~FLoadInitialZonesThread() {
		if (thread != NULL) {
			delete thread;
		}
	}

	TArray<FVector> zone_list;
	ASandboxTerrainController* controller;

	bool IsFinished() {
		return state == TH_STATE_FINISHED;
	}

	virtual void Stop() { 
		if (state == TH_STATE_NEW || state == TH_STATE_RUNNING) {
			state = TH_STATE_STOP;
		}
	}

	virtual void WaitForFinish() {
		while (!IsFinished()) {

		}
	}

	void Start() {
		thread = FRunnableThread::Create(this, TEXT("THREAD_TEST"));
	}

	virtual uint32 Run() {
		state = TH_STATE_RUNNING;
		UE_LOG(LogTemp, Warning, TEXT("zone initial loader %d"), zone_list.Num());
		for (auto i = 0; i < zone_list.Num(); i++) {
			if (!controller->IsValidLowLevel()) {
				// controller is not valid anymore
				state = TH_STATE_FINISHED;
				return 0;
			}

			if (state == TH_STATE_STOP) {
				state = TH_STATE_FINISHED;
				return 0;
			}

			FVector index = zone_list[i];
			FVector v = FVector((float)(index.X * 1000), (float)(index.Y * 1000), (float)(index.Z * 1000));

			//TODO maybe pass index?
			VoxelData* vd = controller->createZoneVoxeldata(v);

			if (vd->getDensityFillState() == VoxelDataFillState::MIX) {
				ASandboxTerrainZone* zone = controller->getZoneByVectorIndex(index);
				if (zone == NULL) {
					controller->invokeLazyZoneAsync(index);
				} else {
					MeshData* md = zone->generateMesh(*vd);
					vd->resetLastMeshRegenerationTime();

					controller->invokeZoneMeshAsync(zone, md);
				}
			}

			controller->OnLoadZoneProgress(i, zone_list.Num());
		}

		controller->OnLoadZoneListFinished();

		state = TH_STATE_FINISHED;
		return 0;
	}

};


ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	ZoneGridSize = 64;
	TerrainSize = 5;
}

ASandboxTerrainController::ASandboxTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	ZoneGridSize = 64;
	TerrainSize = 5;
}

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController ---> BeginPlay"));

	//AddToRoot();

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


	//zone initial generation list
	initial_zone_loader = new FLoadInitialZonesThread();

	//TWeakObjectPtr<ASandboxTerrainController> ptr(this);

	initial_zone_loader->controller = this;
	if (!GenerateOnlySmallSpawnPoint) {
		for (int num = 0; num < TerrainSize; num++) {
			int s = num;
			for (int x = -s; x <= s; x++) {
				for (int y = -s; y <= s; y++) {
					for (int z = -s; z <= s; z++) {
						FVector zone_index = FVector(x, y, z);
						ASandboxTerrainZone* zone = getZoneByVectorIndex(zone_index);
						VoxelData* vd = sandboxGetTerrainVoxelDataByIndex(zone_index);
						if (vd == NULL || (vd->getDensityFillState() == VoxelDataFillState::MIX && zone == NULL)) {
							// Until the end of the process some functions can be unavailable.
							initial_zone_loader->zone_list.Add(zone_index);
						}
					}
				}
			}
		}
	}

	initial_zone_loader->Start();
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	if (initial_zone_loader != NULL) {
		initial_zone_loader->Stop();
		initial_zone_loader->WaitForFinish();
	}
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	
	if (sandboxAsyncIsNextTask()) {
		TerrainControllerTask task = sandboxAsyncGetTask();

		if (task.f) {
			task.f();
		}

		/*
		if (zone_make_task.type == MT_GENERATE_MESH) {

			ASandboxTerrainZone* zone = getZoneByVectorIndex(getZoneIndex(zone_make_task.index));
			if (zone != NULL) {
				if (zone_make_task.mesh_data != NULL) {

					zone_make_task.f();

					//zone->applyTerrainMesh(zone_make_task.mesh_data);

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

			if (vd == NULL) {
				UE_LOG(LogTemp, Warning, TEXT("FAIL"));
				return;
			}

			zone->setVoxelData(vd);

			MeshData* md = zone->generateMesh(*vd);
			vd->resetLastMeshRegenerationTime();
			zone->applyTerrainMesh(md);
		}*/


	}

	
}

//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

SandboxVoxelGenerator ASandboxTerrainController::newTerrainGenerator(VoxelData &voxel_data) {
	return SandboxVoxelGenerator(voxel_data);
};

FString ASandboxTerrainController::getZoneFileName(int tx, int ty, int tz) {
	FString savePath = FPaths::GameSavedDir();

	FString fileName = savePath + TEXT("/Map/") + MapName + TEXT("/zone.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".sbin");
	return fileName;
}

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
				//TODO maybe pass index?
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
	return sandboxGridIndex(v, 1000);
}

ASandboxTerrainZone* ASandboxTerrainController::getZoneByVectorIndex(FVector index) {
	if (terrain_zone_map.Contains(index)) {
		return terrain_zone_map[index];
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
							invokeLazyZoneAsync(zone_index);

							/*
							AsyncTask(ENamedThreads::GameThread, [=]() {
							});
							*/
						}
						
						continue;
					} else {
						UE_LOG(LogTemp, Warning, TEXT("zone and voxel data not found %f %f %f ---> skip"), zone_index.X, zone_index.Y, zone_index.Z);
						continue;
					}
				}

				if (vd == NULL) {
					UE_LOG(LogTemp, Warning, TEXT("ERROR: voxel data not found --> %.8f %.8f %.8f "), zone_index.X, zone_index.Y, zone_index.Z);
					continue;
				}

				bool is_changed = handler(vd, v, radius, s);
				if (is_changed) {
					vd->setChanged();
					MeshData* md = zone->generateMesh(*vd);
					vd->resetLastMeshRegenerationTime();

					invokeZoneMeshAsync(zone, md);
				}

			}
		}
	}

}


void ASandboxTerrainController::invokeZoneMeshAsync(ASandboxTerrainZone* zone, MeshData* md) {
	TerrainControllerTask task;
	task.f = [=]() {
		if (md != NULL) {
			zone->applyTerrainMesh(md);
		}
	};

	sandboxAsyncAddTask(task);
}

void ASandboxTerrainController::invokeLazyZoneAsync(FVector index) {
	TerrainControllerTask task;
	task.f = [=]() {
		FVector v = FVector((float)(index.X * 1000), (float)(index.Y * 1000), (float)(index.Z * 1000));
		ASandboxTerrainZone* zone = addTerrainZone(v);
		VoxelData* vd = sandboxGetTerrainVoxelDataByIndex(index);

		if (vd == NULL) {
			UE_LOG(LogTemp, Warning, TEXT("FAIL"));
			return;
		}

		zone->setVoxelData(vd);

		MeshData* md = zone->generateMesh(*vd);
		vd->resetLastMeshRegenerationTime();
		zone->applyTerrainMesh(md);
	};

	sandboxAsyncAddTask(task);
}

VoxelData* ASandboxTerrainController::createZoneVoxeldata(FVector location) {
	double start = FPlatformTime::Seconds();
	//	if (GetWorld()->GetAuthGameMode() == NULL) {
	//		return;
	//	}

	bool isNew = false;

	VoxelData* vd = new VoxelData(65, 100 * 10);
	vd->setOrigin(location);

	FVector index = getZoneIndex(location);
	FString fileName = getZoneFileName(index.X, index.Y, index.Z);

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*fileName)) {
		sandboxLoadVoxelData(*vd, fileName);
	} else {
		generateTerrain(*vd);
		sandboxSaveVoxelData(*vd, fileName);
		//	isNew = true;
	}

	vd->setChanged();
	vd->resetLastSave();

	sandboxRegisterTerrainVoxelData(vd, index);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::createZoneVoxeldata() -> %.8f %.8f %.8f --> %f ms"), index.X, index.Y, index.Z, time);

	return vd;
}

void ASandboxTerrainController::generateTerrain(VoxelData &voxel_data) {
	SandboxVoxelGenerator generator = newTerrainGenerator(voxel_data);

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


void ASandboxTerrainController::OnLoadZoneProgress(int progress, int total) {

}


void ASandboxTerrainController::OnLoadZoneListFinished() {

}
