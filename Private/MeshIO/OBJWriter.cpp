// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/OBJWriter.h"

#include <set>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "Mesh/MeshAttributeUtils.h"

using namespace GS;


template<typename AttribType>
struct AttributeVertexBlender
{
	struct CombinedValue
	{
		AttribType Value;
		double WeightSum = 0;
	};
	std::vector<CombinedValue> VertexValues;

	void Initialize(int NumVertices, const AttribType& InitialValue)
	{
		VertexValues.resize(NumVertices);
		for (int k = 0; k < NumVertices; ++k)
			VertexValues[k] = { InitialValue, 0.0 };
	}

	void AccumulateValue(int VertexIndex, const AttribType& Value, double Weight = 1.0)
	{
		CombinedValue& C = VertexValues[VertexIndex];
		C.Value += Value;
		C.WeightSum += Weight;
	}

	void NormalizeAllValues()
	{
		size_t N = VertexValues.size();
		for (size_t i = 0; i < N; ++i)
		{
			CombinedValue& C = VertexValues[i];
			if (C.WeightSum > 0)
			{
				C.Value /= C.WeightSum;
				C.WeightSum = 1.0;
			}
		}
	}

	AttribType GetVertexValue(int VertexIndex) const
	{
		const CombinedValue& C = VertexValues[VertexIndex];
		return (C.WeightSum > 0) ? (C.Value / C.WeightSum) : C.Value;
	}
};




bool GS::OBJWriter::WriteOBJ(
	ITextWriter& TextWriter,
	const GS::DenseMesh& Mesh,
	const WriteOptions& Options)
{
	char line_buffer[1024];

	bool bWantNormals = Options.bNormals;
	bool bWantUVs = Options.bUVs;
	bool bWantVtxColors = Options.bVertexColors;

	int NumTriangles = Mesh.GetTriangleCount();
	int NumVertices = Mesh.GetVertexCount();

	AttributeVertexBlender<Vector3f> ColorBlender;
	bool bHaveVtxColors = false;
	if (bWantVtxColors)
	{
		bHaveVtxColors = true;
		ColorBlender.Initialize(NumVertices, Vector3f::Zero());
		for (int ti = 0; ti < NumTriangles; ++ti)
		{
			Index3i TriVertices = Mesh.GetTriangle(ti);
			TriVtxColors VtxColors = Mesh.GetTriVtxColors(ti);
			for (int j = 0; j < 3; ++j)
				ColorBlender.AccumulateValue(TriVertices[j], (Vector3f)VtxColors[j]);
		}
	}


	for (int vi = 0; vi < NumVertices; ++vi)
	{
		Vector3d Pos = Mesh.GetPosition(vi);
		if (bHaveVtxColors)
		{
			Vector3f Color = ColorBlender.GetVertexValue(vi);
			snprintf(line_buffer, 1023, "v %f %f %f %f %f %f", Pos.X, Pos.Y, Pos.Z, Color.X, Color.Y, Color.Z);
		}
		else
		{
			snprintf(line_buffer, 1023, "v %f %f %f", Pos.X, Pos.Y, Pos.Z);
		}
		TextWriter.WriteLine(line_buffer);
	}

	struct MeshTri
	{
		int Index;
		int Group;
		bool operator<(const MeshTri& Other) const { return Group < Other.Group; }
	};
	std::vector<MeshTri> Triangles;

	std::set<int> AllGroupIDs;

	for (int ti = 0; ti < NumTriangles; ++ti)
	{
		int GroupID = Mesh.GetTriGroup(ti);
		AllGroupIDs.insert(GroupID);
		Triangles.push_back({ ti, GroupID });
	}

	bool bHaveGroups = AllGroupIDs.size() > 1;
	std::stable_sort(Triangles.begin(), Triangles.end());

	// write out Normals
	bool bHaveNormals = false;
	AttributeCompressor<Vector3f> NormalsIndex;
	if (bWantNormals)
	{
		bHaveNormals = true;
		for (int ti = 0; ti < NumTriangles; ++ti)
		{
			TriVtxNormals VtxNormals = Mesh.GetTriVtxNormals(ti);
			for (int j = 0; j < 3; ++j)
				NormalsIndex.InsertValue(VtxNormals[j]);
		}
		int NumNormals = (int)NormalsIndex.UniqueValues.size();
		for (int ni = 0; ni < NumNormals; ++ni)
		{
			Vector3f N = NormalsIndex.UniqueValues[ni];
			snprintf(line_buffer, 1023, "vn %f %f %f", N.X, N.Y, N.Z);
			TextWriter.WriteLine(line_buffer);
		}
	}

	// write out UVs
	bool bHaveUVs = false;
	AttributeCompressor<Vector2f> UVsIndex;
	if (bWantUVs)
	{
		bHaveUVs = true;
		for (int ui = 0; ui < NumTriangles; ++ui)
		{
			TriVtxUVs VtxUvs = Mesh.GetTriVtxUVs(ui);
			for (int j = 0; j < 3; ++j)
				UVsIndex.InsertValue(VtxUvs[j]);
		}
		int NumUVs = (int)UVsIndex.UniqueValues.size();
		for (int ni = 0; ni < NumUVs; ++ni)
		{
			Vector2f UV = UVsIndex.UniqueValues[ni];
			snprintf(line_buffer, 1023, "vt %f %f", UV.X, UV.Y);
			TextWriter.WriteLine(line_buffer);
		}
	}

	int VertexOffset = 1;
	int UVOffset = 1;
	int NormalOffset = 1;

	int CurGroupID = -99999;
	for (MeshTri MeshTri : Triangles)
	{
		if (bHaveGroups && MeshTri.Group != CurGroupID)
		{
			snprintf(line_buffer, 1023, "g %d", MeshTri.Group);
			TextWriter.WriteLine(line_buffer);
			CurGroupID = MeshTri.Group;
		}

		int ti = MeshTri.Index;

		Index3i TriVertices = Mesh.GetTriangle(ti);

		Index3i TriNormals = Index3i::Zero();
		if (bHaveNormals)
		{
			TriVtxNormals VtxNormals = Mesh.GetTriVtxNormals(ti);
			for (int j = 0; j < 3; ++j)
				TriNormals[j] = NormalsIndex.GetIndexForValue(VtxNormals[j], false);
		}

		Index3i TriUVs = Index3i::Zero();
		if (bHaveUVs)
		{
			TriVtxUVs VtxUvs = Mesh.GetTriVtxUVs(ti);
			for (int j = 0; j < 3; ++j)
				TriUVs[j] = UVsIndex.GetIndexForValue(VtxUvs[j], false);
		}

		const bool bInvertMesh = true;
		if (bInvertMesh)
		{
			int tmp = TriVertices.A; TriVertices.A = TriVertices.B; TriVertices.B = tmp;
			tmp = TriUVs.A; TriUVs.A = TriUVs.B; TriUVs.B = tmp;
			tmp = TriNormals.A; TriNormals.A = TriNormals.B; TriNormals.B = tmp;
		}

		if (bHaveUVs && bHaveNormals)
		{
			snprintf(line_buffer, 1023, "f %d/%d/%d %d/%d/%d %d/%d/%d",
				(TriVertices.A + VertexOffset), (TriUVs.A + UVOffset), (TriNormals.A + NormalOffset),
				(TriVertices.B + VertexOffset), (TriUVs.B + UVOffset), (TriNormals.B + NormalOffset),
				(TriVertices.C + VertexOffset), (TriUVs.C + UVOffset), (TriNormals.C + NormalOffset));
		}
		else if (bHaveUVs)
		{
			snprintf(line_buffer, 1023, "f %d/%d %d/%d %d/%d",
				(TriVertices.A + VertexOffset), (TriUVs.A + UVOffset),
				(TriVertices.B + VertexOffset), (TriUVs.B + UVOffset),
				(TriVertices.C + VertexOffset), (TriUVs.C + UVOffset));
		}
		else if (bHaveNormals)
		{
			snprintf(line_buffer, 1023, "f %d//%d %d//%d %d//%d",
				(TriVertices.A + VertexOffset), (TriNormals.A + NormalOffset),
				(TriVertices.B + VertexOffset), (TriNormals.B + NormalOffset),
				(TriVertices.C + VertexOffset), (TriNormals.C + NormalOffset));
		}
		else
		{
			snprintf(line_buffer, 1023, "f %d %d %d",
				(TriVertices.A + VertexOffset), (TriVertices.B + VertexOffset), (TriVertices.C + VertexOffset));
		}
		TextWriter.WriteLine(line_buffer);
	}

	return true;
}









bool GS::OBJWriter::WriteOBJ(
	ITextWriter& TextWriter,
	const OBJFormatData& OBJData,
	const WriteOptions& Options)
{
	char line_buffer[1024];

	bool bWantNormals = Options.bNormals;
	bool bWantUVs = Options.bUVs;

	int NumVertices = (int)OBJData.VertexPositions.size();
	bool bHaveVtxColors = Options.bVertexColors && (OBJData.VertexColors.size() == NumVertices);
	for (int vi = 0; vi < NumVertices; ++vi)
	{
		Vector3d Pos = OBJData.VertexPositions[vi];
		if (bHaveVtxColors)
		{
			Vector3f Color = OBJData.VertexColors[vi];
			snprintf(line_buffer, 1023, "v %f %f %f %f %f %f", Pos.X, Pos.Y, Pos.Z, Color.X, Color.Y, Color.Z);
		}
		else
		{
			snprintf(line_buffer, 1023, "v %f %f %f", Pos.X, Pos.Y, Pos.Z);
		}
		TextWriter.WriteLine(line_buffer);
	}

	int NumNormals = (bWantNormals) ? (int)OBJData.Normals.size() : 0;
	if (bWantNormals && NumNormals > 0)
	{
		for (int ni = 0; ni < NumNormals; ++ni)
		{
			Vector3d Normal = OBJData.Normals[ni];
			snprintf(line_buffer, 1023, "vn %f %f %f", Normal.X, Normal.Y, Normal.Z);
			TextWriter.WriteLine(line_buffer);
		}
	}
	auto IsValidNormal = [NumNormals](int normal_index) { return normal_index >= 0 && normal_index < NumNormals; };

	int NumUVs = (bWantUVs) ? (int)OBJData.UVs.size() : 0;
	if (bWantUVs && NumUVs > 0)
	{
		for (int ui = 0; ui < NumUVs; ++ui)
		{
			Vector2d UV = OBJData.UVs[ui];
			snprintf(line_buffer, 1023, "vt %f %f", UV.X, UV.Y);
			TextWriter.WriteLine(line_buffer);
		}
	}
	auto IsValidUV = [NumUVs](int uv_index) { return uv_index >= 0 && uv_index < NumUVs; };


	auto WriteVertexToken = [&](int Vertex, int Normal, int UV, bool bIncludeNormals, bool bIncludeUVs)
	{
		if (bIncludeUVs && bIncludeNormals) {
			snprintf(line_buffer, 1023, "%d/%d/%d", Vertex + 1, UV + 1, Normal + 1);
		}
		else if (bIncludeUVs) {
			snprintf(line_buffer, 1023, "%d/%d", Vertex + 1, UV + 1);
		}
		else if (bIncludeNormals) {
			snprintf(line_buffer, 1023, "%d//%d", Vertex + 1, Normal + 1);
		}
		else
			snprintf(line_buffer, 1023, "%d", Vertex + 1);
		TextWriter.WriteToken(line_buffer);
	};
	auto WriteVertices = [&](int Num, const int* Vertices, const int* Normals, const int* UVs, bool bIncludeNormals, bool bIncludeUVs)
	{
		for (int j = 0; j < Num; ++j )
		{ 
			WriteVertexToken(Vertices[j], Normals[j], UVs[j], bIncludeNormals, bIncludeUVs);
			if (j != Num-1) TextWriter.WriteToken(" ");
		}
	};

	int CurGroupID = std::numeric_limits<int>::max();
	uint32_t NumFaces = (uint32_t)OBJData.FaceStream.size();
	uint32_t NumTriangles = (uint32_t)OBJData.Triangles.size();
	uint32_t NumQuads = (uint32_t)OBJData.Quads.size();
	uint32_t NumPolygons = (uint32_t)OBJData.Polygons.size();
	for (uint32_t fi = 0; fi < NumFaces; ++fi)
	{
		const OBJFace& Face = OBJData.FaceStream[fi];
		if (Face.GroupID != CurGroupID)
		{
			snprintf(line_buffer, 1023, "g %d", Face.GroupID);
			TextWriter.WriteLine(line_buffer);
			CurGroupID = Face.GroupID;
		}

		if (Face.FaceType == 0 && Face.FaceIndex < NumTriangles)
		{
			const OBJTriangle& Triangle = OBJData.Triangles[Face.FaceIndex];
			bool bWriteNormals = IsValidNormal(Triangle.Normals.A) && IsValidNormal(Triangle.Normals.B) && IsValidNormal(Triangle.Normals.C);
			bool bWriteUVs = IsValidUV(Triangle.UVs.A) && IsValidUV(Triangle.UVs.B) && IsValidUV(Triangle.UVs.C);
			TextWriter.WriteToken("f ");
			WriteVertices(3, &Triangle.Positions.A, &Triangle.Normals.A, &Triangle.UVs.A, bWriteNormals, bWriteUVs);
			TextWriter.WriteEndOfLine();
		}
		else if (Face.FaceType == 1 && Face.FaceIndex < NumQuads)
		{
			const OBJQuad& Quad = OBJData.Quads[Face.FaceIndex];
			bool bWriteNormals = IsValidNormal(Quad.Normals.A) && IsValidNormal(Quad.Normals.B) && IsValidNormal(Quad.Normals.C) && IsValidNormal(Quad.Normals.D);
			bool bWriteUVs = IsValidUV(Quad.UVs.A) && IsValidUV(Quad.UVs.B) && IsValidUV(Quad.UVs.C) && IsValidUV(Quad.UVs.D);
			TextWriter.WriteToken("f ");
			WriteVertices(4, &Quad.Positions.A, &Quad.Normals.A, &Quad.UVs.A, bWriteNormals, bWriteUVs);
			TextWriter.WriteEndOfLine();
		}
		else if (Face.FaceType == 2 && Face.FaceIndex < NumPolygons)
		{
			const OBJPolygon& Poly = OBJData.Polygons[Face.FaceIndex];
			int NumPolyVerts = (int)Poly.Positions.size();
			bool bWriteNormals = ((int)Poly.Normals.size() == NumPolyVerts);
			bool bWriteUVs = ((int)Poly.UVs.size() == NumPolyVerts);
			for (int j = 0; j < NumPolyVerts; ++j)
			{
				bWriteNormals = bWriteNormals && IsValidNormal(Poly.Normals[j]);
				bWriteUVs = bWriteUVs && IsValidUV(Poly.UVs[j]);
			}
			TextWriter.WriteToken("f ");
			WriteVertices(NumPolyVerts, &Poly.Positions[0], &Poly.Normals[0], &Poly.UVs[0], bWriteNormals, bWriteUVs);
			TextWriter.WriteEndOfLine();
		}
	}

	return true;
}
