// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/GLTFFormatData.h"

#include <string>
#include <vector>


namespace GS::GLTFWriter
{

struct GRADIENTSPACEIO_API WriteOptions
{
	bool bNormals = true;
	bool bUVs = true;
	bool bPrettyJson = true;
};


// How buffers[0]'s payload is delivered alongside the .gltf JSON.
enum class BufferLayout
{
	Sidecar,   // write a sibling .bin file and point buffers[0].uri at it
	Embedded,  // base64-inline BinaryBlob into buffers[0].uri as a data URI
};


// Convert a DenseMesh to a glTF file written at OutGLTFPath. The binary
// payload (vertex/index data) is written to a sibling .bin file derived from
// OutGLTFPath's stem (e.g. "model.gltf" -> "model.bin"). Both files must be
// kept together for the result to be loadable.
GRADIENTSPACEIO_API
bool WriteGLTF(
	const std::string& OutGLTFPath,
	const DenseMesh& Mesh,
	const WriteOptions& Options = WriteOptions()
);


// Lower-level: write an already-built Root + binary blob. The function
// rewrites buffers[0] to match the chosen Layout: Sidecar emits a "<stem>.bin"
// next to the .gltf and points buffers[0].uri at it; Embedded base64-inlines
// BinaryBlob directly into buffers[0].uri as a data URI.
GRADIENTSPACEIO_API
bool WriteGLTF(
	const std::string& OutGLTFPath,
	const GLTFFormatData::Root& Root,
	const std::vector<uint8_t>& BinaryBlob,
	BufferLayout Layout = BufferLayout::Sidecar,
	bool bPrettyJson = true
);


} // namespace GS::GLTFWriter
