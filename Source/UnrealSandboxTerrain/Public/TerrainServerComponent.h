// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainNetworkCommon.h"
#include "SandboxTerrainCommon.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Common/TcpListener.h"
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

	void HandleRcvData(const FString& ClientRemoteAddr, FSocket* SocketPtr, FArrayReader& Data);

	bool SendVdByIndex(FSocket* SocketPtr, const TVoxelIndex& VoxelIndex);

	bool SendMapInfo(FSocket* SocketPtr, TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area);

	TMap<FString, FSocket*> ConnectedClientsMap;

	FTcpListener* TcpListenerPtr;
	
};
