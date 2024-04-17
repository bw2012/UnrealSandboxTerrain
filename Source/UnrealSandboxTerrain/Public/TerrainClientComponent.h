// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainNetworkCommon.h"
#include "Tasks/Task.h"
#include "TerrainClientComponent.generated.h"



class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainClientComponent : public UTerrainNetworkworkComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginDestroy();

	virtual void BeginPlay();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

public:

	void RequestVoxelData(const TVoxelIndex& Index);

	void RequestMapInfo();

	void RequestMapInfoIfStaled();

	void Init();

	void Start();

private:

	TSharedPtr<FInternetAddr> RemoteAddr;

	UE::Tasks::FTask ClientLoopTask;

	void HandleRcvData(FArrayReader& Data);

	void HandleResponseVd(FArrayReader& Data);

	void RcvThreadLoop();

	int32 StoredVStamp = 0;
};
