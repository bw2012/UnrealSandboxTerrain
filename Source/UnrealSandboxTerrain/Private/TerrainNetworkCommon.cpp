// Copyright blackw 2015-2020

#include "TerrainNetworkCommon.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"


UTerrainNetworkworkComponent::UTerrainNetworkworkComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainNetworkworkComponent::NetworkSend(FSocket* SocketPtr, FBufferArchive& Buffer) {
	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
	FNFSMessageHeader::WrapAndSendPayload(Buffer, SimpleAbstractSocket);
}

/*
void UTerrainNetworkworkComponent::HandleRcvData(FArrayReader& Data) {
	uint32 OpCode;
	Data << OpCode;

	uint32 OpCodeExt;
	Data << OpCodeExt;

	UE_LOG(LogSandboxTerrain, Log, TEXT("OpCode -> %d"), OpCode);

	if (OpcodeHandlerMap.Contains(OpCode)) {
		OpcodeHandlerMap[OpCode](Data);
	}
}
*/

void ConvertVoxelIndex(FArchive& Data, TVoxelIndex& Index) {
	Data << Index.X;
	Data << Index.Y;
	Data << Index.Z;
}

TVoxelIndex DeserializeVoxelIndex(FArrayReader& Data) {
	TVoxelIndex Index;
	ConvertVoxelIndex(Data, Index);
	return Index;
}

