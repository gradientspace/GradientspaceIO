// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLTFReader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


using namespace GS;
using namespace GS::GLTFFormatData;


namespace
{

bool read_file_to_string(const std::string& Path, std::string& Out)
{
	std::ifstream in(Path, std::ios::binary);
	if (!in.is_open())
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	Out = ss.str();
	return true;
}

bool read_file_to_bytes(const std::string& Path, std::vector<uint8_t>& Out)
{
	std::ifstream in(Path, std::ios::binary | std::ios::ate);
	if (!in.is_open())
		return false;
	std::streamsize Size = in.tellg();
	in.seekg(0, std::ios::beg);
	Out.resize((size_t)Size);
	if (Size > 0 && !in.read(reinterpret_cast<char*>(Out.data()), Size))
		return false;
	return true;
}

// Resolve a buffer URI against the glTF file's directory. Data-URIs are
// detected and rejected (caller should treat as a load failure).
bool resolve_buffer(
	const std::string& RootPath,
	const Buffer& BufInfo,
	std::vector<uint8_t>& Out)
{
	if (!BufInfo.uri.has_value())
		return false; // GLB-style inline buffer; handled by GLBReader, not here
	const std::string& URI = *BufInfo.uri;
	if (URI.rfind("data:", 0) == 0)
		return false; // data-URI not yet supported

	std::filesystem::path FullPath = std::filesystem::path(RootPath) / URI;
	return read_file_to_bytes(FullPath.string(), Out);
}

} // namespace


bool GS::GLTFReader::ParseGLTFJsonString(
	const std::string& JsonText,
	Root& RootOut,
	std::string* ErrorOut)
{
	return ParseGLTFJson(JsonText, RootOut, ErrorOut);
}


bool GS::GLTFReader::ReadGLTF(
	const std::string& Path,
	Root& RootOut,
	std::vector<std::vector<uint8_t>>& BuffersOut,
	const ReadOptions& /*Options*/)
{
	std::string JsonText;
	if (!read_file_to_string(Path, JsonText))
		return false;

	std::string Err;
	if (!ParseGLTFJson(JsonText, RootOut, &Err))
		return false;

	// Establish the directory containing the .gltf file so external buffer URIs
	// can be resolved relative to it.
	std::filesystem::path GLTFPath(Path);
	RootOut.RootPath = GLTFPath.parent_path().string();

	BuffersOut.clear();
	BuffersOut.resize(RootOut.buffers.size());
	for (size_t i = 0; i < RootOut.buffers.size(); ++i) {
		// Best-effort: a failure to load any single buffer leaves that slot as
		// an empty vector; downstream extraction skips primitives that point
		// into invalid buffers.
		(void)resolve_buffer(RootOut.RootPath, RootOut.buffers[i], BuffersOut[i]);
	}

	return true;
}


bool GS::GLTFReader::ReadGLTFToDenseMesh(
	const std::string& Path,
	DenseMesh& MeshOut,
	const ReadOptions& Options)
{
	Root R;
	std::vector<std::vector<uint8_t>> Buffers;
	if (!ReadGLTF(Path, R, Buffers, Options))
		return false;

	GS::GLTFToDenseMeshOptions ConvOpts;
	ConvOpts.bIgnoreNormals = !Options.bNormals;
	ConvOpts.bIgnoreUVs = !Options.bUVs;
	GS::GLTFFormatDataToDenseMesh(R, Buffers, MeshOut, ConvOpts);
	return true;
}
