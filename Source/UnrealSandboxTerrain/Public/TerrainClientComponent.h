// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainNetworkCommon.h"
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

	template <typename... Ts>
	void SendToServer(uint32 OpCode, Ts... Args);

	void RequestVoxelData(const TVoxelIndex& Index);

	void RequestMapInfo();

private:

	void HandleRcvData(FArrayReader& Data);

	FSocket* ClientSocketPtr = nullptr;

	void HandleResponseVd(FArrayReader& Data);
	
};
