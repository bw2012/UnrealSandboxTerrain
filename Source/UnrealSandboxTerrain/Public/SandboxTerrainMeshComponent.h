// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SandboxTerrainMeshComponent.generated.h"


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API USandboxTerrainMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;

	friend class FProceduralMeshSceneProxy;
};