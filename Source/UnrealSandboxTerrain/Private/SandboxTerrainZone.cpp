#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainZone.h"

ASandboxTerrainZone::ASandboxTerrainZone(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("------------------ ASandboxTerrainZone init ------------------"));
}

ASandboxTerrainZone::ASandboxTerrainZone() {

}

void ASandboxTerrainZone::BeginPlay() {
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("ASandboxTerrainZone ---> BeginPlay"));
}

void ASandboxTerrainZone::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}

void ASandboxTerrainZone::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}