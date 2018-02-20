
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "TerrainZoneComponent.h"
#include "TerrainRegionComponent.h"
#include "SandboxVoxeldata.h"
#include <cmath>
#include "DrawDebugHelpers.h"
#include "Async.h"
#include "Json.h"

#include "SandboxTerrainMeshComponent.h"


class FAsyncThread : public FRunnable {

private:

	volatile int State = TH_STATE_NEW;

	FRunnableThread* Thread;

	std::function<void(FAsyncThread&)> Function;

public:

	FAsyncThread(std::function<void(FAsyncThread&)> Function) {
		Thread = NULL;
		this->Function = Function;
	}

	~FAsyncThread() {
		if (Thread != NULL) {
			delete Thread;
		}
	}

	bool IsFinished() {
		return State == TH_STATE_FINISHED;
	}

	bool IsNotValid() {
		if (State == TH_STATE_STOP) {
			State = TH_STATE_FINISHED;
			return true;
		}

		return false;
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
		State = TH_STATE_RUNNING;
		Thread = FRunnableThread::Create(this, TEXT("THREAD_TEST"));

	}

	virtual uint32 Run() {
		Function(*this);
		
		State = TH_STATE_FINISHED;
		return 0;
	}
};


void TVoxelDataInfo::Unload() {
	if (Vd != nullptr) {
		delete Vd;
		Vd = nullptr;
	}

	DataState = TVoxelDataState::READY_TO_LOAD;
}


ASandboxTerrainController::ASandboxTerrainController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	TerrainSizeX = 5;
	TerrainSizeY = 5;
	TerrainSizeZ = 5;
	bEnableLOD = false;

	TerrainGeneratorComponent = CreateDefaultSubobject<UTerrainGeneratorComponent>(TEXT("TerrainGenerator"));
	TerrainGeneratorComponent->AttachTo(RootComponent);
}

ASandboxTerrainController::ASandboxTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	TerrainSizeX = 5;
	TerrainSizeY = 5;
	TerrainSizeZ = 5;
	bEnableLOD = false;

	TerrainGeneratorComponent = CreateDefaultSubobject<UTerrainGeneratorComponent>(TEXT("TerrainGenerator"));
	TerrainGeneratorComponent->AttachTo(RootComponent);
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

	if (!GetWorld()) return;

	if (GetWorld()->GetAuthGameMode() != NULL) {
		UE_LOG(LogTemp, Warning, TEXT("SERVER"));
	} else {
		UE_LOG(LogTemp, Warning, TEXT("CLIENT"));
	}

	if (!OpenVdfile()) return;

	UE_LOG(LogSandboxTerrain, Warning, TEXT("vd file ----> %d zones"), VdFile.size());

	//===========================
	// load existing
	//===========================
	RegionIndexSet.Empty();
	OnStartBuildTerrain();
	bIsGeneratingTerrain = true;
	LoadJson(RegionIndexSet);

	// load initial region
	UTerrainRegionComponent* Region1 = GetOrCreateRegion(FVector(0, 0, 0));
	Region1->LoadFile();
	// spawn initial zone
	TSet<FVector> InitialZoneSet = SpawnInitialZone();

	// async loading other zones
	RunThread([&](FAsyncThread& ThisThread) {
		Region1->ForEachMeshData([&](FVector& Index, TMeshDataPtr& MeshDataPtr) {
			if (ThisThread.IsNotValid()) return;
			FVector Pos = GetZonePos(Index);
			SpawnZone(Pos);
		});

		if (ThisThread.IsNotValid()) return;

		for (FVector RegionIndex : RegionIndexSet) {
			if (RegionIndex.Equals(FVector::ZeroVector)) {
				continue;
			}

			UTerrainRegionComponent* Region2 = GetOrCreateRegion(GetRegionPos(RegionIndex));
			Region2->LoadFile();
			if (ThisThread.IsNotValid()) return;

			Region2->ForEachMeshData([&](FVector& Index, TMeshDataPtr& MeshDataPtr) {
				if (ThisThread.IsNotValid()) return;
				FVector Pos = GetZonePos(Index);
				SpawnZone(Pos);
			});

			if (ThisThread.IsNotValid()) return;
		}

		if (!bGenerateOnlySmallSpawnPoint) {
			int Total = (TerrainSizeX * 2 + 1) * (TerrainSizeY * 2 + 1) * (TerrainSizeZ * 2 + 1);
			int Progress = 0;

			for (int x = -TerrainSizeX; x <= TerrainSizeX; x++) {
				for (int y = -TerrainSizeY; y <= TerrainSizeY; y++) {
					for (int z = -TerrainSizeZ; z <= TerrainSizeZ; z++) {
						FVector Index = FVector(x, y, z);
						FVector Pos = GetZonePos(Index);
						if (ThisThread.IsNotValid()) return;

						if (!HasVoxelData(TVoxelIndex(Index.X, Index.Y, Index.Z))) {
							SpawnZone(Pos);
						}

						Progress++;
						GeneratingProgress = (float)Progress / (float)Total;
						// must invoke in main thread
						//OnProgressBuildTerrain(PercentProgress);

						if (ThisThread.IsNotValid()) return;
					}
				}
			}
		}

		RunThread([&](FAsyncThread& ThisThread) {
			bIsGeneratingTerrain = false;
			OnFinishBuildTerrain();
		});

	});
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	// wait threads for finish
	for (auto* ThreadTask : ThreadList) {
		ThreadTask->Stop();
		ThreadTask->WaitForFinish();
	}

	// save only on server side
	if (GetWorld()->GetAuthGameMode() == NULL) {
		return;
	}

	Save();
	VdFile.close();
	ClearVoxelData();

	// clean region mesh data cache and close vd file
	for (auto& Elem : TerrainRegionMap) {
		UTerrainRegionComponent* Region = Elem.Value;
		Region->CleanMeshDataCache();
	}

	TerrainZoneMap.Empty();
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (HasNextAsyncTask()) {
		TerrainControllerTask task = GetAsyncTask();
		if (task.Function) {
			task.Function();
		}
	}	

	auto It = ThreadList.begin();
	while (It != ThreadList.end()) {
		FAsyncThread* ThreadPtr = *It;
		if (ThreadPtr->IsFinished()) {
			ThreadListMutex.lock();
			delete ThreadPtr;
			It = ThreadList.erase(It);
			ThreadListMutex.unlock();
		} else {
			It++;
		}
	}

	if (bIsGeneratingTerrain) {
		OnProgressBuildTerrain(GeneratingProgress);
	}
}

//======================================================================================================================================================================
// Unreal Sandbox 
//======================================================================================================================================================================
typedef struct TSaveBuffer {

	TArray<UTerrainZoneComponent*> ZoneArray;

} TSaveBuffer;


void ASandboxTerrainController::Save() {
	TSet<FVector> RegionIndexSetLocal;

	for (auto& Elem : VoxelDataMap) {
		TVoxelDataInfo& VdInfo = Elem.Value;

		if (VdInfo.Vd == nullptr) {
			continue;
		}

		//============================================
		// TODO remove
		// test
		FBufferArchive TempBuffer;
		TValueData buffer;
		serializeVoxelData(*VdInfo.Vd, TempBuffer);
		buffer.reserve(TempBuffer.Num());

		for (uint8 b : TempBuffer) {
			buffer.push_back(b);
		}

		TVoxelIndex Index = GetZoneIndex(VdInfo.Vd->getOrigin());

		if (VdInfo.Vd->isChanged()) {
			VdFile.put(Index, buffer);
			VdInfo.Vd->resetLastSave();
			UE_LOG(LogSandboxTerrain, Warning, TEXT("save voxel data ----> %d %d %d"), Index.X, Index.Y, Index.Z);
		}

		FVector RegionIndex = GetRegionIndex(VdInfo.Vd->getOrigin());
		VdInfo.Unload();
		RegionIndexSetLocal.Add(RegionIndex);
	}

	// put zones to save buffer
	TMap<FVector, TSaveBuffer> SaveBufferByRegion;
	for (auto& Elem : TerrainZoneMap) {
		FVector ZoneIndex = Elem.Key;
		UTerrainZoneComponent* Zone = Elem.Value;
		FVector RegionIndex = GetRegionIndex(Zone->GetComponentLocation());

		TSaveBuffer& SaveBuffer = SaveBufferByRegion.FindOrAdd(RegionIndex);

		SaveBuffer.ZoneArray.Add(Zone);
		RegionIndexSetLocal.Add(RegionIndex);
	}

	// save regions accordind data from save buffer
	for (auto& Elem : SaveBufferByRegion) {
		FVector RegionIndex = Elem.Key;
		TSaveBuffer& SaveBuffer = Elem.Value;
		UTerrainRegionComponent* Region = GetRegionByVectorIndex(RegionIndex);

		if (Region != nullptr && Region->IsChanged()){
			Region->SaveFile(SaveBuffer.ZoneArray);
			Region->ResetChanged();
		}
	}

	RegionIndexSetLocal.Append(RegionIndexSet);
	SaveJson(RegionIndexSetLocal);
}

void ASandboxTerrainController::SaveMapAsync() {
	UE_LOG(LogSandboxTerrain, Warning, TEXT("Start save terrain async"));
	RunThread([&](FAsyncThread& ThisThread) {
		double Start = FPlatformTime::Seconds();
		Save();
		double End = FPlatformTime::Seconds();
		double Time = (End - Start) * 1000;
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Terrain saved -> %f ms"), Time);
	});
}

void ASandboxTerrainController::SaveJson(const TSet<FVector>& RegionIndexSet) {
	UE_LOG(LogTemp, Warning, TEXT("----------- save json -----------"));

	FString JsonStr;
	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::GameSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	TSharedRef <TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteArrayStart("Regions");

	for (const FVector& Index : RegionIndexSet) {
		float x = Index.X;
		float y = Index.Y;
		float z = Index.Z;

		FVector RegionPos = GetRegionPos(Index);

		//========================================================
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteObjectStart("Region");

		JsonWriter->WriteArrayStart("Index");
		JsonWriter->WriteValue(x);
		JsonWriter->WriteValue(y);
		JsonWriter->WriteValue(z);
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteArrayStart("Pos");
		JsonWriter->WriteValue(RegionPos.X);
		JsonWriter->WriteValue(RegionPos.Y);
		JsonWriter->WriteValue(RegionPos.Z);
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteObjectEnd();
		JsonWriter->WriteObjectEnd();
		//========================================================
	}

	JsonWriter->WriteArrayEnd();
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	FFileHelper::SaveStringToFile(*JsonStr, *FullPath);
}

bool ASandboxTerrainController::OpenVdfile() {
	// open vd file 	
	FString FileName = TEXT("terrain.dat");
	FString SavePath = FPaths::GameSavedDir();
	FString SaveDir = SavePath + TEXT("/Map/") + MapName + TEXT("/");
	FString FullPath = SaveDir + FileName;
	std::string FilePathString = std::string(TCHAR_TO_UTF8(*FullPath));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDir)) {
		PlatformFile.CreateDirectory(*SaveDir);
		if (!PlatformFile.DirectoryExists(*SaveDir)) {
			return false;
		}
	}

	// check terrain db file 
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath)) {
		// create new empty file
		kvdb::KvFile<TVoxelIndex, TValueData>::create(FilePathString, std::unordered_map<TVoxelIndex, TValueData>());
	}

	if (!VdFile.open(FilePathString)) {
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Unable to open terrain file: %s"), *FullPath);
		return false;
	}

	return true;
}

void ASandboxTerrainController::LoadJson(TSet<FVector>& RegionIndexSet) {
	UE_LOG(LogTemp, Warning, TEXT("----------- load json -----------"));

	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::GameSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	FString jsonRaw;
	if (!FFileHelper::LoadFileToString(jsonRaw, *FullPath, FFileHelper::EHashOptions::None)) {
		UE_LOG(LogTemp, Error, TEXT("Error loading json file"));
	}

	TSharedPtr<FJsonObject> jsonParsed;
	TSharedRef<TJsonReader<TCHAR>> jsonReader = TJsonReaderFactory<TCHAR>::Create(jsonRaw);
	if (FJsonSerializer::Deserialize(jsonReader, jsonParsed)) {

		TArray <TSharedPtr<FJsonValue>> array = jsonParsed->GetArrayField("Regions");
		for (int i = 0; i < array.Num(); i++) {
			TSharedPtr<FJsonObject> obj_ptr = array[i]->AsObject();
			TSharedPtr<FJsonObject> RegionObj = obj_ptr->GetObjectField(TEXT("Region"));

			TArray <TSharedPtr<FJsonValue>> IndexValArray = RegionObj->GetArrayField("Index");
			double x = IndexValArray[0]->AsNumber();
			double y = IndexValArray[1]->AsNumber();
			double z = IndexValArray[2]->AsNumber();

			RegionIndexSet.Add(FVector(x, y, z));

			UE_LOG(LogTemp, Warning, TEXT("index: %f %f %f"), x, y, z);
		}
	}
}

void ASandboxTerrainController::InvokeSafe(std::function<void()> Function) {
	if (IsInGameThread()) {
		Function();
	} else {
		TerrainControllerTask AsyncTask;
		AsyncTask.Function = Function;
		AddAsyncTask(AsyncTask);
	}
}

void ASandboxTerrainController::SpawnZone(const FVector& Pos) {
	TVoxelIndex ZoneIndex = GetZoneIndex(Pos);
	FVector ZoneIndexTmp(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

	if (GetZoneByVectorIndex(ZoneIndexTmp) != nullptr) return;

	FVector RegionIndex = GetRegionIndex(Pos);
	UTerrainRegionComponent* Region = GetRegionByVectorIndex(RegionIndex);

	if (Region != nullptr) {
		TMeshDataPtr MeshDataPtr = Region->GetMeshData(ZoneIndexTmp);
		if (MeshDataPtr != nullptr) {
			InvokeSafe([=]() {
				UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
				Zone->ApplyTerrainMesh(MeshDataPtr, false); // already in cache
				OnLoadZone(Zone);
			});
			return;
		}
	} 

	TVoxelDataInfo* VoxelDataInfo = FindOrCreateZoneVoxeldata(ZoneIndex);
	if (VoxelDataInfo->Vd->getDensityFillState() == TVoxelDataFillState::MIX) {
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			Zone->SetVoxelData(VoxelDataInfo->Vd);
			Zone->MakeTerrain();

			if (VoxelDataInfo->IsNewGenerated()) {
				Zone->GetRegion()->SetChanged();
				OnGenerateNewZone(Zone);
			}

			if (VoxelDataInfo->IsNewLoaded()) {
				OnLoadZone(Zone);
			}
		});
	} else {
		if (VoxelDataInfo->IsNewGenerated()) {
			InvokeSafe([=]() {
				// just create region
				UTerrainRegionComponent* Region = GetOrCreateRegion(Pos);
				Region->SetChanged();
			});
		}
	}
}


TSet<FVector> ASandboxTerrainController::SpawnInitialZone() {
	double start = FPlatformTime::Seconds();

	const int s = static_cast<int>(TerrainInitialArea);
	TSet<FVector> InitialZoneSet;

	if (s > 0) {
		for (auto x = -s; x <= s; x++) {
			for (auto y = -s; y <= s; y++) {
				for (auto z = -s; z <= s; z++) {
					FVector Pos = FVector((float)(x * USBT_ZONE_SIZE), (float)(y * USBT_ZONE_SIZE), (float)(z * USBT_ZONE_SIZE));
					SpawnZone(Pos);
					InitialZoneSet.Add(Pos);
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

	return InitialZoneSet;
}

FVector ASandboxTerrainController::GetRegionIndex(FVector v) {
	return sandboxGridIndex(v, USBT_REGION_SIZE);
}

FVector ASandboxTerrainController::GetRegionPos(FVector Index) {
	return FVector(Index.X * USBT_REGION_SIZE, Index.Y * USBT_REGION_SIZE, Index.Z * USBT_REGION_SIZE);
}

UTerrainRegionComponent* ASandboxTerrainController::GetRegionByVectorIndex(FVector index) {
	if (TerrainRegionMap.Contains(index)) {
		return TerrainRegionMap[index];
	}

	return NULL;
}

TVoxelIndex ASandboxTerrainController::GetZoneIndex(FVector v) {
	FVector Tmp = sandboxGridIndex(v, USBT_ZONE_SIZE);
	return TVoxelIndex(Tmp.X, Tmp.Y, Tmp.Z);
}

FVector ASandboxTerrainController::GetZonePos(FVector Index) {
	return FVector(Index.X * USBT_ZONE_SIZE, Index.Y * USBT_ZONE_SIZE, Index.Z * USBT_ZONE_SIZE);
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
		RegionComponent->AttachTo(RootComponent);
		//RegionComponent->SetRelativeLocation(pos);
		RegionComponent->SetWorldLocation(GetRegionPos(RegionIndex));

		TerrainRegionMap.Add(FVector(RegionIndex.X, RegionIndex.Y, RegionIndex.Z), RegionComponent);

		//if (bShowZoneBounds) 
			DrawDebugBox(GetWorld(), GetRegionPos(RegionIndex), FVector(USBT_REGION_SIZE / 2), FColor(255, 0, 255, 100), true);
	}

	return RegionComponent;
}

UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector pos) {
	UTerrainRegionComponent* RegionComponent = GetOrCreateRegion(pos);

	TVoxelIndex Index = GetZoneIndex(pos);
	FVector IndexTmp(Index.X, Index.Y,Index.Z);

	FString zone_name = FString::Printf(TEXT("Zone -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
	UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*zone_name));
	if (ZoneComponent) {
		ZoneComponent->RegisterComponent();
		//ZoneComponent->SetRelativeLocation(pos);
		ZoneComponent->AttachTo(RegionComponent);
		ZoneComponent->SetWorldLocation(pos);

		FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
		USandboxTerrainMeshComponent* TerrainMeshComp = NewObject<USandboxTerrainMeshComponent>(this, FName(*TerrainMeshCompName));
		TerrainMeshComp->RegisterComponent();
		TerrainMeshComp->SetMobility(EComponentMobility::Stationary);
		TerrainMeshComp->AttachTo(ZoneComponent);

		FString CollisionMeshCompName = FString::Printf(TEXT("CollisionMesh -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
		USandboxTerrainCollisionComponent* CollisionMeshComp = NewObject<USandboxTerrainCollisionComponent>(this, FName(*CollisionMeshCompName));
		CollisionMeshComp->RegisterComponent();
		CollisionMeshComp->SetMobility(EComponentMobility::Stationary);
		CollisionMeshComp->SetCanEverAffectNavigation(true);
		CollisionMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
		CollisionMeshComp->AttachTo(ZoneComponent);

		ZoneComponent->MainTerrainMesh = TerrainMeshComp;
		ZoneComponent->CollisionMesh = CollisionMeshComp;
	}

	TerrainZoneMap.Add(IndexTmp, ZoneComponent);
	if(bShowZoneBounds) DrawDebugBox(GetWorld(), pos, FVector(USBT_ZONE_SIZE / 2), FColor(255, 0, 0, 100), true);
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
	ASandboxTerrainController* instance;

	virtual uint32 Run() {
		instance->EditTerrain(origin, radius, zone_handler);
		return 0;
	}
};

struct TZoneEditHandler {
	bool changed = false;
	bool enableLOD = false;
	float Strength;
};

void ASandboxTerrainController::FillTerrainRound(const FVector origin, const float r, const int matId) {
	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd, FVector v, float radius) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= v;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < radius) {
					//2^-((x^2)/20)
					float d = density + 1 / rl * Strength;
					vd->setDensity(x, y, z, d);
					changed = true;
				}

				if (rl < radius + 20) {
					vd->setMaterial(x, y, z, newMaterialId);
				}			
			}, enableLOD);

			return changed;
		}
	} zh;

	zh.newMaterialId = matId;
	zh.enableLOD = bEnableLOD;
	zh.Strength = 5;
	ASandboxTerrainController::PerformTerrainChange(origin, r, zh);
}


void ASandboxTerrainController::DigTerrainRoundHole(FVector origin, float r, float strength) {

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;

		bool operator()(TVoxelData* vd, FVector v, float radius) {
			changed = false;
			
			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= v;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < radius) {
					unsigned short  MatId = vd->getMaterial(x, y, z);
					FSandboxTerrainMaterial& Mat = MaterialMapPtr->FindOrAdd(MatId);

					float ClcStrength = (Mat.RockHardness == 0) ? Strength : (Strength / Mat.RockHardness);
					if (ClcStrength > 0.1) {
						float d = density - 1 / rl * (ClcStrength);
						vd->setDensity(x, y, z, d);
					}

					changed = true;
				}
			}, enableLOD);

			return changed;
		}
	} zh;

	zh.MaterialMapPtr = &MaterialMap;
	zh.enableLOD = bEnableLOD;
	zh.Strength = strength;
	ASandboxTerrainController::PerformTerrainChange(origin, r, zh);
}

void ASandboxTerrainController::DigTerrainCubeHole(FVector origin, float r, float strength) {

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;

		bool operator()(TVoxelData* vd, FVector v, float radius) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= v;
				if (o.X < radius && o.X > -radius && o.Y < radius && o.Y > -radius && o.Z < radius && o.Z > -radius) {
					unsigned short  MatId = vd->getMaterial(x, y, z);
					FSandboxTerrainMaterial& Mat = MaterialMapPtr->FindOrAdd(MatId);
					if (Mat.RockHardness < 100) {
						vd->setDensity(x, y, z, 0);
						changed = true;
					}
				}
			}, enableLOD);

			return changed;
		}
	} zh;

	zh.enableLOD = bEnableLOD;
	zh.MaterialMapPtr = &MaterialMap;
	ASandboxTerrainController::PerformTerrainChange(origin, r, zh);
}

void ASandboxTerrainController::FillTerrainCube(FVector origin, const float r, const int matId) {

	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd, FVector v, float radius) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= v;
				if (o.X < radius && o.X > -radius && o.Y < radius && o.Y > -radius && o.Z < radius && o.Z > -radius) {
					vd->setDensity(x, y, z, 1);
					changed = true;
				}

				float radiusMargin = radius + 20;
				if (o.X < radiusMargin && o.X > -radiusMargin && o.Y < radiusMargin && o.Y > -radiusMargin && o.Z < radiusMargin && o.Z > -radiusMargin) {
					vd->setMaterial(x, y, z, newMaterialId);
				}
			}, enableLOD);

			return changed;
		}
	} zh;

	zh.newMaterialId = matId;
	zh.enableLOD = bEnableLOD;
	ASandboxTerrainController::PerformTerrainChange(origin, r, zh);
}

template<class H>
void ASandboxTerrainController::PerformTerrainChange(FVector Origin, float Radius, H Handler) {
	FTerrainEditThread<H>* EditThread = new FTerrainEditThread<H>();
	EditThread->zone_handler = Handler;
	EditThread->origin = Origin;
	EditThread->radius = Radius;
	EditThread->instance = this;

	FString ThreadName = FString::Printf(TEXT("terrain_change-thread-%d"), FPlatformTime::Seconds());
	FRunnableThread* Thread = FRunnableThread::Create(EditThread, *ThreadName);
	//FIXME delete thread after finish

	FVector TestPoint(Origin);
	TestPoint.Z -= 10;
	TArray<struct FHitResult> OutHits;
	bool bIsOverlap = GetWorld()->SweepMultiByChannel(OutHits, Origin, TestPoint, FQuat(), ECC_Visibility, FCollisionShape::MakeSphere(Radius)); // ECC_Visibility
	if (bIsOverlap) {
		for (auto OverlapItem : OutHits) {
			AActor* Actor = OverlapItem.GetActor();
			if (Cast<ASandboxTerrainController>(OverlapItem.GetActor()) != nullptr) {
				UHierarchicalInstancedStaticMeshComponent* InstancedMesh = Cast<UHierarchicalInstancedStaticMeshComponent>(OverlapItem.GetComponent());
				if (InstancedMesh != nullptr) {
					InstancedMesh->RemoveInstance(OverlapItem.Item);

					UTerrainRegionComponent* Region = GetRegionByVectorIndex(GetRegionIndex(OverlapItem.ImpactPoint));
					Region->SetChanged();
				}
			}
		}
	}
}

template<class H>
void ASandboxTerrainController::EditTerrain(FVector v, float radius, H handler) {
	double Start = FPlatformTime::Seconds();

	static float ZoneVolumeSize = USBT_ZONE_SIZE / 2;

	TVoxelIndex BaseZoneIndex = GetZoneIndex(v);
	FVector BaseZoneIndexTmp(BaseZoneIndex.X, BaseZoneIndex.Y, BaseZoneIndex.Z);

	static const float V[3] = { -1, 0, 1 };
	for (float x : V) {
		for (float y : V) {
			for (float z : V) {
				FVector ZoneIndex(x, y, z);
				ZoneIndex += BaseZoneIndexTmp;

				UTerrainZoneComponent* Zone = GetZoneByVectorIndex(ZoneIndex);
				TVoxelData* VoxelData = GetVoxelDataByIndex(ZoneIndex);

				// check zone bounds
				FVector ZoneOrigin = GetZonePos(ZoneIndex);
				FVector Upper(ZoneOrigin.X + ZoneVolumeSize, ZoneOrigin.Y + ZoneVolumeSize, ZoneOrigin.Z + ZoneVolumeSize);
				FVector Lower(ZoneOrigin.X - ZoneVolumeSize, ZoneOrigin.Y - ZoneVolumeSize, ZoneOrigin.Z - ZoneVolumeSize);

				if (FMath::SphereAABBIntersection(FSphere(v, radius), FBox(Lower, Upper))) {
					if (Zone == NULL) {
						if (VoxelData != NULL) {
							VoxelData->vd_edit_mutex.lock();
							bool bIsChanged = handler(VoxelData, v, radius);
							if (bIsChanged) {
								VoxelData->setChanged();
								VoxelData->vd_edit_mutex.unlock();
								InvokeLazyZoneAsync(ZoneIndex);
							} else {
								VoxelData->vd_edit_mutex.unlock();
							}
							continue;
						} else {
							continue;
						}
					}

					if (VoxelData == NULL) {
						//try to load lazy voxel data
						UTerrainRegionComponent* Region = Zone->GetRegion();
						VoxelData = LoadVoxelDataByIndex(TVoxelIndex(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z));

						if (VoxelData == nullptr) {
							continue;
						}
					}

					VoxelData->vd_edit_mutex.lock();
					bool bIsChanged = handler(VoxelData, v, radius);
					if (bIsChanged) {
						VoxelData->setChanged();
						VoxelData->setCacheToValid();
						Zone->SetVoxelData(VoxelData); // if zone was loaded from mesh cache
						std::shared_ptr<TMeshData> md_ptr = Zone->GenerateMesh();
						VoxelData->resetLastMeshRegenerationTime();
						VoxelData->vd_edit_mutex.unlock();
						Zone->GetRegion()->SetChanged();
						InvokeZoneMeshAsync(Zone, md_ptr);
					} else {
						VoxelData->vd_edit_mutex.unlock();
					}
				}
			}
		}
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::editTerrain-------------> %f %f %f --> %f ms"), v.X, v.Y, v.Z, time);
}


void ASandboxTerrainController::InvokeZoneMeshAsync(UTerrainZoneComponent* zone, std::shared_ptr<TMeshData> mesh_data_ptr) {
	TerrainControllerTask task;
	task.Function = [=]() {
		if (mesh_data_ptr) {
			zone->ApplyTerrainMesh(mesh_data_ptr);
		}
	};

	AddAsyncTask(task);
}

void ASandboxTerrainController::InvokeLazyZoneAsync(FVector ZoneIndex) {
	TerrainControllerTask Task;

	FVector Pos = GetZonePos(ZoneIndex);
	TVoxelData* VoxelData = GetVoxelDataByIndex(ZoneIndex);

	if (VoxelData == nullptr) {
		return;
	}

	Task.Function = [=]() {	
		if(VoxelData != nullptr) {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			Zone->SetVoxelData(VoxelData);
			TMeshDataPtr NewMeshDataPtr = Zone->GenerateMesh();
			VoxelData->resetLastMeshRegenerationTime();
			Zone->GetRegion()->SetChanged();
			Zone->ApplyTerrainMesh(NewMeshDataPtr);
		}
	};

	AddAsyncTask(Task);
}

//======================================================================================================================================================================


TVoxelData* ASandboxTerrainController::LoadVoxelDataByIndex(TVoxelIndex Index) {
	std::shared_ptr<TValueData> DataPtr = VdFile.get(Index);

	if (DataPtr == nullptr || DataPtr->size() == 0) {
		return nullptr;
	}

	double Start = FPlatformTime::Seconds();

	//TODO optimize all
	TArray<uint8> BinaryArray;
	BinaryArray.Reserve(DataPtr->size());

	TValueData& Data = *DataPtr.get();

	for (auto Byte : Data){
		BinaryArray.Add(Byte);
	}

	FVector VoxelDataOrigin = GetZonePos(FVector(Index.X, Index.Y, Index.Z));

	FMemoryReader BinaryData = FMemoryReader(BinaryArray, true); //true, free data after done
	BinaryData.Seek(0);

	TVoxelData* Vd = new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
	Vd->setOrigin(VoxelDataOrigin);

	deserializeVoxelData(*Vd, BinaryData);

	TVoxelDataInfo VdInfo;
	VdInfo.DataState = TVoxelDataState::LOADED;
	VdInfo.Vd = Vd;

	RegisterTerrainVoxelData(VdInfo, Index);

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	UE_LOG(LogTemp, Log, TEXT("loading voxel data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);

	return Vd;
}

TVoxelDataInfo* ASandboxTerrainController::FindOrCreateZoneVoxeldata(TVoxelIndex Index) {
	FVector TmpIndex(Index.X, Index.Y, Index.Z);

	if (HasVoxelData(Index)) {
		return GetVoxelDataInfo(Index);
	} else {
		// TODO check vd file
		TVoxelDataInfo ReturnVdInfo;

		TVoxelData* Vd = GetVoxelDataByIndex(TmpIndex);
		Vd = new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
		Vd->setOrigin(GetZonePos(TmpIndex));

		TerrainGeneratorComponent->GenerateVoxelTerrain(*Vd);

		ReturnVdInfo.DataState = TVoxelDataState::GENERATED;
		ReturnVdInfo.Vd = Vd;

		Vd->setChanged();
		Vd->setCacheToValid();

		RegisterTerrainVoxelData(ReturnVdInfo, Index);

		return VoxelDataMap.Find(TmpIndex);
	}
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

void ASandboxTerrainController::RegisterTerrainVoxelData(TVoxelDataInfo VdInfo, TVoxelIndex Index) {
	VoxelDataMapMutex.lock();
	VoxelDataMap.Add(FVector(Index.X, Index.Y, Index.Z), VdInfo); // TODO: replace with VoxelDataIndexMap
	VoxelDataIndexMap.insert({ Index, VdInfo });
	VoxelDataMapMutex.unlock();
}

void ASandboxTerrainController::RunThread(std::function<void(FAsyncThread&)> Function) {
	FAsyncThread* ThreadTask = new FAsyncThread(Function);
	ThreadListMutex.lock();
	ThreadList.push_back(ThreadTask);
	ThreadTask->Start();
	ThreadListMutex.unlock();
}

TVoxelData* ASandboxTerrainController::GetVoxelDataByPos(FVector point) {
	TVoxelIndex Index = GetZoneIndex(point);
	FVector IndexTmp(Index.X, Index.Y, Index.Z);
	return GetVoxelDataByIndex(IndexTmp);
}

TVoxelData* ASandboxTerrainController::GetVoxelDataByIndex(FVector index) {
	VoxelDataMapMutex.lock();
	if (VoxelDataMap.Contains(index)) {
		TVoxelDataInfo VdInfo = VoxelDataMap[index];
		VoxelDataMapMutex.unlock();
		return VdInfo.Vd;
	}

	VoxelDataMapMutex.unlock();
	return NULL;
}

bool ASandboxTerrainController::HasVoxelData(const TVoxelIndex& Index) const {
		return VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end();
}

TVoxelDataInfo* ASandboxTerrainController::GetVoxelDataInfo(const TVoxelIndex& Index) {
	if (VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end()) {
		return &VoxelDataIndexMap[Index];
	}

	return nullptr;
}

//TVoxelDataInfo* ASandboxTerrainController::GetVoxelDataInfo(const FVector& Index) {
//	return VoxelDataMap.Find(Index);
//}

void ASandboxTerrainController::ClearVoxelData() {
	VoxelDataMap.Empty();
}

//======================================================================================================================================================================
// Sandbox Foliage
//======================================================================================================================================================================

void ASandboxTerrainController::GenerateNewFoliage(UTerrainZoneComponent* Zone) {
	if (FoliageMap.Num() == 0) return;
	if (TerrainGeneratorComponent->GroundLevelFunc(Zone->GetComponentLocation()) > Zone->GetComponentLocation().Z + 500) return;

	int32 Hash = 7;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().X;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().Y;
	Hash = Hash * 31 + (int32)Zone->GetComponentLocation().Z;

	FRandomStream rnd = FRandomStream();
	rnd.Initialize(Hash);
	rnd.Reset();

	static const float s = USBT_ZONE_SIZE / 2;
	static const float step = 25.f;

	for (auto x = -s; x <= s; x += step) {
		for (auto y = -s; y <= s; y += step) {

			FVector v(Zone->getVoxelData()->getOrigin());
			v += FVector(x, y, 0);

			for (auto& Elem : FoliageMap) {
				FSandboxFoliage FoliageType = Elem.Value;
				int32 FoliageTypeId = Elem.Key;

				if ((int)x % (int)FoliageType.SpawnStep == 0 && (int)y % (int)FoliageType.SpawnStep == 0) {
					//UE_LOG(LogTemp, Warning, TEXT("%d - %d"), (int)x, (int)y);
					float Chance = rnd.FRandRange(0.f, 1.f);
					if (Chance <= FoliageType.Probability) {
						float r = std::sqrt(v.X * v.X + v.Y * v.Y);
						SpawnFoliage(FoliageTypeId, FoliageType, v, rnd, Zone);
					}
				}
			}


		}
	}
}

void ASandboxTerrainController::SpawnFoliage(int32 FoliageTypeId, FSandboxFoliage& FoliageType, FVector& v, FRandomStream& rnd, UTerrainZoneComponent* Zone) {

	if (FoliageType.OffsetRange > 0) {
		float ox = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) ox = -ox; v.X += ox;
		float oy = rnd.FRandRange(0.f, FoliageType.OffsetRange); if (rnd.GetFraction() > 0.5) oy = -oy; v.Y += oy;
	}

	const FVector start_trace(v.X, v.Y, v.Z + USBT_ZONE_SIZE / 2);
	const FVector end_trace(v.X, v.Y, v.Z - USBT_ZONE_SIZE / 2);

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
				MeshType.StartCullDistance = FoliageType.StartCullDistance;
				MeshType.EndCullDistance = FoliageType.EndCullDistance;

				Zone->SpawnInstancedMesh(MeshType, Transform);
			}
		}
	}
}

void ASandboxTerrainController::LoadFoliage(UTerrainZoneComponent* Zone) {
	Zone->GetRegion()->SpawnInstMeshFromLoadCache(Zone);
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