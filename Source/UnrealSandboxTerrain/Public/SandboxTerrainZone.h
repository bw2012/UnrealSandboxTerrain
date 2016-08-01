#pragma once

#include "EngineMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SandboxTerrainZone.generated.h"

UCLASS()
class UNREALSANDBOXTERRAIN_API ASandboxTerrainZone : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	ASandboxTerrainZone();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


};
