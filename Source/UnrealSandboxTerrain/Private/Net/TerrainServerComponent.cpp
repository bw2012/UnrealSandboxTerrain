// Copyright blackw 2015-2020

#include "TerrainServerComponent.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"



UTerrainServerComponent::UTerrainServerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UTerrainServerComponent::BeginPlay() {
	Super::BeginPlay();

	const FString VdServerSocketName = TEXT("TerrainServer");
	const int Port = (GetTerrainController()->ServerPort == 0) ? 6000 : GetTerrainController()->ServerPort;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Start at port %d"), Port);

	FIPv4Endpoint Endpoint(FIPv4Address(0, 0, 0, 0), Port);

	const static int32 BufferSize = 2 * 1024 * 1024;

	UdpSocket = FUdpSocketBuilder(*VdServerSocketName).AsNonBlocking().AsReusable().BoundToEndpoint(Endpoint).WithReceiveBufferSize(BufferSize);

	if (UdpSocket) {
		FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
		UDPReceiver = new FUdpSocketReceiver(UdpSocket, ThreadWaitTime, TEXT("UDP RECEIVER"));
		UDPReceiver->OnDataReceived().BindUObject(this, &UTerrainServerComponent::UdpRecv);
		UDPReceiver->Start();
	} else {
		UE_LOG(LogSandboxTerrain, Warning, TEXT("Server: Failed to start udp server"));
	}

}

void UTerrainServerComponent::UdpRecv(const FArrayReaderPtr& ArrayReaderPtr, const FIPv4Endpoint& EndPoint) {
	FArrayReader& Data = *ArrayReaderPtr.Get();
	HandleRcvData(EndPoint, Data);
}

void UTerrainServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Shutdown voxel data server..."));

	if (UDPReceiver) {
		delete UDPReceiver;
		UDPReceiver = nullptr;
	}

	if (UdpSocket) {
		UdpSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(UdpSocket);
	}
}

void UTerrainServerComponent::BeginDestroy() {
	Super::BeginDestroy();
}

FString GetAddr(FSocket* Socket) {
	TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	Socket->GetPeerAddress(*RemoteAddress);
	return RemoteAddress->ToString(true);
}

bool UTerrainServerComponent::SendVdByIndex(const FIPv4Endpoint& EndPoint, const TVoxelIndex& ZoneIndex) {
	TVoxelIndex Index = ZoneIndex;
	static uint32 OpCode = Net_Opcode_ResponseVd;
	static uint32 OpCodeExt = Net_Opcode_None;
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;
	SendBuffer << Index.X;
	SendBuffer << Index.Y;
	SendBuffer << Index.Z;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: SendVdByIndex %d %d %d "), ZoneIndex.X, ZoneIndex.Y, ZoneIndex.Z);

	GetTerrainController()->NetworkSerializeZone(SendBuffer, Index);
	//return FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);

	UdpSend(SendBuffer, EndPoint);

	return true;
}

bool UTerrainServerComponent::SendMapInfo(const FIPv4Endpoint& EndPoint, TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area) {
	static uint32 OpCode = Net_Opcode_ResponseMapInfo;
	static uint32 OpCodeExt = Net_Opcode_None;
	FBufferArchive SendBuffer;
	uint32 Size = Area.Num();

	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;

	SendBuffer << Size;

	for (int32 I = 0; I != Area.Num(); ++I) {
		const auto& Element = Area[I];
		TVoxelIndex ElemIndex = std::get<0>(Element);
		TZoneModificationData ElemData = std::get<1>(Element);
		ConvertVoxelIndex(SendBuffer, ElemIndex);
		SendBuffer << ElemData.ChangeCounter;

		//UE_LOG(LogSandboxTerrain, Log, TEXT("Server: change counter %d %d %d - %d"), ElemIndex.X, ElemIndex.Y, ElemIndex.Z, ElemData.ChangeCounter);
	}

	UdpSend(SendBuffer, EndPoint);

	return true;
}

void UTerrainServerComponent::HandleRcvData(const FIPv4Endpoint& EndPoint, FArrayReader& Data) {
	
	FString Str = EndPoint.ToString();

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: %s"), *Str);

	uint32 OpCode;
	Data << OpCode;

	uint32 OpCodeExt;
	Data << OpCodeExt;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: OpCode -> %d"), OpCode);
	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: OpCodeExt -> %d"), OpCodeExt);

	if (OpCode == Net_Opcode_RequestVd) {
		TVoxelIndex Index = DeserializeVoxelIndex(Data);
		//UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Client %s requests vd at %d %d %d"), *RemoteAddressString, Index.X, Index.Y, Index.Z);
		SendVdByIndex(EndPoint, Index);
	}

	if (OpCode == Net_Opcode_RequestMapInfo) {
		//UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Client %s requests map info"), *RemoteAddressString);
		TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area = GetTerrainController()->NetworkServerMapInfo();
		SendMapInfo(EndPoint, Area);
	}
}