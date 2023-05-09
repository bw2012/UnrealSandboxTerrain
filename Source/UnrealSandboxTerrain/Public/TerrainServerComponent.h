// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainNetworkCommon.h"
#include "SandboxTerrainCommon.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Common/TcpListener.h"
#include <mutex>
#include "TerrainServerComponent.generated.h"



class ASandboxTerrainController;
struct TZoneModificationData;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainServerComponent : public UTerrainNetworkworkComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginDestroy();

	virtual void BeginPlay();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

public:

	bool OnConnectionAccepted(FSocket* SocketPtr, const FIPv4Endpoint& Endpoint);

	//template <typename... Ts>
	//void SendToAllClients(uint32 OpCode, Ts... Args);

	void SendToAllVdEdit(const TEditTerrainParam& EditParams);

private:

	void HandleRcvData(uint32 ClientId, FSocket* SocketPtr, FArrayReader& Data);

	bool SendVdByIndex(FSocket* SocketPtr, const TVoxelIndex& VoxelIndex);

	bool SendMapInfo(FSocket* SocketPtr, TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area);

	std::mutex Mutex;

	TMap<uint32, FSocket*> ConnectedClientsMap;

	FTcpListener* TcpListenerPtr;

	uint32 ClientCount = 0;

	void HandleClientConnection(uint32 ClientId);
	
};
