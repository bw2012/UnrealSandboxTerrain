// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "VoxelMeshComponent.h"

#include "SandboxVoxeldata.h"
#include "SandboxTerrainController.h"
#include "SandboxVoxeldata.h"

#include "Engine.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"


/** Resource array to pass  */
class FProcMeshVertexResourceArray : public FResourceArrayInterface
{
public:
	FProcMeshVertexResourceArray(void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	void* Data;
	uint32 Size;
};

/** Vertex Buffer */
class FProcMeshVertexBuffer : public FVertexBuffer
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override
	{
		const uint32 SizeInBytes = Vertices.Num() * sizeof(FDynamicMeshVertex);

		FProcMeshVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);
		FRHIResourceCreateInfo CreateInfo(&ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
	}

};

/** Index Buffer */
class FProcMeshIndexBuffer : public FIndexBuffer
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.		
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

struct FTerrainMeshBatchInfo {

	FVector* ZoneOriginPtr = nullptr;

	int ZoneLodIndex = 0;
};


const float LOD[LOD_ARRAY_SIZE] = { 0, 1500, 3000, 6000, 12000, 24000, 48000 };
//const float LOD[LOD_ARRAY_SIZE] = { 0, 1500, 3000, 6000, 9000, 12000, 15000 };

int CalculateLodIndex(const FVector& ZoneOrigin, const FVector& ViewOrigin) {
	float Distance = FVector::Dist(ViewOrigin, ZoneOrigin);

	if (Distance <= LOD[1]) {
		return 0;
	}

	for (int Idx = 1; Idx < LOD_ARRAY_SIZE - 1; Idx++) {
		if (Distance > LOD[Idx] && Distance <= LOD[Idx + 1]) {
			return Idx;
		}
	}

	return LOD_ARRAY_SIZE - 1;
}

/** Class representing a single section of the proc mesh */
class FProcMeshProxySection
{
public:
	/** Material applied to this section */
	UMaterialInterface* Material;
	/** Vertex buffer for this section */
	FStaticMeshVertexBuffers VertexBuffers;
	/** Index buffer for this section */
	FProcMeshIndexBuffer IndexBuffer;
	/** Vertex factory for this section */
	FLocalVertexFactory VertexFactory;
	/** Whether this section is currently visible */
	bool bSectionVisible;

	FProcMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel)
		: Material(NULL)
		, VertexFactory(InFeatureLevel, "FProcMeshProxySection")
		, bSectionVisible(true)
	{}

	~FProcMeshProxySection() {
		this->VertexBuffers.PositionVertexBuffer.ReleaseResource();
		this->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		this->VertexBuffers.ColorVertexBuffer.ReleaseResource();
		this->IndexBuffer.ReleaseResource();
		this->VertexFactory.ReleaseResource();
	}
};

typedef TArray<FProcMeshProxySection*> TMeshPtrArray;

class FMeshProxyLodSection {
public:

	/** Array of material sections */
	TMeshPtrArray MaterialMeshPtrArray;

	/** */
	TMeshPtrArray NormalPatchPtrArray[6];

	/** Array of transition sections (todo: should be changed to mat sections)*/
	FProcMeshProxySection* transitionMesh[6];

	// render info
	FTerrainMeshBatchInfo TerrainMeshBatchInfo;


	FMeshProxyLodSection() {
		for (auto i = 0; i < 6; i++) {
			transitionMesh[i] = nullptr;
			NormalPatchPtrArray[i].Empty();
		}
	}

	~FMeshProxyLodSection() {

		for (FProcMeshProxySection* MatSectionPtr : MaterialMeshPtrArray) {
			delete MatSectionPtr;
		}

		for (auto i = 0; i < 6; i++) {
			if (transitionMesh[i] != nullptr) {
				delete transitionMesh[i];
			}

			for (FProcMeshProxySection* Section : NormalPatchPtrArray[i]) {
				if (Section != nullptr) {
					delete Section;
				}
			}
		}
	}
};


/**
*	Struct used to send update to mesh data
*	Arrays may be empty, in which case no update is performed.
*/
class FProcMeshSectionUpdateData {
public:
	/** Section to update */
	int32 TargetSection;
	/** New vertex information */
	TArray<FProcMeshVertex> NewVertexBuffer;
};

static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FProcMeshVertex& ProcVert) {
	Vert.Position.X = ProcVert.PositionX;
	Vert.Position.Y = ProcVert.PositionY;
	Vert.Position.Z = ProcVert.PositionZ;

	switch (ProcVert.MatIdx) {
		case 0:  Vert.Color = FColor(255, 0, 0, 0); break;
		case 1:  Vert.Color = FColor(0, 255, 0, 0); break;
		case 2:  Vert.Color = FColor(0, 0, 255, 0); break;
		default: Vert.Color = FColor(0, 0, 0, 0); break;
	}

	// ignore texture crd
	Vert.TextureCoordinate[0] = FVector2D(0.f, 0.f);

	// ignore tangent
	Vert.TangentX = FVector(1.f, 0.f, 0.f);
	FVector Normal(ProcVert.NormalX, ProcVert.NormalY, ProcVert.NormalZ);
	Vert.TangentZ = Normal;
	Vert.TangentZ.Vector.W = 0;
}

class FProceduralMeshSceneProxy final : public FPrimitiveSceneProxy {

private:
	/** Array of lod sections */
	TArray<FMeshProxyLodSection*> LodSectionArray;

	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;

	FVector ZoneOrigin;

	bool bLodFlag;

	const FVector V[6] = {
		FVector(-USBT_ZONE_SIZE, 0, 0), // -X
		FVector(USBT_ZONE_SIZE, 0, 0),	// +X

		FVector(0, -USBT_ZONE_SIZE, 0), // -Y
		FVector(0, USBT_ZONE_SIZE, 0),	// +Y

		FVector(0, 0, -USBT_ZONE_SIZE), // -Z
		FVector(0, 0, USBT_ZONE_SIZE),	// +Z
	};

public:

	SIZE_T GetTypeHash() const override {
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FProceduralMeshSceneProxy(UVoxelMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, BodySetup(Component->GetBodySetup())
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		bLodFlag = Component->bLodFlag;
		ZoneOrigin = Component->GetComponentLocation();

		// Copy each section
		CopyAll(Component);
	}

	virtual ~FProceduralMeshSceneProxy() {
		for (FMeshProxyLodSection* Section : LodSectionArray) {
			if (Section != nullptr) {
				delete Section;
			}
		}
	}

	template<class T>
	FORCEINLINE void CopyMaterialMesh(UVoxelMeshComponent* Component, TMap<unsigned short, T>& MaterialMap, TMeshPtrArray& TargetMeshPtrArray, std::function<UMaterialInterface*(T)> GetMaterial) {
		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

		for (auto& Element : MaterialMap) {
			unsigned short MatId = Element.Key;

			T& Section = Element.Value;

			TMeshMaterialSection& SrcMaterialSection = static_cast<TMeshMaterialSection&>(Section);
			FProcMeshSection& SourceMaterialSection = SrcMaterialSection.MaterialMesh;

			UMaterialInterface* Material = GetMaterial(Section);
			if (Material == nullptr) { Material = DefaultMaterial; }

			FProcMeshProxySection* NewMaterialProxySection = new FProcMeshProxySection(GetScene().GetFeatureLevel());
			NewMaterialProxySection->Material = Material;

			CopySection(SourceMaterialSection, NewMaterialProxySection, Component);
			TargetMeshPtrArray.Add(NewMaterialProxySection);
		}
	}

	void CopyAll(UVoxelMeshComponent* Component) {
		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		ASandboxTerrainController* TerrainController = Cast<ASandboxTerrainController>(Component->GetAttachmentRootActor());
		
		// if not terrain
		if (TerrainController == nullptr) {
			// grab default material
			DefaultMaterial = Component->GetMaterial(0);
			if (DefaultMaterial == NULL) {
				DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}

		const int32 NumSections = Component->MeshSectionLodArray.Num();

		if (NumSections == 0) return;

		LodSectionArray.AddZeroed(NumSections);

		for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
			FMeshProxyLodSection* NewLodSection = new FMeshProxyLodSection();

			// copy regular material mesh
			TMaterialSectionMap& MaterialMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialSectionMap;
			CopyMaterialMesh<TMeshMaterialSection>(Component, MaterialMap, NewLodSection->MaterialMeshPtrArray,
				[&TerrainController, &DefaultMaterial](TMeshMaterialSection Ms) {return (TerrainController) ? TerrainController->GetRegularTerrainMaterial(Ms.MaterialId) : DefaultMaterial; });

			// copy transition material mesh
			TMaterialTransitionSectionMap& MaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialTransitionSectionMap;
			CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, MaterialTransitionMap, NewLodSection->MaterialMeshPtrArray,
				[&TerrainController, &DefaultMaterial](TMeshMaterialTransitionSection Ms) {return (TerrainController) ? TerrainController->GetTransitionTerrainMaterial(Ms.TransitionName, Ms.MaterialIdSet) : DefaultMaterial; });

			for (auto i = 0; i < 6; i++) {
				// copy regular material mesh
				TMaterialSectionMap& MaterialMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[i].MaterialSectionMap;
				CopyMaterialMesh<TMeshMaterialSection>(Component, MaterialMap, NewLodSection->NormalPatchPtrArray[i],
					[&TerrainController, &DefaultMaterial](TMeshMaterialSection Ms) {return (TerrainController) ? TerrainController->GetRegularTerrainMaterial(Ms.MaterialId) : DefaultMaterial; });

				// copy transition material mesh
				TMaterialTransitionSectionMap& MaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[i].MaterialTransitionSectionMap;
				CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, MaterialTransitionMap, NewLodSection->NormalPatchPtrArray[i],
					[&TerrainController, &DefaultMaterial](TMeshMaterialTransitionSection Ms) {return (TerrainController) ? TerrainController->GetTransitionTerrainMaterial(Ms.TransitionName, Ms.MaterialIdSet) : DefaultMaterial; });
			}

			// Save ref to new section
			LodSectionArray[SectionIdx] = NewLodSection;
		}
	}

	FORCEINLINE void CopySection(FProcMeshSection& SrcSection, FProcMeshProxySection* NewSection, UVoxelMeshComponent* Component) {
		if (SrcSection.ProcIndexBuffer.Num() > 0 && SrcSection.ProcVertexBuffer.Num() > 0) {

			// Copy data from vertex buffer
			const int32 NumVerts = SrcSection.ProcVertexBuffer.Num();

			// Allocate verts
			TArray<FDynamicMeshVertex> Vertices;
			Vertices.SetNumUninitialized(NumVerts);
			// Copy verts
			for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++) {
				const FProcMeshVertex& ProcVert = SrcSection.ProcVertexBuffer[VertIdx];
				FDynamicMeshVertex& Vert = Vertices[VertIdx];
				ConvertProcMeshToDynMeshVertex(Vert, ProcVert);
			}

			// Copy index buffer
			NewSection->IndexBuffer.Indices = SrcSection.ProcIndexBuffer;

			// Init vertex factory
			//NewSection->VertexFactory.Init(&NewSection->VertexBuffer);
			NewSection->VertexBuffers.InitFromDynamicVertex(&NewSection->VertexFactory, Vertices);

			// Enqueue initialization of render resource
			BeginInitResource(&NewSection->VertexBuffers.PositionVertexBuffer);
			BeginInitResource(&NewSection->VertexBuffers.StaticMeshVertexBuffer);
			BeginInitResource(&NewSection->VertexBuffers.ColorVertexBuffer);
			BeginInitResource(&NewSection->IndexBuffer);
			BeginInitResource(&NewSection->VertexFactory);

			// Grab material
			if (NewSection->Material == nullptr) {
				NewSection->Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			// Copy visibility info
			NewSection->bSectionVisible = true;
		}
	}

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility) {
		check(IsInRenderingThread());

		if (SectionIndex < LodSectionArray.Num() && LodSectionArray[SectionIndex] != nullptr) {
			//	Sections[SectionIndex]->bSectionVisible = bNewVisibility;
		}
	}


	void DrawStaticMeshSection(FStaticPrimitiveDrawInterface* PDI, FProcMeshProxySection* Section, FMaterialRenderProxy* Material, FTerrainMeshBatchInfo* TerrainMeshBatchInfo) {
		FMeshBatch MeshBatch;
		MeshBatch.Elements.Empty(1);

		MeshBatch.bWireframe = false;
		MeshBatch.VertexFactory = &Section->VertexFactory;
		MeshBatch.MaterialRenderProxy = Material;
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		MeshBatch.bCanApplyViewModeOverrides = false;
		MeshBatch.bDitheredLODTransition = false;
		MeshBatch.bRequiresPerElementVisibility = true;

		FMeshBatchElement* BatchElement = new(MeshBatch.Elements) FMeshBatchElement;
		BatchElement->IndexBuffer = &Section->IndexBuffer;
		BatchElement->PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
		BatchElement->FirstIndex = 0;
		BatchElement->NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
		BatchElement->MinVertexIndex = 0;
		BatchElement->MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
		BatchElement->UserData = TerrainMeshBatchInfo;

		PDI->DrawMesh(MeshBatch, FLT_MAX);
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {
		for (int LodSectionIdx = 0; LodSectionIdx < LodSectionArray.Num(); LodSectionIdx++) {
			FMeshProxyLodSection* LodSectionProxy = LodSectionArray[LodSectionIdx];
			if (LodSectionProxy != nullptr) {
				LodSectionProxy->TerrainMeshBatchInfo.ZoneLodIndex = LodSectionIdx;
				LodSectionProxy->TerrainMeshBatchInfo.ZoneOriginPtr = &ZoneOrigin;
				for (FProcMeshProxySection* MatSection : LodSectionProxy->MaterialMeshPtrArray) {
					if (MatSection != nullptr) {
						FMaterialRenderProxy* MaterialInstance = MatSection->Material->GetRenderProxy(IsSelected());
						DrawStaticMeshSection(PDI, MatSection, MaterialInstance, &LodSectionProxy->TerrainMeshBatchInfo);
					}
				}
			}
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override {
		if (LodSectionArray.Num() == 0) return;

		// Set up wireframe material (if needed)
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
		if (bWireframe) {
			WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
				FLinearColor(0, 0.5f, 1.f)
			);

			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		}

		// For each view..
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++) {
			if (VisibilityMap & (1 << ViewIndex)) {
				// calculate lod index
				const FSceneView* View = Views[ViewIndex];
				const FBoxSphereBounds& ProxyBounds = GetBounds();
				//const float ScreenSize = ComputeBoundsScreenSize(ProxyBounds.Origin, ProxyBounds.SphereRadius, *View);
				const int LodIndex = GetLodIndex(ZoneOrigin, View->ViewMatrices.GetViewOrigin());

				// draw section according lod index
				FMeshProxyLodSection* LodSectionProxy = LodSectionArray[LodIndex];
				if (LodSectionProxy != nullptr) {
					// draw each material section
					for (FProcMeshProxySection* MatSection : LodSectionProxy->MaterialMeshPtrArray) {
						if (MatSection != nullptr &&  MatSection->Material != nullptr) {
							FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : MatSection->Material->GetRenderProxy(IsSelected());
							DrawDynamicMeshSection(MatSection, Collector, MaterialProxy, bWireframe, ViewIndex);
						}
					}

					if (LodIndex > 0) {
						// draw transition patches
						for (auto i = 0; i < 6; i++) {
							const FVector  NeighborZoneOrigin = ZoneOrigin + V[i];
							const int NeighborLodIndex = GetLodIndex(NeighborZoneOrigin, View->ViewMatrices.GetViewOrigin());

							if (NeighborLodIndex != LodIndex) {
								for (FProcMeshProxySection* MatSection : LodSectionProxy->NormalPatchPtrArray[i]) {
									if (MatSection != nullptr &&  MatSection->Material != nullptr) {
										FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : MatSection->Material->GetRenderProxy(IsSelected());
										DrawDynamicMeshSection(MatSection, Collector, MaterialProxy, bWireframe, ViewIndex);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	FORCEINLINE void DrawDynamicMeshSection(const FProcMeshProxySection* Section, FMeshElementCollector& Collector, FMaterialRenderProxy* MaterialProxy, bool bWireframe, int32 ViewIndex) const {
		if (Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() == 0) return;

		// Draw the mesh.
		FMeshBatch& Mesh = Collector.AllocateMesh();
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &Section->IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = &Section->VertexFactory;
		Mesh.MaterialRenderProxy = MaterialProxy;
		BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = false;
		Collector.AddMesh(ViewIndex, Mesh);
	}

	int GetLodIndex(const FVector& ZoneOrigin, const FVector& ViewOrigin) const {
		if (bLodFlag) {
			return CalculateLodIndex(ZoneOrigin, ViewOrigin);
		}

		return 0;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const {
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bStaticRelevance = false;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override {
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const {
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const {
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}
};




// ================================================================================================================================================

UVoxelMeshComponent::UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bLodFlag = false;
	bUseComplexAsSimpleCollision = true;
	//test = NewObject<UZoneMeshCollisionData>(this, FName(TEXT("test")));
}

FPrimitiveSceneProxy* UVoxelMeshComponent::CreateSceneProxy() {
	return new FProceduralMeshSceneProxy(this);
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
	OutMaterials.Append(LocalMaterials);
}

void UVoxelMeshComponent::SetMeshData(TMeshDataPtr mdPtr) {
	ASandboxTerrainController* TerrainController = Cast<ASandboxTerrainController>(GetAttachmentRootActor());
	//if (TerrainController == nullptr) return;

	LocalMaterials.Empty();
	//LocalMaterials.Reserve(10);

	MeshSectionLodArray.SetNum(LOD_ARRAY_SIZE, false);

	if (mdPtr) {
		TMeshData* meshData = mdPtr.get();

		auto lodIndex = 0;
		for (auto& sectionLOD : meshData->MeshSectionLodArray) {
			MeshSectionLodArray[lodIndex].WholeMesh = sectionLOD.WholeMesh;

			MeshSectionLodArray[lodIndex].RegularMeshContainer.MaterialSectionMap = sectionLOD.RegularMeshContainer.MaterialSectionMap;
			MeshSectionLodArray[lodIndex].RegularMeshContainer.MaterialTransitionSectionMap = sectionLOD.RegularMeshContainer.MaterialTransitionSectionMap;

			if (TerrainController != nullptr) {
				for (auto& Element : sectionLOD.RegularMeshContainer.MaterialSectionMap) {
					LocalMaterials.Add(TerrainController->GetRegularTerrainMaterial(Element.Key));
				}

				for (auto& Element : sectionLOD.RegularMeshContainer.MaterialTransitionSectionMap) {
					LocalMaterials.Add(TerrainController->GetTransitionTerrainMaterial(Element.Value.TransitionName, Element.Value.MaterialIdSet));
				}
			}

			if (bLodFlag) {
				for (auto i = 0; i < 6; i++) {
					MeshSectionLodArray[lodIndex].TransitionPatchArray[i].MaterialSectionMap = sectionLOD.TransitionPatchArray[i].MaterialSectionMap;
					MeshSectionLodArray[lodIndex].TransitionPatchArray[i].MaterialTransitionSectionMap = sectionLOD.TransitionPatchArray[i].MaterialTransitionSectionMap;
				}
			}

			lodIndex++;
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


bool UZoneMeshCollisionData::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {
	return ((UVoxelMeshComponent*)GetOuter())->GetPhysicsTriMeshData(CollisionData, InUseAllTriData);
}

bool UVoxelMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) {
	int32 VertexBase = 0; 
						  
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	if (bCopyUVs) {
		CollisionData->UVs.AddZeroed(1); // only one UV channel
	}

	if (TriMeshData.ProcVertexBuffer.Num() == 0) return false;

	// Copy vert data
	for (int32 VertIdx = 0; VertIdx < TriMeshData.ProcVertexBuffer.Num(); VertIdx++) {
		FProcMeshVertex Vertex = TriMeshData.ProcVertexBuffer[VertIdx];
		FVector Position(Vertex.PositionX, Vertex.PositionY, Vertex.PositionZ);
		CollisionData->Vertices.Add(Position);

		// Copy UV if desired
		if (bCopyUVs) {
			//CollisionData->UVs[0].Add(TriMeshData.ProcVertexBuffer[VertIdx].UV0);
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

void UVoxelMeshComponent::UpdateLocalBounds() {
	FBox LocalBox(EForceInit::ForceInitToZero);

	if (TriMeshData.ProcVertexBuffer.Num() == 0) return;

	LocalBox += TriMeshData.SectionLocalBox;
	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds
	
	UpdateBounds();
	// Need to send to render thread
	MarkRenderTransformDirty();
}

bool UZoneMeshCollisionData::ContainsPhysicsTriMeshData(bool InUseAllTriData) const {
	return ((UVoxelMeshComponent*)GetOuter())->ContainsPhysicsTriMeshData(InUseAllTriData);
}

bool UVoxelMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const {
	if (TriMeshData.ProcVertexBuffer.Num() == 0) {
		return false;
	}

	return true;
}

void UVoxelMeshComponent::CreateProcMeshBodySetup() {
	if (ProcMeshBodySetup == NULL) {
		// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
		ProcMeshBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
		ProcMeshBodySetup->BodySetupGuid = FGuid::NewGuid();

		ProcMeshBodySetup->bGenerateMirroredCollision = false;
		ProcMeshBodySetup->bDoubleSidedGeometry = true;
		ProcMeshBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
	}
}

void UVoxelMeshComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
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

void UVoxelMeshComponent::UpdateCollision() {
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

#if WITH_EDITOR
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

UBodySetup* UVoxelMeshComponent::GetBodySetup() {
	CreateProcMeshBodySetup();
	return ProcMeshBodySetup;
}

void UVoxelMeshComponent::SetCollisionMeshData(TMeshDataPtr MeshDataPtr) {
	TriMeshData.Reset();
	TriMeshData.ProcIndexBuffer = MeshDataPtr->CollisionMeshPtr->ProcIndexBuffer;
	TriMeshData.ProcVertexBuffer = MeshDataPtr->CollisionMeshPtr->ProcVertexBuffer;
	TriMeshData.SectionLocalBox = MeshDataPtr->CollisionMeshPtr->SectionLocalBox;

	UpdateLocalBounds();
	UpdateCollision();
}