// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "Components/MeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Mesh.h"
#include "VoxelMeshData.h"
#include "VoxelMeshComponent.generated.h"

typedef std::shared_ptr<TMeshData> TMeshDataPtr;

/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API UVoxelMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

public:

	// zone position inside terrain
	TVoxelIndex ZoneIndex;

	void SetMeshData(TMeshDataPtr MeshDataPtr);

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

	bool bLodFlag;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	// ======================================================================
	// collision 
	// ======================================================================

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	//~ End Interface_CollisionDataProvider Interface

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
	bool bUseComplexAsSimpleCollision;

	UPROPERTY(Instanced)
	class UBodySetup* ProcMeshBodySetup;

	void SetCollisionMeshData(TMeshDataPtr MeshDataPtr);

	void AddCollisionConvexMesh(TArray<FVector> ConvexVerts);

	TMaterialId GetMaterialIdFromCollisionFaceIndex(int32 FaceIndex) const;

private:

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	void UpdateLocalBounds();

	/** Array of sections of mesh */
	TArray<TMeshLodSection> MeshSectionLodArray;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;

	TArray<UMaterialInterface*> LocalMaterials;

	friend class FVoxelMeshSceneProxy;

	UPROPERTY(transient)
	TArray<UBodySetup*> AsyncBodySetupQueue;

	UBodySetup* CreateBodySetupHelper();

	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup);


	// ======================================================================
	// collision 
	// ======================================================================

	void AddCollisionSection(struct FTriMeshCollisionData* CollisionData, const FProcMeshSection& MeshSection, const int32 MatId, const int32 VertexBase);

	//FProcMeshSection TriMeshData;
	TMeshLodSection CollisionLodSection;

	void CreateProcMeshBodySetup();

	void UpdateCollision();

	/** Convex shapes used for simple collision */
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;

};
