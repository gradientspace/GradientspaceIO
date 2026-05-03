// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/GLTFFormatData.h"

#include <string>
#include <vector>


namespace GS::GLTFReader
{

struct GRADIENTSPACEIO_API ReadOptions
{
	bool bNormals = true;
	bool bUVs = true;
};


// Read a .gltf JSON file from disk along with its referenced binary buffers.
// External buffers are resolved relative to the .gltf file's directory.
// On success, RootOut is populated and BuffersOut[i] holds the bytes for
// RootOut.buffers[i] (an empty vector for buffers that could not be loaded).
GRADIENTSPACEIO_API
bool ReadGLTF(
	const std::string& Path,
	GLTFFormatData::Root& RootOut,
	std::vector<std::vector<uint8_t>>& BuffersOut,
	const ReadOptions& Options = ReadOptions()
);


// Convenience: read a .gltf file and flatten its active scene into a single
// combined DenseMesh (positions/normals/UV0, with one triangle group per
// scene-level primitive instance).
GRADIENTSPACEIO_API
bool ReadGLTFToDenseMesh(
	const std::string& Path,
	DenseMesh& MeshOut,
	const ReadOptions& Options = ReadOptions()
);


// Lower-level: parse a glTF JSON string into a Root struct (no buffer loading).
// Handy for callers that already have the JSON in memory (e.g. GLB chunk0).
GRADIENTSPACEIO_API
bool ParseGLTFJsonString(
	const std::string& JsonText,
	GLTFFormatData::Root& RootOut,
	std::string* ErrorOut = nullptr
);


} // namespace GS::GLTFReader
