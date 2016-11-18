// Copyright blackw 2015-2020

#pragma once

#include "EngineMinimal.h"
#include "ProceduralMeshComponent.h"

#include "Components/MeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"

#include "SandboxTerrainMeshComponent.generated.h"


/**
*
*/
UCLASS()
class UNREALSANDBOXTERRAIN_API USandboxTerrainMeshComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	void ClearMeshSection(int32 SectionIndex);

	void ClearAllMeshSections();

	void SetMeshSectionVisible(int32 sectionIndex, bool bNewVisibility);

	bool IsMeshSectionVisible(int32 SectionIndex) const;

	int32 GetNumSections() const;

	void SetProcMeshSection(int32 SectionIndex, const FProcMeshSection& Section);

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


private:
	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.


	void UpdateLocalBounds();

	/** Array of sections of mesh */
	UPROPERTY()
	TArray<FProcMeshSection> ProcMeshSections;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;


	friend class FProceduralMeshSceneProxy;
};