
#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "TerrainZoneComponent.h"
#include "SandboxVoxeldata.h"
#include <cmath>
#include "DrawDebugHelpers.h"
#include "Async.h"
#include "Json.h"
#include "VdServerComponent.h"
#include "VdClientComponent.h"
#include "VoxelMeshComponent.h"

#include "serialization.hpp"
#include "utils.hpp"


bool LoadDataFromKvFile(TKvFile& KvFile, const TVoxelIndex& Index, std::function<void(TArray<uint8>&)> Function);

void SerializeMeshData(TMeshData const * MeshDataPtr, TArray<uint8>& CompressedData);

void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData);

void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData);

void deserializeVoxelDataFast(TVoxelData* vd, TArray<uint8>& Data, bool createSubstanceCache);


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
		FString ThreadName = FString::Printf(TEXT("voxel-controller-thread-%d"), FPlatformTime::Seconds());
		Thread = FRunnableThread::Create(this, *ThreadName);

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
	SaveGeneratedZones = 1000;

	ServerPort = 6000;

	TerrainGeneratorComponent = CreateDefaultSubobject<UTerrainGeneratorComponent>(TEXT("TerrainGenerator"));
	TerrainGeneratorComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
}

ASandboxTerrainController::ASandboxTerrainController() {
	PrimaryActorTick.bCanEverTick = true;
	MapName = TEXT("World 0");
	TerrainSizeX = 5;
	TerrainSizeY = 5;
	TerrainSizeZ = 5;
	bEnableLOD = false;
	SaveGeneratedZones = 1000;

	ServerPort = 6000;

	TerrainGeneratorComponent = CreateDefaultSubobject<UTerrainGeneratorComponent>(TEXT("TerrainGenerator"));
	TerrainGeneratorComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
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

	if (!OpenFile()) return;

	if (GetWorld()->IsServer()) {
		UE_LOG(LogTemp, Warning, TEXT("SERVER"));
		BeginServer();
	} else {
		UE_LOG(LogTemp, Warning, TEXT("CLIENT"));
		BeginClient();
	}
}

void ASandboxTerrainController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	// wait threads for finish
	for (auto* ThreadTask : ThreadList) {
		ThreadTask->Stop();
		ThreadTask->WaitForFinish();
	}

	Save();

	VdFile.close();
	MdFile.close();
	ObjFile.close();

	ClearVoxelData();
	TerrainZoneMap.Empty();
}

void ASandboxTerrainController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (HasNextAsyncTask()) {
		TControllerTaskTaskPtr Task = GetAsyncTask();
		if (Task->Function) {
			Task->Function();
			Task->bIsFinished = true;
		}
	}	

	auto It = ThreadList.begin();
	while (It != ThreadList.end()) {
		FAsyncThread* ThreadPtr = *It;
		if (ThreadPtr->IsFinished()) {
			std::unique_lock<std::shared_mutex> Lock(ThreadListMutex);
			delete ThreadPtr;
			It = ThreadList.erase(It);
			Lock.unlock();
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

void ASandboxTerrainController::BeginServer() {
	//===========================
	// load existing
	//===========================
	OnStartBuildTerrain();
	bIsGeneratingTerrain = true;
	LoadJson();

	SpawnInitialZone();
	// async loading other zones
	RunThread([&](FAsyncThread& ThisThread) {
		if (!bGenerateOnlySmallSpawnPoint) {
			int Total = (TerrainSizeX * 2 + 1) * (TerrainSizeY * 2 + 1) * (TerrainSizeZ * 2 + 1);
			int Progress = 0;

			ReverseSpiralWalkthrough(TerrainSizeX * 2, TerrainSizeY * 2, [&](int x, int y) {
				x -= TerrainSizeX;
				y -= TerrainSizeY;

				for (int z = -TerrainSizeZ; z <= TerrainSizeZ; z++) {
					TVoxelIndex Index(x, y, z);
					if (ThisThread.IsNotValid()) return;

					if (!HasVoxelData(Index)) {
						SpawnZone(Index);
					}

					Progress++;
					GeneratingProgress = (float)Progress / (float)Total;
					// must invoke in main thread
					//OnProgressBuildTerrain(PercentProgress);

					if (GeneratedVdConter > SaveGeneratedZones) {
						TControllerTaskTaskPtr TaskPtr = InvokeSafe([=]() { Save(); });
						TControllerTask::WaitForFinish(TaskPtr.get());
						GeneratedVdConter = 0;
					}

					if (ThisThread.IsNotValid()) return;
				}

			});

			/*
			for (int x = -TerrainSizeX; x <= TerrainSizeX; x++) {
				for (int y = -TerrainSizeY; y <= TerrainSizeY; y++) {
					for (int z = -TerrainSizeZ; z <= TerrainSizeZ; z++) {
						TVoxelIndex Index(x, y, z);
						if (ThisThread.IsNotValid()) return;

						if (!HasVoxelData(Index)) {
							SpawnZone(Index);
						}

						Progress++;
						GeneratingProgress = (float)Progress / (float)Total;
						// must invoke in main thread
						//OnProgressBuildTerrain(PercentProgress);

						if (GeneratedVdConter > SaveGeneratedZones) {
							TControllerTaskTaskPtr TaskPtr = InvokeSafe([=]() { Save(); });
							TControllerTask::WaitForFinish(TaskPtr.get());
							GeneratedVdConter = 0;
						}

						if (ThisThread.IsNotValid()) return;
					}
				}
			}
			*/
		}

		TControllerTask::WaitForFinish(InvokeSafe([=]() { Save(); }).get());

		RunThread([&](FAsyncThread& ThisThread) {
			bIsGeneratingTerrain = false;
			OnFinishBuildTerrain(); // FIXME: thread-safe

			InvokeSafe([&]() {
				UVdServerComponent* VdServerComponent = NewObject<UVdServerComponent>(this, TEXT("VdServer"));
				VdServerComponent->RegisterComponent();
				VdServerComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
			});
		});
	});
}

void ASandboxTerrainController::BeginClient() {
	UVdClientComponent* VdClientComponent = NewObject<UVdClientComponent>(this, TEXT("VdClient"));
	VdClientComponent->RegisterComponent();
	VdClientComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
}

void ASandboxTerrainController::NetworkSerializeVd(FBufferArchive& Buffer, const TVoxelIndex& VoxelIndex) {
	TVoxelDataInfo* VoxelDataInfo = GetVoxelDataInfo(VoxelIndex);
	// TODO: shared lock Vd
	if (VoxelDataInfo) {
		if (VoxelDataInfo->DataState == TVoxelDataState::READY_TO_LOAD) {
			TVoxelData* Vd = LoadVoxelDataByIndex(VoxelIndex);
			serializeVoxelData(*Vd, Buffer);
			delete Vd;
		} else if (VoxelDataInfo->DataState == TVoxelDataState::LOADED)  {
			serializeVoxelData(*VoxelDataInfo->Vd, Buffer);
		}

	}
}

void ASandboxTerrainController::Save() {
	UE_LOG(LogSandboxTerrain, Log, TEXT("Start save terrain data..."));

	double Start = FPlatformTime::Seconds();
	uint32 SavedVd = 0;
	uint32 SavedMd = 0;
	uint32 SavedObj = 0;

	for (auto& It : VoxelDataIndexMap) {
		TVoxelDataInfo& VdInfo = VoxelDataIndexMap[It.first];

		if (VdInfo.Vd == nullptr) {
			continue;
		}

		//============================================
		// TODO remove
		// test
		FBufferArchive TempBufferVd;
		TValueData buffer;

		serializeVoxelData(*VdInfo.Vd, TempBufferVd);
		buffer.reserve(TempBufferVd.Num());

		for (uint8 b : TempBufferVd) {
			buffer.push_back(b);
		}

		TVoxelIndex Index = GetZoneIndex(VdInfo.Vd->getOrigin());
		if (VdInfo.Vd->isChanged()) {
			VdFile.save(Index, buffer);
			VdInfo.Vd->resetLastSave();
			SavedVd++;
		}

		VdInfo.Unload();
	}
	UE_LOG(LogSandboxTerrain, Log, TEXT("Save voxel data ----> %d"), SavedVd);

	for (auto& Elem : TerrainZoneMap) {
		FVector ZoneIndex = Elem.Key;
		UTerrainZoneComponent* Zone = Elem.Value;

		TMeshData const * MeshDataPtr = Zone->GetCachedMeshData();
		if (MeshDataPtr) {
			TArray<uint8> TempBufferMd;
			SerializeMeshData(MeshDataPtr, TempBufferMd);

			TVoxelIndex Index2(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

			TValueData buffer;
			buffer.reserve(TempBufferMd.Num());
			for (uint8 b : TempBufferMd) {
				buffer.push_back(b);
			}

			MdFile.save(Index2, buffer);

			Zone->ClearCachedMeshData();
			SavedMd++;
		}

		if (Zone->IsNeedSave()) {
			FBufferArchive BinaryData;
			Zone->SerializeInstancedMeshes(BinaryData);

			TVoxelIndex Index2(ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

			TValueData buffer;
			buffer.reserve(BinaryData.Num());
			for (uint8 b : BinaryData) {
				buffer.push_back(b);
			}

			ObjFile.save(Index2, buffer);

			Zone->ResetNeedSave();
			SavedObj++;
		}
	}
	UE_LOG(LogSandboxTerrain, Log, TEXT("Save mesh data ----> %d"), SavedMd);
	UE_LOG(LogSandboxTerrain, Log, TEXT("Save objects data ----> %d"), SavedObj);

	SaveJson();

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Terrain saved -> %f ms"), Time);
}

void ASandboxTerrainController::SaveMapAsync() {
	UE_LOG(LogSandboxTerrain, Log, TEXT("Start save terrain async"));
	RunThread([&](FAsyncThread& ThisThread) {
		double Start = FPlatformTime::Seconds();
		Save();
	});
}

void ASandboxTerrainController::SaveJson() {
	UE_LOG(LogTemp, Log, TEXT("----------- save json -----------"));

	FString JsonStr;
	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
	FString FullPath = SavePath + TEXT("/Map/") + MapName + TEXT("/") + FileName;

	TSharedRef <TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteArrayStart("Regions");

	/*
	for (const FVector& Index : RegionIndexSet) {
		float x = Index.X;
		float y = Index.Y;
		float z = Index.Z;

		//FVector RegionPos = GetRegionPos(Index);
		FVector RegionPos;

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
	*/

	JsonWriter->WriteArrayEnd();
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	FFileHelper::SaveStringToFile(*JsonStr, *FullPath);
}

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

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDir)) {
		PlatformFile.CreateDirectory(*SaveDir);
		if (!PlatformFile.DirectoryExists(*SaveDir)) {
			UE_LOG(LogTemp, Warning, TEXT("Unable to create save directory -> %s"), *SaveDir);
			return false;
		}
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

void ASandboxTerrainController::LoadJson() {
	UE_LOG(LogTemp, Warning, TEXT("----------- load json -----------"));

	FString FileName = TEXT("terrain.json");
	FString SavePath = FPaths::ProjectSavedDir();
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
		}
	}
}

TControllerTaskTaskPtr ASandboxTerrainController::InvokeSafe(std::function<void()> Function) {
	if (IsInGameThread()) {
		Function();
		return nullptr;
	} else {
		TControllerTaskTaskPtr TaskPtr(new TControllerTask);
		TaskPtr->Function = Function;
		AddAsyncTask(TaskPtr);
		return TaskPtr;
	}
}

// spawn received zone on client
void ASandboxTerrainController::NetworkSpawnClientZone(const TVoxelIndex& Index, FArrayReader& RawVdData) {
	FVector Pos = GetZonePos(Index);

	TVoxelDataInfo VdInfo;
	VdInfo.Vd = new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
	VdInfo.Vd->setOrigin(Pos);

	FMemoryReader BinaryData = FMemoryReader(RawVdData, true); 
	BinaryData.Seek(RawVdData.Tell());
	deserializeVoxelData(*VdInfo.Vd, BinaryData);

	VdInfo.DataState = TVoxelDataState::GENERATED;
	VdInfo.Vd->setChanged();
	VdInfo.Vd->setCacheToValid();

	RegisterTerrainVoxelData(VdInfo, Index);

	if (VdInfo.Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
		TMeshDataPtr MeshDataPtr = GenerateMesh(VdInfo.Vd);
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			Zone->ApplyTerrainMesh(MeshDataPtr);
		});
	}
}


// load or generate new zone voxel data and mesh
void ASandboxTerrainController::SpawnZone(const TVoxelIndex& Index) {
	FVector Pos = GetZonePos(Index);

	// cancel if zone already exist
	if (GetZoneByVectorIndex(Index) != nullptr) return; 

	//if no voxel data in memory
	if (!HasVoxelData(Index)) {
		TVoxelDataInfo VdInfo;
		// if voxel data exist in file
		if (VdFile.isExist(Index)) {
			VdInfo.DataState = TVoxelDataState::READY_TO_LOAD;
			RegisterTerrainVoxelData(VdInfo, Index);
		} else {
			// generate new voxel data
			VdInfo.Vd = new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
			VdInfo.Vd->setOrigin(Pos);

			TerrainGeneratorComponent->GenerateVoxelTerrain(*VdInfo.Vd);
			GeneratedVdConter++;

			VdInfo.DataState = TVoxelDataState::GENERATED;
			VdInfo.Vd->setChanged();
			VdInfo.Vd->setCacheToValid();

			RegisterTerrainVoxelData(VdInfo, Index);
		}
	}

	// voxel data must exist in this point
	TVoxelDataInfo* VoxelDataInfo = GetVoxelDataInfo(Index);

	// if mesh data exist in file - load, apply and return
	TMeshDataPtr MeshDataPtr = LoadMeshDataByIndex(Index);
	if (MeshDataPtr != nullptr) {
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			Zone->ApplyTerrainMesh(MeshDataPtr, false); // already in cache
			OnLoadZone(Zone);
		});
		return;
	}

	// if no mesh data in file - generate mesh from voxel data
	if (VoxelDataInfo->Vd != nullptr && VoxelDataInfo->Vd->getDensityFillState() == TVoxelDataFillState::MIXED) {
		TMeshDataPtr MeshDataPtr = GenerateMesh(VoxelDataInfo->Vd);
		InvokeSafe([=]() {
			UTerrainZoneComponent* Zone = AddTerrainZone(Pos);
			Zone->ApplyTerrainMesh(MeshDataPtr);

			if (VoxelDataInfo->IsNewGenerated()) {
				OnGenerateNewZone(Zone);
			}

			if (VoxelDataInfo->IsNewLoaded()) {
				OnLoadZone(Zone);
			}
		});
	}
}


void ASandboxTerrainController::SpawnInitialZone() {
	const int s = static_cast<int>(TerrainInitialArea);
	TSet<FVector> InitialZoneSet;

	if (s > 0) {
		for (auto x = -s; x <= s; x++) {
			for (auto y = -s; y <= s; y++) {
				for (auto z = -s; z <= s; z++) {
					SpawnZone(TVoxelIndex(x, y, z));
				}
			}
		}
	} else {
		FVector Pos = FVector(0);
		SpawnZone(TVoxelIndex(0, 0, 0));
		InitialZoneSet.Add(Pos);
	}	
}

TVoxelIndex ASandboxTerrainController::GetZoneIndex(const FVector& Pos) {
	FVector Tmp = sandboxGridIndex(Pos, USBT_ZONE_SIZE);
	return TVoxelIndex(Tmp.X, Tmp.Y, Tmp.Z);
}

FVector ASandboxTerrainController::GetZonePos(const TVoxelIndex& Index) {
	return FVector((float)Index.X * USBT_ZONE_SIZE, (float)Index.Y * USBT_ZONE_SIZE, (float)Index.Z * USBT_ZONE_SIZE);
}

UTerrainZoneComponent* ASandboxTerrainController::GetZoneByVectorIndex(const TVoxelIndex& Index) {
	FVector TmpIndex(Index.X, Index.Y, Index.Z);
	if (TerrainZoneMap.Contains(TmpIndex)) {
		return TerrainZoneMap[TmpIndex];
	}

	return NULL;
}

UTerrainZoneComponent* ASandboxTerrainController::AddTerrainZone(FVector pos) {
	TVoxelIndex Index = GetZoneIndex(pos);
	FVector IndexTmp(Index.X, Index.Y,Index.Z);

	FString ZoneName = FString::Printf(TEXT("Zone -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
	UTerrainZoneComponent* ZoneComponent = NewObject<UTerrainZoneComponent>(this, FName(*ZoneName));
	if (ZoneComponent) {
		ZoneComponent->RegisterComponent();
		//ZoneComponent->SetRelativeLocation(pos);

		ZoneComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
		ZoneComponent->SetWorldLocation(pos);

		FString TerrainMeshCompName = FString::Printf(TEXT("TerrainMesh -> [%.0f, %.0f, %.0f]"), IndexTmp.X, IndexTmp.Y, IndexTmp.Z);
		UVoxelMeshComponent* TerrainMeshComp = NewObject<UVoxelMeshComponent>(this, FName(*TerrainMeshCompName));
		TerrainMeshComp->RegisterComponent();
		TerrainMeshComp->SetMobility(EComponentMobility::Stationary);
		TerrainMeshComp->SetCanEverAffectNavigation(true);
		TerrainMeshComp->SetCollisionProfileName(TEXT("InvisibleWall"));
		TerrainMeshComp->AttachToComponent(ZoneComponent, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);

		ZoneComponent->MainTerrainMesh = TerrainMeshComp;
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
	H ZoneHandler;
	ASandboxTerrainController* ControllerInstance;

	virtual uint32 Run() {
		ControllerInstance->EditTerrain(ZoneHandler);
		return 0;
	}
};

struct TZoneEditHandler {
	bool changed = false;

	bool enableLOD = false;

	float Strength;

	FVector Pos;

	float Extend;
};

void ASandboxTerrainController::FillTerrainRound(const FVector& Origin, float Extend, int MatId) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Pos;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < Extend) {
					//2^-((x^2)/20)
					float d = density + 1 / rl * Strength;
					vd->setDensity(x, y, z, d);
					changed = true;
				}

				if (rl < Extend + 20) {
					vd->setMaterial(x, y, z, newMaterialId);
				}			
			}, enableLOD);

			return changed;
		}
	} Zh;

	Zh.newMaterialId = MatId;
	Zh.enableLOD = bEnableLOD;
	Zh.Strength = 5;
	Zh.Pos = Origin;
	Zh.Extend = Extend;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}


void ASandboxTerrainController::DigTerrainRoundHole(const FVector& Origin, float Radius, float Strength) {
	if (GetWorld()->IsServer()) {
		UVdServerComponent* VdServerComponent = Cast<UVdServerComponent>(GetComponentByClass(UVdServerComponent::StaticClass()));
		VdServerComponent->SendToAllClients(USBT_NET_OPCODE_DIG_ROUND, Origin.X, Origin.Y, Origin.Z, Radius, Strength);
	} else {
		UVdClientComponent* VdClientComponent = Cast<UVdClientComponent>(GetComponentByClass(UVdClientComponent::StaticClass()));
		VdClientComponent->SendToServer(USBT_NET_OPCODE_DIG_ROUND, Origin.X, Origin.Y, Origin.Z, Radius, Strength);
		return;
	}

	DigTerrainRoundHole_Internal(Origin, Radius, Strength);
}

void ASandboxTerrainController::DigTerrainRoundHole_Internal(const FVector& Origin, float Radius, float Strength) {
	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;

		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				float density = vd->getDensity(x, y, z);
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Pos;

				float rl = std::sqrt(o.X * o.X + o.Y * o.Y + o.Z * o.Z);
				if (rl < Extend) {
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
	} Zh;

	Zh.MaterialMapPtr = &MaterialMap;
	Zh.enableLOD = bEnableLOD;
	Zh.Strength = Strength;
	Zh.Pos = Origin;
	Zh.Extend = Radius;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}


void ASandboxTerrainController::DigTerrainCubeHole(const FVector& Origin, float Extend) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		TMap<uint16, FSandboxTerrainMaterial>* MaterialMapPtr;

		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Pos;
				if (o.X < Extend && o.X > -Extend && o.Y < Extend && o.Y > -Extend && o.Z < Extend && o.Z > -Extend) {
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
	} Zh;

	Zh.enableLOD = bEnableLOD;
	Zh.MaterialMapPtr = &MaterialMap;
	Zh.Pos = Origin;
	Zh.Extend = Extend;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

void ASandboxTerrainController::FillTerrainCube(const FVector& Origin, float Extend, int MatId) {
	if (!GetWorld()->IsServer()) return;

	struct ZoneHandler : TZoneEditHandler {
		int newMaterialId;
		bool operator()(TVoxelData* vd) {
			changed = false;

			vd->forEachWithCache([&](int x, int y, int z) {
				FVector o = vd->voxelIndexToVector(x, y, z);
				o += vd->getOrigin();
				o -= Pos;
				if (o.X < Extend && o.X > -Extend && o.Y < Extend && o.Y > -Extend && o.Z < Extend && o.Z > -Extend) {
					vd->setDensity(x, y, z, 1);
					changed = true;
				}

				float radiusMargin = Extend + 20;
				if (o.X < radiusMargin && o.X > -radiusMargin && o.Y < radiusMargin && o.Y > -radiusMargin && o.Z < radiusMargin && o.Z > -radiusMargin) {
					vd->setMaterial(x, y, z, newMaterialId);
				}
			}, enableLOD);

			return changed;
		}
	} Zh;

	Zh.newMaterialId = MatId;
	Zh.enableLOD = bEnableLOD;
	Zh.Pos = Origin;
	Zh.Extend = Extend;
	ASandboxTerrainController::PerformTerrainChange(Zh);
}

template<class H>
void ASandboxTerrainController::PerformTerrainChange(H Handler) {
	FTerrainEditThread<H>* EditThread = new FTerrainEditThread<H>();
	EditThread->ZoneHandler = Handler;
	EditThread->ControllerInstance = this;

	FString ThreadName = FString::Printf(TEXT("terrain_change-thread-%d"), FPlatformTime::Seconds());
	FRunnableThread* Thread = FRunnableThread::Create(EditThread, *ThreadName);
	//FIXME delete thread after finish

	FVector TestPoint(Handler.Pos);
	TestPoint.Z -= 10;
	TArray<struct FHitResult> OutHits;
	bool bIsOverlap = GetWorld()->SweepMultiByChannel(OutHits, Handler.Pos, TestPoint, FQuat(), ECC_Visibility, FCollisionShape::MakeSphere(Handler.Extend)); // ECC_Visibility
	if (bIsOverlap) {
		for (auto OverlapItem : OutHits) {
			AActor* Actor = OverlapItem.GetActor();
			if (Cast<ASandboxTerrainController>(OverlapItem.GetActor()) != nullptr) {
				UHierarchicalInstancedStaticMeshComponent* InstancedMesh = Cast<UHierarchicalInstancedStaticMeshComponent>(OverlapItem.GetComponent());
				if (InstancedMesh != nullptr) {
					InstancedMesh->RemoveInstance(OverlapItem.Item);

					TArray<USceneComponent*> Parents;
					InstancedMesh->GetParentComponents(Parents);
					if (Parents.Num() > 0) {
						UTerrainZoneComponent* Zone = Cast<UTerrainZoneComponent>(Parents[0]);
						if (Zone) {
							Zone->SetNeedSave();
						}
					}
				}
			}
		}
	}
}

template<class H>
void ASandboxTerrainController::PerformZoneEditHandler(TVoxelData* Vd, H handler, std::function<void(TMeshDataPtr)> OnComplete) {
	bool bIsChanged = false;
	TMeshDataPtr MeshDataPtr = nullptr;

	Vd->vd_edit_mutex.lock();
	bIsChanged = handler(Vd);
	if (bIsChanged) {
		Vd->setChanged();
		Vd->setCacheToValid();
		MeshDataPtr = GenerateMesh(Vd);
		Vd->resetLastMeshRegenerationTime();
		MeshDataPtr->TimeStamp = FPlatformTime::Seconds();
	}
	Vd->vd_edit_mutex.unlock();

	if (bIsChanged) {
		OnComplete(MeshDataPtr);
	}
}

template<class H>
void ASandboxTerrainController::EditTerrain(const H& ZoneHandler) {
	double Start = FPlatformTime::Seconds();
	
	static float ZoneVolumeSize = USBT_ZONE_SIZE / 2;
	TVoxelIndex BaseZoneIndex = GetZoneIndex(ZoneHandler.Pos);

	static const float V[3] = { -1, 0, 1 };
	for (float x : V) {
		for (float y : V) {
			for (float z : V) {
				TVoxelIndex ZoneIndex = BaseZoneIndex + TVoxelIndex(x, y, z);
				UTerrainZoneComponent* Zone = GetZoneByVectorIndex(ZoneIndex);
				TVoxelDataInfo* VoxelDataInfo = GetVoxelDataInfo(ZoneIndex);
				TVoxelData* Vd = nullptr;

				// check zone bounds
				FVector ZoneOrigin = GetZonePos(ZoneIndex);
				FVector Upper(ZoneOrigin.X + ZoneVolumeSize, ZoneOrigin.Y + ZoneVolumeSize, ZoneOrigin.Z + ZoneVolumeSize);
				FVector Lower(ZoneOrigin.X - ZoneVolumeSize, ZoneOrigin.Y - ZoneVolumeSize, ZoneOrigin.Z - ZoneVolumeSize);

				if (FMath::SphereAABBIntersection(FSphere(ZoneHandler.Pos, ZoneHandler.Extend), FBox(Lower, Upper))) {
					if (VoxelDataInfo == nullptr) {
						continue;
					}

					if (VoxelDataInfo->DataState == TVoxelDataState::READY_TO_LOAD) {
						// double-check locking
						VoxelDataInfo->LoadVdMutexPtr->lock();
						if ((VoxelDataInfo->DataState == TVoxelDataState::READY_TO_LOAD)) {
							Vd = LoadVoxelDataByIndex(ZoneIndex);
							VoxelDataInfo->DataState = TVoxelDataState::LOADED;
							VoxelDataInfo->Vd = Vd;
						} else {
							Vd = VoxelDataInfo->Vd;
						}
						VoxelDataInfo->LoadVdMutexPtr->unlock();
					} else {
						Vd = VoxelDataInfo->Vd;
					}

					if (Vd == nullptr) {
						continue;
					}

					if (Zone == nullptr) {
						PerformZoneEditHandler(Vd, ZoneHandler, [&](TMeshDataPtr MeshDataPtr){ InvokeLazyZoneAsync(ZoneIndex, MeshDataPtr); });
					} else {
						PerformZoneEditHandler(Vd, ZoneHandler, [&](TMeshDataPtr MeshDataPtr){ InvokeZoneMeshAsync(Zone, MeshDataPtr); });
					}
				}
			}
		}
	}

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;
	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainController::editTerrain -------------> %f %f %f --> %f ms"), v.X, v.Y, v.Z, Time);
}


void ASandboxTerrainController::InvokeZoneMeshAsync(UTerrainZoneComponent* Zone, TMeshDataPtr MeshDataPtr) {
	TControllerTaskTaskPtr TaskPtr(new TControllerTask);
	TaskPtr->Function = [=]() {
		if (MeshDataPtr) {
			Zone->ApplyTerrainMesh(MeshDataPtr);
		}
	};

	AddAsyncTask(TaskPtr);
}

void ASandboxTerrainController::InvokeLazyZoneAsync(TVoxelIndex& Index, TMeshDataPtr MeshDataPtr) {
	TControllerTaskTaskPtr TaskPtr(new TControllerTask);

	FVector ZonePos = GetZonePos(Index);
	TaskPtr->Function = [&]() {
		UTerrainZoneComponent* Zone = AddTerrainZone(ZonePos);
		Zone->ApplyTerrainMesh(MeshDataPtr);
	};

	AddAsyncTask(TaskPtr);
}

//======================================================================================================================================================================

// TODO: use shared_ptr
TVoxelData* ASandboxTerrainController::LoadVoxelDataByIndex(const TVoxelIndex& Index) {
	double Start = FPlatformTime::Seconds();

	TVoxelData* Vd = new TVoxelData(USBT_ZONE_DIMENSION, USBT_ZONE_SIZE);
	Vd->setOrigin(GetZonePos(Index));

	bool bIsLoaded = LoadDataFromKvFile(VdFile, Index, [=](TArray<uint8>& Data) { 
		//FMemoryReader BinaryData = FMemoryReader(Data, true); //true, free data after done
		//BinaryData.Seek(0);
		//deserializeVoxelData(*Vd, BinaryData);
		deserializeVoxelDataFast(Vd, Data, false);
	});

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	if (bIsLoaded) {
		UE_LOG(LogTemp, Log, TEXT("loading voxel data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	}

	return Vd;
}

void ASandboxTerrainController::OnGenerateNewZone(UTerrainZoneComponent* Zone) {
	if (!bDisableFoliage) {
		TerrainGeneratorComponent->GenerateNewFoliage(Zone);
	}
}

void ASandboxTerrainController::OnLoadZone(UTerrainZoneComponent* Zone) {
	if (!bDisableFoliage) {
		LoadFoliage(Zone);
	}
}

void ASandboxTerrainController::AddAsyncTask(TControllerTaskTaskPtr TaskPtr) {
	std::unique_lock<std::shared_mutex> Lock(AsyncTaskListMutex);
	AsyncTaskList.push(TaskPtr);
}

TControllerTaskTaskPtr ASandboxTerrainController::GetAsyncTask() {
	std::shared_lock<std::shared_mutex> Lock(AsyncTaskListMutex);
	TControllerTaskTaskPtr Task = AsyncTaskList.front();
	AsyncTaskList.pop();
	return Task;
}

bool ASandboxTerrainController::HasNextAsyncTask() {
	return AsyncTaskList.size() > 0;
}

void ASandboxTerrainController::RegisterTerrainVoxelData(TVoxelDataInfo VdInfo, TVoxelIndex Index) {
	std::unique_lock<std::shared_mutex> Lock(VoxelDataMapMutex);
	auto It = VoxelDataIndexMap.find(Index);
	if (It != VoxelDataIndexMap.end()) {
		VoxelDataIndexMap.erase(It);
	}
	VoxelDataIndexMap.insert({ Index, VdInfo });
}

void ASandboxTerrainController::RunThread(std::function<void(FAsyncThread&)> Function) {
	FAsyncThread* ThreadTask = new FAsyncThread(Function);
	std::unique_lock<std::shared_mutex> Lock(ThreadListMutex);
	ThreadList.push_back(ThreadTask);
	ThreadTask->Start();
}

TVoxelData* ASandboxTerrainController::GetVoxelDataByPos(const FVector& Pos) {
	return GetVoxelDataByIndex(GetZoneIndex(Pos));
}

TVoxelData* ASandboxTerrainController::GetVoxelDataByIndex(const TVoxelIndex& Index) {
	std::shared_lock<std::shared_mutex> Lock(VoxelDataMapMutex);
	if (VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end()) {
		TVoxelDataInfo VdInfo = VoxelDataIndexMap[Index];
		return VdInfo.Vd;
	}

	return NULL;
}

bool ASandboxTerrainController::HasVoxelData(const TVoxelIndex& Index) {
	std::shared_lock<std::shared_mutex> Lock(VoxelDataMapMutex);
	return VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end();
}

TVoxelDataInfo* ASandboxTerrainController::GetVoxelDataInfo(const TVoxelIndex& Index) {
	std::shared_lock<std::shared_mutex> Lock(VoxelDataMapMutex);
	if (VoxelDataIndexMap.find(Index) != VoxelDataIndexMap.end()) {
		return &VoxelDataIndexMap[Index];
	}

	return nullptr;
}

void ASandboxTerrainController::ClearVoxelData() {
	std::unique_lock<std::shared_mutex> Lock(VoxelDataMapMutex);
	VoxelDataIndexMap.clear();
}

//======================================================================================================================================================================
// Sandbox Foliage
//======================================================================================================================================================================

void ASandboxTerrainController::LoadFoliage(UTerrainZoneComponent* Zone) {
	if (bDisableFoliage) return;

	TInstMeshTypeMap ZoneInstMeshMap;
	LoadObjectDataByIndex(Zone, ZoneInstMeshMap);

	for (auto& Elem : ZoneInstMeshMap) {
		TInstMeshTransArray& InstMeshTransArray = Elem.Value;

		for (FTransform& Transform : InstMeshTransArray.TransformArray) {
			Zone->SpawnInstancedMesh(InstMeshTransArray.MeshType, Transform);
		}
	}
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

UMaterialInterface* ASandboxTerrainController::GetTransitionTerrainMaterial(std::set<unsigned short>& MaterialIdSet) {
	if (TransitionMaterial == nullptr) {
		return nullptr;
	}

	uint64 Code = TMeshMaterialTransitionSection::GenerateTransitionCode(MaterialIdSet);
	if (!TransitionMaterialCache.Contains(Code)) {
		TTransitionMaterialCode tmp;
		tmp.Code = Code;

		UE_LOG(LogTemp, Warning, TEXT("create new transition terrain material instance ----> id: %llu (%lu-%lu-%lu)"), Code, tmp.TriangleMatId[0], tmp.TriangleMatId[1], tmp.TriangleMatId[2]);
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

		TransitionMaterialCache.Add(Code, DynMaterial);
		return DynMaterial;
	}

	return TransitionMaterialCache[Code];
}

float ASandboxTerrainController::GetRealGroungLevel(float X, float Y) {
	return TerrainGeneratorComponent->GroundLevelFunc(FVector(X, Y, 0)); ;
}

//======================================================================================================================================================================
// mesh data de/serealization
//======================================================================================================================================================================

std::shared_ptr<TMeshData> ASandboxTerrainController::GenerateMesh(TVoxelData* Vd) {
	double start = FPlatformTime::Seconds();

	if (Vd == NULL || Vd->getDensityFillState() == TVoxelDataFillState::ZERO ||	Vd->getDensityFillState() == TVoxelDataFillState::FULL) {
		return NULL;
	}

	TVoxelDataParam Vdp;

	if (bEnableLOD) {
		Vdp.bGenerateLOD = true;
		Vdp.collisionLOD = GetCollisionMeshSectionLodIndex();
	} else {
		Vdp.bGenerateLOD = false;
		Vdp.collisionLOD = 0;
	}

	TMeshDataPtr MeshDataPtr = sandboxVoxelGenerateMesh(*Vd, Vdp);

	double end = FPlatformTime::Seconds();
	double time = (end - start) * 1000;

	//UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone::generateMesh -------------> %f %f %f --> %f ms"), GetComponentLocation().X, GetComponentLocation().Y, GetComponentLocation().Z, time);

	return MeshDataPtr;
}

//======================================================================================================================================================================
// mesh data de/serealization
//======================================================================================================================================================================

void SerializeMeshContainer(const TMeshContainer& MeshContainer, FBufferArchive& BinaryData) {
	// save regular materials
	int32 LodSectionRegularMatNum = MeshContainer.MaterialSectionMap.Num();
	BinaryData << LodSectionRegularMatNum;
	for (auto& Elem : MeshContainer.MaterialSectionMap) {
		unsigned short MatId = Elem.Key;
		const TMeshMaterialSection& MaterialSection = Elem.Value;

		BinaryData << MatId;

		const FProcMeshSection& Mesh = MaterialSection.MaterialMesh;
		Mesh.SerializeMesh(BinaryData);
	}

	// save transition materials
	int32 LodSectionTransitionMatNum = MeshContainer.MaterialTransitionSectionMap.Num();
	BinaryData << LodSectionTransitionMatNum;
	for (auto& Elem : MeshContainer.MaterialTransitionSectionMap) {
		unsigned short MatId = Elem.Key;
		const TMeshMaterialTransitionSection& TransitionMaterialSection = Elem.Value;

		BinaryData << MatId;

		int MatSetSize = TransitionMaterialSection.MaterialIdSet.size();
		BinaryData << MatSetSize;

		for (unsigned short MatSetElement : TransitionMaterialSection.MaterialIdSet) {
			BinaryData << MatSetElement;
		}

		const FProcMeshSection& Mesh = TransitionMaterialSection.MaterialMesh;
		Mesh.SerializeMesh(BinaryData);
	}
}

void SerializeMeshData(TMeshData const * MeshDataPtr, TArray<uint8>& CompressedData) {
	FBufferArchive BinaryData;

	int32 LodArraySize = MeshDataPtr->MeshSectionLodArray.Num();
	BinaryData << LodArraySize;

	for (int32 LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
		const TMeshLodSection& LodSection = MeshDataPtr->MeshSectionLodArray[LodIdx];
		BinaryData << LodIdx;

		// save whole mesh
		LodSection.WholeMesh.SerializeMesh(BinaryData);

		SerializeMeshContainer(LodSection.RegularMeshContainer, BinaryData);

		if (LodIdx > 0) {
			for (auto i = 0; i < 6; i++) {
				SerializeMeshContainer(LodSection.TransitionPatchArray[i], BinaryData);
			}
		}
	}

	FArchiveSaveCompressedProxy Compressor = FArchiveSaveCompressedProxy(CompressedData, ECompressionFlags::COMPRESS_ZLIB);
	Compressor << BinaryData;
	Compressor.Flush();
}

void DeserializeMeshContainer(TMeshContainer& MeshContainer, FMemoryReader& BinaryData) {
	// regular materials
	int32 LodSectionRegularMatNum;
	BinaryData << LodSectionRegularMatNum;

	for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
		unsigned short MatId;
		BinaryData << MatId;

		TMeshMaterialSection& MatSection = MeshContainer.MaterialSectionMap.FindOrAdd(MatId);
		MatSection.MaterialId = MatId;

		MatSection.MaterialMesh.DeserializeMesh(BinaryData);
	}

	// transition materials
	int32 LodSectionTransitionMatNum;
	BinaryData << LodSectionTransitionMatNum;

	for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {
		unsigned short MatId;
		BinaryData << MatId;

		int MatSetSize;
		BinaryData << MatSetSize;

		std::set<unsigned short> MatSet;
		for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
			unsigned short MatSetElement;
			BinaryData << MatSetElement;

			MatSet.insert(MatSetElement);
		}

		TMeshMaterialTransitionSection& MatTransSection = MeshContainer.MaterialTransitionSectionMap.FindOrAdd(MatId);
		MatTransSection.MaterialId = MatId;
		MatTransSection.MaterialIdSet = MatSet;
		MatTransSection.TransitionName = TMeshMaterialTransitionSection::GenerateTransitionName(MatSet);

		MatTransSection.MaterialMesh.DeserializeMesh(BinaryData);
	}
}

void DeserializeMeshContainerFast(TMeshContainer& MeshContainer, FastUnsafeDeserializer& Deserializer) {
	// regular materials
	int32 LodSectionRegularMatNum;
	Deserializer.readObj(LodSectionRegularMatNum);

	for (int RMatIdx = 0; RMatIdx < LodSectionRegularMatNum; RMatIdx++) {
		unsigned short MatId;
		Deserializer.readObj(MatId);

		TMeshMaterialSection& MatSection = MeshContainer.MaterialSectionMap.FindOrAdd(MatId);
		MatSection.MaterialId = MatId;

		MatSection.MaterialMesh.DeserializeMeshFast(Deserializer);
	}

	// transition materials
	int32 LodSectionTransitionMatNum;
	Deserializer.readObj(LodSectionTransitionMatNum);

	for (int TMatIdx = 0; TMatIdx < LodSectionTransitionMatNum; TMatIdx++) {
		unsigned short MatId;
		Deserializer.readObj(MatId);

		int MatSetSize;
		Deserializer.readObj(MatSetSize);

		std::set<unsigned short> MatSet;
		for (int MatSetIdx = 0; MatSetIdx < MatSetSize; MatSetIdx++) {
			unsigned short MatSetElement;
			Deserializer.readObj(MatSetElement);

			MatSet.insert(MatSetElement);
		}

		TMeshMaterialTransitionSection& MatTransSection = MeshContainer.MaterialTransitionSectionMap.FindOrAdd(MatId);
		MatTransSection.MaterialId = MatId;
		MatTransSection.MaterialIdSet = MatSet;
		MatTransSection.TransitionName = TMeshMaterialTransitionSection::GenerateTransitionName(MatSet);

		MatTransSection.MaterialMesh.DeserializeMeshFast(Deserializer);
	}
}

TMeshDataPtr DeserializeMeshData(FMemoryReader& BinaryData, uint32 CollisionMeshSectionLodIndex) {
	TMeshDataPtr MeshDataPtr(new TMeshData);

	int32 LodArraySize;
	BinaryData << LodArraySize;

	for (int LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
		int32 LodIndex;
		BinaryData << LodIndex;

		// whole mesh
		MeshDataPtr.get()->MeshSectionLodArray[LodIndex].WholeMesh.DeserializeMesh(BinaryData);
		DeserializeMeshContainer(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer, BinaryData);

		if (LodIdx > 0) {
			for (auto i = 0; i < 6; i++) {
				DeserializeMeshContainer(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].TransitionPatchArray[i], BinaryData);
			}
		}
	}

	MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[CollisionMeshSectionLodIndex].WholeMesh;
	return MeshDataPtr;
}


TMeshDataPtr DeserializeMeshDataFast(const TArray<uint8>& Data, uint32 CollisionMeshSectionLodIndex) {
	TMeshDataPtr MeshDataPtr(new TMeshData);
	FastUnsafeDeserializer Deserializer(Data.GetData());

	int32 LodArraySize;
	Deserializer.readObj(LodArraySize);

	for (int LodIdx = 0; LodIdx < LodArraySize; LodIdx++) {
		int32 LodIndex;
		Deserializer.readObj(LodIndex);

		// whole mesh
		MeshDataPtr.get()->MeshSectionLodArray[LodIndex].WholeMesh.DeserializeMeshFast(Deserializer);
		DeserializeMeshContainerFast(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].RegularMeshContainer, Deserializer);

		if (LodIdx > 0) {
			for (auto i = 0; i < 6; i++) {
				DeserializeMeshContainerFast(MeshDataPtr.get()->MeshSectionLodArray[LodIndex].TransitionPatchArray[i], Deserializer);
			}
		}
	}

	MeshDataPtr.get()->CollisionMeshPtr = &MeshDataPtr.get()->MeshSectionLodArray[CollisionMeshSectionLodIndex].WholeMesh;
	return MeshDataPtr;
}

bool LoadDataFromKvFile(TKvFile& KvFile, const TVoxelIndex& Index, std::function<void(TArray<uint8>&)> Function) {
	TValueDataPtr DataPtr = KvFile.loadData(Index);

	if (DataPtr == nullptr || DataPtr->size() == 0) {
		return false;
	}

	// we need TArray to deseralization 
	// so just copy memory from std::vector<char> to TArray<uint8>
	// TODO refactor deseralization to use std::vector<char>
	TArray<uint8> BinaryArray;
	BinaryArray.SetNum(DataPtr->size());
	FMemory::Memcpy(BinaryArray.GetData(), DataPtr->data(), DataPtr->size());

	Function(BinaryArray);
	return true;
}


TMeshDataPtr ASandboxTerrainController::LoadMeshDataByIndex(const TVoxelIndex& Index) {
	double Start = FPlatformTime::Seconds();
	TMeshDataPtr MeshDataPtr = nullptr;

	bool bIsLoaded = LoadDataFromKvFile(MdFile, Index, [&](TArray<uint8>& Data) {
		FArchiveLoadCompressedProxy Decompressor = FArchiveLoadCompressedProxy(Data, ECompressionFlags::COMPRESS_ZLIB);
		if (Decompressor.GetError()) {
			UE_LOG(LogTemp, Log, TEXT("FArchiveLoadCompressedProxy -> ERROR : File was not compressed"));
			return;
		}

		//FBufferArchive DecompressedBinaryArray;
		//Decompressor << DecompressedBinaryArray;
		//FMemoryReader BinaryData = FMemoryReader(DecompressedBinaryArray, true); //true, free data after done
		//BinaryData.Seek(0);

		TArray<uint8> DecompressedData;
		Decompressor << DecompressedData;

		UE_LOG(LogTemp, Log, TEXT("DecompressedData -> %d bytes"), DecompressedData.Num());

		//MeshDataPtr = DeserializeMeshData(BinaryData, GetCollisionMeshSectionLodIndex());
		MeshDataPtr = DeserializeMeshDataFast(DecompressedData, GetCollisionMeshSectionLodIndex());

		Data.Empty();
		Decompressor.FlushCache();
		//BinaryData.FlushCache();
		//DecompressedBinaryArray.Empty();
		//DecompressedBinaryArray.Close();
	});

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	if (bIsLoaded) {
		UE_LOG(LogTemp, Log, TEXT("loading mesh data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	}

	return MeshDataPtr;
}

void ASandboxTerrainController::LoadObjectDataByIndex(UTerrainZoneComponent* Zone, TInstMeshTypeMap& ZoneInstMeshMap) {
	double Start = FPlatformTime::Seconds();
	TVoxelIndex Index = GetZoneIndex(Zone->GetComponentLocation());

	bool bIsLoaded = LoadDataFromKvFile(ObjFile, Index, [&](TArray<uint8>& Data) {
		FMemoryReader BinaryData = FMemoryReader(Data, true); //true, free data after done
		BinaryData.Seek(0);

		Zone->DeserializeInstancedMeshes(BinaryData, ZoneInstMeshMap);
	});

	double End = FPlatformTime::Seconds();
	double Time = (End - Start) * 1000;

	if (bIsLoaded) {
		//UE_LOG(LogTemp, Log, TEXT("loading inst-objects data block -> %d %d %d -> %f ms"), Index.X, Index.Y, Index.Z, Time);
	}
}



#define DATA_END_MARKER 666999

void serializeVoxelData(TVoxelData& vd, FBufferArchive& binaryData) {
	int32 num = vd.num();
	float size = vd.size();
	unsigned char volume_state = 0;

	binaryData << num;
	binaryData << size;

	// save density
	if (vd.getDensityFillState() == TVoxelDataFillState::ZERO) {
		volume_state = 0;
	}

	if (vd.getDensityFillState() == TVoxelDataFillState::FULL) {
		volume_state = 1;
	}

	if (vd.getDensityFillState() == TVoxelDataFillState::MIXED) {
		volume_state = 2;
	}

	binaryData << volume_state;

	// save material
	if (vd.material_data == NULL) {
		volume_state = 0;
	} else {
		volume_state = 2;
	}

	unsigned short base_mat = vd.base_fill_mat;

	binaryData << volume_state;
	binaryData << base_mat;

	if (vd.getDensityFillState() == TVoxelDataFillState::MIXED) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density = vd.getRawDensityUnsafe(x, y, z);
					binaryData << density;
				}
			}
		}
	}

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned short matId = vd.getRawMaterialUnsafe(x, y, z);
					binaryData << matId;
				}
			}
		}
	}

	int32 end_marker = DATA_END_MARKER;
	binaryData << end_marker;
}

//===============================================================================================
// test
//===============================================================================================

void deserializeVoxelDataFast(TVoxelData* vd, TArray<uint8>& Data, bool createSubstanceCache) {
	FastUnsafeDeserializer deserializer(Data.GetData());

	TVoxelDataHeader header;
	deserializer.readObj(header);

	vd->voxel_num = header.voxel_num;
	vd->volume_size = header.volume_size;
	vd->base_fill_mat = header.base_fill_mat;

	if (header.density_state == 2) {
		const size_t s = header.voxel_num * header.voxel_num * header.voxel_num;

		vd->density_data = new unsigned char[s];
		deserializer.read(vd->density_data, s);

		vd->material_data = new unsigned short[s];
		deserializer.read(vd->material_data, s);

		uint32 test;
		deserializer.readObj(test);

		vd->density_state = TVoxelDataFillState::MIXED;

		if (createSubstanceCache) {
			auto num = header.voxel_num;
			for (uint32 x = 0; x < num; x++) {
				for (uint32 y = 0; y < num; y++) {
					for (uint32 z = 0; z < num; z++) {
						vd->performSubstanceCacheLOD(x, y, z);
					}
				}
			}
		}
	} else {
		if (header.density_state == 0) {
			vd->deinitializeDensity(TVoxelDataFillState::ZERO);
		}

		if (header.density_state == 1) {
			vd->deinitializeDensity(TVoxelDataFillState::FULL);
		}

		vd->deinitializeMaterial(header.base_fill_mat);
	}

	UE_LOG(LogTemp, Log, TEXT("test -> %d %f %d %d"), header.voxel_num, header.volume_size, header.density_state, header.base_fill_mat);
}


void deserializeVoxelData(TVoxelData &vd, FMemoryReader& binaryData) {
	int32 num;
	float size;
	unsigned char volume_state;
	unsigned short base_mat;

	binaryData << num;
	binaryData << size;

	// load density
	binaryData << volume_state;

	if (volume_state == 0) {
		vd.deinitializeDensity(TVoxelDataFillState::ZERO);
	}

	if (volume_state == 1) {
		vd.deinitializeDensity(TVoxelDataFillState::FULL);
	}

	// load material
	binaryData << volume_state;
	binaryData << base_mat;

	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned char density;
					binaryData << density;
					vd.setVoxelPointDensity(x, y, z, density);
					vd.performSubstanceCacheLOD(x, y, z);
				}
			}
		}
	}
	
	if (volume_state == 2) {
		for (int x = 0; x < num; x++) {
			for (int y = 0; y < num; y++) {
				for (int z = 0; z < num; z++) {
					unsigned short matId;
					binaryData << matId;
					vd.setVoxelPointMaterial(x, y, z, matId);
				}
			}
		}
	} else {
		vd.deinitializeMaterial(base_mat);
	}

	int32 end_marker;
	binaryData << end_marker;

	if (end_marker != DATA_END_MARKER) {
		UE_LOG(LogTemp, Warning, TEXT("Broken data! - end marker is not found"));
	}
}