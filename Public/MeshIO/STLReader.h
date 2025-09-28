// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/OBJFormatData.h"

#include <string>
#include <vector>

namespace GS::STLReader
{

struct GRADIENTSPACEIO_API STLTriangle
{
	Vector3f Normal;
	Vector3f Vertex1;
	Vector3f Vertex2;
	Vector3f Vertex3;
	uint16_t Attribute;  // usually zero
	// see https://en.wikipedia.org/wiki/STL_(file_format) for some notes on how
	// Attribute can be used to represent color
};


struct GRADIENTSPACEIO_API STLMeshData
{
	// note: Magics stores color info in header...should implement reader
	std::vector<char> Header;
	std::vector<STLTriangle> Triangles;
};


GRADIENTSPACEIO_API
bool ReadSTL(
	const std::string& Path,
	STLMeshData& STLMeshOut
);




/**
 * Extract a DenseMesh out of OBJFormatData.
 * Currently Quads and Polygons are tessellated strictly topologically, ie tris (0,1,2), (0,2,3), ...
 */
GRADIENTSPACEIO_API
void STLMeshToDenseMesh(const STLMeshData& STLMesh, DenseMesh& MeshOut);




}