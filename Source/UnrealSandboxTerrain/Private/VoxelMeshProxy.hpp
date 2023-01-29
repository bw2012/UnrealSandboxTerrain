/**
* VoxelMeshProxy.hpp
*
* blackw
* 
* Created:29.01.2023
* 
*/

#include "VoxelMeshComponent.h"
#include "SandboxTerrainController.h"
#include "Engine.h"
#include "DynamicMeshBuilder.h"
#include "SceneManagement.h"
#include "Runtime/Launch/Resources/Version.h"


/** Resource array to pass  */
class FProcMeshVertexResourceArray : public FResourceArrayInterface {
public:
	FProcMeshVertexResourceArray(void* InData, uint32 InSize) : Data(InData), Size(InSize) { }

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
class FProcMeshVertexBuffer : public FVertexBuffer {
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override {
		const uint32 SizeInBytes = Vertices.Num() * sizeof(FDynamicMeshVertex);

		FProcMeshVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);

#if ENGINE_MAJOR_VERSION == 5
		FRHIResourceCreateInfo CreateInfo(TEXT("FProcMeshVertexBuffer"), &ResourceArray);
#else
		FRHIResourceCreateInfo CreateInfo(&ResourceArray);
#endif

		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
	}
};

/** Index Buffer */
class FProcMeshIndexBuffer : public FIndexBuffer {
public:
	TArray<int32> Indices;

	virtual void InitRHI() override {
		FRHIResourceCreateInfo CreateInfo(TEXT("FProcMeshIndexBuffer"));
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.		
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

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

	FProcMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel) : Material(NULL), VertexFactory(InFeatureLevel, "FProcMeshProxySection"), bSectionVisible(true) {}

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
#if ENGINE_MAJOR_VERSION == 5
	Vert.TextureCoordinate[0] = FVector2f(0.f, 0.f);
#else
	Vert.TextureCoordinate[0] = FVector2D(0.f, 0.f);
#endif

	// ignore tangent
	Vert.TangentX = FVector(1.f, 0.f, 0.f);
	FVector Normal(ProcVert.NormalX, ProcVert.NormalY, ProcVert.NormalZ);
	Vert.TangentZ = Normal;
	Vert.TangentZ.Vector.W = 0;
}

// ================================================================================================================================================
// FAbstractMeshSceneProxy
// ================================================================================================================================================

class FAbstractMeshSceneProxy : public FPrimitiveSceneProxy {

protected:

	FMaterialRelevance MaterialRelevance;

public:

	FAbstractMeshSceneProxy(UVoxelMeshComponent* Component) : FPrimitiveSceneProxy(Component) {

	}

	SIZE_T GetTypeHash() const override {
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
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

	FORCEINLINE void CopySection(FProcMeshSection& SrcSection, FProcMeshProxySection* NewSection) {
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

	//================================================================================================
	// Draw static mesh
	//================================================================================================

	void DrawStaticMeshSection(FStaticPrimitiveDrawInterface* PDI, FProcMeshProxySection* Section, int LODIndex) {
		FMaterialRenderProxy* MaterialInstance = Section->Material->GetRenderProxy();

		FMeshBatch Mesh;
		Mesh.bWireframe = false;
		Mesh.VertexFactory = &Section->VertexFactory;
		Mesh.MaterialRenderProxy = MaterialInstance;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = false;
		Mesh.LODIndex = LODIndex;
		Mesh.bDitheredLODTransition = false;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &Section->IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

		const float ScreenSize = LodScreenSizeArray[LODIndex];
		//PDI->DrawMesh(Mesh, MAX_FLT); // no LOD
		PDI->DrawMesh(Mesh, ScreenSize);
	}

	//================================================================================================
	// Draw dynamic mesh
	//================================================================================================

	FORCEINLINE void DrawDynamicMeshSection(const FProcMeshProxySection* Section, FMeshElementCollector& Collector, FMaterialRenderProxy* MaterialProxy, bool bWireframe, int32 ViewIndex) const {
		if (Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() == 0) return;

		// Draw the mesh.
		FMeshBatch& Mesh = Collector.AllocateMesh();
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &Section->IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = &Section->VertexFactory;
		Mesh.MaterialRenderProxy = MaterialProxy;

		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity;
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

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

	//================================================================================================
	// Draw wire box
	//================================================================================================

	void DrawBox(FMeshElementCollector& Collector, int32 ViewIndex, const FLinearColor Color, FVector Pos) const {
		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
		auto Box = FBox{ Pos - FVector(1.f, 1.f, 1.f) * 500.f, Pos + FVector(1.f, 1.f, 1.f) * 500.f };
		DrawWireBox(PDI, Box, Color, SDPG_World, 8.f, 0.f, false);
	}
};

// ================================================================================================================================================
// FVoxelMeshSceneProxy
// ================================================================================================================================================

extern float LodScreenSizeArray[LOD_ARRAY_SIZE];

class FVoxelMeshSceneProxy final : public FAbstractMeshSceneProxy {

private:
	/** Array of lod sections */
	TArray<FMeshProxyLodSection*> LodSectionArray;
	UBodySetup* BodySetup;
	FMaterialRelevance MaterialRelevance;
	FVector ZoneOrigin;
	float CullDistance = 20000.f;

	const FVector V[6] = {
		FVector(-USBT_ZONE_SIZE, 0, 0), // -X
		FVector(USBT_ZONE_SIZE, 0, 0),	// +X

		FVector(0, -USBT_ZONE_SIZE, 0), // -Y
		FVector(0, USBT_ZONE_SIZE, 0),	// +Y

		FVector(0, 0, -USBT_ZONE_SIZE), // -Z
		FVector(0, 0, USBT_ZONE_SIZE),	// +Z
	};

public:

	FVoxelMeshSceneProxy(UVoxelMeshComponent* Component) : FAbstractMeshSceneProxy(Component), BodySetup(Component->GetBodySetup()), MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel())) {
		ZoneOrigin = Component->GetComponentLocation();

		ASandboxTerrainController* Controller = Cast<ASandboxTerrainController>(Component->GetAttachmentRootActor());
		if (Controller) {
			CullDistance = Controller->ActiveAreaSize * 1.5 * USBT_ZONE_SIZE;
		}

		// Copy each section
		CopyAll(Component);
	}

	virtual ~FVoxelMeshSceneProxy() {
		for (FMeshProxyLodSection* Section : LodSectionArray) {
			if (Section != nullptr) {
				delete Section;
			}
		}
	}

	template<class T>
	void CopyMaterialMesh(UVoxelMeshComponent* Component, TMap<unsigned short, T>& MaterialMap, TMeshPtrArray& TargetMeshPtrArray, std::function<UMaterialInterface* (T)> GetMaterial) {
		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

		for (auto& Element : MaterialMap) {
			unsigned short MatId = Element.Key;

			T& Section = Element.Value;

			TMeshMaterialSection& SrcMaterialSection = static_cast<TMeshMaterialSection&>(Section);
			FProcMeshSection& SourceMaterialSection = SrcMaterialSection.MaterialMesh;

			UMaterialInterface* Material = GetMaterial(Section);
			if (Material == nullptr) {
				Material = DefaultMaterial;
			}

			FProcMeshProxySection* NewMaterialProxySection = new FProcMeshProxySection(GetScene().GetFeatureLevel());
			NewMaterialProxySection->Material = Material;

			CopySection(SourceMaterialSection, NewMaterialProxySection);
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

		if (NumSections == 0) {
			return;
		}

		LodSectionArray.AddZeroed(NumSections);

		for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
			FMeshProxyLodSection* NewLodSection = new FMeshProxyLodSection();

			// copy regular material mesh
			TMaterialSectionMap& MaterialMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialSectionMap;
			auto MatProvider = [&TerrainController, &DefaultMaterial](TMeshMaterialSection Ms) { return (TerrainController) ? TerrainController->GetRegularTerrainMaterial(Ms.MaterialId) : DefaultMaterial; };
			CopyMaterialMesh<TMeshMaterialSection>(Component, MaterialMap, NewLodSection->MaterialMeshPtrArray, MatProvider);

			// copy transition material mesh
			TMaterialTransitionSectionMap& MaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialTransitionSectionMap;
			CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, MaterialTransitionMap, NewLodSection->MaterialMeshPtrArray,
				[&TerrainController, &DefaultMaterial](TMeshMaterialTransitionSection Ms) { return (TerrainController) ? TerrainController->GetTransitionMaterial(Ms.MaterialIdSet) : DefaultMaterial; });

			for (auto i = 0; i < 6; i++) {
				// copy regular material mesh
				TMaterialSectionMap& LodMaterialMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[i].MaterialSectionMap;
				CopyMaterialMesh<TMeshMaterialSection>(Component, LodMaterialMap, NewLodSection->NormalPatchPtrArray[i],
					[&TerrainController, &DefaultMaterial](TMeshMaterialSection Ms) { return (TerrainController) ? TerrainController->GetRegularTerrainMaterial(Ms.MaterialId) : DefaultMaterial; });

				// copy transition material mesh
				TMaterialTransitionSectionMap& LodMaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[i].MaterialTransitionSectionMap;
				CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, LodMaterialTransitionMap, NewLodSection->NormalPatchPtrArray[i],
					[&TerrainController, &DefaultMaterial](TMeshMaterialTransitionSection Ms) { return (TerrainController) ? TerrainController->GetTransitionMaterial(Ms.MaterialIdSet) : DefaultMaterial; });
			}

			// Save ref to new section
			LodSectionArray[SectionIdx] = NewLodSection;
		}
	}

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility) {
		check(IsInRenderingThread());

		if (SectionIndex < LodSectionArray.Num() && LodSectionArray[SectionIndex] != nullptr) {
			//	Sections[SectionIndex]->bSectionVisible = bNewVisibility;
		}
	}

	//================================================================================================
	// Draw main zone as static mesh
	//================================================================================================

	void DrawStaticLodSection(FStaticPrimitiveDrawInterface* PDI, const FMeshProxyLodSection* LodSection, int LODIndex) {
		if (LodSection != nullptr) {
			for (FProcMeshProxySection* MatSection : LodSection->MaterialMeshPtrArray) {
				if (MatSection != nullptr) {
					DrawStaticMeshSection(PDI, MatSection, LODIndex);
				}
			}
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {
		if (LodSectionArray.Num() > 0) {
			for (int LODIndex = 0; LODIndex < LodSectionArray.Num(); LODIndex++) {
				const FMeshProxyLodSection* LodSection = LodSectionArray[LODIndex];
				DrawStaticLodSection(PDI, LodSection, LODIndex);
			}
		}
	}

	int32 ComputeLodIndexByScreenSize(const FSceneView* View, const FVector& Pos) const {
		const FBoxSphereBounds& ProxyBounds = GetBounds();
		const float ScreenSize = ComputeBoundsScreenSize(Pos, ProxyBounds.SphereRadius, *View);

		int32 I = 0;
		for (int LODIndex = 0; LODIndex < LOD_ARRAY_SIZE; LODIndex++) { // TODO: fix LODIndex < 4
			if (ScreenSize < LodScreenSizeArray[LODIndex]) {
				I = LODIndex;
			}
		}

		return I;
	}

	//================================================================================================
	// Draw transvoxel patches as dynamic mesh  
	//================================================================================================

	FORCENOINLINE virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override {
		if (LodSectionArray.Num() == 0) {
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++) {
			//DrawBox(Collector, ViewIndex, FLinearColor::Blue);

			if (VisibilityMap & (1 << ViewIndex)) {
				const FSceneView* View = Views[ViewIndex];
				const FBoxSphereBounds& ProxyBounds = GetBounds();
				int32 LodIndex = ComputeLodIndexByScreenSize(View, ProxyBounds.Origin);

				if (LodIndex > 0) {
					// draw transition patches
					for (auto i = 0; i < 6; i++) {
						const FVector  NeighborZoneOrigin = ZoneOrigin + V[i];
						const auto NeighborLodIndex = ComputeLodIndexByScreenSize(View, NeighborZoneOrigin);
						if (NeighborLodIndex < LodIndex) {
							FMeshProxyLodSection* LodSectionProxy = LodSectionArray[LodIndex];
							if (LodSectionProxy != nullptr) {
								for (FProcMeshProxySection* MatSection : LodSectionProxy->NormalPatchPtrArray[i]) {
									if (MatSection != nullptr && MatSection->Material != nullptr) {
										//FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : MatSection->Material->GetRenderProxy();
										//DrawDynamicMeshSection(MatSection, Collector, MaterialProxy, bWireframe, ViewIndex);
										DrawDynamicMeshSection(MatSection, Collector, MatSection->Material->GetRenderProxy(), false, ViewIndex);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const {
		FPrimitiveViewRelevance Result;

		const FVector O(View->ViewMatrices.GetViewOrigin().X, View->ViewMatrices.GetViewOrigin().Y, 0);
		const FVector ZO(ZoneOrigin.X, ZoneOrigin.Y, 0);
		const float Distance = FVector::Distance(O, ZO);

		Result.bDrawRelevance = (Distance <= CullDistance) && IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bStaticRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}
};