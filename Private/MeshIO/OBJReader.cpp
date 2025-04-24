// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/OBJReader.h"

#include <fstream>
#include <set>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#include <filesystem>
#include <stdio.h>

using namespace GS;


constexpr char null_char = '\0';


struct OBJParsingState
{
	int CurrentGroupID = 0;

	bool bHaveMeshmixerGroupIDs = false;
};


static bool is_line_space(char c)
{
	return c == ' ' || c == '\t';
}

static void trim_start_end_in_place(char*& String, int& N)
{
	if (String[N - 1] == '\n') {
		N--;
		String[N] = null_char;
	}
	if (String[N - 1] == '\r') {
		N--;
		String[N] = null_char;
	}
	// trim empty space
	while ( is_line_space(String[0]) && N > 0)
	{
		String++;
		N--;
	}
}

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
	int M = (int)strnlen_s(Substring, 1024);
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

	FILE* FilePtr = nullptr;
	[[maybe_unused]] errno_t error = fopen_s(&FilePtr, Path.c_str(), "r");
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
		int N = (int)strnlen_s(String, BufferSize);
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
				OBJDataOut.FaceStream.add(NewFace);
			}
		}
		else if (String[0] == 'g')
		{


			// todo...
			ParsingState.CurrentGroupID++;
		}
	}

	// currently not supporting partial color specification
	if (OBJDataOut.VertexColors.size() != OBJDataOut.VertexPositions.size())
		OBJDataOut.VertexColors.clear();


	fclose(FilePtr);
	return true;
}
