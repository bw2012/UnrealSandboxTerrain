// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "VdClientComponent.h"
#include "NetworkMessage.h"


UVdClientComponent::UVdClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	OpcodeHandlerMap.Add(USBT_NET_OPCODE_RESPONSE_VD, [&](FArrayReader& Data) { HandleResponseVd(Data); });

	OpcodeHandlerMap.Add(USBT_NET_OPCODE_DIG_ROUND, [&](FArrayReader& Data) {
		FVector Origin(0);
		float Radius, Strength;

		Data << Origin.X;
		Data << Origin.Y;
		Data << Origin.Z;
		Data << Radius;
		Data << Strength;

		//GetTerrainController()->DigTerrainRoundHole_Internal(Origin, Radius, Strength);
	});
}

void UVdClientComponent::BeginPlay() {
	Super::BeginPlay();

	const int Port = 6000;

	UE_LOG(LogSandboxTerrain, Log, TEXT("UVdClientComponent::BeginPlay()"));

	FString ServerHost = GetWorld()->URL.Host;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Game Server Host -> %s"), *ServerHost);

	ISocketSubsystem* const SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (SocketSubSystem) {
		auto ResolveInfo = SocketSubSystem->GetHostByName(TCHAR_TO_ANSI(*ServerHost));
		while (!ResolveInfo->IsComplete());

		if (ResolveInfo->GetErrorCode() == 0) {
			const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
			uint32 OutIP = 0;
			Addr->GetIp(OutIP);

			UE_LOG(LogSandboxTerrain, Log, TEXT("Vd server IP -> %d.%d.%d.%d: "), 0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP);

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
					UE_LOG(LogSandboxTerrain, Log, TEXT("Vd server -> Connected"));

					ClientSocketPtr = SocketPtr;

					GetTerrainController()->RunThread([=]() {
						while (true) {
							if (ClientSocketPtr->GetConnectionState() != ESocketConnectionState::SCS_Connected) {
								UE_LOG(LogSandboxTerrain, Log, TEXT("Client -> connection finished"));
								return;
							}

							if (ClientSocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1))) {
								FArrayReader Data;
								FSimpleAbstractSocket_FSocket SimpleAbstractSocket(ClientSocketPtr);
								FNFSMessageHeader::ReceivePayload(Data, SimpleAbstractSocket);

								Super::HandleRcvData(Data);
							}
						}
					});

				} else {
					UE_LOG(LogSandboxTerrain, Log, TEXT("Vd server -> Not connected"));
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

void UVdClientComponent::HandleResponseVd(FArrayReader& Data) {
	TVoxelIndex VoxelIndex(0, 0, 0);

	Data << VoxelIndex.X;
	Data << VoxelIndex.Y;
	Data << VoxelIndex.Z;

	GetTerrainController()->NetworkSpawnClientZone(VoxelIndex, Data);
}

template <typename... Ts>
void UVdClientComponent::SendToServer(uint32 OpCode, Ts... Args) {
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;

	for (auto& Arg : { Args... }) {
		auto Tmp = Arg; // workaround - avoid 'const' 
		SendBuffer << Tmp;
	}

	Super::NetworkSend(ClientSocketPtr, SendBuffer);
}
