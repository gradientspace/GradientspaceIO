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

	//! if true, triangle orientation will be inverted on write (by swapping A and B in each tri)
	bool bReverseTriOrientation = false;
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
