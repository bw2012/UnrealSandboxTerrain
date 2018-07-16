// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Networking.h"
#include "VdServerComponent.generated.h"



class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UVdServerComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginDestroy();

	virtual void BeginPlay();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

public:

	bool OnConnectionAccepted(FSocket* SocketPtr, const FIPv4Endpoint& Endpoint);

	bool SendVdByIndex(FSocket* SocketPtr, TVoxelIndex& VoxelIndex);

private:

	FTcpListener* TcpListenerPtr;

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};
	
};
