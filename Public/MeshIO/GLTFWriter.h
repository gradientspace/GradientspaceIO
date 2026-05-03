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


// Lower-level: write an already-built Root + binary blob. The caller is
// responsible for setting Root.buffers[0].uri (or leaving it unset if the
// payload is being written elsewhere, e.g. as a GLB chunk). If a .bin
// sibling needs writing, set bWriteBinarySidecar=true and the function will
// emit BinaryBlob to "<stem>.bin" and overwrite buffers[0].uri accordingly.
GRADIENTSPACEIO_API
bool WriteGLTF(
	const std::string& OutGLTFPath,
	const GLTFFormatData::Root& Root,
	const std::vector<uint8_t>& BinaryBlob,
	bool bWriteBinarySidecar = true,
	bool bPrettyJson = true
);


} // namespace GS::GLTFWriter
