
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "TerrainZoneComponent.h"
#include "TerrainRegionComponent.h"
#include "SandboxVoxeldata.h"
#include <cmath>
#include "DrawDebugHelpers.h"
#include "Async.h"

#include "SandboxTerrainMeshComponent.h"


class FLoadInitialZonesThread : public FRunnable {

private:
	 
	volatile int State = TH_STATE_NEW;

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

	TArray<FVector> ZoneList;
	ASandboxTerrainController* Controller;

	bool IsFinished() {
		return State == TH_STATE_FINISHED;
	}

	virtual void Stop() { 
		if (State == TH_STATE_NEW || State == TH_STATE_RUNNING) {
			State = TH_STATE_STOP;
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
		State = TH_STATE_RUNNING;
		UE_LOG(LogTemp, Warning, TEXT("zone initial loader %d"), ZoneList.Num());
		for (auto i = 0; i < ZoneList.Num(); i++) {
			if (!Controller->IsValidLowLevel()) {
				// controller is not valid anymore
				State = TH_STATE_FINISHED;
				return 0;
			}

			if (State == TH_STATE_STOP) {
				State = TH_STATE_FINISHED;
				return 0;
			}

			FVector Index = ZoneList[i];
			FVector Pos = FVector((float)(Index.X * 1000), (float)(Index.Y * 1000), (float)(Index.Z * 1000));

			//TODO maybe pass index?
			TVoxelData* NewVoxelData = Controller->CreateZoneVoxeldata(Pos);
			if (NewVoxelData->getDensityFillState() == TVoxelDataFillState::MIX) {
				UTerrainZoneComponent* Zone = Controller->GetZoneByVectorIndex(Index);
				if (Zone == NULL) {
					Controller->InvokeLazyZoneAsync(Index);
				} else {
					Zone->SetVoxelData(NewVoxelData);
					std::shared_ptr<TMeshData> MeshDataPtr = Zone->GenerateMesh();
					Zone->getVoxelData()->resetLastMeshRegenerationTime();
					Controller->InvokeZoneMeshAsync(Zone, MeshDataPtr);
				}
			}

			Controller->OnLoadZoneProgress(i, ZoneList.Num());
		}

		Controller->OnLoadZoneListFinished();

		State = TH_STATE_FINISHED;
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

void ASandboxTerrainController::PostLoad() {
	Super::PostLoad();

	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController ---> PostLoad"));

#if WITH_EDITOR
	//spawnInitialZone();
#endif

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

	UTerrainRegionComponent* Region = GetOrCreateRegion(FVector(0, 0, 0));
	Region->LoadFile();


	TSet<FVector> InitialZoneSet = spawnInitialZone();
	
	//zone initial generation list
	InitialZoneLoader = new FLoadInitialZonesThread();

	InitialZoneLoader->Controller = this;
	if (!bGenerateOnlySmallSpawnPoint) {
		for (int num = 0; num < TerrainSize; num++) {
			int s = num;
			for (int x = -s; x <= s; x++) {
				for (int y = -s; y <= s; y++) {
					for (int z = -s; z <= s; z++) {
						FVector zone_index = FVector(x, y, z);
						UTerrainZoneComponent* zone = GetZoneByVectorIndex(zone_index);
						TVoxelData* vd = GetTerrainVoxelDataByIndex(zone_index);

						if(!InitialZoneSet.Contains(zone_index)) {
							// Until the end of the process some functions can be unavailable.
							InitialZoneLoader->ZoneList.Add(zone_index);
							InitialZoneSet.Add(zone_index);
						}
					}
				}
			}
		}
	}

	InitialZoneLoader->Start();
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	if (InitialZoneLoader != NULL) {
		InitialZoneLoader->Stop();
		InitialZoneLoader->WaitForFinish();
	}

	if (GetWorld()->GetAuthGameMode() == NULL) {
		return;
	}

	for (auto& Elem : TerrainRegionMap) {
		UTerrainRegionComponent* Region = Elem.Value;
		Region->SaveFile();
		Region->CleanMeshDataCache();
	}

	for (auto& Elem : VoxelDataMap) {
		TVoxelData* VoxelData = Elem.Value;

		if (VoxelData->isChanged()) {
			// save voxel data
			FVector index = GetZoneIndex(VoxelData->getOrigin());
			FString fileName = getZoneFileName(index.X, index.Y, index.Z);

			UE_LOG(LogTemp, Warning, TEXT("save voxeldata -> %f %f %f"), index.X, index.Y, index.Z);
			sandboxSaveVoxelData(*VoxelData, fileName);
		}
		
		//TODO replace with share pointer
		VoxelDataMap.Remove(Elem.Key);
		delete VoxelData;
	}

	if (!bDisableFoliage) {
		for (auto& Elem : TerrainZoneMap) {
			UTerrainZoneComponent* Zone = Elem.Value;
			Zone->SaveInstancedMeshesToFile();
		}
	}

	TerrainZoneMap.Empty();
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (HasNextAsyncTask()) {
		TerrainControllerTask task = GetAsyncTask();
		if (task.f) {
			task.f();
		}
	}	
}

//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================

SandboxVoxelGenerator ASandboxTerrainController::newTerrainGenerator(TVoxelData &voxel_data) {
	return SandboxVoxelGenerator(voxel_data, Seed);
};

FString ASandboxTerrainController::getZoneFileName(int tx, int ty, int tz) {
	FString savePath = FPaths::GameSavedDir();

	FString fileName = savePath + TEXT("/Map/") + MapName + TEXT("/zone.") + FString::FromInt(tx) + TEXT(".") + FString::FromInt(ty) + TEXT(".") + FString::FromInt(tz) + TEXT(".sbin");
	return fileName;
}


void ASandboxTerrainController::SpawnZone(FVector Pos) {
	FVector ZoneIndex = GetZoneIndex(Pos);

	TVoxelData* VoxelData = CreateZoneVoxeldata(Pos);

	if (VoxelData->getDensityFillState() == TVoxelDataFillState::MIX) {
		UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
		Zone->SetVoxelData(VoxelData);

		TMeshDataPtr MeshDataPtr = Zone->GetRegion()->GetMeshData(ZoneIndex);
		if (MeshDataPtr != nullptr) {
			Zone->ApplyTerrainMesh(MeshDataPtr, false); // already in cache
		} else {
			Zone->MakeTerrain();
		}
	}
}


TSet<FVector> ASandboxTerrainController::spawnInitialZone() {
	double start = FPlatformTime::Seconds();

	const int s = static_cast<int>(TerrainInitialArea);

	TSet<FVector> InitialZoneSet;

	UE_LOG(LogTemp, Warning, TEXT("TerrainInitialArea = %d"), s);

	if (s > 0) {
		for (auto x = -s; x <= s; x++) {
			for (auto y = -s; y <= s; y++) {
				for (auto z = -s; z <= s; z++) {
					FVector Pos = FVector((float)(x * 1000), (float)(y * 1000), (float)(z * 1000));
					SpawnZone(Pos);
					InitialZoneSet.Add(FVector(x, y, z));
				}
			}
		}
	} else {
		FVector Pos = FVector(0);
		SpawnZone(Pos);
		InitialZoneSet.Add(Pos);
	}	

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("initial zones was generated -> %f ms"), time);

	return InitialZoneSet;
}

FVector ASandboxTerrainController::GetRegionIndex(FVector v) {
	return sandboxGridIndex(v, 4500);
}

UTerrainRegionComponent* ASandboxTerrainController::GetRegionByVectorIndex(FVector index) {
	if (TerrainRegionMap.Contains(index)) {
		return TerrainRegionMap[index];
	}

	return NULL;
}

FVector ASandboxTerrainController::GetZoneIndex(FVector v) {
	return sandboxGridIndex(v, 1000);
}

UTerrainZoneComponent* ASandboxTerrainController::GetZoneByVectorIndex(FVector index) {
	if (TerrainZoneMap.Contains(index)) {
		return TerrainZoneMap[index];
	}

	return NULL;
}

UTerrainRegionComponent* ASandboxTerrainController::GetOrCreateRegion(FVector pos) {
	FVector RegionIndex = GetRegionIndex(pos);
	UTerrainRegionComponent* RegionComponent = GetRegionByVectorIndex(RegionIndex);
	if (RegionComponent == NULL) {
		FString RegionName = FString::Printf(TEXT("Region -> [%.0f, %.0f, %.0f]"), RegionIndex.X, RegionIndex.Y, RegionIndex.Z);
		RegionComponent = NewObject<UTerrainRegionComponent>(this, FName(*RegionName));
		RegionComponent->RegisterComponent();
		RegionComponent->SetRelativeLocation(pos);

		TerrainRegionMap.Add(FVector(RegionIndex.X, RegionIndex.Y, RegionIndex.Z), RegionComponent);
	}

	return RegionComponent;
}

UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector pos) {
	UTerrainRegionComponent* RegionComponent = GetOrCreateRegion(pos);

	FVector index = GetZoneIndex(pos);
	FString zone_name = FString::Printf(TEXT("Zone -> [%.0f, %.0f, %.0f]"), index.X, index.Y, index.Z);
	UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*zone_name));
	if (ZoneComponent) {
		ZoneComponent->RegisterComponent();
		ZoneComponent->SetRelativeLocation(pos);
		ZoneComponent->AttachTo(RegionComponent);

		FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh -> [%.0f, %.0f, %.0f]"), index.X, index.Y, index.Z);
		USandboxTerrainMeshComponent* TerrainMeshComp = NewObject<USandboxTerrainMeshComponent>(this, FName(*TerrainMeshCompName));
		TerrainMeshComp->RegisterComponent();
		TerrainMeshComp->SetMobility(EComponentMobility::Stationary);
		TerrainMeshComp->AttachTo(ZoneComponent);

		FString CollisionMeshCompName = FString::Printf(TEXT("CollisionMesh -> [%.0f, %.0f, %.0f]"), index.X, index.Y, index.Z);
		USandboxTerrainCollisionComponent* CollisionMeshComp = NewObject<USandboxTerrainCollisionComponent>(this, FName(*CollisionMeshCompName));
		CollisionMeshComp->RegisterComponent();
		CollisionMeshComp->SetMobility(EComponentMobility::Stationary);
		CollisionMeshComp->SetCanEverAffectNavigation(true);
		CollisionMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
		CollisionMeshComp->AttachTo(ZoneComponent);

		ZoneComponent->MainTerrainMesh = TerrainMeshComp;
		ZoneComponent->CollisionMesh = CollisionMeshComp;
	}

	TerrainZoneMap.Add(FVector(index.X, index.Y, index.Z), ZoneComponent);

	if(bShowZoneBounds) DrawDebugBox(GetWorld(), pos, FVector(500), FColor(255, 0, 0, 100), true);

	return ZoneComponent;
}

//======================================================================================================================================================================
// Edit Terrain
//======================================================================================================================================================================

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

void ASandboxTerrainController::fillTerrainRound(const FVector origin, const float r, const float strength, const int matId) {
	//if (GetWorld() == NULL) return;

	struct ZoneHandler {
		int newMaterialId;
		bool changed;
		bool enableLOD = false;
		bool operator()(TVoxelData* vd, FVector v, float radius, float strength) {
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
							//bool bNewPoint = vd->getDensity(x, y, z) <= 0.5;

							//2^-((x^2)/20)
							float d = density + 1 / rl * strength;
							vd->setDensity(x, y, z, d);

							//if (d > 0.5 && bNewPoint) {
								//vd->setMaterial(x, y, z, 1);
							//}

							changed = true;
						}

						if (rl < radius + 20) {
							vd->setMaterial(x, y, z, newMaterialId);
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

			return changed;
		}
	} zh;

	zh.newMaterialId = matId;
	zh.enableLOD = bEnableLOD;
	ASandboxTerrainController::performTerrainChange(origin, r, strength, zh);
}


void ASandboxTerrainController::digTerrainRoundHole(FVector origin, float r, float strength) {
	//if (GetWorld() == NULL) return;

	struct ZoneHandler {
		bool changed;
		bool enableLOD = false;
		bool operator()(TVoxelData* vd, FVector v, float radius, float strength) {
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
		bool operator()(TVoxelData* vd, FVector v, float radius, float strength) {
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
							} else {
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
	FRunnableThread* thread = FRunnableThread::Create(te, *thread_name, true, true);
	//FIXME delete thread after finish


	FVector ttt(origin);
	ttt.Z -= 10;
	TArray<struct FHitResult> OutHits;
	bool overlap = GetWorld()->SweepMultiByChannel(OutHits, origin, ttt, FQuat(), ECC_Visibility, FCollisionShape::MakeSphere(radius)); // ECC_Visibility
	if (overlap) {
		for (auto item : OutHits) {
			AActor* actor = item.GetActor();

			if (Cast<ASandboxTerrainController>(item.GetActor()) != nullptr) {
				UHierarchicalInstancedStaticMeshComponent* InstancedMesh = Cast<UHierarchicalInstancedStaticMeshComponent>(item.GetComponent());
				if (InstancedMesh != nullptr) {
					InstancedMesh->RemoveInstance(item.Item);
					//UE_LOG(LogTemp, Warning, TEXT("overlap %s -> %s -> %d"), *actor->GetName(), *item.Component->GetName(), item.Item);
				}
			}
		}
	}

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
	
	FVector base_zone_index = GetZoneIndex(v);

	static const float vvv[3] = { -1, 0, 1 };
	for (float x : vvv) {
		for (float y : vvv) {
			for (float z : vvv) {
				FVector zone_index(x, y, z);
				zone_index += base_zone_index;

				UTerrainZoneComponent* zone = GetZoneByVectorIndex(zone_index);
				TVoxelData* vd = GetTerrainVoxelDataByIndex(zone_index);

				if (zone == NULL) {
					if (vd != NULL) {
						vd->vd_edit_mutex.lock();
						bool is_changed = handler(vd, v, radius, s);
						if (is_changed) {
							vd->setChanged();
							vd->vd_edit_mutex.unlock();
							invokeLazyZoneAsync(zone_index);
						} else {
							vd->vd_edit_mutex.unlock();
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

				vd->vd_edit_mutex.lock();
				bool is_changed = handler(vd, v, radius, s);
				if (is_changed) {
					vd->setChanged();
					vd->setCacheToValid();
					std::shared_ptr<TMeshData> md_ptr = zone->GenerateMesh();
					vd->resetLastMeshRegenerationTime();
					vd->vd_edit_mutex.unlock();
					invokeZoneMeshAsync(zone, md_ptr);
				} else {
					vd->vd_edit_mutex.unlock();
				}
			}
		}
	}

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::editTerrain-------------> %f %f %f --> %f ms"), v.X, v.Y, v.Z, time);
}


void ASandboxTerrainController::InvokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<TMeshData> mesh_data_ptr) {
	TerrainControllerTask task;
	task.f = [=]() {
		if (mesh_data_ptr) {
			zone->ApplyTerrainMesh(mesh_data_ptr);
		}
	};

	AddAsyncTask(task);
}

void ASandboxTerrainController::InvokeLazyZoneAsync(FVector index) {
	TerrainControllerTask task;
	FVector v = FVector((float)(index.X * 1000), (float)(index.Y * 1000), (float)(index.Z * 1000));
	TVoxelData* vd = GetTerrainVoxelDataByIndex(index);

	if (vd == NULL) {
		UE_LOG(LogTemp, Warning, TEXT("FAIL"));
		return;
	}

	task.f = [=]() {
		UTerrainZoneComponent* zone = AddTerrainZone(v);
		zone->SetVoxelData(vd);

		std::shared_ptr<TMeshData> md_ptr = zone->GenerateMesh();
		vd->resetLastMeshRegenerationTime();
		zone->ApplyTerrainMesh(md_ptr);
	};

	AddAsyncTask(task);
}

//======================================================================================================================================================================

TVoxelData* ASandboxTerrainController::CreateZoneVoxeldata(FVector location) {
	double start = FPlatformTime::Seconds();

	int dim = static_cast<int>(ZoneGridDimension);
	TVoxelData* vd = new TVoxelData(dim, 100 * 10);
	vd->setOrigin(location);

	FVector index = GetZoneIndex(location);
	FString fileName = getZoneFileName(index.X, index.Y, index.Z);

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*fileName)) {
		sandboxLoadVoxelData(*vd, fileName);
		vd->DataState = TVoxelDataState::NEW_LOADED;
	} else {
		generateTerrain(*vd);
		sandboxSaveVoxelData(*vd, fileName);
		vd->DataState = TVoxelDataState::NEW_GENERATED;
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

void ASandboxTerrainController::generateTerrain(TVoxelData &voxel_data) {
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
		voxel_data.deinitializeDensity(TVoxelDataFillState::ZERO);
	}

	if (fc == s) {
		voxel_data.deinitializeDensity(TVoxelDataFillState::ALL);
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

void ASandboxTerrainController::OnGenerateNewZone(UTerrainZoneComponent* Zone) {
	if (!bDisableFoliage) {
		GenerateNewFoliage(Zone);
	}
}

void ASandboxTerrainController::OnLoadZone(UTerrainZoneComponent* Zone) {
	if (!bDisableFoliage) {
		LoadFoliage(Zone);
	}
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

void ASandboxTerrainController::RegisterTerrainVoxelData(TVoxelData* vd, FVector index) {
	VoxelDataMapMutex.lock();
	VoxelDataMap.Add(index, vd);
	VoxelDataMapMutex.unlock();
}

TVoxelData* ASandboxTerrainController::GetTerrainVoxelDataByPos(FVector point) {
	FVector index = sandboxSnapToGrid(point, 1000) / 1000;

	VoxelDataMapMutex.lock();
	if (VoxelDataMap.Contains(index)) {
		TVoxelData* vd = VoxelDataMap[index];
		VoxelDataMapMutex.unlock();
		return vd;
	}

	VoxelDataMapMutex.unlock();
	return NULL;
}

TVoxelData* ASandboxTerrainController::GetTerrainVoxelDataByIndex(FVector index) {
	VoxelDataMapMutex.lock();
	if (VoxelDataMap.Contains(index)) {
		TVoxelData* vd = VoxelDataMap[index];
		VoxelDataMapMutex.unlock();
		return vd;
	}

	VoxelDataMapMutex.unlock();
	return NULL;
}

//======================================================================================================================================================================
// Sandbox Foliage
//======================================================================================================================================================================

void ASandboxTerrainController::GenerateNewFoliage(UTerrainZoneComponent* Zone) {
	if (FoliageMap.Num() == 0) return;

	FRandomStream rnd = FRandomStream();
	rnd.Initialize(0);
	rnd.Reset();

	static const float s = 500;
	static const float step = 25;
	float counter = 0;

	for (auto x = -s; x <= s; x += step) {
		for (auto y = -s; y <= s; y += step) {

			FVector v(Zone->getVoxelData()->getOrigin());
			v += FVector(x, y, 0);

			for (auto& Elem : FoliageMap) {
				FSandboxFoliage FoliageType = Elem.Value;
				int32 FoliageTypeId = Elem.Key;

				float r = std::sqrt(v.X * v.X + v.Y * v.Y);
				if ((int)counter % (int)FoliageType.SpawnStep == 0) {
					SpawnFoliage(FoliageTypeId, FoliageType, v, rnd, Zone);
				}
			}

			counter += step;
		}
	}
}

void ASandboxTerrainController::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone) {

	if (FoliageType.OffsetRange > 0) {
		float ox = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) ox = -ox; v.X += ox;
		float oy = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) oy = -oy; v.Y += oy;
	}

	const FVector start_trace(v.X, v.Y, v.Z + 500);
	const FVector end_trace(v.X, v.Y, v.Z - 500);

	FHitResult hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(hit, start_trace, end_trace, ECC_WorldStatic);

	if (hit.bBlockingHit) {
		if (Cast<ASandboxTerrainController>(hit.Actor.Get()) != NULL) {
			if (Cast<USandboxTerrainCollisionComponent>(hit.Component.Get()) != NULL) {

				float angle = rnd.FRandRange(0.f, 360.f);
				float ScaleZ = rnd.FRandRange(FoliageType.ScaleMinZ, FoliageType.ScaleMaxZ);
				FTransform Transform(FRotator(0, angle, 0), hit.ImpactPoint, FVector(1, 1, ScaleZ));

				FTerrainInstancedMeshType MeshType;
				MeshType.MeshTypeId = FoliageTypeId;
				MeshType.Mesh = FoliageType.Mesh;

				Zone->SpawnInstancedMesh(MeshType, Transform);
			}
		}
	}
}

void ASandboxTerrainController::LoadFoliage(UTerrainZoneComponent* Zone) {
	Zone->LoadInstancedMeshesFromFile();
}

//======================================================================================================================================================================
// Materials
//======================================================================================================================================================================

UMaterialInterface* ASandboxTerrainController::GetRegularTerrainMaterial(uint16 MaterialId) {
	if (RegularMaterial == nullptr) {
		return nullptr;
	}

	if (!RegularMaterialCache.Contains(MaterialId)) {
		UE_LOG(LogTemp, Warning, TEXT("create new regular terrain material instance ----> id: %d"), MaterialId);

		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(RegularMaterial, this);

		if (MaterialMap.Contains(MaterialId)) {
			FSandboxTerrainMaterial Mat = MaterialMap[MaterialId];

			DynMaterial->SetTextureParameterValue("TextureTopMicro", Mat.TextureTopMicro);
			//DynMaterial->SetTextureParameterValue("TextureSideMicro", Mat.TextureSideMicro);
			DynMaterial->SetTextureParameterValue("TextureMacro", Mat.TextureMacro);
			DynMaterial->SetTextureParameterValue("TextureNormal", Mat.TextureNormal);
		}

		RegularMaterialCache.Add(MaterialId, DynMaterial);
		return DynMaterial;
	}

	return RegularMaterialCache[MaterialId];
}

UMaterialInterface* ASandboxTerrainController::GetTransitionTerrainMaterial(FString& TransitionName, std::set<unsigned short>& MaterialIdSet) {
	if (TransitionMaterial == nullptr) {
		return nullptr;
	}

	if (!TransitionMaterialCache.Contains(TransitionName)) {
		UE_LOG(LogTemp, Warning, TEXT("create new transition terrain material instance ----> id: %s"), *TransitionName);

		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(TransitionMaterial, this);

		int Idx = 0;
		for (unsigned short MatId : MaterialIdSet) {
			if (MaterialMap.Contains(MatId)) {
				FSandboxTerrainMaterial Mat = MaterialMap[MatId];

				FName TextureTopMicroParam = FName(*FString::Printf(TEXT("TextureTopMicro%d"), Idx));
				//FName TextureSideMicroParam = FName(*FString::Printf(TEXT("TextureSideMicro%d"), Idx));
				FName TextureMacroParam = FName(*FString::Printf(TEXT("TextureMacro%d"), Idx));
				FName TextureNormalParam = FName(*FString::Printf(TEXT("TextureNormal%d"), Idx));

				DynMaterial->SetTextureParameterValue(TextureTopMicroParam, Mat.TextureTopMicro);
				//DynMaterial->SetTextureParameterValue(TextureSideMicroParam, Mat.TextureSideMicro);
				DynMaterial->SetTextureParameterValue(TextureMacroParam, Mat.TextureMacro);
				DynMaterial->SetTextureParameterValue(TextureNormalParam, Mat.TextureNormal);
			}

			Idx++;
		}

		TransitionMaterialCache.Add(TransitionName, DynMaterial);
		return DynMaterial;
	}

	return TransitionMaterialCache[TransitionName];
}