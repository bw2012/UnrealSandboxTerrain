#pragma once


// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Net/UnrealNetwork.h"
#include "VoxelIndex.h"
#include "TerrainNetworkCommon.generated.h"


#define Net_Opcode_None					0

#define Net_Opcode_RequestVd			10
#define Net_Opcode_RequestMapInfo		11

#define Net_Opcode_ResponseVd			100
#define Net_Opcode_ResponseMapInfo		101

#define Net_Opcode_EditVd				200



class ASandboxTerrainController;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UTerrainNetworkworkComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

protected:

	void NetworkSend(FSocket* SocketPtr, FBufferArchive& Buffer);

	ASandboxTerrainController* GetTerrainController() {
		return (ASandboxTerrainController*)GetAttachmentRootActor();
	};

	//void HandleRcvData(FArrayReader& Data);

	//TMap<uint32, std::function<void(FArrayReader&)>> OpcodeHandlerMap;

};


void ConvertVoxelIndex(FArchive& Data, TVoxelIndex& Index);

TVoxelIndex DeserializeVoxelIndex(FArrayReader& Data);