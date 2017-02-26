// Copyright blackw 2015-2020

#include "UnrealSandboxTerrainPrivatePCH.h"
#include "SandboxTerrainMeshComponent.h"

#include "SandboxVoxeldata.h"

#include "Engine.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/PhysicsSettings.h"


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

/** Vertex Factory */
class FProcMeshVertexFactory : public FLocalVertexFactory
{
public:

	FProcMeshVertexFactory()
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FProcMeshVertexBuffer* VertexBuffer)
	{
		check(IsInRenderingThread());

		// Initialize the vertex factory's stream components.
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
			);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);
		SetData(NewData);
	}

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FProcMeshVertexBuffer* VertexBuffer)
	{
		if (IsInRenderingThread())
		{
			Init_RenderThread(VertexBuffer);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitProcMeshVertexFactory,
				FProcMeshVertexFactory*, VertexFactory, this,
				const FProcMeshVertexBuffer*, VertexBuffer, VertexBuffer,
				{
					VertexFactory->Init_RenderThread(VertexBuffer);
				});
		}
	}
};


/** Class representing a single section of the proc mesh */
class FProcMeshProxySection
{
public:
	/** Material applied to this section */
	UMaterialInterface* Material;
	/** Vertex buffer for this section */
	FProcMeshVertexBuffer VertexBuffer;
	/** Index buffer for this section */
	FProcMeshIndexBuffer IndexBuffer;
	/** Vertex factory for this section */
	FProcMeshVertexFactory VertexFactory;
	/** Whether this section is currently visible */
	bool bSectionVisible;

	FProcMeshProxySection()
		: Material(NULL)
		, bSectionVisible(true)
	{}

	~FProcMeshProxySection() {
		this->VertexBuffer.ReleaseResource();
		this->IndexBuffer.ReleaseResource();
		this->VertexFactory.ReleaseResource();
	}
};

class FMeshProxyLodSection
{
public:

	FProcMeshProxySection mainMesh;

	FProcMeshProxySection* transitionMesh[6];

	~FMeshProxyLodSection() {
		for (auto i = 0; i < 6; i++) {
			if (transitionMesh[i] != nullptr) {
				delete transitionMesh[i];
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
	Vert.Position = ProcVert.Position;
	Vert.Color = ProcVert.Color;
	Vert.TextureCoordinate = ProcVert.UV0;
	Vert.TangentX = ProcVert.Tangent.TangentX;
	Vert.TangentZ = ProcVert.Normal;
	Vert.TangentZ.Vector.W = ProcVert.Tangent.bFlipTangentY ? 0 : 255;
}

class FProceduralMeshSceneProxy : public FPrimitiveSceneProxy {

private:
	/** Array of lod sections */
	TArray<FMeshProxyLodSection*> LodSectionArray;

	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;

	bool bLodFlag;

public:

	FProceduralMeshSceneProxy(USandboxTerrainMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, BodySetup(Component->GetBodySetup())
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		bLodFlag = Component->bLodFlag;

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

	FORCEINLINE void CopyAll(USandboxTerrainMeshComponent* Component) {
		const int32 NumSections = Component->MeshSectionLodArray.Num();
		LodSectionArray.AddZeroed(NumSections);

		for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
			FProcMeshSection& SrcSection = Component->MeshSectionLodArray[SectionIdx].mainMesh;

			if (SrcSection.ProcIndexBuffer.Num() > 0 && SrcSection.ProcVertexBuffer.Num() > 0) {
				FMeshProxyLodSection* NewLodSection = new FMeshProxyLodSection();

				CopySection(SrcSection, &NewLodSection->mainMesh, Component);

				if (SectionIdx > 0) {
					FProcMeshSection& SrcTransitionSection = Component->MeshSectionLodArray[SectionIdx].transitionMeshArray[0];
					for (auto i = 0; i < 6; i++) {
						FProcMeshSection& SrcTransitionSection = Component->MeshSectionLodArray[SectionIdx].transitionMeshArray[i];

						if (SrcTransitionSection.ProcIndexBuffer.Num() > 0 && SrcTransitionSection.ProcVertexBuffer.Num() > 0) {
							NewLodSection->transitionMesh[i] = new FProcMeshProxySection();
							CopySection(SrcTransitionSection, NewLodSection->transitionMesh[i], Component);
						}
					}
				}

				// Save ref to new section
				LodSectionArray[SectionIdx] = NewLodSection;
			}
		}
	}

	FORCEINLINE void CopySection(FProcMeshSection& SrcSection, FProcMeshProxySection* NewSection, USandboxTerrainMeshComponent* Component) {
		if (SrcSection.ProcIndexBuffer.Num() > 0 && SrcSection.ProcVertexBuffer.Num() > 0) {

			// Copy data from vertex buffer
			const int32 NumVerts = SrcSection.ProcVertexBuffer.Num();

			// Allocate verts
			NewSection->VertexBuffer.Vertices.SetNumUninitialized(NumVerts);
			// Copy verts
			for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++) {
				const FProcMeshVertex& ProcVert = SrcSection.ProcVertexBuffer[VertIdx];
				FDynamicMeshVertex& Vert = NewSection->VertexBuffer.Vertices[VertIdx];
				ConvertProcMeshToDynMeshVertex(Vert, ProcVert);
			}

			// Copy index buffer
			NewSection->IndexBuffer.Indices = SrcSection.ProcIndexBuffer;

			// Init vertex factory
			NewSection->VertexFactory.Init(&NewSection->VertexBuffer);

			// Enqueue initialization of render resource
			BeginInitResource(&NewSection->VertexBuffer);
			BeginInitResource(&NewSection->IndexBuffer);
			BeginInitResource(&NewSection->VertexFactory);

			// Grab material
			NewSection->Material = Component->GetMaterial(0);
			if (NewSection->Material == NULL) {
				NewSection->Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			// Copy visibility info
			NewSection->bSectionVisible = true;
		}
	}

	/** Called on render thread to assign new dynamic data */
	/*
	void UpdateSection_RenderThread(FProcMeshSectionUpdateData* SectionData)
	{
		check(IsInRenderingThread());

		// Check we have data
		if (SectionData != nullptr)
		{
			// Check it references a valid section
			if (SectionData->TargetSection < Sections.Num() &&
				Sections[SectionData->TargetSection] != nullptr)
			{
				FProcMeshProxySection* Section = Sections[SectionData->TargetSection];

				// Lock vertex buffer
				const int32 NumVerts = SectionData->NewVertexBuffer.Num();
				FDynamicMeshVertex* VertexBufferData = (FDynamicMeshVertex*)RHILockVertexBuffer(Section->VertexBuffer.VertexBufferRHI, 0, NumVerts * sizeof(FDynamicMeshVertex), RLM_WriteOnly);

				// Iterate through vertex data, copying in new info
				for (int32 VertIdx = 0; VertIdx<NumVerts; VertIdx++)
				{
					const FProcMeshVertex& ProcVert = SectionData->NewVertexBuffer[VertIdx];
					FDynamicMeshVertex& Vert = VertexBufferData[VertIdx];
					ConvertProcMeshToDynMeshVertex(Vert, ProcVert);
				}

				// Unlock vertex buffer
				RHIUnlockVertexBuffer(Section->VertexBuffer.VertexBufferRHI);
			}

			// Free data sent from game thread
			delete SectionData;
		}
	}
	*/

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility) {
		check(IsInRenderingThread());

		if (SectionIndex < LodSectionArray.Num() && LodSectionArray[SectionIndex] != nullptr) {
		//	Sections[SectionIndex]->bSectionVisible = bNewVisibility;
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override {
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

				const FSceneView* View = Views[ViewIndex];

				const FBoxSphereBounds& ProxyBounds = GetBounds();
				const float ScreenSize = ComputeBoundsScreenSize(ProxyBounds.Origin, ProxyBounds.SphereRadius, *View);

				const int LodIndex = GetLodIndex(View);
				const FProcMeshProxySection* Section = &LodSectionArray[LodIndex]->mainMesh;

				if (Section != nullptr) {
					FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy(IsSelected());
					DrawSection(Section, Collector, MaterialProxy, bWireframe, ViewIndex);
					
					if (LodIndex > 0) {
						// draw transition patches

						for (auto i = 0; i < 6; i++) {
							const FProcMeshProxySection* TransitionSection = LodSectionArray[LodIndex]->transitionMesh[i];

							if (TransitionSection != nullptr) {
								DrawSection(TransitionSection, Collector, MaterialProxy, bWireframe, ViewIndex);
							}
						}
					}

				}
			}
		}
	}

	FORCEINLINE void DrawSection(const FProcMeshProxySection* Section, FMeshElementCollector& Collector, FMaterialRenderProxy* MaterialProxy, bool bWireframe, int32 ViewIndex) const {
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
		BatchElement.MaxVertexIndex = Section->VertexBuffer.Vertices.Num() - 1;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = false;
		Collector.AddMesh(ViewIndex, Mesh);
	}

	int GetLodIndex(const FSceneView* View) const {
		if (bLodFlag) {
			//const FBoxSphereBounds& ProxyBounds = GetBounds();
			//const float ScreenSize = ComputeBoundsScreenSize(ProxyBounds.Origin, ProxyBounds.SphereRadius, *View);

			FVector ViewOrigin = View->ViewMatrices.GetViewOrigin();
			float Distance = FVector::Dist(ViewOrigin, GetBounds().Origin);

			const static float LodThreshold = 1500.0f;

			if (Distance <= LodThreshold) {
				return 0;
			}

			float LodThresholdMin = LodThreshold;
			for (int Idx = 1; Idx < LOD_ARRAY_SIZE; Idx++) {
				float LodThresholdMax = 2.0f * LodThresholdMin;

				if (Distance > LodThresholdMin && Distance <= LodThresholdMax) {
					return Idx;
				}

				LodThresholdMin *= 2;
			}

			return LOD_ARRAY_SIZE - 1;
		}

		return 0;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const {
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
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

USandboxTerrainMeshComponent::USandboxTerrainMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	bLodFlag = false;
}

FPrimitiveSceneProxy* USandboxTerrainMeshComponent::CreateSceneProxy() {
	FProceduralMeshSceneProxy* proxy = new FProceduralMeshSceneProxy(this);
	return proxy;
}

void USandboxTerrainMeshComponent::PostLoad() {
	Super::PostLoad();
}

/*/
void USandboxTerrainMeshComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility) {
	if (SectionIndex < ProcMeshSections.Num()) {
		// Set game thread state
		ProcMeshSections[SectionIndex].bSectionVisible = bNewVisibility;

		if (SceneProxy) {
			// Enqueue command to modify render thread info
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FProcMeshSectionVisibilityUpdate,
				FProceduralMeshSceneProxy*, ProcMeshSceneProxy, (FProceduralMeshSceneProxy*)SceneProxy,
				int32, SectionIndex, SectionIndex,
				bool, bNewVisibility, bNewVisibility,
				{
					ProcMeshSceneProxy->SetSectionVisibility_RenderThread(SectionIndex, bNewVisibility);
				}
			);
		}
	}
}
*/

void USandboxTerrainMeshComponent::UpdateLocalBounds() {
	FBox LocalBox(0);

	LocalBox += MeshSectionLodArray[0].mainMesh.SectionLocalBox;

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds
	UpdateBounds(); // Update global bounds
	MarkRenderTransformDirty(); // Need to send to render thread
}

int32 USandboxTerrainMeshComponent::GetNumMaterials() const {
	return MeshSectionLodArray.Num();
}


void USandboxTerrainMeshComponent::SetMeshData(TMeshDataPtr mdPtr) {
	MeshSectionLodArray.SetNum(LOD_ARRAY_SIZE, false);

	if (mdPtr) {
		TMeshData* meshData = mdPtr.get();

		auto lodIndex = 0;
		for (auto& sectionLOD : meshData->MeshSectionLodArray) {
			MeshSectionLodArray[lodIndex].mainMesh = sectionLOD.mainMesh;
			MeshSectionLodArray[lodIndex].MaterialSectionMap = sectionLOD.MaterialSectionMap;

			if (bLodFlag) {
				for (auto i = 0; i < 6; i++) {
					MeshSectionLodArray[lodIndex].transitionMeshArray[i] = sectionLOD.transitionMeshArray[i];
				}
			}

			lodIndex++;
		}
	}

	UpdateLocalBounds(); // Update overall bounds
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

FBoxSphereBounds USandboxTerrainMeshComponent::CalcBounds(const FTransform& LocalToWorld) const {
	return LocalBounds.TransformBy(LocalToWorld);
}

UBodySetup* USandboxTerrainMeshComponent::GetBodySetup() {

	return NULL;
}

