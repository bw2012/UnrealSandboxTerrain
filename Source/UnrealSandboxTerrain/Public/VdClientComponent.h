// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Networking.h"
#include "VdNetCommon.h"
#include "VdClientComponent.generated.h"



class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UVdClientComponent : public UVdNetworkComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginDestroy();

	virtual void BeginPlay();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

public:

	template <typename... Ts>
	void SendToServer(uint32 OpCode, Ts... Args);


private:

	FSocket* ClientSocketPtr = nullptr;

	void HandleServerResponse(FArrayReader& Data);

	void HandleResponseVd(FArrayReader& Data);
	
};
