// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Core/TextIO.h"
#include "Core/BinaryIO.h"
#include "Mesh/DenseMesh.h"

#include <string>

namespace GS::STLWriter
{


GRADIENTSPACEIO_API
bool WriteSTL(
	const std::string& Filename,
	const DenseMesh& Mesh,
	const std::string& MeshName = "mesh",
	bool bWriteBinary = true
);


GRADIENTSPACEIO_API
bool WriteSTL(
	ITextWriter& TextWriter,
	const DenseMesh& Mesh,
	const std::string& MeshName = "mesh"
);


GRADIENTSPACEIO_API
bool WriteSTL(
	IBinaryWriter& BinaryWriter,
	const DenseMesh& Mesh
);



}