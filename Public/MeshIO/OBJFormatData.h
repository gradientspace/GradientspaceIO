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
	// Sentinel for MaterialID meaning "no usemtl was active when this face
	// was parsed". -1 so callers that clamp to a valid range fall to 0.
	static constexpr int32_t NO_MATERIAL_ASSIGNED = -1;

	uint8_t FaceType : 4;			// 0 = triangle, 1 = quad, 2 = polygon
	uint32_t FaceIndex : 28;

	uint32_t GroupID;

	// Index into OBJFormatData::Materials of the material active when this
	// face was parsed (i.e. the most recent "usemtl" before the face).
	// NO_MATERIAL_ASSIGNED means no usemtl was active.
	int32_t MaterialID;
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

	// .mtl material library filenames referenced by "mtllib" lines.
	// An OBJ file may contain multiple mtllib lines, and each line may
	// list multiple files; all are accumulated here in encounter order.
	// Paths are stored verbatim (typically relative to the .obj file).
	std::vector<std::string> MTLLibs;
};


/**
 * Convert a DenseMesh to OBJFormatData for writing/export
 */
GRADIENTSPACEIO_API
void DenseMeshToOBJFormatData(const DenseMesh& Mesh, OBJFormatData& OBJDataOut);


struct GRADIENTSPACEIO_API OBJToDenseMeshOptions
{
	bool bIgnoreUVs = false;
	bool bIgnoreNormals = false;
	bool bIgnoreColors = false;
};

/**
 * Extract a DenseMesh out of OBJFormatData. 
 * Currently Quads and Polygons are tessellated strictly topologically, ie tris (0,1,2), (0,2,3), ...
 */
GRADIENTSPACEIO_API
void OBJFormatDataToDenseMesh(const OBJFormatData& OBJData, DenseMesh& MeshOut,
	const OBJToDenseMeshOptions& Options = OBJToDenseMeshOptions());

/**
 * Convert a PolyMesh to OBJFormatData for writing/export
 */
GRADIENTSPACEIO_API
void PolyMeshToOBJFormatData(const PolyMesh& Mesh, OBJFormatData& OBJDataOut);

};
