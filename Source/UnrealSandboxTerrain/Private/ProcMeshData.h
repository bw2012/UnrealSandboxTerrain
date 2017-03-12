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
class FProcMeshSection {

public:

	/** Vertex buffer for this section */
	TArray<FProcMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	TArray<int32> ProcIndexBuffer;

	/** Local bounding box of section */
	FBox SectionLocalBox;

	FProcMeshSection() : SectionLocalBox(0)	{ }

	/** Reset this section, clear all mesh info. */
	void Reset() {
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
	}

	void AddVertex(FProcMeshVertex& Vertex) {
		ProcVertexBuffer.Add(Vertex);
		SectionLocalBox += Vertex.Position;
	}

	void SerializeMesh(FBufferArchive& BinaryData) const {
		// vertexes
		int32 VertexNum = ProcVertexBuffer.Num();
		BinaryData << VertexNum;
		for (auto& Vertex : ProcVertexBuffer) {

			float PosX = Vertex.Position.X;
			float PosY = Vertex.Position.Y;
			float PosZ = Vertex.Position.Z;

			BinaryData << PosX;
			BinaryData << PosY;
			BinaryData << PosZ;

			float NormalX = Vertex.Normal.X;
			float NormalY = Vertex.Normal.Y;
			float NormalZ = Vertex.Normal.Z;

			BinaryData << NormalX;
			BinaryData << NormalY;
			BinaryData << NormalZ;

			uint8 ColorR = Vertex.Color.R;
			uint8 ColorG = Vertex.Color.G;
			uint8 ColorB = Vertex.Color.B;
			uint8 ColorA = Vertex.Color.A;

			BinaryData << ColorR;
			BinaryData << ColorG;
			BinaryData << ColorB;
			BinaryData << ColorA;
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

			BinaryData << Vertex.Position.X;
			BinaryData << Vertex.Position.Y;
			BinaryData << Vertex.Position.Z;

			BinaryData << Vertex.Normal.X;
			BinaryData << Vertex.Normal.Y;
			BinaryData << Vertex.Normal.Z;

			BinaryData << Vertex.Color.R;
			BinaryData << Vertex.Color.G;
			BinaryData << Vertex.Color.B;
			BinaryData << Vertex.Color.A;

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