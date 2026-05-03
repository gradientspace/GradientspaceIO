// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/GLTFFormatData.h"

#include <string>
#include <vector>


namespace GS::GLBWriter
{

struct GRADIENTSPACEIO_API WriteOptions
{
	bool bNormals = true;
	bool bUVs = true;
};


// Convert a DenseMesh to a self-contained .glb file at OutGLBPath. Both the
// mesh JSON and binary payload are packed into the single .glb container.
GRADIENTSPACEIO_API
bool WriteGLB(
	const std::string& OutGLBPath,
	const DenseMesh& Mesh,
	const WriteOptions& Options = WriteOptions()
);


// Lower-level: write an already-built Root + binary blob as a .glb file. The
// binary blob is emitted as the GLB BIN chunk and becomes buffers[0]; the
// caller's Root.buffers[0].uri is forced to unset (per the GLB spec) and
// Root.buffers[0].byteLength is set to the blob's size.
GRADIENTSPACEIO_API
bool WriteGLB(
	const std::string& OutGLBPath,
	const GLTFFormatData::Root& Root,
	const std::vector<uint8_t>& BinaryBlob
);


} // namespace GS::GLBWriter
