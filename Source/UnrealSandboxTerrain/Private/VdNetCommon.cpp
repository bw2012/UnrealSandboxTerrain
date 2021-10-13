// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "VdNetCommon.h"
#include "NetworkMessage.h"


UVdNetworkComponent::UVdNetworkComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UVdNetworkComponent::NetworkSend(FSocket* SocketPtr, FBufferArchive& Buffer) {
	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
	FNFSMessageHeader::WrapAndSendPayload(Buffer, SimpleAbstractSocket);
}


void UVdNetworkComponent::HandleRcvData(FArrayReader& Data) {
	uint32 OpCode;
	Data << OpCode;

	UE_LOG(LogSandboxTerrain, Log, TEXT("OpCode -> %d"), OpCode);

	if (OpcodeHandlerMap.Contains(OpCode)) {
		OpcodeHandlerMap[OpCode](Data);
	}
}



