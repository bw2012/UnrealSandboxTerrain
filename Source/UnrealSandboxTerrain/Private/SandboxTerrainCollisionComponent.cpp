// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainCollisionComponent.h"


#include "PhysicsEngine/BodySetup.h"

#include "Engine.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"



USandboxTerrainCollisionComponent::USandboxTerrainCollisionComponent(const FObjectInitializer& ObjectInitializer)	: Super(ObjectInitializer) {
	bUseComplexAsSimpleCollision = true;
}

bool USandboxTerrainCollisionComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {

	int32 VertexBase = 0; // Base vertex index for current section

	// See if we should copy UVs
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	if (bCopyUVs) {
		CollisionData->UVs.AddZeroed(1); // only one UV channel
	}

	FProcMeshSection* Section = GetProcMeshSection();
		// Do we have collision enabled?
		if (Section->bEnableCollision) {
			// Copy vert data
			for (int32 VertIdx = 0; VertIdx < Section->ProcVertexBuffer.Num(); VertIdx++) {
				CollisionData->Vertices.Add(Section->ProcVertexBuffer[VertIdx].Position);

				// Copy UV if desired
				if (bCopyUVs) {
					CollisionData->UVs[0].Add(Section->ProcVertexBuffer[VertIdx].UV0);
				}
			}

			// Copy triangle data
			const int32 NumTriangles = Section->ProcIndexBuffer.Num() / 3;
			for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)	{
				// Need to add base offset for indices
				FTriIndices Triangle;
				Triangle.v0 = Section->ProcIndexBuffer[(TriIdx * 3) + 0] + VertexBase;
				Triangle.v1 = Section->ProcIndexBuffer[(TriIdx * 3) + 1] + VertexBase;
				Triangle.v2 = Section->ProcIndexBuffer[(TriIdx * 3) + 2] + VertexBase;
				CollisionData->Indices.Add(Triangle);

				// Also store material info
				CollisionData->MaterialIndices.Add(0);
			}

			// Remember the base index that new verts will be added from in next section
			VertexBase = CollisionData->Vertices.Num();
		}
	
	CollisionData->bFlipNormals = true;
	return true;
}

FPrimitiveSceneProxy* USandboxTerrainCollisionComponent::CreateSceneProxy() {
	return NULL;
}

void USandboxTerrainCollisionComponent::PostLoad() {
	Super::PostLoad();

	if (ProcMeshBodySetup && IsTemplate())	{
		ProcMeshBodySetup->SetFlags(RF_Public);
	}
}

void USandboxTerrainCollisionComponent::ClearMeshSection(int32 SectionIndex) {
	if (SectionIndex < ProcMeshSections.Num())	{
		ProcMeshSections[SectionIndex].Reset();
		UpdateLocalBounds();
		UpdateCollision();
		MarkRenderStateDirty();
	}
}

void USandboxTerrainCollisionComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
{
	if (ConvexVerts.Num() >= 4)
	{
		// New element
		FKConvexElem NewConvexElem;
		// Copy in vertex info
		NewConvexElem.VertexData = ConvexVerts;
		// Update bounding box
		NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);
		// Add to array of convex elements
		CollisionConvexElems.Add(NewConvexElem);
		// Refresh collision
		UpdateCollision();
	}
}

void USandboxTerrainCollisionComponent::ClearCollisionConvexMeshes()
{
	// Empty simple collision info
	CollisionConvexElems.Empty();
	// Refresh collision
	UpdateCollision();
}

void USandboxTerrainCollisionComponent::SetCollisionConvexMeshes(const TArray< TArray<FVector> >& ConvexMeshes)
{
	CollisionConvexElems.Reset();

	// Create element for each convex mesh
	for (int32 ConvexIndex = 0; ConvexIndex < ConvexMeshes.Num(); ConvexIndex++)
	{
		FKConvexElem NewConvexElem;
		NewConvexElem.VertexData = ConvexMeshes[ConvexIndex];
		NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);

		CollisionConvexElems.Add(NewConvexElem);
	}

	UpdateCollision();
}


void USandboxTerrainCollisionComponent::UpdateLocalBounds()
{
	FBox LocalBox(0);

	for (const FProcMeshSection& Section : ProcMeshSections)
	{
		LocalBox += Section.SectionLocalBox;
	}

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds

																														   // Update global bounds
	UpdateBounds();
	// Need to send to render thread
	MarkRenderTransformDirty();
}

int32 USandboxTerrainCollisionComponent::GetNumMaterials() const
{
	return 0;
}


FProcMeshSection* USandboxTerrainCollisionComponent::GetProcMeshSection()
{
	/*
	if (SectionIndex < ProcMeshSections.Num())
	{
		return &ProcMeshSections[SectionIndex];
	}
	else
	{
		return nullptr;
	}
	*/

	return NULL;
}


void USandboxTerrainCollisionComponent::SetProcMeshSection(const FProcMeshSection& Section)
{
	/*
	// Ensure sections array is long enough
	if (SectionIndex >= ProcMeshSections.Num())
	{
		ProcMeshSections.SetNum(SectionIndex + 1, false);
	}

	ProcMeshSections[SectionIndex] = Section;
	*/

	UpdateLocalBounds(); // Update overall bounds
	UpdateCollision(); // Mark collision as dirty
	//MarkRenderStateDirty(); // New section requires recreating scene proxy
}

FBoxSphereBounds USandboxTerrainCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds.TransformBy(LocalToWorld);
}

bool USandboxTerrainCollisionComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	for (const FProcMeshSection& Section : ProcMeshSections)
	{
		if (Section.ProcIndexBuffer.Num() >= 3 && Section.bEnableCollision)
		{
			return true;
		}
	}

	return false;
}

void USandboxTerrainCollisionComponent::CreateProcMeshBodySetup()
{
	if (ProcMeshBodySetup == NULL)
	{
		// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
		ProcMeshBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
		ProcMeshBodySetup->BodySetupGuid = FGuid::NewGuid();

		ProcMeshBodySetup->bGenerateMirroredCollision = false;
		ProcMeshBodySetup->bDoubleSidedGeometry = true;
		ProcMeshBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	}
}

void USandboxTerrainCollisionComponent::UpdateCollision()
{

	bool bCreatePhysState = false; // Should we create physics state at the end of this function?

								   // If its created, shut it down now
	if (bPhysicsStateCreated)
	{
		DestroyPhysicsState();
		bCreatePhysState = true;
	}

	// Ensure we have a BodySetup
	CreateProcMeshBodySetup();

	// Fill in simple collision convex elements
	ProcMeshBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

	// Set trace flag
	ProcMeshBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

	// New GUID as collision has changed
	ProcMeshBodySetup->BodySetupGuid = FGuid::NewGuid();

#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
	// Clear current mesh data
	ProcMeshBodySetup->InvalidatePhysicsData();
	// Create new mesh data
	ProcMeshBodySetup->CreatePhysicsMeshes();
#endif // WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR

	// Create new instance state if desired
	if (bCreatePhysState)
	{
		CreatePhysicsState();
	}
}

UBodySetup* USandboxTerrainCollisionComponent::GetBodySetup()
{
	CreateProcMeshBodySetup();
	return ProcMeshBodySetup;
}