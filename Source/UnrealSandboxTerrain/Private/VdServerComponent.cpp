// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "VdServerComponent.h"


UVdServerComponent::UVdServerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

bool FormatIP4ToNumber(const FString& IP, uint8(&Out)[4]) {
	TArray<FString> Parts;
	IP.Replace(TEXT(" "), TEXT(""));
	IP.ParseIntoArray(Parts, TEXT("."), true);

	if (Parts.Num() != 4)
		return false;

	for (int32 i = 0; i < 4; ++i) {
		Out[i] = FCString::Atoi(*Parts[i]);
	}

	return true;
}

void UVdServerComponent::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("UVdServerComponent::BeginPlay()"));

	const FString VdServerSocketName = TEXT("test");
	const int Port = 6000;
	const FString IP = TEXT("127.0.0.1");

	uint8 IP4Nums[4];
	if (!FormatIP4ToNumber(IP, IP4Nums)) {
		UE_LOG(LogTemp, Warning, TEXT("error"));
		return;
	}

	FIPv4Endpoint Endpoint(FIPv4Address(IP4Nums[0], IP4Nums[1], IP4Nums[2], IP4Nums[3]), Port);
	FSocket* ListenerSocketPtr = FTcpSocketBuilder(*VdServerSocketName).AsReusable().BoundToEndpoint(Endpoint).Listening(8);

	const int32 ReceiveBufferSize = 2 * 1024 * 1024;
	int32 NewSize = 0;
	ListenerSocketPtr->SetReceiveBufferSize(ReceiveBufferSize, NewSize);

	//FTcpListener TcpListener(ListenerSocket);


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

	GetTerrainController()->RunThread([=](FAsyncThread& ThisThread) {
		while (true) {
			if (ThisThread.IsNotValid()) return;

			ESocketConnectionState test = SocketPtr->GetConnectionState();
			if (test != ESocketConnectionState::SCS_Connected) {
				UE_LOG(LogTemp, Warning, TEXT("connection finished"));
				return;
			}

			SocketPtr->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1));

			TArray<uint8> ReceivedData;
			uint32 Size;
			while (SocketPtr->HasPendingData(Size)) {
				ReceivedData.Init(0, FMath::Min(Size, 65507u));

				int32 Read = 0;
				SocketPtr->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);
			}

			if (ReceivedData.Num() > 0) {
				UE_LOG(LogTemp, Warning, TEXT("rcv data %d "), ReceivedData.Num());
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("FINISHED"));
	});


	return true;
}