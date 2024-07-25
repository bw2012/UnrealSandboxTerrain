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
#include "MaterialDomain.h"


/** Resource array to pass  */
class TMeshVertexResourceArray : public FResourceArrayInterface {
public:
	TMeshVertexResourceArray(void* InData, uint32 InSize) : Data(InData), Size(InSize) { }

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

/** Class representing a single section of the proc mesh */
class FProcMeshProxySection
{
public:
	/** Material applied to this section */
	UMaterialInterface* Material;
	/** Vertex buffer for this section */
	FStaticMeshVertexBuffers VertexBuffers;
	/** Index buffer for this section */
	FDynamicMeshIndexBuffer32 IndexBuffer;
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

	/** Array of transition sections (TODO: should be changed to mat sections)*/
	FProcMeshProxySection* TransitionMesh[6];

	FMeshProxyLodSection() {
		for (auto I = 0; I < 6; I++) {
			TransitionMesh[I] = nullptr;
			NormalPatchPtrArray[I].Empty();
		}
	}

	~FMeshProxyLodSection() {

		for (FProcMeshProxySection* MatSectionPtr : MaterialMeshPtrArray) {
			delete MatSectionPtr;
		}

		for (auto I = 0; I < 6; I++) {
			if (TransitionMesh[I] != nullptr) {
				delete TransitionMesh[I];
			}

			for (FProcMeshProxySection* Section : NormalPatchPtrArray[I]) {
				if (Section != nullptr) {
					delete Section;
				}
			}
		}
	}
};

extern float LodScreenSizeArray[LOD_ARRAY_SIZE];

static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const TMeshVertex& ProcVert) {
	Vert.Position.X = ProcVert.Pos.X;
	Vert.Position.Y = ProcVert.Pos.Y;
	Vert.Position.Z = ProcVert.Pos.Z;

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
	Vert.TangentZ = ProcVert.Normal;
	Vert.TangentZ.Vector.W = 0;
}

// ================================================================================================================================================
// FAbstractMeshSceneProxy
// ================================================================================================================================================

class FAbstractMeshSceneProxy : public FPrimitiveSceneProxy {

protected:

	FMaterialRelevance MaterialRelevance;

public:

	FAbstractMeshSceneProxy(UVoxelMeshComponent* Component) : FPrimitiveSceneProxy(Component), 
		MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel())) { }

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
				const TMeshVertex& ProcVert = SrcSection.ProcVertexBuffer[VertIdx];
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
		
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4 
		DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
#else
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
#endif

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

const FVector NDir[6] = {
	FVector(-USBT_ZONE_SIZE, 0, 0), // -X
	FVector(USBT_ZONE_SIZE, 0, 0),	// +X

	FVector(0, -USBT_ZONE_SIZE, 0), // -Y
	FVector(0, USBT_ZONE_SIZE, 0),	// +Y

	FVector(0, 0, -USBT_ZONE_SIZE), // -Z
	FVector(0, 0, USBT_ZONE_SIZE),	// +Z
};

class FVoxelMeshSceneProxy final : public FAbstractMeshSceneProxy {

private:
	/** Array of lod sections */
	TArray<FMeshProxyLodSection*> LodSectionArray;

	FVector ZoneOrigin;
	float CullDistance = 20000.f;

	ASandboxTerrainController* Controller;

public:

	FVoxelMeshSceneProxy(UVoxelMeshComponent* Component) : FAbstractMeshSceneProxy(Component) {
		ZoneOrigin = Component->GetComponentLocation();
		Controller = Cast<ASandboxTerrainController>(Component->GetAttachmentRootActor());
		if (Controller) {
			CullDistance = Controller->ActiveAreaSize * 1.5 * USBT_ZONE_SIZE;
			CopyAll(Component);
		}
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
		ASandboxTerrainController* TerrainController = Controller;

		const int32 NumSections = Component->MeshSectionLodArray.Num();
		if (NumSections == 0) {
			return;
		}

		LodSectionArray.AddZeroed(NumSections);

		for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
			FMeshProxyLodSection* NewLodSection = new FMeshProxyLodSection();

			auto MatProviderR = [&TerrainController](TMeshMaterialSection Ms) { 
				return TerrainController->GetRegularTerrainMaterial(Ms.MaterialId); 
			};

			auto MatProviderT = [&TerrainController](TMeshMaterialTransitionSection Ms) { 
				return TerrainController->GetTransitionMaterial(Ms.MaterialIdSet); 
			};

			// copy regular material mesh
			TMaterialSectionMap& MaterialMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialSectionMap;
			CopyMaterialMesh<TMeshMaterialSection>(Component, MaterialMap, NewLodSection->MaterialMeshPtrArray, MatProviderR);

			// copy transition material mesh
			TMaterialTransitionSectionMap& MaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].RegularMeshContainer.MaterialTransitionSectionMap;
			CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, MaterialTransitionMap, NewLodSection->MaterialMeshPtrArray, MatProviderT);

			for (auto I = 0; I < 6; I++) {
				// copy regular material mesh
				TMaterialSectionMap& LodMaterialMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[I].MaterialSectionMap;
				CopyMaterialMesh<TMeshMaterialSection>(Component, LodMaterialMap, NewLodSection->NormalPatchPtrArray[I], MatProviderR);

				// copy transition material mesh
				TMaterialTransitionSectionMap& LodMaterialTransitionMap = Component->MeshSectionLodArray[SectionIdx].TransitionPatchArray[I].MaterialTransitionSectionMap;
				CopyMaterialMesh<TMeshMaterialTransitionSection>(Component, LodMaterialTransitionMap, NewLodSection->NormalPatchPtrArray[I], MatProviderT);
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

	bool CheckCullDistance(const FSceneView* View) const {
		const FVector OriginXY(View->ViewMatrices.GetViewOrigin().X, View->ViewMatrices.GetViewOrigin().Y, 0);
		const FVector ZoneOriginXY(ZoneOrigin.X, ZoneOrigin.Y, 0);
		const float Distance = FVector::Distance(OriginXY, ZoneOriginXY);
		return Distance <= CullDistance;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const {
		FPrimitiveViewRelevance Result;

		Result.bDrawRelevance = CheckCullDistance(View) && IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bStaticRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	//================================================================================================
	// Lod index by Screen size
	//================================================================================================

	int32 ComputeLodIndexByScreenSize(const FSceneView* View, const FVector& Pos) const {
		const FBoxSphereBounds& ProxyBounds = GetBounds();
		const float ScreenSize = ComputeBoundsScreenSize(Pos, ProxyBounds.SphereRadius, *View);

		int32 I = 0;
		for (int LODIndex = 0; LODIndex < LOD_ARRAY_SIZE; LODIndex++) { 
			if (ScreenSize < LodScreenSizeArray[LODIndex]) {
				I = LODIndex;
			}
		}

		return I;
	}

	//================================================================================================
	// Draw main zone as static mesh
	//================================================================================================

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {
		if (LodSectionArray.Num() > 0) {
			for (int LODIndex = 0; LODIndex < LodSectionArray.Num(); LODIndex++) {
				const FMeshProxyLodSection* LodSection = LodSectionArray[LODIndex];
				if (LodSection != nullptr) {
					for (FProcMeshProxySection* MatSection : LodSection->MaterialMeshPtrArray) {
						if (MatSection != nullptr) {
							DrawStaticMeshSection(PDI, MatSection, LODIndex);
						}
					}
				}
			}
		}
	}

	//================================================================================================
	// Draw transvoxel patches as dynamic mesh  
	//================================================================================================

	FORCENOINLINE virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override {
		if (LodSectionArray.Num() == 0) {
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++) {
			//DrawBox(Collector, ViewIndex, FLinearColor::Blue, ZoneOrigin);

			if (VisibilityMap & (1 << ViewIndex)) {
				const FSceneView* View = Views[ViewIndex];
				const FBoxSphereBounds& ProxyBounds = GetBounds();
				int32 LodIndex = ComputeLodIndexByScreenSize(View, ProxyBounds.Origin);

				if (LodIndex > 0) {
					// draw transition patches
					for (auto i = 0; i < 6; i++) {
						const FVector  NeighborZoneOrigin = ZoneOrigin + NDir[i];
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

};