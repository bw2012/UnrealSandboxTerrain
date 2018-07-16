// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"
#include "VdServerComponent.h"


UVdServerComponent::UVdServerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UVdServerComponent::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("UVdServerComponent::BeginPlay()"));

	const FString VdServerSocketName = TEXT("test");
	const int Port = 6000;

	FIPv4Endpoint Endpoint(FIPv4Address(0, 0, 0, 0), Port);
	FSocket* ListenerSocketPtr = FTcpSocketBuilder(*VdServerSocketName).AsReusable().BoundToEndpoint(Endpoint).Listening(8);

	const int32 ReceiveBufferSize = 2 * 1024 * 1024;
	int32 NewSize = 0;
	ListenerSocketPtr->SetReceiveBufferSize(ReceiveBufferSize, NewSize);

	TcpListenerPtr = new FTcpListener(*ListenerSocketPtr);
	TcpListenerPtr->OnConnectionAccepted().BindUObject(this, &UVdServerComponent::OnConnectionAccepted);
}

void UVdServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	UE_LOG(LogTemp, Warning, TEXT("UVdServerComponent::EndPlay"));

	if (TcpListenerPtr) {
		TcpListenerPtr->GetSocket()->Close();
		delete TcpListenerPtr;
	}
}


void UVdServerComponent::BeginDestroy() {
	Super::BeginDestroy();
}

bool UVdServerComponent::OnConnectionAccepted(FSocket* SocketPtr, const FIPv4Endpoint& Endpoint) {
	UE_LOG(LogTemp, Warning, TEXT("UVdServerComponent::OnConnectionAccepted"));

	// test
	TVoxelIndex TestIndex(0, 0, 0);
	SendVdByIndex(SocketPtr, TestIndex);
	// test

	GetTerrainController()->RunThread([=](FAsyncThread& ThisThread) {
		FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);

		while (true) {
			if (ThisThread.IsNotValid()) return;

			if (SocketPtr->GetConnectionState() != ESocketConnectionState::SCS_Connected) {
				UE_LOG(LogTemp, Warning, TEXT("Server -> connection finished"));
				return;
			}

			if (SocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1))) {
				FArrayReader Data;
				FNFSMessageHeader::ReceivePayload(Data, SimpleAbstractSocket);
			}
		}
	});


	return true;
}


bool UVdServerComponent::SendVdByIndex(FSocket* SocketPtr, TVoxelIndex& VoxelIndex) {
	static uint32 OpCode = USBT_NET_OPCODE_RESPONSE_VD;

	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;
	SendBuffer << VoxelIndex.X;
	SendBuffer << VoxelIndex.Y;
	SendBuffer << VoxelIndex.Z;

	GetTerrainController()->NetworkSerializeVd(SendBuffer, VoxelIndex);
	return FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);
}