#pragma once

#include "EngineMinimal.h"

/**
*	Struct used to specify a tangent vector for a vertex
*	The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member.
*/
struct FProcMeshTangent {

	/** Direction of X tangent for this vertex */
	FVector TangentX;

	/** Bool that indicates whether we should flip the Y tangent when we compute it using cross product */
	bool bFlipTangentY;

	FProcMeshTangent()
		: TangentX(1.f, 0.f, 0.f)
		, bFlipTangentY(false)
	{}

	FProcMeshTangent(float X, float Y, float Z)
		: TangentX(X, Y, Z)
		, bFlipTangentY(false)
	{}

	FProcMeshTangent(FVector InTangentX, bool bInFlipTangentY)
		: TangentX(InTangentX)
		, bFlipTangentY(bInFlipTangentY)
	{}
};

/** One vertex for the procedural mesh, used for storing data internally */
struct FProcMeshVertex {
	/** Vertex position */
	FVector Position;

	/** Vertex normal */
	FVector Normal;

	/** Vertex tangent */
	FProcMeshTangent Tangent;

	/** Vertex color */
	FColor Color;

	/** Vertex texture co-ordinate */
	FVector2D UV0;

	FProcMeshVertex()
		: Position(0.f, 0.f, 0.f)
		, Normal(0.f, 0.f, 1.f)
		, Tangent(FVector(1.f, 0.f, 0.f), false)
		, Color(255, 255, 255)
		, UV0(0.f, 0.f)
	{}
};

/** One section of the procedural mesh. Each material has its own section. */
struct FProcMeshSection {
	/** Vertex buffer for this section */
	TArray<FProcMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	TArray<int32> ProcIndexBuffer;

	/** Local bounding box of section */
	FBox SectionLocalBox;

	/** Should we build collision data for triangles in this section */
	bool bEnableCollision;

	/** Should we display this section */
	bool bSectionVisible;

	FProcMeshSection()
		: SectionLocalBox(0)
		, bEnableCollision(false)
		, bSectionVisible(true)
	{}

	/** Reset this section, clear all mesh info. */
	void Reset()
	{
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
		bEnableCollision = false;
		bSectionVisible = true;
	}
};