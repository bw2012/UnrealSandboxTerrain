// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "VdClientComponent.h"
#include "NetworkMessage.h"


UVdClientComponent::UVdClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UVdClientComponent::BeginPlay() {
	Super::BeginPlay();

	const int Port = 6000;

	UE_LOG(LogTemp, Warning, TEXT("UVdClientComponent::BeginPlay()"));

	FString ServerHost = GetWorld()->URL.Host;

	UE_LOG(LogTemp, Warning, TEXT("Game Server Host -> %s"), *ServerHost);

	ISocketSubsystem* const SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (SocketSubSystem) {
		auto ResolveInfo = SocketSubSystem->GetHostByName(TCHAR_TO_ANSI(*ServerHost));
		while (!ResolveInfo->IsComplete());

		if (ResolveInfo->GetErrorCode() == 0) {
			const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
			uint32 OutIP = 0;
			Addr->GetIp(OutIP);

			UE_LOG(LogTemp, Warning, TEXT("Vd server IP -> %d.%d.%d.%d: "), 0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP);

			const int Port = 6000;
			FIPv4Address IP(0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP);

			TSharedRef<FInternetAddr> ServerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			ServerAddr->SetIp(IP.Value);
			ServerAddr->SetPort(Port);

			FSocket* SocketPtr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

			bool isConnected = false;
			do {
				isConnected = SocketPtr->Connect(*ServerAddr);

				if (isConnected) {
					UE_LOG(LogTemp, Warning, TEXT("Vd server -> Connected"));

					ClientSocketPtr = SocketPtr;

					GetTerrainController()->RunThread([=](FAsyncThread& ThisThread) {
						while (true) {
							if (ThisThread.IsNotValid()) return;

							if (ClientSocketPtr->GetConnectionState() != ESocketConnectionState::SCS_Connected) {
								UE_LOG(LogTemp, Warning, TEXT("Client -> connection finished"));
								return;
							}

							if (ClientSocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1))) {
								FArrayReader Data;
								FSimpleAbstractSocket_FSocket SimpleAbstractSocket(ClientSocketPtr);
								FNFSMessageHeader::ReceivePayload(Data, SimpleAbstractSocket);

								HandleServerResponse(Data);
							}
						}
					});

				} else {
					UE_LOG(LogTemp, Warning, TEXT("Vd server -> Not connected"));
				}
			} while (!isConnected);
		}
	}
}

void UVdClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

}


void UVdClientComponent::BeginDestroy() {
	Super::BeginDestroy();
}

void UVdClientComponent::HandleServerResponse(FArrayReader& Data) {
	//FMemoryReader DataReader = FMemoryReader(Data, true);

	uint32 OpCode;
	Data << OpCode;

	//UE_LOG(LogTemp, Warning, TEXT("OpCode -> %d"), OpCode);

	switch (OpCode) {
		case USBT_NET_OPCODE_RESPONSE_VERSION: ;
		case USBT_NET_OPCODE_RESPONSE_VD: HandleResponseVd(Data);
	}
}

void UVdClientComponent::HandleResponseVd(FArrayReader& Data) {
	TVoxelIndex VoxelIndex(0, 0, 0);

	Data << VoxelIndex.X;
	Data << VoxelIndex.Y;
	Data << VoxelIndex.Z;

	GetTerrainController()->NetworkSpawnClientZone(VoxelIndex, Data);
}