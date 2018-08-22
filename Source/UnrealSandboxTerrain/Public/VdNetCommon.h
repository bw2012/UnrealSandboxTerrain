#pragma once


// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Networking.h"
#include "VdNetCommon.generated.h"



class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UVdNetworkComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

protected:

	void NetworkSend(FSocket* SocketPtr, FBufferArchive& Buffer);

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	void HandleRcvData(FArrayReader& Data);

	TMap<uint32, std::function<void(FArrayReader&)>> OpcodeHandlerMap;

};