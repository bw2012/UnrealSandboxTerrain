// Copyright blackw 2015-2020

#include "VoxelMeshComponent.h"
#include "SandboxTerrainController.h"
#include "Engine.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Core/VoxelMeshProxy.hpp"


// ================================================================================================================================================
// UVoxelMeshComponent
// ================================================================================================================================================

UVoxelMeshComponent::UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bLodFlag = USBT_ENABLE_LOD;
	bUseComplexAsSimpleCollision = true;
}

FPrimitiveSceneProxy* UVoxelMeshComponent::CreateSceneProxy() {
	FVoxelMeshSceneProxy* NewProxy = new FVoxelMeshSceneProxy(this);
	MeshSectionLodArray.Empty(); // clear mesh data to reduce memory usage
	return NewProxy;
}

void UVoxelMeshComponent::PostLoad() {
	Super::PostLoad();

	if (ProcMeshBodySetup && IsTemplate()) {
		ProcMeshBodySetup->SetFlags(RF_Public);
	}
}

int32 UVoxelMeshComponent::GetNumMaterials() const {
	return LocalMaterials.Num();
}

void UVoxelMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const {
	UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	OutMaterials.Add(DefaultMaterial);
	OutMaterials.Append(LocalMaterials);
}

void UVoxelMeshComponent::SetMeshData(TMeshDataPtr NewMeshDataPtr) {
	ASandboxTerrainController* TerrainController = Cast<ASandboxTerrainController>(GetAttachmentRootActor());

	LocalMaterials.Empty();
	LocalMaterials.Reserve(10);
	MeshSectionLodArray.SetNum(LOD_ARRAY_SIZE, false);

	if (NewMeshDataPtr) {
		auto LodIndex = 0;
		for (auto& SectionLOD : NewMeshDataPtr->MeshSectionLodArray) {
			const auto* SourceMesh = &SectionLOD;

			MeshSectionLodArray[LodIndex].WholeMesh = SourceMesh->WholeMesh;
			MeshSectionLodArray[LodIndex].RegularMeshContainer.MaterialSectionMap = SourceMesh->RegularMeshContainer.MaterialSectionMap;
			MeshSectionLodArray[LodIndex].RegularMeshContainer.MaterialTransitionSectionMap = SourceMesh->RegularMeshContainer.MaterialTransitionSectionMap;

			if (TerrainController != nullptr) {
				for (auto& Element : SourceMesh->RegularMeshContainer.MaterialSectionMap) {
					UMaterialInterface* Material = TerrainController->GetRegularTerrainMaterial(Element.Key);
					LocalMaterials.Add(Material);
				}

				for (const auto& Element : SourceMesh->RegularMeshContainer.MaterialTransitionSectionMap) {
					LocalMaterials.Add(TerrainController->GetTransitionMaterial(Element.Value.MaterialIdSet));
				}
			}
			if (bLodFlag) {
				for (auto i = 0; i < 6; i++) {
					MeshSectionLodArray[LodIndex].TransitionPatchArray[i].MaterialSectionMap = SourceMesh->TransitionPatchArray[i].MaterialSectionMap;
					MeshSectionLodArray[LodIndex].TransitionPatchArray[i].MaterialTransitionSectionMap = SourceMesh->TransitionPatchArray[i].MaterialTransitionSectionMap;

					if (TerrainController != nullptr) {
						for (auto& Element : SourceMesh->TransitionPatchArray[i].MaterialSectionMap) {
							UMaterialInterface* Material = TerrainController->GetRegularTerrainMaterial(Element.Key);
							LocalMaterials.Add(Material);
						}

						for (const auto& Element : SourceMesh->TransitionPatchArray[i].MaterialTransitionSectionMap) {
							LocalMaterials.Add(TerrainController->GetTransitionMaterial(Element.Value.MaterialIdSet));
						}
					}

				}
			}

			LodIndex++;
		}
	}

	UpdateLocalBounds(); // Update overall bounds
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

FBoxSphereBounds UVoxelMeshComponent::CalcBounds(const FTransform& LocalToWorld) const {
	return LocalBounds.TransformBy(LocalToWorld);
}

// ======================================================================
// collision 
// ======================================================================

void UVoxelMeshComponent::AddCollisionSection(struct FTriMeshCollisionData* CollisionData, const FProcMeshSection& MeshSection, const int32 MatId, const int32 VertexBase) {

	// Copy vert data
	for (int32 VertIdx = 0; VertIdx < MeshSection.ProcVertexBuffer.Num(); VertIdx++) {
		TMeshVertex Vertex = MeshSection.ProcVertexBuffer[VertIdx];

#if ENGINE_MAJOR_VERSION == 5
		CollisionData->Vertices.Add((FVector3f)Vertex.Pos);
#else
		CollisionData->Vertices.Add(Vertex.Pos);
#endif

		// Copy UV if desired
		//if (bCopyUVs) {
		//	CollisionData->UVs[0].Add(TriMeshData.ProcVertexBuffer[VertIdx].UV0);
		//}
	}

	// Copy triangle data
	const int32 NumTriangles = MeshSection.ProcIndexBuffer.Num() / 3;
	for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++) {
		// Need to add base offset for indices
		FTriIndices Triangle;
		Triangle.v0 = MeshSection.ProcIndexBuffer[(TriIdx * 3) + 0] + VertexBase;
		Triangle.v1 = MeshSection.ProcIndexBuffer[(TriIdx * 3) + 1] + VertexBase;
		Triangle.v2 = MeshSection.ProcIndexBuffer[(TriIdx * 3) + 2] + VertexBase;
		CollisionData->Indices.Add(Triangle);

		// Also store material info
		CollisionData->MaterialIndices.Add(MatId);
	}
}

bool UVoxelMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {
	int32 VertexBase = 0;

	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	if (bCopyUVs) {
		CollisionData->UVs.AddZeroed(1); // only one UV channel
	}

	const TMeshLodSection& CollisionSection = CollisionLodSection;
	for (const auto& Elem : CollisionSection.RegularMeshContainer.MaterialSectionMap) {
		int32 MatId = (int32)Elem.Key;
		const TMeshMaterialSection& MaterialSection = Elem.Value;
		const FProcMeshSection& MeshSection = MaterialSection.MaterialMesh;
		AddCollisionSection(CollisionData, MeshSection, MatId, VertexBase);

		// Remember the base index that new verts will be added from in next section
		VertexBase = CollisionData->Vertices.Num();
	}

	for (const auto& Elem : CollisionSection.RegularMeshContainer.MaterialTransitionSectionMap) {
		int32 MatId = (int32)Elem.Key;
		const TMeshMaterialTransitionSection& MaterialSection = Elem.Value;
		const FProcMeshSection& MeshSection = MaterialSection.MaterialMesh;
		AddCollisionSection(CollisionData, MeshSection, MatId, VertexBase);

		// Remember the base index that new verts will be added from in next section
		VertexBase = CollisionData->Vertices.Num();
	}

	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;
	return true;
}

void UVoxelMeshComponent::UpdateLocalBounds() {
	//FBox LocalBox(EForceInit::ForceInitToZero);
	//CollisionLodSection.WholeMesh.SectionLocalBox;
	//LocalBox += CollisionLodSection.WholeMesh.SectionLocalBox;
	//LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(500, 500, 500), 500); // fallback to reset box sphere bounds

	const FVector BoxExt(USBT_ZONE_SIZE / 2.f);

#if ENGINE_MAJOR_VERSION == 5
	const double Radius = BoxExt.Length(); 
#else
	const double Radius = BoxExt.Size();
#endif

	LocalBounds = FBoxSphereBounds(FVector(0), BoxExt, Radius);

	UpdateBounds();
	MarkRenderTransformDirty();
}


bool UVoxelMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const {
	if (CollisionLodSection.RegularMeshContainer.MaterialSectionMap.Num() == 0) {
		return false;
	}

	return true;
}

void UVoxelMeshComponent::CreateProcMeshBodySetup() {
	if (!ProcMeshBodySetup) {
		// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
		ProcMeshBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
		ProcMeshBodySetup->BodySetupGuid = FGuid::NewGuid();
		ProcMeshBodySetup->bGenerateMirroredCollision = false;
		ProcMeshBodySetup->bDoubleSidedGeometry = true;
		ProcMeshBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	}
}

void UVoxelMeshComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts) {
	if (ConvexVerts.Num() >= 4) {
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

UBodySetup* UVoxelMeshComponent::CreateBodySetupHelper() {
	// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
	UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
	NewBodySetup->BodySetupGuid = FGuid::NewGuid();

	NewBodySetup->bGenerateMirroredCollision = false;
	NewBodySetup->bDoubleSidedGeometry = true;
	NewBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

	return NewBodySetup;
}

void UVoxelMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup) {
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx)) {
		if (bSuccess) {
			//The new body was found in the array meaning it's newer so use it
			ProcMeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			//remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx) {
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else {
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}

	UpdateNavigationData();

	ASandboxTerrainController* TerrainController = Cast<ASandboxTerrainController>(GetAttachmentRootActor());
	if (TerrainController) {
		TerrainController->OnFinishAsyncPhysicsCook(ZoneIndex);
	}
}

void UVoxelMeshComponent::UpdateCollision() {
	// Abort all previous ones still standing
	for (UBodySetup* OldBody : AsyncBodySetupQueue) {
		OldBody->AbortPhysicsMeshAsyncCreation();
	}

	AsyncBodySetupQueue.Add(CreateBodySetupHelper());
	UBodySetup* UseBodySetup = AsyncBodySetupQueue.Last();

	// Fill in simple collision convex elements
	UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

	// Set trace flag
	UseBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UVoxelMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
}

UBodySetup* UVoxelMeshComponent::GetBodySetup() {
	CreateProcMeshBodySetup();
	return ProcMeshBodySetup;
}

void UVoxelMeshComponent::SetCollisionMeshData(TMeshDataPtr MeshDataPtr) {
	CollisionLodSection = MeshSectionLodArray[0];
	//UpdateLocalBounds();
	UpdateCollision();
}


TMaterialId UVoxelMeshComponent::GetMaterialIdFromCollisionFaceIndex(int32 FaceIndex) const {
	if (FaceIndex >= 0) {
		int32 TotalFaceCount = 0;
		const TMeshLodSection& CollisionSection = CollisionLodSection;
		for (const auto& Elem : CollisionSection.RegularMeshContainer.MaterialSectionMap) {
			int32 MatId = (int32)Elem.Key;
			const TMeshMaterialSection& MaterialSection = Elem.Value;
			const FProcMeshSection& MeshSection = MaterialSection.MaterialMesh;

			int32 NumFaces = MeshSection.ProcIndexBuffer.Num() / 3;
			TotalFaceCount += NumFaces;

			if (FaceIndex < TotalFaceCount) {
				return MaterialSection.MaterialId;
			}

		}
	}

	return 0;
}
