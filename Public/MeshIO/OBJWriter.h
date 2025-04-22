// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Core/TextIO.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/OBJFormatData.h"

#include <string>

namespace GS::OBJWriter
{

struct GRADIENTSPACEIO_API WriteOptions
{
	bool bVertexColors = true;
	bool bNormals = true;
	bool bUVs = true;
};

GRADIENTSPACEIO_API
bool WriteOBJ(
	ITextWriter& TextWriter,
	const DenseMesh& Mesh,
	const WriteOptions& Options = WriteOptions()
);


GRADIENTSPACEIO_API
bool WriteOBJ(
	ITextWriter& TextWriter,
	const OBJFormatData& OBJData,
	const WriteOptions& Options = WriteOptions()
);


}
