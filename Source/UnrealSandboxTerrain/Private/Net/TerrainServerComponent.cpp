// Copyright blackw 2015-2020

#include "TerrainServerComponent.h"
#include "SandboxTerrainController.h"
#include "NetworkMessage.h"



UTerrainServerComponent::UTerrainServerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	/*
	OpcodeHandlerMap.Add(USBT_NET_OPCODE_DIG_ROUND, [&](FArrayReader& Data) { 
		FVector Origin(0);
		float Radius, Strength;

		Data << Origin.X;
		Data << Origin.Y;
		Data << Origin.Z;
		Data << Radius;
		Data << Strength;

		GetTerrainController()->DigTerrainRoundHole(Origin, Radius, Strength);
	});
	*/
}

void UTerrainServerComponent::BeginPlay() {
	Super::BeginPlay();

	const FString VdServerSocketName = TEXT("TerrainServer");
	const int Port = (GetTerrainController()->ServerPort == 0) ? 6000 : GetTerrainController()->ServerPort;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Start at port %d"), Port);

	FIPv4Endpoint Endpoint(FIPv4Address(0, 0, 0, 0), Port);
	FSocket* ListenerSocketPtr = FTcpSocketBuilder(*VdServerSocketName).AsReusable().BoundToEndpoint(Endpoint).Listening(8);

	const int32 ReceiveBufferSize = 2 * 1024 * 1024;
	int32 NewSize = 0;
	ListenerSocketPtr->SetReceiveBufferSize(ReceiveBufferSize, NewSize);

	TcpListenerPtr = new FTcpListener(*ListenerSocketPtr);
	TcpListenerPtr->OnConnectionAccepted().BindUObject(this, &UTerrainServerComponent::OnConnectionAccepted);
}

void UTerrainServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server:Shutdown voxel data server..."));

	if (TcpListenerPtr) {
		TcpListenerPtr->GetSocket()->Close();
		delete TcpListenerPtr;
	}
}

void UTerrainServerComponent::BeginDestroy() {
	Super::BeginDestroy();
}

bool UTerrainServerComponent::OnConnectionAccepted(FSocket* SocketPtr, const FIPv4Endpoint& Endpoint) {
	TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	SocketPtr->GetPeerAddress(*RemoteAddress);
	const FString RemoteAddressString = RemoteAddress->ToString(true);
	ConnectedClientsMap.Add(RemoteAddressString, SocketPtr);

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: connection accepted -> %s"), *RemoteAddressString);

	// test
	//TVoxelIndex TestIndex(0, 0, 0);
	//SendVdByIndex(SocketPtr, TestIndex);
	// test

	// TODO use native threads 
	GetTerrainController()->AddAsyncTask([=]() {
		FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
		while (true) {
			if (SocketPtr->GetConnectionState() != ESocketConnectionState::SCS_Connected) {
				UE_LOG(LogSandboxTerrain, Log, TEXT("Server: connection finished"));
				// TODO remove from client list
				return;
			}

			if (SocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1))) {
				FArrayReader Data;
				FNFSMessageHeader::ReceivePayload(Data, SimpleAbstractSocket);
				HandleRcvData(RemoteAddressString, SocketPtr, Data);
			}

			if (GetTerrainController()->IsWorkFinished()) {
				break;
			}
		}
	});


	return true;
}

/*
template <typename... Ts>
void UTerrainServerComponent::SendToAllClients(uint32 OpCode, Ts... Args) {
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;

	for (auto Arg : { Args... }) {
		SendBuffer << Arg;
	}

	for (auto& Elem : ConnectedClientsMap) {
		FSocket* SocketPtr = Elem.Value;
		Super::NetworkSend(SocketPtr, SendBuffer);
	}
}
*/

void UTerrainServerComponent::SendToAllVdEdit(const TEditTerrainParam& EditParams) {
	TEditTerrainParam Params = EditParams; // workaround
	FBufferArchive SendBuffer;

	static uint32 OpCode = Net_Opcode_EditVd;
	static uint32 OpCodeExt = 1;

	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;
	SendBuffer << Params.Origin.X;
	SendBuffer << Params.Origin.Y;;
	SendBuffer << Params.Origin.Z;

	for (auto& Elem : ConnectedClientsMap) {
		FSocket* SocketPtr = Elem.Value;
		Super::NetworkSend(SocketPtr, SendBuffer);
	}
}

bool UTerrainServerComponent::SendVdByIndex(FSocket* SocketPtr, const TVoxelIndex& ZoneIndex) {
	TVoxelIndex Index = ZoneIndex;
	static uint32 OpCode = Net_Opcode_ResponseVd;
	static uint32 OpCodeExt = Net_Opcode_None;

	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
	FBufferArchive SendBuffer;

	SendBuffer << OpCode;
	SendBuffer << OpCodeExt;
	SendBuffer << Index.X;
	SendBuffer << Index.Y;
	SendBuffer << Index.Z;

	GetTerrainController()->NetworkSerializeZone(SendBuffer, Index);
	return FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);
}

bool UTerrainServerComponent::SendMapInfo(FSocket* SocketPtr, TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area) {
	static uint32 OpCode = Net_Opcode_ResponseMapInfo;
	static uint32 OpCodeExt = Net_Opcode_None;

	FSimpleAbstractSocket_FSocket SimpleAbstractSocket(SocketPtr);
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

		UE_LOG(LogSandboxTerrain, Log, TEXT("Server: change counter %d %d %d - %d"), ElemIndex.X, ElemIndex.Y, ElemIndex.Z, ElemData.ChangeCounter);
	}

	return FNFSMessageHeader::WrapAndSendPayload(SendBuffer, SimpleAbstractSocket);
}

void UTerrainServerComponent::HandleRcvData(const FString& ClientRemoteAddr, FSocket* SocketPtr, FArrayReader& Data) {
	uint32 OpCode;
	Data << OpCode;

	uint32 OpCodeExt;
	Data << OpCodeExt;

	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: OpCode -> %d"), OpCode);
	UE_LOG(LogSandboxTerrain, Log, TEXT("Server: OpCodeExt -> %d"), OpCodeExt);

	if (OpCode == Net_Opcode_RequestVd) {
		TVoxelIndex Index = DeserializeVoxelIndex(Data);
		UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Client %s requests vd at %d %d %d"), *ClientRemoteAddr, Index.X, Index.Y, Index.Z);
		SendVdByIndex(SocketPtr, Index);
	}

	if (OpCode == Net_Opcode_RequestVd) {
		TVoxelIndex Index = DeserializeVoxelIndex(Data);
		UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Client %s requests vd at %d %d %d"), *ClientRemoteAddr, Index.X, Index.Y, Index.Z);
		SendVdByIndex(SocketPtr, Index);
	}

	if (OpCode == Net_Opcode_RequestMapInfo) {
		UE_LOG(LogSandboxTerrain, Log, TEXT("Server: Client %s requests map info"), *ClientRemoteAddr);

		// TODO remove hardcode
		TArray<std::tuple<TVoxelIndex, TZoneModificationData>> Area = GetTerrainController()->NetworkServerMapInfo();
		SendMapInfo(SocketPtr, Area);
	}
}