// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Core/unsafe_vector.h"
#include "Math/GSIndex3.h"
#include "Math/GSIndex4.h"
#include "Mesh/DenseMesh.h"
#include "Mesh/PolyMesh.h"

#include <string>
#include <vector>


namespace GS
{

struct GRADIENTSPACEIO_API OBJTriangle
{
	Index3i Positions;
	Index3i Normals;
	Index3i UVs;
};

struct GRADIENTSPACEIO_API OBJQuad
{
	Index4i Positions;
	Index4i Normals;
	Index4i UVs;
};

struct GRADIENTSPACEIO_API OBJPolygon
{
	std::vector<int> Positions;
	std::vector<int> Normals;
	std::vector<int> UVs;
};

struct GRADIENTSPACEIO_API OBJGroup
{
	std::string GroupName;
};

struct GRADIENTSPACEIO_API OBJMaterial
{
	std::string MaterialName;
};

struct GRADIENTSPACEIO_API OBJFace
{
	uint8_t FaceType : 4;
	uint32_t FaceIndex : 28;

	uint32_t GroupID;
};

struct GRADIENTSPACEIO_API OBJFormatData
{
	std::vector<std::string> HeaderComments;

	unsafe_vector<Vector3d> VertexPositions;
	unsafe_vector<Vector3f> VertexColors;
	unsafe_vector<Vector3d> Normals;
	unsafe_vector<Vector2d> UVs;

	unsafe_vector<OBJTriangle> Triangles;
	unsafe_vector<OBJQuad> Quads;
	unsafe_vector<OBJPolygon> Polygons;

	// ordered indexing into Triangles/Quads/Polygons
	unsafe_vector<OBJFace> FaceStream;

	unsafe_vector<OBJGroup> Groups;
	unsafe_vector<OBJMaterial> Materials;
};



GRADIENTSPACEIO_API
void DenseMeshToOBJFormatData(const DenseMesh& Mesh, OBJFormatData& OBJDataOut);

GRADIENTSPACEIO_API
void PolyMeshToOBJFormatData(const PolyMesh& Mesh, OBJFormatData& OBJDataOut);

};
