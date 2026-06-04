// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLTFReader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


using namespace GS;
using namespace GS::GLTFFormatData;


// Named (not anonymous) so the UE unity build doesn't ODR-collide our helpers
// against the same-named ones in GLBReader.cpp. Function-local `using namespace`
// directives in the GS::GLTFReader::* members below pull these into scope without
// polluting the global namespace for sibling .cpp files in the unity TU.
namespace gltf_reader_local
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

// Decode a standard base64 string (RFC 4648) into raw bytes. ASCII whitespace
// is skipped; '=' padding terminates input. Returns false on invalid chars.
bool decode_base64(const char* Data, size_t Size, std::vector<uint8_t>& Out)
{
	uint32_t Buffer = 0;
	int BufferBits = 0;
	for (size_t i = 0; i < Size; ++i) {
		char c = Data[i];
		int v;
		if (c >= 'A' && c <= 'Z')      v = c - 'A';
		else if (c >= 'a' && c <= 'z') v = 26 + (c - 'a');
		else if (c >= '0' && c <= '9') v = 52 + (c - '0');
		else if (c == '+')             v = 62;
		else if (c == '/')             v = 63;
		else if (c == '=')             break;
		else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
		else return false;

		Buffer = (Buffer << 6) | (uint32_t)v;
		BufferBits += 6;
		if (BufferBits >= 8) {
			BufferBits -= 8;
			Out.push_back((uint8_t)((Buffer >> BufferBits) & 0xFF));
		}
	}
	return true;
}

// Resolve a buffer URI against the glTF file's directory. Inline base64 data
// URIs (gltf-embedded) are decoded directly; other data-URI encodings are
// rejected (caller should treat as a load failure).
bool resolve_buffer(
	const std::string& RootPath,
	const Buffer& BufInfo,
	std::vector<uint8_t>& Out)
{
	if (!BufInfo.uri.has_value())
		return false; // GLB-style inline buffer; handled by GLBReader, not here
	const std::string& URI = *BufInfo.uri;
	if (URI.rfind("data:", 0) == 0) {
		// Form is "data:<mediatype>[;param]*[;base64],<payload>". Per the
		// glTF 2.0 spec, buffer data URIs MUST be base64-encoded.
		size_t Comma = URI.find(',');
		if (Comma == std::string::npos)
			return false;
		size_t B64Pos = URI.find(";base64", 5);
		if (B64Pos == std::string::npos || B64Pos >= Comma)
			return false;
		Out.clear();
		Out.reserve(((URI.size() - Comma - 1) * 3) / 4);
		return decode_base64(URI.data() + Comma + 1, URI.size() - Comma - 1, Out);
	}

	std::filesystem::path FullPath = std::filesystem::path(RootPath) / URI;
	return read_file_to_bytes(FullPath.string(), Out);
}

} // namespace gltf_reader_local


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
	using namespace gltf_reader_local;

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
