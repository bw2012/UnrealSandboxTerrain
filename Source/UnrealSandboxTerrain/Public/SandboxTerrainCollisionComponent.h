// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "ProcMeshData.h"

#include "Components/MeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"

#include <memory>
#include "SandboxVoxeldata.h"
#include "SandboxTerrainCollisionComponent.generated.h"


struct TMeshData;
//class USandboxTerrainCollisionComponent;

UCLASS()
class UZoneMeshCollisionData : public UObject, public IInterface_CollisionDataProvider {
	GENERATED_BODY()

	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
};

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API USandboxTerrainCollisionComponent : public UMeshComponent, public IInterface_CollisionDataProvider 
{
	GENERATED_UCLASS_BODY()

	friend class UZoneMeshCollisionData;

public:

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	//~ End Interface_CollisionDataProvider Interface


	bool bUseComplexAsSimpleCollision;

	UPROPERTY(Instanced)
	class UBodySetup* ProcMeshBodySetup;

	UPROPERTY()
	class UZoneMeshCollisionData* test;

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual class UBodySetup* GetBodySetup() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface.


	void SetMeshData(TMeshDataPtr MeshDataPtr);

private:

	FProcMeshSection TriMeshData;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.


	void UpdateLocalBounds();

	void CreateProcMeshBodySetup();

	void UpdateCollision();

	/** Array of sections of mesh */
	//UPROPERTY()
	//TArray<FProcMeshSection> ProcMeshSections;

	/** Convex shapes used for simple collision */
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
};