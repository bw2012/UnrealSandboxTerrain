
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "TerrainZoneComponent.h"
#include "SandboxVoxeldata.h"
#include <cmath>
#include "DrawDebugHelpers.h"
#include "Async.h"

#include "SandboxTerrainMeshComponent.h"


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
			VoxelData* new_vd = controller->createZoneVoxeldata(v);
			if (new_vd->getDensityFillState() == VoxelDataFillState::MIX) {
				UTerrainZoneComponent* zone = controller->getZoneByVectorIndex(index);
				if (zone == NULL) {
					controller->invokeLazyZoneAsync(index);
				} else {
					zone->setVoxelData(new_vd);
					std::shared_ptr<MeshData> md_ptr = zone->generateMesh();
					zone->getVoxelData()->resetLastMeshRegenerationTime();
					controller->invokeZoneMeshAsync(zone, md_ptr);
				}
			}

			controller->OnLoadZoneProgress(i, zone_list.Num());
		}

		controller->OnLoadZoneListFinished();

		state = TH_STATE_FINISHED;
		return 0;
	}

};


ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	TerrainSize = 5;
	ZoneGridDimension = EVoxelDimEnum::VS_64;
	bEnableLOD = false;
}

ASandboxTerrainController::ASandboxTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	TerrainSize = 5;
	ZoneGridDimension = EVoxelDimEnum::VS_64;
	bEnableLOD = false;
}

void ASandboxTerrainController::BeginPlay() {
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController ---> BeginPlay"));

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

	initial_zone_loader->controller = this;
	if (!GenerateOnlySmallSpawnPoint) {
		for (int num = 0; num < TerrainSize; num++) {
			int s = num;
			for (int x = -s; x <= s; x++) {
				for (int y = -s; y <= s; y++) {
					for (int z = -s; z <= s; z++) {
						FVector zone_index = FVector(x, y, z);
						UTerrainZoneComponent* zone = getZoneByVectorIndex(zone_index);
						VoxelData* vd = GetTerrainVoxelDataByIndex(zone_index);
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

	if (GetWorld()->GetAuthGameMode() == NULL) {
		return;
	}

	for (auto& Elem : VoxelDataMap) {
		VoxelData* voxel_data = Elem.Value;

		if (voxel_data->isChanged()) {
			// save voxel data
			FVector index = getZoneIndex(voxel_data->getOrigin());
			FString fileName = getZoneFileName(index.X, index.Y, index.Z);

			UE_LOG(LogTemp, Warning, TEXT("save voxeldata -> %f %f %f"), index.X, index.Y, index.Z);
			sandboxSaveVoxelData(*voxel_data, fileName);
		}
		
		//TODO replace with share pointer
		VoxelDataMap.Remove(Elem.Key);
		delete voxel_data;
	}
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	
	if (HasNextAsyncTask()) {
		TerrainControllerTask task = GetAsyncTask();

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

void ASandboxTerrainController::spawnInitialZone() {
	const int s = static_cast<int>(TerrainInitialArea);

	UE_LOG(LogTemp, Warning, TEXT("TerrainInitialArea = %d"), s);

	if (s > 0) {
		for (auto x = -s; x <= s; x++) {
			for (auto y = -s; y <= s; y++) {
				for (auto z = -s; z <= s; z++) {
					FVector v = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
					//TODO maybe pass index?
					VoxelData* vd = createZoneVoxeldata(v);

					if (vd->getDensityFillState() == VoxelDataFillState::MIX) {
						UTerrainZoneComponent* zone = addTerrainZone(v);
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
	} else {
		FVector v = FVector(0);
		//TODO maybe pass index?
		VoxelData* vd = createZoneVoxeldata(v);

		if (vd->getDensityFillState() == VoxelDataFillState::MIX) {
			UTerrainZoneComponent* zone = addTerrainZone(v);
			zone->setVoxelData(vd);
			zone->makeTerrain();
		}
	}
	
}

FVector ASandboxTerrainController::getZoneIndex(FVector v) {
	return sandboxGridIndex(v, 1000);
}

UTerrainZoneComponent* ASandboxTerrainController::getZoneByVectorIndex(FVector index) {
	if (terrain_zone_map.Contains(index)) {
		return terrain_zone_map[index];
	}

	return NULL;
}

UTerrainZoneComponent* ASandboxTerrainController::addTerrainZone(FVector pos) {
	FVector index = getZoneIndex(pos);

	FString zone_name = FString::Printf(TEXT("Zone-%d"), FPlatformTime::Seconds());
	UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*zone_name));
	if (ZoneComponent) {
		ZoneComponent->RegisterComponent();
		ZoneComponent->SetWorldLocation(pos);

		FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh-%d"), FPlatformTime::Seconds());
		USandboxTerrainMeshComponent* TerrainMeshComp = NewObject<USandboxTerrainMeshComponent>(this, FName(*TerrainMeshCompName));
		TerrainMeshComp->RegisterComponent();
		TerrainMeshComp->SetMobility(EComponentMobility::Stationary);
		TerrainMeshComp->AttachTo(ZoneComponent);

		FString CollisionMeshCompName = FString::Printf(TEXT("CollisionMesh-%d"), FPlatformTime::Seconds());
		USandboxTerrainCollisionComponent* CollisionMeshComp = NewObject<USandboxTerrainCollisionComponent>(this, FName(*CollisionMeshCompName));
		CollisionMeshComp->RegisterComponent();
		CollisionMeshComp->SetMobility(EComponentMobility::Stationary);
		CollisionMeshComp->SetCanEverAffectNavigation(true);
		CollisionMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
		CollisionMeshComp->AttachTo(ZoneComponent);

		ZoneComponent->MainTerrainMesh = TerrainMeshComp;
		ZoneComponent->CollisionMesh = CollisionMeshComp;
	}

	terrain_zone_map.Add(FVector(index.X, index.Y, index.Z), ZoneComponent);

	if(ShowZoneBounds) DrawDebugBox(GetWorld(), pos, FVector(500), FColor(255, 0, 0, 100), true);

	return ZoneComponent;
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
	//if (GetWorld() == NULL) return;

	struct ZoneHandler {
		bool changed;
		bool enableLOD = false;
		bool operator()(VoxelData* vd, FVector v, float radius, float strength) {
			changed = false;
			vd->clearSubstanceCache();

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

						if (enableLOD) {
							vd->performSubstanceCacheLOD(x, y, z); 
						} else {
							vd->performSubstanceCacheNoLOD(x, y, z);
						}
	
					}
				}
			}

			return changed;
		}
	} zh;

	zh.enableLOD = bEnableLOD;
	ASandboxTerrainController::performTerrainChange(origin, r, strength, zh);
}

void ASandboxTerrainController::digTerrainCubeHole(FVector origin, float r, float strength) {

	struct ZoneHandler {
		bool changed;
		bool enableLOD = false;
		bool not_empty = false;
		bool operator()(VoxelData* vd, FVector v, float radius, float strength) {
			changed = false;

			if (!not_empty) {

				vd->clearSubstanceCache();

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

							if (enableLOD) {
								vd->performSubstanceCacheLOD(x, y, z);
							}
							else {
								vd->performSubstanceCacheNoLOD(x, y, z);
							}
						}
					}
				}
			}

			return changed;
		}
	} zh;

	zh.enableLOD = bEnableLOD;
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


FORCEINLINE float squared(float v) {
	return v * v;
}

bool isCubeIntersectSphere(FVector lower, FVector upper, FVector sphereOrigin, float radius) {
	float ds = radius * radius;

	if (sphereOrigin.X < lower.X) ds -= squared(sphereOrigin.X - lower.X);
	else if (sphereOrigin.X > upper.X) ds -= squared(sphereOrigin.X - upper.X);

	if (sphereOrigin.Y < lower.Y) ds -= squared(sphereOrigin.Y - lower.Y);
	else if (sphereOrigin.Y > upper.Y) ds -= squared(sphereOrigin.Y - upper.Y);

	if (sphereOrigin.Z < lower.Z) ds -= squared(sphereOrigin.Z - lower.Z);
	else if (sphereOrigin.Z > upper.Z) ds -= squared(sphereOrigin.Z - upper.Z);

	return ds > 0;
}

template<class H>
void ASandboxTerrainController::editTerrain(FVector v, float radius, float s, H handler) {
	double start = FPlatformTime::Seconds();
	
	FVector base_zone_index = getZoneIndex(v);

	static const float vvv[3] = { -1, 0, 1 };
	for (float x : vvv) {
		for (float y : vvv) {
			for (float z : vvv) {
				FVector zone_index(x, y, z);
				zone_index += base_zone_index;

				UTerrainZoneComponent* zone = getZoneByVectorIndex(zone_index);
				VoxelData* vd = GetTerrainVoxelDataByIndex(zone_index);

				if (zone == NULL) {
					if (vd != NULL) {
						bool is_changed = handler(vd, v, radius, s);
						if (is_changed) {
							vd->setChanged();
							invokeLazyZoneAsync(zone_index);
						}

						continue;
					} else {
						continue;
					}
				}

				if (vd == NULL) {
					UE_LOG(LogTemp, Warning, TEXT("ERROR: voxel data not found --> %.8f %.8f %.8f "), zone_index.X, zone_index.Y, zone_index.Z);
					continue;
				}

				if (!isCubeIntersectSphere(vd->getLower(), vd->getUpper(), v, radius)) {
					//UE_LOG(LogTemp, Warning, TEXT("skip: voxel data --> %.8f %.8f %.8f "), zone_index.X, zone_index.Y, zone_index.Z);
					continue;
				}

				bool is_changed = handler(vd, v, radius, s);
				if (is_changed) {
					vd->setChanged();
					vd->setCacheToValid();
					std::shared_ptr<MeshData> md_ptr = zone->generateMesh();
					vd->resetLastMeshRegenerationTime();
					invokeZoneMeshAsync(zone, md_ptr);
				}

			}
		}
	}

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::editTerrain-------------> %f %f %f --> %f ms"), v.X, v.Y, v.Z, time);
}


void ASandboxTerrainController::invokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<MeshData> mesh_data_ptr) {
	TerrainControllerTask task;
	task.f = [=]() {
		if (mesh_data_ptr) {
			zone->applyTerrainMesh(mesh_data_ptr);
		}
	};

	AddAsyncTask(task);
}

void ASandboxTerrainController::invokeLazyZoneAsync(FVector index) {
	TerrainControllerTask task;
	FVector v = FVector((float)(index.X * 1000), (float)(index.Y * 1000), (float)(index.Z * 1000));
	VoxelData* vd = GetTerrainVoxelDataByIndex(index);

	if (vd == NULL) {
		UE_LOG(LogTemp, Warning, TEXT("FAIL"));
		return;
	}

	task.f = [=]() {
		UTerrainZoneComponent* zone = addTerrainZone(v);
		zone->setVoxelData(vd);

		std::shared_ptr<MeshData> md_ptr = zone->generateMesh();
		vd->resetLastMeshRegenerationTime();
		zone->applyTerrainMesh(md_ptr);
	};

	AddAsyncTask(task);
}

VoxelData* ASandboxTerrainController::createZoneVoxeldata(FVector location) {
	double start = FPlatformTime::Seconds();
	//	if (GetWorld()->GetAuthGameMode() == NULL) {
	//		return;
	//	}

	bool isNew = false;

	int dim = static_cast<int>(ZoneGridDimension);
	VoxelData* vd = new VoxelData(dim, 100 * 10);
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
	vd->setCacheToValid();

	RegisterTerrainVoxelData(vd, index);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::createZoneVoxeldata() -> %.8f %.8f %.8f --> %f ms"), index.X, index.Y, index.Z, time);

	return vd;
}

void ASandboxTerrainController::generateTerrain(VoxelData &voxel_data) {
	double start = FPlatformTime::Seconds();
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

				voxel_data.performSubstanceCacheLOD(x, y, z);

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

	voxel_data.setCacheToValid();

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::generateTerrain ----> %f %f %f --> %f ms"), voxel_data.getOrigin().X, voxel_data.getOrigin().Y, voxel_data.getOrigin().Z, time);

}


void ASandboxTerrainController::OnLoadZoneProgress(int progress, int total) {

}


void ASandboxTerrainController::OnLoadZoneListFinished() {

}


void ASandboxTerrainController::AddAsyncTask(TerrainControllerTask zone_make_task) {
	AsyncTaskListMutex.lock();
	AsyncTaskList.push(zone_make_task);
	AsyncTaskListMutex.unlock();
}

TerrainControllerTask ASandboxTerrainController::GetAsyncTask() {
	AsyncTaskListMutex.lock();
	TerrainControllerTask NewTask = AsyncTaskList.front();
	AsyncTaskList.pop();
	AsyncTaskListMutex.unlock();

	return NewTask;
}

bool ASandboxTerrainController::HasNextAsyncTask() {
	return AsyncTaskList.size() > 0;
}

void ASandboxTerrainController::RegisterTerrainVoxelData(VoxelData* vd, FVector index) {
	VoxelDataMapMutex.lock();
	VoxelDataMap.Add(index, vd);
	VoxelDataMapMutex.unlock();
}

VoxelData* ASandboxTerrainController::GetTerrainVoxelDataByPos(FVector point) {
	FVector index = sandboxSnapToGrid(point, 1000) / 1000;

	VoxelDataMapMutex.lock();
	if (VoxelDataMap.Contains(index)) {
		VoxelData* vd = VoxelDataMap[index];
		VoxelDataMapMutex.unlock();
		return vd;
	}

	VoxelDataMapMutex.unlock();
	return NULL;
}

VoxelData* ASandboxTerrainController::GetTerrainVoxelDataByIndex(FVector index) {
	VoxelDataMapMutex.lock();
	if (VoxelDataMap.Contains(index)) {
		VoxelData* vd = VoxelDataMap[index];
		VoxelDataMapMutex.unlock();
		return vd;
	}

	VoxelDataMapMutex.unlock();
	return NULL;
}