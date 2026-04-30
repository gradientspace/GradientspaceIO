// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/OBJReader.h"

#include <fstream>
#include <set>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <stdio.h>

#include "MeshIO/parse_utils.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // disable secure-crt warnings for this file
#endif

using namespace GS;



struct OBJParsingState
{
	int CurrentGroupID = 0;

	// material currently active (index into OBJFormatData::Materials),
	// or OBJFace::NO_MATERIAL_ASSIGNED if no usemtl has been seen yet
	int32_t CurrentMaterialID = OBJFace::NO_MATERIAL_ASSIGNED;
	// map from material name to its index in OBJFormatData::Materials,
	// used to dedupe across multiple usemtl references to the same name
	std::unordered_map<std::string, int32_t> MaterialNameToIndex;

	bool bHaveMeshmixerGroupIDs = false;
};


// returns char* ptr to character after next space, and index
static char* find_after_next_space(char* String, int* first_space_index = nullptr)
{
	int i = 0;
	while ( !is_line_space(String[i]) && String[i] != null_char )
	{
		i++;
	}
	if (first_space_index != nullptr)
		*first_space_index = i;

	while (is_line_space(String[i]))
		i++;
	if (String[i] == null_char) 
		return nullptr;

	return (String + i);
}


static void append_vertex(char* String, GS::OBJFormatData& Data)
{
	char* coord_x = find_after_next_space(String);
	char* coord_y = find_after_next_space(coord_x);
	char* coord_z = find_after_next_space(coord_y);
	double X = (coord_x != nullptr) ? std::strtod(coord_x, nullptr) : 0;
	double Y = (coord_y != nullptr) ? std::strtod(coord_y, nullptr) : 0;
	double Z = (coord_z != nullptr) ? std::strtod(coord_z, nullptr) : 0;

	Data.VertexPositions.add(Vector3d(X, Y, Z));

	char* color_r = find_after_next_space(coord_z);
	if (color_r != nullptr)
	{
		char* color_g = find_after_next_space(color_r);
		char* color_b = find_after_next_space(color_g);
		float R = (color_r != nullptr) ? std::strtof(color_r, nullptr) : 0;
		float G = (color_g != nullptr) ? std::strtof(color_g, nullptr) : 0;
		float B = (color_b != nullptr) ? std::strtof(color_b, nullptr) : 0;
		Data.VertexColors.add(Vector3f(R, G, B));
	}
}

static void append_normal(char* String, GS::OBJFormatData& Data)
{
	char* coord_nx = find_after_next_space(String);
	char* coord_ny = find_after_next_space(coord_nx);
	char* coord_nz = find_after_next_space(coord_ny);
	double NX = (coord_nx != nullptr) ? std::strtod(coord_nx, nullptr) : 0;
	double NY = (coord_ny != nullptr) ? std::strtod(coord_ny, nullptr) : 0;
	double NZ = (coord_nz != nullptr) ? std::strtod(coord_nz, nullptr) : 0;

	Data.Normals.add(Vector3d(NX, NY, NZ));
}

static void append_uv(char* String, GS::OBJFormatData& Data)
{
	char* coord_u = find_after_next_space(String);
	char* coord_v = find_after_next_space(coord_u);
	//char* coord_z = find_after_next_space(coord_y);
	double U = (coord_u != nullptr) ? std::strtod(coord_u, nullptr) : 0;
	double V = (coord_v != nullptr) ? std::strtod(coord_v, nullptr) : 0;
	//double Z = (coord_z != nullptr) ? std::strtod(coord_z, nullptr) : 0;

	Data.UVs.add(Vector2d(U, V));
}


static int index_of_nonspace(const char* String, int start_index, char c)
{
	int cur_index = start_index;
	//bool bFound = false;
	while (1)
	{
		if (String[cur_index] == c)
			return cur_index;
		cur_index++;
		if (isspace(String[cur_index]) || String[cur_index] == null_char)
			return -1;
	}
}

static int index_of_substring(const char* String, const char* Substring)
{
	// arbitrary max...
	int M = (int)strnlen(Substring, 1024);
	if (M >= 1023) return -1;
	int i = 0;
	while (String[i] != null_char)
	{
		if (String[i] == Substring[0])
		{
			int j = 1, iStart = i;
			i++;
			while (String[i] == Substring[j])
			{
				j++;
				i++;
				if (j == M)
					return iStart;
			}

		}
		else
			i++;
	}
	return -1;
}

struct OBJParsedFace
{
	std::vector<int> PositionIndices;
	std::vector<int> NormalIndices;
	std::vector<int> UVIndices;
};

static void parse_face(char* String, OBJParsedFace& Face)
{
	Face.PositionIndices.resize(0);
	Face.NormalIndices.resize(0);
	Face.UVIndices.resize(0);

	// vertex group is either just an integer vertex index, or vertidx//normalidx, or vertidx/uvidx, or vertidx/uvidx/normalidx
	char* next_vertex_group = find_after_next_space(String);
	while (next_vertex_group != nullptr)
	{
		int first_space_index = -1;
		char* nextnext = find_after_next_space(next_vertex_group, &first_space_index);

		// insert null at first space character to terminate substring
		next_vertex_group[first_space_index] = null_char;

		int start_index = 0;
		int first_slash_index = index_of_nonspace(next_vertex_group, start_index, '/');
		if (first_slash_index == -1)
		{
			int VertexIndex = std::atoi(next_vertex_group);
			Face.PositionIndices.push_back(VertexIndex);
		}
		else if (next_vertex_group[first_slash_index+1] == '/')
		{
			// have two slashes in a row, null-terminate the first substring and parse, and then parse the second
			next_vertex_group[first_slash_index] = null_char;
			int VertexIndex = std::atoi(next_vertex_group);
			char* normal_token = &next_vertex_group[first_slash_index + 2];
			int NormalIndex = std::atoi(normal_token);
			Face.PositionIndices.push_back(VertexIndex);
			Face.NormalIndices.push_back(NormalIndex);
		}
		else
		{
			int second_slash_index = index_of_nonspace(next_vertex_group, first_slash_index + 1, '/');

			next_vertex_group[first_slash_index] = null_char;
			int VertexIndex = std::atoi(next_vertex_group);
			Face.PositionIndices.push_back(VertexIndex);

			char* uv_token = &next_vertex_group[first_slash_index+1];

			if (second_slash_index == -1)
			{
				int UVIndex = std::atoi(uv_token);
				Face.UVIndices.push_back(UVIndex);
			}
			else
			{
				next_vertex_group[second_slash_index] = null_char;
				int UVIndex = std::atoi(uv_token);
				Face.UVIndices.push_back(UVIndex);
				char* normal_token = &next_vertex_group[second_slash_index + 1];
				int NormalIndex = std::atoi(normal_token);
				Face.NormalIndices.push_back(NormalIndex);
			}
		}

		next_vertex_group = nextnext;
	}


	if (Face.NormalIndices.size() != Face.PositionIndices.size())
		Face.NormalIndices.resize(0);
	if (Face.UVIndices.size() != Face.PositionIndices.size())
		Face.UVIndices.resize(0);
}



static void process_comment(
	char* CommentString, int Length, OBJParsingState& ParsingState)
{
	int mmgid_index = index_of_substring(CommentString, "mm_gid");
	if (mmgid_index >= 0)
	{
		char* groupid_token = find_after_next_space( &CommentString[mmgid_index] );
		int GroupID = std::atoi(groupid_token);
		ParsingState.bHaveMeshmixerGroupIDs = true;
		ParsingState.CurrentGroupID = GroupID;
	}
}



bool GS::OBJReader::ReadOBJ(
	const std::string& Path,
	OBJFormatData& OBJDataOut,
	const ReadOptions& Options )
{
	std::filesystem::path FilePath(Path);
	if (!std::filesystem::exists(FilePath))
		return false;

	// why is this using FILE* api...?

	FILE* FilePtr = fopen(Path.c_str(), "r");
	if (!FilePtr)
		return false;

	std::vector<char> LineBuffer;
	const int BufferSize = 4096;
	LineBuffer.resize(BufferSize);

	// face that we will parse into
	OBJParsedFace CurFace;
	CurFace.PositionIndices.reserve(32);
	CurFace.NormalIndices.reserve(32);
	CurFace.UVIndices.reserve(32);

	// current parsing info
	OBJParsingState ParsingState;

	bool bMayBeInFileHeader = true;
	bool bLastLineReadOK = true;
	while ( bLastLineReadOK && !feof(FilePtr) )
	{
		char* Result = fgets(&LineBuffer[0], BufferSize-1, FilePtr);
		if (Result == nullptr) { bLastLineReadOK = false; continue; }

		char* String = &LineBuffer[0];
		int N = (int)strnlen(String, BufferSize);
		if (N == 0) continue;		// empty line?
		if (String[N] != null_char)
		{
			// this would happen if line is longer than buffer...
			gs_runtime_assert(false);
			bLastLineReadOK = false; continue;
		}

		trim_start_end_in_place(String, N);
		if (N == 0) continue;

		if (String[0] == '#' || String[0] == '/') {
			if (bMayBeInFileHeader) {
				OBJDataOut.HeaderComments.push_back(std::string(String));
			}
			process_comment(String, N, ParsingState);
			continue;
		}

		// if the line wasn't a comment it must be a contents line, so header is done...
		bMayBeInFileHeader = false;

		if (String[0] == 'v')
		{
			if (String[1] == 'n')
			{
				append_normal(String, OBJDataOut);
			}
			else if (String[1] == 't')
			{
				append_uv(String, OBJDataOut);
			}
			else
			{
				append_vertex(String, OBJDataOut);
			}
		}
		else if (String[0] == 'f')
		{
			parse_face(String, CurFace);
			if (CurFace.PositionIndices.size() == 3)
			{
				uint32_t tri_index = (uint32_t)OBJDataOut.Triangles.size();

				OBJTriangle NewTri;
				NewTri.Positions = Index3i(CurFace.PositionIndices[0]-1, CurFace.PositionIndices[1]-1, CurFace.PositionIndices[2]-1);
				if (CurFace.NormalIndices.size() == 3)
					NewTri.Normals = Index3i(CurFace.NormalIndices[0]-1, CurFace.NormalIndices[1]-1, CurFace.NormalIndices[2]-1);
				if (CurFace.UVIndices.size() == 3)
					NewTri.UVs = Index3i(CurFace.UVIndices[0]-1, CurFace.UVIndices[1]-1, CurFace.UVIndices[2]-1);
				OBJDataOut.Triangles.add(NewTri);

				OBJFace NewFace;
				NewFace.FaceType = 0;
				NewFace.FaceIndex = tri_index;
				NewFace.GroupID = ParsingState.CurrentGroupID;
				NewFace.MaterialID = ParsingState.CurrentMaterialID;
				OBJDataOut.FaceStream.add(NewFace);
			}
			else if (CurFace.PositionIndices.size() == 4)
			{
				uint32_t quad_index = (uint32_t)OBJDataOut.Quads.size();

				OBJQuad NewQuad;
				NewQuad.Positions = Index4i(CurFace.PositionIndices[0]-1, CurFace.PositionIndices[1]-1, CurFace.PositionIndices[2]-1, CurFace.PositionIndices[3]-1);
				if (CurFace.NormalIndices.size() == 4)
					NewQuad.Normals = Index4i(CurFace.NormalIndices[0]-1, CurFace.NormalIndices[1]-1, CurFace.NormalIndices[2]-1, CurFace.NormalIndices[3]-1);
				if (CurFace.UVIndices.size() == 4)
					NewQuad.UVs = Index4i(CurFace.UVIndices[0]-1, CurFace.UVIndices[1]-1, CurFace.UVIndices[2]-1, CurFace.UVIndices[3]-1);
				OBJDataOut.Quads.add(NewQuad);

				OBJFace NewFace;
				NewFace.FaceType = 1;
				NewFace.FaceIndex = quad_index;
				NewFace.GroupID = ParsingState.CurrentGroupID;
				NewFace.MaterialID = ParsingState.CurrentMaterialID;
				OBJDataOut.FaceStream.add(NewFace);
			}
			else if (CurFace.PositionIndices.size() > 4)
			{
				uint32_t poly_index = (uint32_t)OBJDataOut.Polygons.size();

				int NV = (int)CurFace.PositionIndices.size();
				OBJPolygon NewPoly;
				NewPoly.Positions.resize(NV);
				for ( int j = 0; j < NV; ++j )
					NewPoly.Positions[j] = CurFace.PositionIndices[j]-1;
				
				bool bPolyNormalsValid = (CurFace.NormalIndices.size() == NV);
				NewPoly.Normals.resize(NV);
				for ( int j = 0; j < NV; ++j )
					NewPoly.Normals[j] = (bPolyNormalsValid) ? (CurFace.NormalIndices[j]-1) : 0;

				bool bPolyUVsValid = (CurFace.UVIndices.size() == NV);
				NewPoly.UVs.resize(NV);
				for (int j = 0; j < NV; ++j)
					NewPoly.UVs[j] = (bPolyUVsValid) ? CurFace.UVIndices[j]-1 : 0;

				OBJDataOut.Polygons.add(std::move(NewPoly));

				OBJFace NewFace;
				NewFace.FaceType = 2;
				NewFace.FaceIndex = poly_index;
				NewFace.GroupID = ParsingState.CurrentGroupID;
				NewFace.MaterialID = ParsingState.CurrentMaterialID;
				OBJDataOut.FaceStream.add(NewFace);
			}
		}
		else if (String[0] == 'g')
		{


			// todo...
			ParsingState.CurrentGroupID++;
		}
		else if (String[0] == 'm'
			&& strncmp(String, "mtllib", 6) == 0
			&& (String[6] == null_char || is_line_space(String[6])))
		{
			// "mtllib file1.mtl file2.mtl ..." — collect each space-separated
			// token as a filename. Multiple mtllib lines accumulate.
			char* token = find_after_next_space(String);
			while (token != nullptr)
			{
				int first_space_index = -1;
				char* nextnext = find_after_next_space(token, &first_space_index);
				// null-terminate this token at its trailing whitespace
				token[first_space_index] = null_char;
				if (token[0] != null_char)
					OBJDataOut.MTLLibs.push_back(std::string(token));
				token = nextnext;
			}
		}
		else if (String[0] == 'u'
			&& strncmp(String, "usemtl", 6) == 0
			&& (String[6] == null_char || is_line_space(String[6])))
		{
			// "usemtl <name>" — switch the active material. Allocate a new
			// linear index the first time a name is seen; reuse on repeats.
			char* name_token = find_after_next_space(String);
			if (name_token != nullptr)
			{
				int first_space_index = -1;
				find_after_next_space(name_token, &first_space_index);
				name_token[first_space_index] = null_char;
				if (name_token[0] != null_char)
				{
					std::string MatName(name_token);
					auto Found = ParsingState.MaterialNameToIndex.find(MatName);
					if (Found != ParsingState.MaterialNameToIndex.end())
					{
						ParsingState.CurrentMaterialID = Found->second;
					}
					else
					{
						int32_t NewIndex = (int32_t)OBJDataOut.Materials.size();
						OBJMaterial NewMat;
						NewMat.MaterialName = MatName;
						OBJDataOut.Materials.add(NewMat);
						ParsingState.MaterialNameToIndex.emplace(std::move(MatName), NewIndex);
						ParsingState.CurrentMaterialID = NewIndex;
					}
				}
			}
		}
	}

	// currently not supporting partial color specification
	if (OBJDataOut.VertexColors.size() != OBJDataOut.VertexPositions.size())
		OBJDataOut.VertexColors.clear();


	fclose(FilePtr);
	return true;
}




// ===== MTL parsing =====

// Read a single float from a "<keyword> <value>" line. Returns 0 if parse fails.
static float mtl_parse_float(char* String)
{
	char* val = find_after_next_space(String);
	return (val != nullptr) ? std::strtof(val, nullptr) : 0.0f;
}

// Read three floats from a "<keyword> r g b" line. Missing components default to 0.
static Vector3f mtl_parse_color3(char* String)
{
	char* r = find_after_next_space(String);
	char* g = (r != nullptr) ? find_after_next_space(r) : nullptr;
	char* b = (g != nullptr) ? find_after_next_space(g) : nullptr;
	float R = (r != nullptr) ? std::strtof(r, nullptr) : 0.0f;
	float G = (g != nullptr) ? std::strtof(g, nullptr) : 0.0f;
	float B = (b != nullptr) ? std::strtof(b, nullptr) : 0.0f;
	return Vector3f(R, G, B);
}

// Skip past option arguments on a map_* line (-clamp, -bm, -s, -o, etc.) and
// return a pointer to the texture filename token. The .mtl spec allows
// option flags before the filename, e.g. "map_Kd -clamp on -bm 1.0 file.png".
// We treat any token starting with '-' as an option, consuming one extra
// value token for known single-value options and three for vector options.
static char* mtl_find_map_filename(char* String)
{
	char* token = find_after_next_space(String);
	while (token != nullptr && token[0] == '-')
	{
		// option flag — figure out how many value tokens to skip
		int num_skip = 1;
		if (strncmp(token, "-s", 2) == 0 || strncmp(token, "-o", 2) == 0 || strncmp(token, "-t", 2) == 0)
			num_skip = 3; // -s/-o/-t take 3 floats (u,v,w)
		else if (strncmp(token, "-mm", 3) == 0)
			num_skip = 2; // -mm base gain
		// step past the option flag itself
		token = find_after_next_space(token);
		for (int i = 0; i < num_skip && token != nullptr; ++i)
			token = find_after_next_space(token);
	}
	return token;
}

// Null-terminate a token at the first trailing whitespace and return the token.
static std::string mtl_token_to_string(char* token)
{
	if (token == nullptr) return std::string();
	int i = 0;
	while (token[i] != null_char && !is_line_space(token[i]))
		i++;
	return std::string(token, token + i);
}

static std::string mtl_parse_map_filename(char* String)
{
	return mtl_token_to_string(mtl_find_map_filename(String));
}

// Match a keyword at the start of a line, where the next char must be whitespace
// or end-of-string. Avoids "Ka" matching "Kaboom" or "map_Kd" being seen as "map".
static bool mtl_keyword_match(const char* String, const char* Keyword)
{
	int i = 0;
	while (Keyword[i] != null_char)
	{
		if (String[i] != Keyword[i]) return false;
		i++;
	}
	char next = String[i];
	return next == null_char || is_line_space(next);
}


bool GS::OBJReader::ReadMTL(
	const std::string& Path,
	MTLFormatData& MTLDataOut)
{
	std::filesystem::path FilePath(Path);
	if (!std::filesystem::exists(FilePath))
		return false;

	FILE* FilePtr = fopen(Path.c_str(), "r");
	if (!FilePtr)
		return false;

	std::vector<char> LineBuffer;
	const int BufferSize = 4096;
	LineBuffer.resize(BufferSize);

	MTLMaterial* Cur = nullptr;
	bool bMayBeInFileHeader = true;
	bool bLastLineReadOK = true;
	while (bLastLineReadOK && !feof(FilePtr))
	{
		char* Result = fgets(&LineBuffer[0], BufferSize - 1, FilePtr);
		if (Result == nullptr) { bLastLineReadOK = false; continue; }

		char* String = &LineBuffer[0];
		int N = (int)strnlen(String, BufferSize);
		if (N == 0) continue;
		if (String[N] != null_char) {
			gs_runtime_assert(false);
			bLastLineReadOK = false; continue;
		}
		trim_start_end_in_place(String, N);
		if (N == 0) continue;

		if (String[0] == '#' || String[0] == '/') {
			if (bMayBeInFileHeader)
				MTLDataOut.HeaderComments.push_back(std::string(String));
			continue;
		}
		bMayBeInFileHeader = false;

		// newmtl <name> — start a new material record
		if (mtl_keyword_match(String, "newmtl")) {
			char* name_token = find_after_next_space(String);
			MTLMaterial NewMat;
			NewMat.MaterialName = mtl_token_to_string(name_token);
			MTLDataOut.Materials.push_back(std::move(NewMat));
			Cur = &MTLDataOut.Materials.back();
			continue;
		}

		// any subsequent line requires an active material
		if (Cur == nullptr)
			continue;

		// scalar / color fields
		if (mtl_keyword_match(String, "Ka")) {
			Cur->Ka = mtl_parse_color3(String); Cur->bHas_Ka = true;
		}
		else if (mtl_keyword_match(String, "Kd")) {
			Cur->Kd = mtl_parse_color3(String); Cur->bHas_Kd = true;
		}
		else if (mtl_keyword_match(String, "Ks")) {
			Cur->Ks = mtl_parse_color3(String); Cur->bHas_Ks = true;
		}
		else if (mtl_keyword_match(String, "Ke")) {
			Cur->Ke = mtl_parse_color3(String); Cur->bHas_Ke = true;
		}
		else if (mtl_keyword_match(String, "Tf")) {
			Cur->Tf = mtl_parse_color3(String); Cur->bHas_Tf = true;
		}
		else if (mtl_keyword_match(String, "Ns")) {
			Cur->Ns = mtl_parse_float(String); Cur->bHas_Ns = true;
		}
		else if (mtl_keyword_match(String, "Ni")) {
			Cur->Ni = mtl_parse_float(String); Cur->bHas_Ni = true;
		}
		else if (mtl_keyword_match(String, "d")) {
			Cur->d = mtl_parse_float(String); Cur->bHas_d = true;
		}
		else if (mtl_keyword_match(String, "Tr")) {
			// Tr is the inverse of d; convert so consumers only need to look at d
			Cur->d = 1.0f - mtl_parse_float(String); Cur->bHas_d = true;
		}
		else if (mtl_keyword_match(String, "illum")) {
			char* val = find_after_next_space(String);
			Cur->illum = (val != nullptr) ? std::atoi(val) : 0;
			Cur->bHas_illum = true;
		}
		// PBR scalar extensions
		else if (mtl_keyword_match(String, "Pr")) {
			Cur->Pr = mtl_parse_float(String); Cur->bHas_Pr = true;
		}
		else if (mtl_keyword_match(String, "Pm")) {
			Cur->Pm = mtl_parse_float(String); Cur->bHas_Pm = true;
		}
		else if (mtl_keyword_match(String, "Ps")) {
			Cur->Ps = mtl_parse_float(String); Cur->bHas_Ps = true;
		}
		else if (mtl_keyword_match(String, "Pc")) {
			Cur->Pc = mtl_parse_float(String); Cur->bHas_Pc = true;
		}
		else if (mtl_keyword_match(String, "Pcr")) {
			Cur->Pcr = mtl_parse_float(String); Cur->bHas_Pcr = true;
		}
		else if (mtl_keyword_match(String, "aniso")) {
			Cur->Aniso = mtl_parse_float(String); Cur->bHas_Aniso = true;
		}
		else if (mtl_keyword_match(String, "anisor")) {
			Cur->AnisoR = mtl_parse_float(String); Cur->bHas_AnisoR = true;
		}
		// texture maps
		else if (mtl_keyword_match(String, "map_Ka")) {
			Cur->map_Ka = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Kd")) {
			Cur->map_Kd = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Ks")) {
			Cur->map_Ks = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Ke")) {
			Cur->map_Ke = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Ns")) {
			Cur->map_Ns = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_d")) {
			Cur->map_d = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_bump") || mtl_keyword_match(String, "map_Bump") || mtl_keyword_match(String, "bump")) {
			Cur->map_bump = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "disp")) {
			Cur->map_disp = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "decal")) {
			Cur->map_decal = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "refl")) {
			Cur->map_refl = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "norm") || mtl_keyword_match(String, "map_norm") || mtl_keyword_match(String, "map_Norm")) {
			Cur->map_norm = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Pr")) {
			Cur->map_Pr = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Pm")) {
			Cur->map_Pm = mtl_parse_map_filename(String);
		}
		else if (mtl_keyword_match(String, "map_Ps")) {
			Cur->map_Ps = mtl_parse_map_filename(String);
		}
	}

	fclose(FilePtr);
	return true;
}


#if defined(_MSC_VER)
#pragma warning(pop)
#endif