#pragma once

#include "EngineMinimal.h"
#include "serialization.hpp"

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
struct TMeshVertex {
	FVector Pos;
	FVector Normal;
	int32 MatIdx = -1;

	void operator = (const TMeshVertex& m) {
		Pos = m.Pos;
		Normal = m.Normal;
		MatIdx = m.MatIdx;
	}
};

inline TMeshVertex operator + (const TMeshVertex& m1, const TMeshVertex& m2) {
	return TMeshVertex{ m1.Pos + m2.Pos, m1.Normal + m2.Normal, -1 };
}

inline TMeshVertex operator / (const TMeshVertex& m, float k) {
	return TMeshVertex{ m.Pos / k, m.Normal / k, -1 };
}



/** One section of the procedural mesh. Each material has its own section. */
class FProcMeshSection {

public:

	/** Vertex buffer for this section */
	TArray<TMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	TArray<uint32> ProcIndexBuffer;

	/** Local bounding box of section */
	FBox SectionLocalBox;

	FProcMeshSection() : SectionLocalBox(EForceInit::ForceInitToZero)	{ }

	/** Reset this section, clear all mesh info. */
	void Reset() {
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
	}

	void operator = (const FProcMeshSection& A) {
		Reset();
		ProcVertexBuffer = A.ProcVertexBuffer;
		ProcIndexBuffer = A.ProcIndexBuffer;
		SectionLocalBox = A.SectionLocalBox;
	}

	void AddVertex(const TMeshVertex& Vertex) {
		ProcVertexBuffer.Add(Vertex);
		SectionLocalBox += Vertex.Pos;
	}

	typedef struct TMeshParamData {
		int32 VertexNum;
		float MaxX;
		float MaxY;
		float MaxZ;
		float MinX;
		float MinY;
		float MinZ;
	} TMeshParamData;

	void SerializeMesh(usbt::TFastUnsafeSerializer& Serializer) const {
		// vertexes
		TMeshParamData D;
		D.VertexNum = ProcVertexBuffer.Num();
		D.MaxX = SectionLocalBox.Max.X;
		D.MaxY = SectionLocalBox.Max.Y;
		D.MaxZ = SectionLocalBox.Max.Z;
		D.MinX = SectionLocalBox.Min.X;
		D.MinY = SectionLocalBox.Min.Y;
		D.MinZ = SectionLocalBox.Min.Z;
		Serializer << D;
		for (auto& Vertex : ProcVertexBuffer) {	Serializer << Vertex; }

		// indexes
		Serializer << ProcIndexBuffer.Num();
		for (int32 Index : ProcIndexBuffer) { Serializer << Index; }
	}

	void DeserializeMeshFast(usbt::TFastUnsafeDeserializer& Deserializer) {
		int32 VertexNum;
		Deserializer.readObj(VertexNum);

		float Min[3];
		float Max[3];

		Deserializer.read(&Min[0], 3);
		Deserializer.read(&Max[0], 3);

		ProcVertexBuffer.SetNum(VertexNum);
		Deserializer.read(ProcVertexBuffer.GetData(), VertexNum);

		int32 IndexNum;
		Deserializer.readObj(IndexNum);
		ProcIndexBuffer.SetNum(IndexNum);
		Deserializer.read(ProcIndexBuffer.GetData(), IndexNum);

		FBox Box(FVector(Min[0], Min[1], Min[2]), FVector(Max[0], Max[1], Max[2]));
		SectionLocalBox = Box;
	}
};