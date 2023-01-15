#pragma once

#include "EngineMinimal.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestActor.generated.h"

UCLASS()
class UNREALSANDBOXTERRAIN_API ATestActor : public AActor {
	GENERATED_UCLASS_BODY()

protected:
	virtual void BeginPlay() override;


};