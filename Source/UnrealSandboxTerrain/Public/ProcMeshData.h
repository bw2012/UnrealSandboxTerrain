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
	float PositionX;
	float PositionY;
	float PositionZ;

	float NormalX;
	float NormalY;
	float NormalZ;

	int32 MatIdx;
};

/** One section of the procedural mesh. Each material has its own section. */
class FProcMeshSection {

public:

	/** Vertex buffer for this section */
	TArray<FProcMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	TArray<int32> ProcIndexBuffer;

	/** Local bounding box of section */
	FBox SectionLocalBox;

	FProcMeshSection() : SectionLocalBox(EForceInit::ForceInitToZero)	{ }

	/** Reset this section, clear all mesh info. */
	void Reset() {
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
	}

	void AddVertex(FProcMeshVertex& Vertex) {
		ProcVertexBuffer.Add(Vertex);
		FVector Pos(Vertex.PositionX, Vertex.PositionY, Vertex.PositionZ);
		SectionLocalBox += Pos;
	}

	void SerializeMesh(FBufferArchive& BinaryData) const {
		// vertexes
		int32 VertexNum = ProcVertexBuffer.Num();
		BinaryData << VertexNum;
		for (auto& Vertex : ProcVertexBuffer) {

			float PosX = Vertex.PositionX;
			float PosY = Vertex.PositionY;
			float PosZ = Vertex.PositionZ;

			BinaryData << PosX;
			BinaryData << PosY;
			BinaryData << PosZ;

			float NormalX = Vertex.NormalX;
			float NormalY = Vertex.NormalY;
			float NormalZ = Vertex.NormalZ;

			BinaryData << NormalX;
			BinaryData << NormalY;
			BinaryData << NormalZ;

			int32 MatIdx = Vertex.MatIdx;

			BinaryData << MatIdx;
		}

		// indexes
		int32 IndexNum = ProcIndexBuffer.Num();
		BinaryData << IndexNum;
		for (int32 Index : ProcIndexBuffer) {
			BinaryData << Index;
		}
	}

	void DeserializeMesh(FMemoryReader& BinaryData) {
		int32 VertexNum;
		BinaryData << VertexNum;

		for (int Idx = 0; Idx < VertexNum; Idx++) {
			FProcMeshVertex Vertex;

			BinaryData << Vertex.PositionX;
			BinaryData << Vertex.PositionY;
			BinaryData << Vertex.PositionZ;

			BinaryData << Vertex.NormalX;
			BinaryData << Vertex.NormalY;
			BinaryData << Vertex.NormalZ;

			BinaryData << Vertex.MatIdx;

			AddVertex(Vertex);
		}

		int32 IndexNum;
		BinaryData << IndexNum;

		for (int Idx = 0; Idx < IndexNum; Idx++) {
			int32 Index;
			BinaryData << Index;
			ProcIndexBuffer.Add(Index);
		}
	}
};