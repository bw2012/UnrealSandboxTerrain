// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainController.h"
#include "VdClientComponent.h"


UVdClientComponent::UVdClientComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {

}

void UVdClientComponent::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("UVdClientComponent::BeginPlay()"));

	const int Port = 6000;
	FIPv4Address IP(127, 0, 0, 1);

	TSharedRef<FInternetAddr> ServerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	ServerAddr->SetIp(IP.Value);
	ServerAddr->SetPort(Port);

	FIPv4Endpoint Endpoint(IP, Port);

	FSocket* Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);
	bool isConnected = Socket->Connect(*ServerAddr);


	do {
		bool isConnected = Socket->Connect(*ServerAddr);

		if (isConnected) {
			UE_LOG(LogTemp, Warning, TEXT("not connected"));
		} else {
			UE_LOG(LogTemp, Warning, TEXT("connected"));
		}

	} while (!isConnected); 

}

void UVdClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);

}


void UVdClientComponent::BeginDestroy() {
	Super::BeginDestroy();
}