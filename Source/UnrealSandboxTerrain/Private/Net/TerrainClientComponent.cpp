// Copyright blackw 2015-2020


#include "TerrainClientComponent.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


UTerrainClientComponent::UTerrainClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	/*
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
	*/
}

void UTerrainClientComponent::BeginPlay() {
	Super::BeginPlay();

	const int Port = 6000;
	FString ServerHost = GetWorld()->URL.Host;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Game Server Host -> %s"), *ServerHost);

	ISocketSubsystem* const SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubSystem) {
		auto ResolveInfo = SocketSubSystem->GetHostByName(TCHAR_TO_ANSI(*ServerHost));
		while (!ResolveInfo->IsComplete());

		if (ResolveInfo->GetErrorCode() == 0) {
			const int Port1 = 6000;

			const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
			uint32 OutIP = 0;
			Addr->GetIp(OutIP);

			UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Try to connect to server IP -> %d.%d.%d.%d:%d"), 0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP, Port1);

			FIPv4Address IP(0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP);

			TSharedRef<FInternetAddr> ServerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			ServerAddr->SetIp(IP.Value);
			ServerAddr->SetPort(Port1);

			FSocket* SocketPtr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

			bool isConnected = false;
			do {
				isConnected = SocketPtr->Connect(*ServerAddr);

				if (isConnected) {
					UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Connected to voxel data server"));
					ClientSocketPtr = SocketPtr;

					// TODO use native threads 
					GetTerrainController()->AddAsyncTask([=]() {
						GetTerrainController()->OnClientConnected();

						while (true) {
							if (ClientSocketPtr->GetConnectionState() != ESocketConnectionState::SCS_Connected) {
								UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Connection finished"));
								return;
							}

							if (ClientSocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1))) {
								FArrayReader Data;
								FSimpleAbstractSocket_FSocket SimpleAbstractSocket(ClientSocketPtr);
								FNFSMessageHeader::ReceivePayload(Data, SimpleAbstractSocket);
								HandleRcvData(Data);
							}

							if (GetTerrainController()->IsWorkFinished()) {
								break;
							}
						}
					});

				} else {
					UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Not connected"));
				}
			} while (!isConnected);
		}
	}
}

void UTerrainClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void UTerrainClientComponent::BeginDestroy() {
	Super::BeginDestroy();
}

void UTerrainClientComponent::HandleRcvData(FArrayReader& Data) {
	uint32 OpCode;
	Data << OpCode;

	uint32 OpCodeExt;
	Data << OpCodeExt;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: OpCode -> %d"), OpCode);
	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: OpCodeExt -> %d"), OpCodeExt);

	if (OpCode == Net_Opcode_ResponseVd) {
		HandleResponseVd(Data);
	}

	if (OpCode == Net_Opcode_EditVd) {

		//float Radius, Strength;
		double X, Y, Z;

		Data << X;
		Data << Y;
		Data << Z;

		UE_LOG(LogSandboxTerrain, Log, TEXT("Client: %f %f %f"), X, Y, Z);

		FVector Origin(X, Y, Z);

		//Data << Radius;
		//Data << Strength;

		AsyncTask(ENamedThreads::GameThread, [=] {
			GetTerrainController()->DigTerrainRoundHole(Origin, 80);
		});
	}

	if (OpCode == Net_Opcode_ResponseMapInfo) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("Client: ResponseMapInfo"));

		int32 Size = 0;
		Data << Size;

		TMap<TVoxelIndex, TZoneModificationData> ServerMap;
		for (int32 I = 0; I < Size; I++) {
			TVoxelIndex ElemIndex;
			uint32 ChangeCounter = 0;
			ConvertVoxelIndex(Data, ElemIndex);
			Data << ChangeCounter;
			TZoneModificationData MData;
			MData.ChangeCounter = ChangeCounter;
			ServerMap.Add(ElemIndex, MData);
			UE_LOG(LogSandboxTerrain, Log, TEXT("Client: change counter %d %d %d - %d"), ElemIndex.X, ElemIndex.Y, ElemIndex.Z, ChangeCounter);
		}

		GetTerrainController()->OnReceiveServerMapInfo(ServerMap);
	}
}

void UTerrainClientComponent::HandleResponseVd(FArrayReader& Data) {
	TVoxelIndex VoxelIndex(0, 0, 0);

	Data << VoxelIndex.X;
	Data << VoxelIndex.Y;
	Data << VoxelIndex.Z;

	GetTerrainController()->NetworkSpawnClientZone(VoxelIndex, Data);
}

template <typename... Ts>
void UTerrainClientComponent::SendToServer(uint32 OpCode, Ts... Args) {
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;

	for (auto& Arg : { Args... }) {
		auto Tmp = Arg; // workaround - avoid 'const' 
		SendBuffer << Tmp;
	}

	Super::NetworkSend(ClientSocketPtr, SendBuffer);
}


void UTerrainClientComponent::RequestVoxelData(const TVoxelIndex& ZoneIndex) {
	TVoxelIndex Index = ZoneIndex;
	static uint32 OpCode = Net_Opcode_RequestVd;
	static uint32 OpCodeExt = 0;

	FBufferArchive SendBuffer;
	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;
	SendBuffer << Index.X;
	SendBuffer << Index.Y;
	SendBuffer << Index.Z;

	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(ClientSocketPtr);
	FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);
}

void UTerrainClientComponent::RequestMapInfo() {
	static uint32 OpCode = Net_Opcode_RequestMapInfo;
	static uint32 OpCodeExt = 0;

	FBufferArchive SendBuffer;
	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;

	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(ClientSocketPtr);
	FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);
}