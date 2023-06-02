// Copyright blackw 2015-2020


#include "TerrainClientComponent.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


UTerrainClientComponent::UTerrainClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainClientComponent::BeginPlay() {
	Super::BeginPlay();

	if (GetTerrainController()->bAutoConnect) {
		Connect();
	}
}

void UTerrainClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void UTerrainClientComponent::BeginDestroy() {
	Super::BeginDestroy();
}

void UTerrainClientComponent::Connect() {
	const int Port = 6000;
	FString ServerHost = GetWorld()->URL.Host;
	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: Game Server Host -> %s"), *ServerHost);

	RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bIsValid;
	RemoteAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	RemoteAddr->SetPort(6000);

	int32 BufferSize = 2 * 1024 * 1024;

	UdpSocket = FUdpSocketBuilder(TEXT("test_udp")).AsReusable().WithBroadcast();
	UdpSocket->SetSendBufferSize(BufferSize, BufferSize);
	UdpSocket->SetReceiveBufferSize(BufferSize, BufferSize);

	ClientLoopTask = UE::Tasks::Launch(TEXT("vd_client"), [=] { RcvThreadLoop(); });

	RequestMapInfo();
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
			//UE_LOG(LogSandboxTerrain, Log, TEXT("Client: change counter %d %d %d - %d"), ElemIndex.X, ElemIndex.Y, ElemIndex.Z, ChangeCounter);
		}

		GetTerrainController()->OnReceiveServerMapInfo(ServerMap);
	}
}

void UTerrainClientComponent::HandleResponseVd(FArrayReader& Data) {
	TVoxelIndex VoxelIndex(0, 0, 0);

	Data << VoxelIndex.X;
	Data << VoxelIndex.Y;
	Data << VoxelIndex.Z;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: HandleResponseVd %d %d %d"), VoxelIndex.X, VoxelIndex.Y, VoxelIndex.Z);

	GetTerrainController()->NetworkSpawnClientZone(VoxelIndex, Data);
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

	UdpSend(SendBuffer, *RemoteAddr);
}

void UTerrainClientComponent::RequestMapInfo() {
	static uint32 OpCode = Net_Opcode_RequestMapInfo;
	static uint32 OpCodeExt = 0;

	FBufferArchive SendBuffer;
	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;

	UdpSend(SendBuffer, *RemoteAddr);
}

void UTerrainClientComponent::RcvThreadLoop() {

	while (!GetTerrainController()->bIsWorkFinished) {
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		uint32 Size;
		while (UdpSocket->HasPendingData(Size)) {

			uint32 MaxReadBufferSize = 2 * 1024 * 1024;
			//FArrayReaderPtr Reader = MakeShared<FArrayReader, ESPMode::ThreadSafe>(true);

			FArrayReader Data;

			Data.SetNumUninitialized(FMath::Min(Size, MaxReadBufferSize));

			int32 Read = 0;

			if (UdpSocket->RecvFrom(Data.GetData(), Data.Num(), Read, *Sender)) {

				UE_LOG(LogSandboxTerrain, Log, TEXT("Client: udp rcv %d"), Read);

				HandleRcvData(Data);

			}
		}
	}
	
	UE_LOG(LogSandboxTerrain, Log, TEXT("Client: finish rcv loop"));
}