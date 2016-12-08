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
};

class FMeshProxyLodSection
{
public:
	FProcMeshProxySection mainMesh;
	FProcMeshProxySection transitionMesh[6];
};


/**
*	Struct used to send update to mesh data
*	Arrays may be empty, in which case no update is performed.
*/
class FProcMeshSectionUpdateData
{
public:
	/** Section to update */
	int32 TargetSection;
	/** New vertex information */
	TArray<FProcMeshVertex> NewVertexBuffer;
};

static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FProcMeshVertex& ProcVert)
{
	Vert.Position = ProcVert.Position;
	Vert.Color = ProcVert.Color;
	Vert.TextureCoordinate = ProcVert.UV0;
	Vert.TangentX = ProcVert.Tangent.TangentX;
	Vert.TangentZ = ProcVert.Normal;
	Vert.TangentZ.Vector.W = ProcVert.Tangent.bFlipTangentY ? 0 : 255;
}

class FProceduralMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FProceduralMeshSceneProxy(USandboxTerrainMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, BodySetup(Component->GetBodySetup())
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		bLodFlag = Component->bLodFlag;

		// Copy each section
		const int32 NumSections = Component->ProcMeshSections.Num();
		Sections.AddZeroed(NumSections);

		for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
			FProcMeshSection& SrcSection = Component->ProcMeshSections[SectionIdx].mainMesh;

			if (SrcSection.ProcIndexBuffer.Num() > 0 && SrcSection.ProcVertexBuffer.Num() > 0) {
				FMeshProxyLodSection* NewLodSection = new FMeshProxyLodSection();

				// Copy data from vertex buffer
				const int32 NumVerts = SrcSection.ProcVertexBuffer.Num();

				// Allocate verts
				NewLodSection->mainMesh.VertexBuffer.Vertices.SetNumUninitialized(NumVerts);
				// Copy verts
				for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++) {
					const FProcMeshVertex& ProcVert = SrcSection.ProcVertexBuffer[VertIdx];
					FDynamicMeshVertex& Vert = NewLodSection->mainMesh.VertexBuffer.Vertices[VertIdx];
					ConvertProcMeshToDynMeshVertex(Vert, ProcVert);
				}

				// Copy index buffer
				NewLodSection->mainMesh.IndexBuffer.Indices = SrcSection.ProcIndexBuffer;

				// Init vertex factory
				NewLodSection->mainMesh.VertexFactory.Init(&NewLodSection->mainMesh.VertexBuffer);

				// Enqueue initialization of render resource
				BeginInitResource(&NewLodSection->mainMesh.VertexBuffer);
				BeginInitResource(&NewLodSection->mainMesh.IndexBuffer);
				BeginInitResource(&NewLodSection->mainMesh.VertexFactory);

				// Grab material
				NewLodSection->mainMesh.Material = Component->GetMaterial(0);
				if (NewLodSection->mainMesh.Material == NULL) {
					NewLodSection->mainMesh.Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				// Copy visibility info
				//NewSection->bSectionVisible = SrcSection.bSectionVisible;
				NewLodSection->mainMesh.bSectionVisible = true;

				// Save ref to new section
				Sections[SectionIdx] = NewLodSection;
			}
		}

	}

	virtual ~FProceduralMeshSceneProxy() {
		for (FMeshProxyLodSection* Section : Sections) {
			if (Section != nullptr) {
				Section->mainMesh.VertexBuffer.ReleaseResource();
				Section->mainMesh.IndexBuffer.ReleaseResource();
				Section->mainMesh.VertexFactory.ReleaseResource();
				delete Section;
			}
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

		if (SectionIndex < Sections.Num() && Sections[SectionIndex] != nullptr) {
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
				const FProcMeshProxySection* Section = &Sections[LodIndex]->mainMesh;

				if (Section != nullptr) {
					FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy(IsSelected());

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
			}
		}
	}

	int GetLodIndex(const FSceneView* View) const {
		if (bLodFlag) {
			const FBoxSphereBounds& ProxyBounds = GetBounds();
			const float ScreenSize = ComputeBoundsScreenSize(ProxyBounds.Origin, ProxyBounds.SphereRadius, *View);

			float LodThreshold = 0.2f;

			if (ScreenSize >= LodThreshold) {
				return 0;
			}

			for (int Idx = 0; Idx < LOD_ARRAY_SIZE; Idx++) {
				if (ScreenSize < (LodThreshold / (Idx + 1)) && ScreenSize >= (LodThreshold / (Idx + 2))) {
					return Idx;
				}
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

private:
	/** Array of sections */
	TArray<FMeshProxyLodSection*> Sections;

	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;

	bool bLodFlag;
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

	LocalBox += ProcMeshSections[0].mainMesh.SectionLocalBox;

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds
	UpdateBounds(); // Update global bounds
	MarkRenderTransformDirty(); // Need to send to render thread
}

int32 USandboxTerrainMeshComponent::GetNumMaterials() const {
	return ProcMeshSections.Num();
}


void USandboxTerrainMeshComponent::SetMeshData(MeshDataPtr mdPtr) {
	ProcMeshSections.SetNum(LOD_ARRAY_SIZE, false);

	if (mdPtr) {
		MeshData* meshData = mdPtr.get();

		auto lodIndex = 0;
		for (auto& sectionLOD : meshData->MeshSectionLodArray) {
			ProcMeshSections[lodIndex].mainMesh = sectionLOD.mainMesh;
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

