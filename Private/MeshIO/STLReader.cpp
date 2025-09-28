// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/STLReader.h"

#include <vector>
#include <cstdlib>
#include <charconv>

#include <filesystem>

#include "Core/TextIO.h"
#include "Core/BinaryIO.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // disable secure-crt warnings for this file
#endif

using namespace GS;
using namespace GS::STLReader;

static constexpr int MaxLineBufferSize = 4096;
static constexpr int MaxTokenSize = 64;
constexpr char null_char = '\0';

static bool is_line_space(char c) {
	return c == ' ' || c == '\t';
}
static bool is_end_of_line(char c) {
	return c == '\r' || c == '\n' || c == null_char;
}

static bool get_next_token(const char* LineString, int& index, char* tokenOut, int& tokenLen) 
{
	tokenLen = 0;

	// find start of token
	while (is_line_space(LineString[index]) && index < MaxLineBufferSize) 
		index++;
	if (is_end_of_line(LineString[index]))
		return false;

	// extract token until more whitespace or EOL
	while ( !is_line_space(LineString[index]) 
		&& !is_end_of_line(LineString[index])
		&& (tokenLen < (MaxTokenSize-1)) )
	{
		tokenOut[tokenLen++] = LineString[index];
		index++;
	}
	tokenOut[tokenLen] = null_char;

	return true;
}

static bool check_eol(const char* LineString, int index)
{
	return is_end_of_line(LineString[index]);
}

static bool is_same(const std::vector<char>& token, const char* expected)
{
	// should we to-upper??
	return strncmp(token.data(), expected, MaxTokenSize) == 0;
}

enum class ETokenSequence
{
	SOLID = 0,
	FACET = 1,
	NORMAL = 2,
	NORMAL_X = 3, NORMAL_Y = 4, NORMAL_Z = 5,
	OUTER = 6, LOOP = 7,
	VERTEX1 = 8,
	VERTEX1_X = 9, VERTEX1_Y = 10, VERTEX1_Z = 11,
	VERTEX2 = 12,
	VERTEX2_X = 13, VERTEX2_Y = 14, VERTEX2_Z = 15,
	VERTEX3 = 16,
	VERTEX3_X = 17, VERTEX3_Y = 18, VERTEX3_Z = 19,
	ENDLOOP = 20,
	ENDFACET = 21,
	ENDSOLID = 22
};
bool is_float_token(ETokenSequence token)
{
	return (token >= ETokenSequence::NORMAL_X && token <= ETokenSequence::NORMAL_Z)
		|| (token >= ETokenSequence::VERTEX1_X && token <= ETokenSequence::VERTEX1_Z)
		|| (token >= ETokenSequence::VERTEX2_X && token <= ETokenSequence::VERTEX2_Z)
		|| (token >= ETokenSequence::VERTEX3_X && token <= ETokenSequence::VERTEX3_Z);
}
void set_float(STLTriangle& triangle, ETokenSequence token, float value)
{
	switch (token) {
	case ETokenSequence::NORMAL_X: triangle.Normal.X = value; break;
	case ETokenSequence::NORMAL_Y: triangle.Normal.Y = value; break;
	case ETokenSequence::NORMAL_Z: triangle.Normal.Z = value; break;
	case ETokenSequence::VERTEX1_X: triangle.Vertex1.X = value; break;
	case ETokenSequence::VERTEX1_Y: triangle.Vertex1.Y = value; break;
	case ETokenSequence::VERTEX1_Z: triangle.Vertex1.Z = value; break;
	case ETokenSequence::VERTEX2_X: triangle.Vertex2.X = value; break;
	case ETokenSequence::VERTEX2_Y: triangle.Vertex2.Y = value; break;
	case ETokenSequence::VERTEX2_Z: triangle.Vertex2.Z = value; break;
	case ETokenSequence::VERTEX3_X: triangle.Vertex3.X = value; break;
	case ETokenSequence::VERTEX3_Y: triangle.Vertex3.Y = value; break;
	case ETokenSequence::VERTEX3_Z: triangle.Vertex3.Z = value; break;
	default:
		gs_debug_assert(false); // should not be here
		break;
	}
}

static const char* TokenStrings[] = {
	"solid",
	"facet",
	"normal",
	"", "", "",
	"outer",
	"loop",
	"vertex",
	"", "", "",
	"vertex",
	"", "", "",
	"vertex",
	"", "", "",
	"endloop",
	"endfacet",
	"endsolid"
};




static bool ReadSTL_Ascii(
	ITextReader& Reader,
	STLMeshData& STLMeshOut)
{
	std::vector<char> LineBuffer;
	LineBuffer.resize(MaxLineBufferSize);
	int cur_index = -1;
	std::vector<char> Token;
	Token.resize(MaxTokenSize);

	ETokenSequence NextToken = ETokenSequence::SOLID;
	STLTriangle CurTriangle;

	// read initial line
	bool bNextLineNeeded = true;
	bool bDoneFile = false;
	int Error = 0;
	while (!Reader.IsEndOfFile() && !bDoneFile)
	{
		if (bNextLineNeeded) {
			if ( Reader.ReadLine(&LineBuffer[0], MaxLineBufferSize - 1) == false) {
				bDoneFile = false; 
				continue; 
			}
			cur_index = 0;
			bNextLineNeeded = false;
		}

		int tokenLen = 0;
		if (NextToken == ETokenSequence::SOLID) {
			if (get_next_token(LineBuffer.data(), cur_index, Token.data(), tokenLen) == false) { 
				Error = 1; break; 
			}
			if (is_same(Token, TokenStrings[(int)NextToken]) == false) {
				Error = 2; break; 
			}
			NextToken = ETokenSequence::FACET;
			// skip rest of line
			bNextLineNeeded = true;
			continue;
		}

		// extract next token string
		if (get_next_token(LineBuffer.data(), cur_index, Token.data(), tokenLen) == false)
			{ bNextLineNeeded = true; continue; }

		// if next token is a float, parse it and set in triangle
		if (is_float_token(NextToken)) {
			float f = 0;
			std::from_chars_result result = std::from_chars(Token.data(), Token.data() + tokenLen, f);
			if (result.ec != std::errc()) {
				Error = 3; break;
			}
			set_float(CurTriangle, NextToken, f);
			NextToken = (ETokenSequence)((int)NextToken + 1);
			bNextLineNeeded = check_eol(LineBuffer.data(), cur_index);
			continue;
		}

		// if next token is FACET but we got ENDSOLID, we are done
		if (NextToken == ETokenSequence::FACET && is_same(Token, TokenStrings[(int)ETokenSequence::ENDSOLID])) {
			NextToken = ETokenSequence::ENDSOLID;
			bDoneFile = true;
			continue;
		}

		// otherwise make sure we got the string we are expecting
		if (is_same(Token, TokenStrings[(int)NextToken]) == false) {
			Error = 3; break;
		}

		// if we finished a triangle, save it
		if (NextToken == ETokenSequence::ENDFACET) {
			STLMeshOut.Triangles.push_back(CurTriangle);
			CurTriangle = STLTriangle();
		}

		if (NextToken == ETokenSequence::ENDFACET) {
			NextToken = ETokenSequence::FACET;
		} else {
			NextToken = (ETokenSequence)((int)NextToken + 1);
		}
		bNextLineNeeded = check_eol(LineBuffer.data(), cur_index);
	}

	// todo verify that we got ENDSOLID??

	return true;
}



static bool ReadSTL_Binary(
	IBinaryReader& Reader,
	STLMeshData& STLMeshOut)
{
	bool bIncomplete = false;

	STLMeshOut.Header.resize(80);
	Reader.ReadBytes(STLMeshOut.Header.data(), 80);

	uint32_t numTriangles = 0;
	Reader.ReadBytes(&numTriangles, 4);

	STLMeshOut.Triangles.resize(numTriangles);
	for (uint32_t tid = 0; tid < numTriangles; ++tid)
	{
		STLTriangle& tri = STLMeshOut.Triangles[tid];
		Reader.ReadBytes(&tri.Normal, 4 * 3);
		Reader.ReadBytes(&tri.Vertex1, 4 * 3);
		Reader.ReadBytes(&tri.Vertex2, 4 * 3);
		Reader.ReadBytes(&tri.Vertex3, 4 * 3);
		Reader.ReadBytes(&tri.Attribute, 2);

		if (Reader.IsEndOfFile()) {
			if (tid != numTriangles)
				bIncomplete = true;
			break;
		}
	}

	// indicate some error if we got bIncomplete?

	return true;
}


bool GS::STLReader::ReadSTL(
	const std::string& Path,
	STLMeshData& STLMeshOut)
{
	std::filesystem::path FilePath(Path);
	if (!std::filesystem::exists(FilePath))
		return false;

	// create temporary binary reader to figure out binary or ascii
	FileBinaryReader binaryReader = FileBinaryReader::OpenFile(Path);
	if (!binaryReader)
		return false;	
	
	char header[7];
	binaryReader.ReadBytes(header, 5);
	header[5] = null_char;
	bool bIsAscii = (strncmp(header, "solid", 5) == 0);

	if (bIsAscii) {
		binaryReader.CloseFile();
		FileTextReader textReader = FileTextReader::OpenFile(Path);
		if (!textReader)
			return false;
		return ReadSTL_Ascii(textReader, STLMeshOut);
	} else {
		binaryReader.SetPosition(0);			
		return ReadSTL_Binary(binaryReader, STLMeshOut);
	}

	//// why is this using FILE* api...?
	//FILE* FilePtr = fopen(Path.c_str(), "r");
	//if (!FilePtr)
	//	return false;

	//std::vector<char> LineBuffer;
	//LineBuffer.resize(MaxLineBufferSize);
	//int cur_index = -1;
	//std::vector<char> Token;
	//Token.resize(MaxTokenSize);

	//ETokenSequence NextToken = ETokenSequence::SOLID;
	//STLTriangle CurTriangle;

	//// read initial line
	//bool bNextLineNeeded = true;
	//bool bDoneFile = false;
	//int Error = 0;
	//while (!feof(FilePtr) && !bDoneFile)
	//{
	//	if (bNextLineNeeded) {
	//		char* Result = fgets(&LineBuffer[0], MaxLineBufferSize - 1, FilePtr);
	//		if (Result == nullptr) { bDoneFile = false; continue; }
	//		cur_index = 0;
	//		bNextLineNeeded = false;
	//	}

	//	int tokenLen = 0;
	//	if (NextToken == ETokenSequence::SOLID) {
	//		if (get_next_token(LineBuffer.data(), cur_index, Token.data(), tokenLen) == false) { 
	//			Error = 1; break; 
	//		}
	//		if (is_same(Token, TokenStrings[(int)NextToken]) == false) {
	//			Error = 2; break; 
	//		}
	//		NextToken = ETokenSequence::FACET;
	//		// skip rest of line
	//		bNextLineNeeded = true;
	//		continue;
	//	}

	//	// extract next token string
	//	if (get_next_token(LineBuffer.data(), cur_index, Token.data(), tokenLen) == false)
	//		{ bNextLineNeeded = true; continue; }

	//	// if next token is a float, parse it and set in triangle
	//	if (is_float_token(NextToken)) {
	//		float f = 0;
	//		std::from_chars_result result = std::from_chars(Token.data(), Token.data() + tokenLen, f);
	//		if (result.ec != std::errc()) {
	//			Error = 3; break;
	//		}
	//		set_float(CurTriangle, NextToken, f);
	//		NextToken = (ETokenSequence)((int)NextToken + 1);
	//		bNextLineNeeded = check_eol(LineBuffer.data(), cur_index);
	//		continue;
	//	}

	//	// otherwise make sure we got the string we are expecting
	//	if (is_same(Token, TokenStrings[(int)NextToken]) == false) {
	//		Error = 3; break;
	//	}

	//	// if we finished a triangle, save it
	//	if (NextToken == ETokenSequence::ENDFACET) {
	//		STLMeshOut.Triangles.push_back(CurTriangle);
	//		CurTriangle = STLTriangle();
	//	}

	//	if (NextToken == ETokenSequence::ENDFACET) {
	//		NextToken = ETokenSequence::FACET;
	//	} else {
	//		NextToken = (ETokenSequence)((int)NextToken + 1);
	//	}
	//	bNextLineNeeded = check_eol(LineBuffer.data(), cur_index);
	//}

	//return true;
 }





void GS::STLReader::STLMeshToDenseMesh(const STLMeshData& STLMesh, DenseMesh& MeshOut)
{
	int NumTriangles = (int)STLMesh.Triangles.size();
	int NumVertices = NumTriangles * 3;

	MeshOut.Resize(NumVertices, NumTriangles);

	for (int tid = 0; tid < NumTriangles; ++tid)
	{
		MeshOut.SetPosition(3*tid, (Vector3d)STLMesh.Triangles[tid].Vertex1);
		MeshOut.SetPosition(3*tid+1, (Vector3d)STLMesh.Triangles[tid].Vertex2);
		MeshOut.SetPosition(3*tid+2, (Vector3d)STLMesh.Triangles[tid].Vertex3);

		// todo - save these normals?
		//MeshOut.SetTriVtxNormals();

		MeshOut.SetTriangle(tid, Index3i(3*tid, 3*tid+1, 3*tid+2));
	}
}



#if defined(_MSC_VER)
#pragma warning(pop)
#endif