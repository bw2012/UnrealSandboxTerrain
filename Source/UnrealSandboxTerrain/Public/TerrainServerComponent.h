// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "TerrainNetworkCommon.h"
#include "SandboxTerrainCommon.h"
//#include "Interfaces/IPv4/IPv4Endpoint.h"
//#include "Common/TcpListener.h"
//#include <mutex>
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

private:

	void UdpRecv(const FArrayReaderPtr& ArrayReaderPtr, const FIPv4Endpoint& EndPoint);

	void HandleRcvData(const FIPv4Endpoint& EndPoint, FArrayReader& Data);

	bool SendVdByIndex(const FIPv4Endpoint& EndPoint, const TVoxelIndex& VoxelIndex);

	bool SendMapInfo(const FIPv4Endpoint& EndPoint, TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area);

	//std::mutex Mutex;

	//TMap<uint32, FSocket*> ClientMap;

	FUdpSocketReceiver* UDPReceiver;
	
};
