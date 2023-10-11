// Copyright blackw 2015-2020


#include "TerrainClientComponent.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IPAddressAsyncResolve.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


UTerrainClientComponent::UTerrainClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainClientComponent::BeginPlay() {
	Super::BeginPlay();
	Init();
}

void UTerrainClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void UTerrainClientComponent::BeginDestroy() {
	Super::BeginDestroy();
}

void UTerrainClientComponent::Init() {
	const int Port = (GetTerrainController()->ServerPort == 0) ? 6000 : GetTerrainController()->ServerPort;
	FString ServerHost = GetWorld()->URL.Host;
	UE_LOG(LogVt, Log, TEXT("Client: Game Server Host -> %s"), *ServerHost);

	auto SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	auto* ResolveInfo = SocketSubSystem->GetHostByName(TCHAR_TO_ANSI(*ServerHost));
	while (!ResolveInfo->IsComplete());

	if (ResolveInfo->GetErrorCode() == 0) {

		const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
		uint32 OutIP = 0;
		Addr->GetIp(OutIP);

		UE_LOG(LogVt, Log, TEXT("Client: server IP -> %d.%d.%d.%d:%d"), 0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP, Port);

		FIPv4Address IP(0xff & (OutIP >> 24), 0xff & (OutIP >> 16), 0xff & (OutIP >> 8), 0xff & OutIP);

		RemoteAddr = SocketSubSystem->CreateInternetAddr();
		RemoteAddr->SetIp(IP.Value);
		RemoteAddr->SetPort(Port);

		int32 BufferSize = 2 * 1024 * 1024;

		UdpSocket = FUdpSocketBuilder(TEXT("vd_client_udp")).AsReusable().WithBroadcast();
		UdpSocket->SetSendBufferSize(BufferSize, BufferSize);
		UdpSocket->SetReceiveBufferSize(BufferSize, BufferSize);

		ClientLoopTask = UE::Tasks::Launch(TEXT("vd_client"), [=, this] { RcvThreadLoop(); });

		if (GetTerrainController()->bAutoConnect) {
			Start();
		}
	}
}

void UTerrainClientComponent::Start() {
	RequestMapInfo();
}

void UTerrainClientComponent::HandleRcvData(FArrayReader& Data) {

	uint32 OpCode;
	Data << OpCode;

	uint32 OpCodeExt;
	Data << OpCodeExt;

	//UE_LOG(LogVt, Log, TEXT("Client: OpCode -> %d, OpCodeExt -> %d"), OpCode, OpCodeExt);

	if (OpCode == Net_Opcode_ResponseVd) {
		HandleResponseVd(Data);
	} else if (OpCode == Net_Opcode_ResponseMapInfo) {
		UE_LOG(LogVt, Log, TEXT("Client: ResponseMapInfo"));

		uint32 MapVStamp = 0;
		uint32 Size = 0;

		Data << MapVStamp;
		Data << Size;

		UE_LOG(LogVt, Warning, TEXT("Client: remote MapVStamp %d"), MapVStamp);

		TMap<TVoxelIndex, TZoneModificationData> ServerMap;
		for (uint32 I = 0; I < Size; I++) {
			TVoxelIndex ElemIndex;
			uint32 VStamp = 0;
			ConvertVoxelIndex(Data, ElemIndex);
			Data << VStamp;
			TZoneModificationData MData;
			MData.VStamp = VStamp;
			ServerMap.Add(ElemIndex, MData);
			UE_LOG(LogVt, Log, TEXT("Client: vstamp %d %d %d - %d"), ElemIndex.X, ElemIndex.Y, ElemIndex.Z, VStamp);
		}

		StoredVStamp = MapVStamp;

		GetTerrainController()->OnReceiveServerMapInfo(ServerMap);
	} else {
		UE_LOG(LogVt, Warning, TEXT("Invalid OpCode = %d"), OpCode);
	}
}

void UTerrainClientComponent::HandleResponseVd(FArrayReader& Data) {
	TVoxelIndex VoxelIndex(0, 0, 0);

	Data << VoxelIndex.X;
	Data << VoxelIndex.Y;
	Data << VoxelIndex.Z;

	UE_LOG(LogVt, Log, TEXT("Client: HandleResponseVd %d %d %d"), VoxelIndex.X, VoxelIndex.Y, VoxelIndex.Z);

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

void UTerrainClientComponent::RequestMapInfoIfStaled() {
	static uint32 OpCode = Net_Opcode_RequestMapInfo;
	static uint32 OpCodeExt = 1;

	FBufferArchive SendBuffer;
	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;
	SendBuffer << StoredVStamp;

	UdpSend(SendBuffer, *RemoteAddr);
}

void UTerrainClientComponent::RcvThreadLoop() {

	while (!GetTerrainController()->bIsWorkFinished) {
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		uint32 Size;
		while (UdpSocket->HasPendingData(Size)) {
			uint32 MaxReadBufferSize = 2 * 1024 * 1024;

			FArrayReader Data;
			Data.SetNumZeroed(MaxReadBufferSize);

			int32 Read = 0;
			if (UdpSocket->RecvFrom(Data.GetData(), Data.Num(), Read, *Sender)) {
				//UE_LOG(LogVt, Log, TEXT("Client: udp rcv %d"), Read);
				Data.SetNum(Read);
				HandleRcvData(Data);
			}
		}
	}
	
	UE_LOG(LogVt, Log, TEXT("Client: finish rcv loop"));
}
