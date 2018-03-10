// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainCollisionComponent.h"

#include "PhysicsEngine/BodySetup.h"

#include "Engine.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include "SandboxVoxeldata.h"



USandboxTerrainCollisionComponent::USandboxTerrainCollisionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bUseComplexAsSimpleCollision = true;
	test = NewObject<UZoneMeshCollisionData>(this, FName(TEXT("test")));
}

bool UZoneMeshCollisionData::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {
	return ((USandboxTerrainCollisionComponent*) GetOuter())->GetPhysicsTriMeshData(CollisionData, InUseAllTriData);
}

bool USandboxTerrainCollisionComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {

	int32 VertexBase = 0; // Base vertex index for current section

	// See if we should copy UVs
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	if (bCopyUVs) {
		CollisionData->UVs.AddZeroed(1); // only one UV channel
	}

	if (TriMeshData.ProcVertexBuffer.Num() == 0) return false;

	// Copy vert data
	for (int32 VertIdx = 0; VertIdx < TriMeshData.ProcVertexBuffer.Num(); VertIdx++) {
		CollisionData->Vertices.Add(TriMeshData.ProcVertexBuffer[VertIdx].Position);

		// Copy UV if desired
		if (bCopyUVs) {
			CollisionData->UVs[0].Add(TriMeshData.ProcVertexBuffer[VertIdx].UV0);
		}
	}

	// Copy triangle data
	const int32 NumTriangles = TriMeshData.ProcIndexBuffer.Num() / 3;
	for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++) {
		// Need to add base offset for indices
		FTriIndices Triangle;
		Triangle.v0 = TriMeshData.ProcIndexBuffer[(TriIdx * 3) + 0] + VertexBase;
		Triangle.v1 = TriMeshData.ProcIndexBuffer[(TriIdx * 3) + 1] + VertexBase;
		Triangle.v2 = TriMeshData.ProcIndexBuffer[(TriIdx * 3) + 2] + VertexBase;
		CollisionData->Indices.Add(Triangle);

		// Also store material info
		CollisionData->MaterialIndices.Add(0);
	}

	// Remember the base index that new verts will be added from in next section
	VertexBase = CollisionData->Vertices.Num();

	CollisionData->bFlipNormals = true;
	return true;
}

FPrimitiveSceneProxy* USandboxTerrainCollisionComponent::CreateSceneProxy() {
	return NULL;
}

void USandboxTerrainCollisionComponent::PostLoad() {
	Super::PostLoad();

	if (ProcMeshBodySetup && IsTemplate()) {
		ProcMeshBodySetup->SetFlags(RF_Public);
	}
}

void USandboxTerrainCollisionComponent::UpdateLocalBounds() {
	FBox LocalBox(EForceInit::ForceInitToZero);

	if (TriMeshData.ProcVertexBuffer.Num() == 0) return;

	LocalBox += TriMeshData.SectionLocalBox;

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds
	UpdateBounds();
	// Need to send to render thread
	MarkRenderTransformDirty();
}

int32 USandboxTerrainCollisionComponent::GetNumMaterials() const {
	return 0;
}

FBoxSphereBounds USandboxTerrainCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const {
	return LocalBounds.TransformBy(LocalToWorld);
}

bool UZoneMeshCollisionData::ContainsPhysicsTriMeshData(bool InUseAllTriData) const {
	return ((USandboxTerrainCollisionComponent*)GetOuter())->ContainsPhysicsTriMeshData(InUseAllTriData);
}

bool USandboxTerrainCollisionComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const {
	if (TriMeshData.ProcVertexBuffer.Num() == 0) {
		return false;
	}

	return true;
}

void USandboxTerrainCollisionComponent::CreateProcMeshBodySetup() {
	if (ProcMeshBodySetup == NULL)	{
		// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
		ProcMeshBodySetup = NewObject<UBodySetup>(test, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
		ProcMeshBodySetup->BodySetupGuid = FGuid::NewGuid();

		ProcMeshBodySetup->bGenerateMirroredCollision = false;
		ProcMeshBodySetup->bDoubleSidedGeometry = true;
		ProcMeshBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	}
}

void USandboxTerrainCollisionComponent::UpdateCollision() {
	bool bCreatePhysState = false; // Should we create physics state at the end of this function?

	// If its created, shut it down now
	if (bPhysicsStateCreated) {
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
	if (bCreatePhysState) {
		CreatePhysicsState();
	}
}

UBodySetup* USandboxTerrainCollisionComponent::GetBodySetup() {
	CreateProcMeshBodySetup();
	return ProcMeshBodySetup;
}

void USandboxTerrainCollisionComponent::SetMeshData(TMeshDataPtr MeshDataPtr) {
	TriMeshData.Reset();
	TriMeshData.ProcIndexBuffer = MeshDataPtr->CollisionMeshPtr->ProcIndexBuffer;
	TriMeshData.ProcVertexBuffer = MeshDataPtr->CollisionMeshPtr->ProcVertexBuffer;
	TriMeshData.SectionLocalBox = MeshDataPtr->CollisionMeshPtr->SectionLocalBox;

	UpdateLocalBounds();
	UpdateCollision();
}