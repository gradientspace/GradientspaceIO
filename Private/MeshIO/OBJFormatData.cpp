// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/OBJFormatData.h"
#include "Mesh/MeshAttributeUtils.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>


using namespace GS;

void GS::DenseMeshToOBJFormatData(const DenseMesh& Mesh, OBJFormatData& OBJDataOut)
{
	bool bWantNormals = true, bWantUVs = true, bWantVtxColors = true;

	int NumVertices = Mesh.GetVertexCount();
	OBJDataOut.VertexPositions.reserve(NumVertices);
	for (int vi = 0; vi < NumVertices; ++vi)
		OBJDataOut.VertexPositions.add(Mesh.GetPosition(vi));

	int NumTriangles = Mesh.GetTriangleCount();

	if (bWantVtxColors)
	{
		AttributeVertexCombiner<Vector3f> ColorBlender;
		ColorBlender.Initialize(NumVertices, Vector3f::Zero());
		for (int ti = 0; ti < NumTriangles; ++ti)
		{
			Index3i TriVertices = Mesh.GetTriangle(ti);
			TriVtxColors VtxColors = Mesh.GetTriVtxColors(ti);
			for (int j = 0; j < 3; ++j)
				ColorBlender.AccumulateValue(TriVertices[j], (Vector3f)VtxColors[j]);
		}
		OBJDataOut.VertexColors.reserve(NumVertices);
		for (int vi = 0; vi < NumVertices; ++vi)
			OBJDataOut.VertexColors.add(ColorBlender.GetVertexValue(vi));
	}

	struct MeshTri
	{
		int Index;
		int Group;
		bool operator<(const MeshTri& Other) const { return Group < Other.Group; }
	};
	std::vector<MeshTri> Triangles;
	std::unordered_set<int> AllGroupIDs;

	Triangles.reserve(NumTriangles);
	for (int ti = 0; ti < NumTriangles; ++ti)
	{
		int GroupID = Mesh.GetTriGroup(ti);
		AllGroupIDs.insert(GroupID);
		Triangles.push_back({ ti, GroupID });
	}

	[[maybe_unused]] bool bHaveGroups = AllGroupIDs.size() > 1;
	std::stable_sort(Triangles.begin(), Triangles.end());

	bool bHaveNormalIndexMap = false;
	AttributeCompressor<Vector3d> NormalsIndex;
	if (bWantNormals)
	{
		for (int ti = 0; ti < NumTriangles; ++ti)
		{
			TriVtxNormals VtxNormals = Mesh.GetTriVtxNormals(ti);
			for (int j = 0; j < 3; ++j)
				NormalsIndex.InsertValue( (Vector3d)VtxNormals[j] );
		}
		OBJDataOut.Normals = std::move(NormalsIndex.UniqueValues);
		bHaveNormalIndexMap = true;
	}

	bool bHaveUVIndexMap = false;
	AttributeCompressor<Vector2d> UVsIndex;
	if (bWantUVs)
	{
		for (int ti = 0; ti < NumTriangles; ++ti)
		{
			TriVtxUVs VtxUVs = Mesh.GetTriVtxUVs(ti);
			for (int j = 0; j < 3; ++j)
				UVsIndex.InsertValue((Vector2d)VtxUVs[j]);
		}
		OBJDataOut.UVs = std::move(UVsIndex.UniqueValues);
		bHaveUVIndexMap = true;
	}

	OBJDataOut.FaceStream.reserve(NumTriangles);
	OBJDataOut.Triangles.reserve(NumTriangles);

	//int CurGroupID = std::numeric_limits<int>::max();
	for ( int k = 0; k < NumTriangles; ++k )
	{
		const MeshTri& SortedTri = Triangles[k];

		int ti = SortedTri.Index;
		Index3i TriVertices = Mesh.GetTriangle(ti);

		OBJTriangle NewTri;
		NewTri.Positions = TriVertices;

		NewTri.Normals = Index3i(-1,-1,-1);
		if (bHaveNormalIndexMap)
		{
			TriVtxNormals VtxNormals = Mesh.GetTriVtxNormals(ti);
			for (int j = 0; j < 3; ++j)
				NewTri.Normals[j] = NormalsIndex.GetIndexForValue( (Vector3d)VtxNormals[j], false );
		}

		NewTri.UVs = Index3i(-1, -1, -1);
		if (bHaveUVIndexMap)
		{
			TriVtxUVs VtxUvs = Mesh.GetTriVtxUVs(ti);
			for (int j = 0; j < 3; ++j)
				NewTri.UVs[j] = UVsIndex.GetIndexForValue( (Vector2d)VtxUvs[j], false );
		}

		OBJFace Face;
		Face.FaceType = 0;
		Face.FaceIndex = (uint32_t)OBJDataOut.Triangles.size();
		OBJDataOut.FaceStream.add(Face);
		OBJDataOut.Triangles.add(NewTri);
	}

}


void GS::OBJFormatDataToDenseMesh(const OBJFormatData& OBJData, DenseMesh& MeshOut, const OBJToDenseMeshOptions& Options)
{
	int NumUVs = (int)OBJData.UVs.size();
	bool bWantUVs = (Options.bIgnoreUVs == false && NumUVs > 0);
	auto get_uv_tri = [&](Index3i Tri) {
		TriVtxUVs result;
		for (int j = 0; j < 3; ++j)
			result[j] = (Tri[j] >= 0 && Tri[j] < NumUVs) ? (Vector2f)OBJData.UVs[Tri[j]] : Vector2f::Zero();
		return result;
	};

	int NumNormals = (int)OBJData.Normals.size();
	bool bWantNormals = (Options.bIgnoreNormals == false && NumNormals > 0);
	auto get_normal_tri = [&](Index3i Tri) {
		TriVtxNormals result;
		for (int j = 0; j < 3; ++j)
			result[j] = (Tri[j] >= 0 && Tri[j] < NumNormals) ? (Vector3f)OBJData.Normals[Tri[j]] : Vector3f::UnitZ();
		return result;
	};

	int NumColors = (int)OBJData.VertexColors.size();
	bool bWantVertexColors = (Options.bIgnoreColors == false && NumColors > 0);
	auto get_color_tri = [&](Index3i Tri) {
		TriVtxColors result;
		for (int j = 0; j < 3; ++j) {
			Vector3f Color = (Tri[j] >= 0 && Tri[j] < NumColors) ? OBJData.VertexColors[Tri[j]] : Vector3f::UnitZ();
			result[j] = Color4b(Color);
		}
		return result;
	};


	int NumVertices = (int)OBJData.VertexPositions.size();

	int NumFaces = (int)OBJData.FaceStream.size();
	int TotalNumTriangles = (int)OBJData.Triangles.size() + 2 * (int)OBJData.Quads.size();

	// for now do simple polygon tessellation where count can be predicted
	int NumPolygons = (int)OBJData.Polygons.size();
	for (int pi = 0; pi < NumPolygons; ++pi) {
		TotalNumTriangles += (int)OBJData.Polygons[pi].Positions.size() - 2;
	}

	MeshOut.Resize(NumVertices, TotalNumTriangles);

	for (int vid = 0; vid < OBJData.VertexPositions.size(); ++vid)
		MeshOut.SetPosition(vid, OBJData.VertexPositions[vid]);

	int tid = 0;
	for (int fid = 0; fid < OBJData.FaceStream.size(); ++fid) {
		int FaceType = OBJData.FaceStream[fid].FaceType;
		int TypeIndex = (int)OBJData.FaceStream[fid].FaceIndex;
		int FaceGroupID = OBJData.FaceStream[fid].GroupID;
		if (FaceType == 0) 
		{
			const OBJTriangle& Tri = OBJData.Triangles[TypeIndex];
			Index3i TriV = Tri.Positions;
			MeshOut.SetTriangle(tid, TriV);
			MeshOut.SetTriGroup(tid, FaceGroupID);
			if (bWantUVs)
				MeshOut.SetTriVtxUVs(tid, get_uv_tri(Tri.UVs));
			if (bWantNormals)
				MeshOut.SetTriVtxNormals(tid, get_normal_tri(Tri.Normals));
			if (bWantVertexColors)
				MeshOut.SetTriVtxColors(tid, get_color_tri(TriV));
			tid++;
		}
		else if (FaceType == 1)
		{
			const OBJQuad& Quad = OBJData.Quads[TypeIndex];
			MeshOut.SetTriangle(tid, Index3i(Quad.Positions.A, Quad.Positions.B, Quad.Positions.C));
			MeshOut.SetTriGroup(tid, FaceGroupID);
			if (bWantUVs)
				MeshOut.SetTriVtxUVs(tid, get_uv_tri( Index3i(Quad.UVs.A, Quad.UVs.B, Quad.UVs.C) ));
			if (bWantNormals)
				MeshOut.SetTriVtxNormals(tid, get_normal_tri( Index3i(Quad.Normals.A, Quad.Normals.B, Quad.Normals.C) ));
			if (bWantVertexColors)
				MeshOut.SetTriVtxColors(tid, get_color_tri( Index3i(Quad.Positions.A, Quad.Positions.B, Quad.Positions.C) ));
			tid++;
			MeshOut.SetTriangle(tid, Index3i(Quad.Positions.A, Quad.Positions.C, Quad.Positions.D));
			MeshOut.SetTriGroup(tid, FaceGroupID);
			if (bWantUVs)
				MeshOut.SetTriVtxUVs(tid, get_uv_tri( Index3i(Quad.UVs.A, Quad.UVs.C, Quad.UVs.D) ));
			if (bWantNormals)
				MeshOut.SetTriVtxNormals(tid, get_normal_tri( Index3i(Quad.Normals.A, Quad.Normals.C, Quad.Normals.D) ));
			if (bWantVertexColors)
				MeshOut.SetTriVtxColors(tid, get_color_tri(Index3i(Quad.Positions.A, Quad.Positions.C, Quad.Positions.D)) );
			tid++;
		}
		else if (FaceType == 2)
		{
			const OBJPolygon& Poly = OBJData.Polygons[TypeIndex];
			int NumV = (int)Poly.Positions.size();
			for (int i = 1; i < NumV - 1; ++i) {
				MeshOut.SetTriangle(tid, Index3i(Poly.Positions[0], Poly.Positions[i], Poly.Positions[i+1]) );
				MeshOut.SetTriGroup(tid, FaceGroupID);
				if (bWantUVs)
					MeshOut.SetTriVtxUVs(tid, get_uv_tri( Index3i(Poly.UVs[0], Poly.UVs[i], Poly.UVs[i+1]) ));
				if (bWantNormals)
					MeshOut.SetTriVtxNormals(tid, get_normal_tri( Index3i(Poly.Normals[0], Poly.Normals[i], Poly.Normals[i+1]) ));
				if (bWantVertexColors)
					MeshOut.SetTriVtxColors(tid, get_color_tri( Index3i(Poly.Positions[0], Poly.Positions[i], Poly.Positions[i+1]) ));
				tid++;
			}
		}

	}

}



void GS::PolyMeshToOBJFormatData(const PolyMesh& Mesh, OBJFormatData& OBJDataOut)
{
	bool bWantNormals = true;
	bool bWantUVs = true;
	bool bWantVtxColors = true;

	int UseNormalSet = 0;
	bool bHaveNormals = bWantNormals && (Mesh.GetNumNormalSets() > UseNormalSet) && (Mesh.GetNormalCount(UseNormalSet) > 0);

	int UseUVSet = 0;
	bool bHaveUVs = bWantUVs && (Mesh.GetNumUVSets() > UseUVSet) && (Mesh.GetUVCount(UseUVSet) > 0);

	int UseColorSet = 0;
	bool bHaveColors = bWantVtxColors && (Mesh.GetNumColorSets() > UseColorSet) && (Mesh.GetColorCount(UseColorSet) > 0);

	int UseGroupSet = 0;
	bool bHaveGroups = (Mesh.GetNumFaceGroupSets() > UseGroupSet);

	int NumVertices = Mesh.GetVertexCount();
	OBJDataOut.VertexPositions.reserve(NumVertices);
	for (int vi = 0; vi < NumVertices; ++vi)
		OBJDataOut.VertexPositions.add(Mesh.GetPosition(vi));

	int NumFaces = Mesh.GetFaceCount();

	if (bHaveColors)
	{
		AttributeVertexCombiner<Vector3f> ColorBlender;
		ColorBlender.Initialize(NumVertices, Vector3f::Zero());
		for (int fi = 0; fi < NumFaces; ++fi)
		{
			PolyMesh::Face Face = Mesh.GetFace(fi);
			int FaceVertexCount = Mesh.GetFaceVertexCount(Face);
			for (int j = 0; j < FaceVertexCount; ++j) {
				int Vertex = Mesh.GetFaceVertex(Face, j);
				Vector4f VertexColor = Mesh.GetFaceVertexColor(Face, j, UseColorSet);
				ColorBlender.AccumulateValue(Vertex, Vector3f(VertexColor.X, VertexColor.Y, VertexColor.Z));
			}
		}
		OBJDataOut.VertexColors.reserve(NumVertices);
		for (int vi = 0; vi < NumVertices; ++vi)
			OBJDataOut.VertexColors.add(ColorBlender.GetVertexValue(vi));
	}

	struct SortedFace
	{
		int Index;
		int Group;
		bool operator<(const SortedFace& Other) const { return Group < Other.Group; }
	};
	std::vector<SortedFace> SortedFaces;
	std::unordered_set<int> AllGroupIDs;

	SortedFaces.reserve(NumFaces);
	for (int fi = 0; fi < NumFaces; ++fi)
	{
		int GroupID = (bHaveGroups) ? Mesh.GetFaceGroup(fi, UseGroupSet) : 0;
		AllGroupIDs.insert(GroupID);
		SortedFaces.push_back({ fi, GroupID });
	}

	bHaveGroups = (AllGroupIDs.size() > 1);
	std::stable_sort(SortedFaces.begin(), SortedFaces.end());

	if (bHaveNormals)
	{
		int NumNormals = Mesh.GetNormalCount(UseNormalSet);
		for (int ni = 0; ni < NumNormals; ++ni)
		{
			Vector3f Normal = Mesh.GetNormal(ni, UseNormalSet);
			OBJDataOut.Normals.add((Vector3d)Normal);
		}
	}

	if (bHaveUVs)
	{
		int NumUVs = Mesh.GetUVCount(UseUVSet);
		for (int ui = 0; ui < NumUVs; ++ui)
		{
			Vector2d UV = Mesh.GetUV(ui, UseUVSet);
			OBJDataOut.UVs.add(UV);
		}
	}


	OBJDataOut.FaceStream.reserve(NumFaces);
	OBJDataOut.Triangles.reserve(Mesh.GetTriangleCount());
	OBJDataOut.Quads.reserve(Mesh.GetQuadCount());
	OBJDataOut.Polygons.reserve(Mesh.GetPolygonCount());

	//int CurGroupID = std::numeric_limits<int>::max();
	for (int k = 0; k < NumFaces; ++k)
	{
		int GroupID = SortedFaces[k].Group;
		PolyMesh::Face Face = Mesh.GetFace(SortedFaces[k].Index);
		int OBJFaceIndex = -1;

		if (Face.IsTriangle())
		{
			OBJTriangle NewTri;
			NewTri.Positions = Mesh.GetTriangle(Face);
		
			if (bHaveNormals) {
				for (int j = 0; j < 3; ++j)
					NewTri.Normals[j] = Mesh.GetFaceVertexNormalIndex(Face, j, UseNormalSet);
			} else 
				NewTri.Normals = Index3i(-1, -1, -1);

			if (bHaveUVs) {
				for (int j = 0; j < 3; ++j)
					NewTri.UVs[j] = Mesh.GetFaceVertexUVIndex(Face, j, UseUVSet);
			}
			else
				NewTri.UVs = Index3i(-1, -1, -1);

			OBJFaceIndex = (int)OBJDataOut.Triangles.add(NewTri);
		}
		else if (Face.IsQuad())
		{
			OBJQuad NewQuad;
			NewQuad.Positions = Mesh.GetQuad(Face);

			if (bHaveNormals) {
				for (int j = 0; j < 4; ++j)
					NewQuad.Normals[j] = Mesh.GetFaceVertexNormalIndex(Face, j, UseNormalSet);
			}
			else
				NewQuad.Normals = Index4i(-1, -1, -1, -1);

			if (bHaveUVs) {
				for (int j = 0; j < 4; ++j)
					NewQuad.UVs[j] = Mesh.GetFaceVertexUVIndex(Face, j, UseUVSet);
			}
			else
				NewQuad.UVs = Index4i(-1, -1, -1, -1);

			OBJFaceIndex = (int)OBJDataOut.Quads.add(NewQuad);
		}
		else if (Face.IsPolygon())
		{
			int NumFaceVertices = Mesh.GetNumFaceVertices(Face);

			const PolyMesh::Polygon& SourcePoly = Mesh.GetPolygon(Face);
			OBJPolygon NewPolygon;
			NewPolygon.Positions.resize(NumFaceVertices);
			for (int j = 0; j < NumFaceVertices; ++j)
				NewPolygon.Positions[j] = SourcePoly.Vertices[j];

			if (bHaveNormals) {
				NewPolygon.Normals.resize(NumFaceVertices);
				for (int j = 0; j < NumFaceVertices; ++j)
					NewPolygon.Normals[j] = Mesh.GetFaceVertexNormalIndex(Face, j, UseNormalSet);
			}

			if (bHaveUVs) {
				NewPolygon.UVs.resize(NumFaceVertices);
				for (int j = 0; j < NumFaceVertices; ++j)
					NewPolygon.UVs[j] = Mesh.GetFaceVertexUVIndex(Face, j, UseUVSet);
			}

			OBJFaceIndex = (int)OBJDataOut.Polygons.add(std::move(NewPolygon));
		}
		if (OBJFaceIndex == -1) 
			continue;

		OBJFace NewOBJFace;
		NewOBJFace.FaceType = Face.Type;		// todo risky
		NewOBJFace.FaceIndex = (uint32_t)OBJFaceIndex;
		NewOBJFace.GroupID = GroupID;
		OBJDataOut.FaceStream.add(NewOBJFace);
	}
}
