// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Math/GSVector3.h"

#include <string>
#include <vector>


namespace GS
{

/**
 * Single material record from a Wavefront .mtl file. Covers the classic
 * Phong/Blinn fields, the common PBR extensions (Pr/Pm/Ps/...), and the
 * standard texture maps. Defaults match the .mtl spec where applicable.
 *
 * Texture map filenames are stored as written in the file, which is
 * typically a path relative to the .mtl file's directory. Resolving them
 * to absolute paths is left to the caller.
 */
struct GRADIENTSPACEIO_API MTLMaterial
{
	std::string MaterialName;

	// Phong/Blinn color terms
	Vector3f Ka = Vector3f(0.2f, 0.2f, 0.2f);   // ambient
	Vector3f Kd = Vector3f(0.8f, 0.8f, 0.8f);   // diffuse
	Vector3f Ks = Vector3f(0.0f, 0.0f, 0.0f);   // specular
	Vector3f Ke = Vector3f(0.0f, 0.0f, 0.0f);   // emissive
	Vector3f Tf = Vector3f(1.0f, 1.0f, 1.0f);   // transmission filter

	float Ns = 0.0f;        // specular exponent (shininess), typ. 0..1000
	float Ni = 1.0f;        // optical density (index of refraction)
	float d  = 1.0f;        // dissolve (1 = opaque, 0 = fully transparent)
	int   illum = 2;        // illumination model (0..10)

	// PBR extensions (PBR-MTL convention used by many DCC tools)
	float Pr = 0.5f;        // roughness
	float Pm = 0.0f;        // metallic
	float Ps = 0.0f;        // sheen
	float Pc = 0.0f;        // clearcoat thickness
	float Pcr = 0.0f;       // clearcoat roughness
	float Aniso = 0.0f;     // anisotropy
	float AnisoR = 0.0f;    // anisotropy rotation

	// flags indicating which fields were explicitly set in the file
	// (useful to distinguish "not specified" from "set to default")
	bool bHas_Ka = false;
	bool bHas_Kd = false;
	bool bHas_Ks = false;
	bool bHas_Ke = false;
	bool bHas_Tf = false;
	bool bHas_Ns = false;
	bool bHas_Ni = false;
	bool bHas_d  = false;
	bool bHas_illum = false;
	bool bHas_Pr = false;
	bool bHas_Pm = false;
	bool bHas_Ps = false;
	bool bHas_Pc = false;
	bool bHas_Pcr = false;
	bool bHas_Aniso = false;
	bool bHas_AnisoR = false;

	// Texture maps. Empty string means the map was not specified.
	// Stored as the last token of the corresponding map_* line, which
	// for simple .mtl files is the texture filename. (Map options like
	// "-clamp on", "-bm 1.0", etc. are not currently parsed.)
	std::string map_Ka;     // ambient map
	std::string map_Kd;     // diffuse / albedo map
	std::string map_Ks;     // specular map
	std::string map_Ke;     // emissive map
	std::string map_Ns;     // specular highlight map
	std::string map_d;      // alpha / opacity map
	std::string map_bump;   // bump map (also "bump" / "map_Bump")
	std::string map_disp;   // displacement map
	std::string map_decal;  // decal map
	std::string map_refl;   // reflection map
	std::string map_norm;   // normal map ("norm" or "map_norm")

	// PBR map extensions
	std::string map_Pr;     // roughness map
	std::string map_Pm;     // metallic map
	std::string map_Ps;     // sheen map
};


/**
 * Parsed contents of a Wavefront .mtl file.
 */
struct GRADIENTSPACEIO_API MTLFormatData
{
	std::vector<std::string> HeaderComments;
	std::vector<MTLMaterial> Materials;
};


};
