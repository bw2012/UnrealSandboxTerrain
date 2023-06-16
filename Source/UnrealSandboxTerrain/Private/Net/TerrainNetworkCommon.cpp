// Copyright blackw 2015-2020

#include "TerrainNetworkCommon.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"


UTerrainNetworkworkComponent::UTerrainNetworkworkComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

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

int32 UTerrainNetworkworkComponent::UdpSend(FBufferArchive SendBuffer, const FIPv4Endpoint& EndPoint) {
	int32 BytesSent = 0;
	UdpSocket->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSent, *EndPoint.ToInternetAddr());
	return BytesSent;
}

int32 UTerrainNetworkworkComponent::UdpSend(FBufferArchive SendBuffer, const FInternetAddr& Addr) {
	int32 BytesSent = 0;
	UdpSocket->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSent, Addr);
	return BytesSent;
}


ASandboxTerrainController* UTerrainNetworkworkComponent::GetTerrainController() {
	return (ASandboxTerrainController*)GetAttachmentRootActor();
};
