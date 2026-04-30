// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/OBJFormatData.h"
#include "MeshIO/MTLFormatData.h"

#include <string>
#include <vector>


namespace GS::OBJReader
{

struct GRADIENTSPACEIO_API ReadOptions
{
	bool bVertexColors = true;
	bool bNormals = true;
	bool bUVs = true;

	bool bEnableMeshmixerTriGroupProcessing = true;
};


GRADIENTSPACEIO_API
bool ReadOBJ(
	const std::string& Path,
	OBJFormatData& OBJDataOut,
	const ReadOptions& Options = ReadOptions()
);


/**
 * Read a Wavefront .mtl material library file. Texture map paths in MTLDataOut
 * are stored verbatim from the file (typically relative to the .mtl directory).
 * Returns false if the file does not exist or cannot be opened.
 */
GRADIENTSPACEIO_API
bool ReadMTL(
	const std::string& Path,
	MTLFormatData& MTLDataOut
);


}  // end namespace GS::OBJReader
