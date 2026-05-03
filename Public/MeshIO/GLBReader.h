// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/GLTFFormatData.h"

#include <string>
#include <vector>


namespace GS::GLBReader
{

struct GRADIENTSPACEIO_API ReadOptions
{
	bool bNormals = true;
	bool bUVs = true;
};


// Read a .glb (GLTF binary container) file from disk.
// On success, RootOut is populated from the embedded JSON chunk and BuffersOut[0]
// holds the BIN chunk bytes; subsequent buffers (if any are referenced via
// external URIs) are best-effort resolved relative to the .glb file's directory
// (an empty vector is left for any that fail to load).
// glb file format spec:
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification
GRADIENTSPACEIO_API
bool ReadGLB(
	const std::string& Path,
	GLTFFormatData::Root& RootOut,
	std::vector<std::vector<uint8_t>>& BuffersOut,
	const ReadOptions& Options = ReadOptions()
);


// Convenience: read a .glb file and flatten its active scene into a single
// combined DenseMesh (positions/normals/UV0, with one triangle group per
// scene-level primitive instance).
GRADIENTSPACEIO_API
bool ReadGLBToDenseMesh(
	const std::string& Path,
	DenseMesh& MeshOut,
	const ReadOptions& Options = ReadOptions()
);


} // namespace GS::GLBReader
