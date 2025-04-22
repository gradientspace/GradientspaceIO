// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"
#include "MeshIO/OBJFormatData.h"

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


}  // end namespace GS::OBJReader
