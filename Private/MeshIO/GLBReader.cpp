// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLBReader.h"
#include "MeshIO/GLTFReader.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>


using namespace GS;
using namespace GS::GLTFFormatData;


namespace
{

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

// Resolve any non-GLB external buffer URIs alongside the .glb file. Data-URIs
// are not supported and are treated as a load failure.
bool resolve_external_buffer(
	const std::string& RootPath,
	const Buffer& BufInfo,
	std::vector<uint8_t>& Out)
{
	if (!BufInfo.uri.has_value())
		return false;
	const std::string& URI = *BufInfo.uri;
	if (URI.rfind("data:", 0) == 0)
		return false;

	std::filesystem::path FullPath = std::filesystem::path(RootPath) / URI;
	return read_file_to_bytes(FullPath.string(), Out);
}

} // namespace


bool GS::GLBReader::ReadGLB(
	const std::string& Path,
	Root& RootOut,
	std::vector<std::vector<uint8_t>>& BuffersOut,
	const ReadOptions& /*Options*/)
{
	std::vector<uint8_t> FileBytes;
	if (!read_file_to_bytes(Path, FileBytes))
		return false;

	// Header: magic(4) + version(4) + length(4) = 12 bytes
	if (FileBytes.size() < 12)
		return false;

	GLBHeader Header{};
	std::memcpy(&Header.magic, FileBytes.data() + 0, 4);
	std::memcpy(&Header.version, FileBytes.data() + 4, 4);
	std::memcpy(&Header.length, FileBytes.data() + 8, 4);

	if (Header.magic != GLBMagicNumber)
		return false;
	if (Header.version != 2)
		return false;
	// Header.length is the entire file size; tolerate truncated/extra trailing data
	// by clamping our cursor to FileBytes.size() rather than trusting the header.

	size_t Cursor = 12;

	auto ReadU32 = [&](uint32_t& OutVal) -> bool {
		if (Cursor + 4 > FileBytes.size())
			return false;
		std::memcpy(&OutVal, FileBytes.data() + Cursor, 4);
		Cursor += 4;
		return true;
	};

	// Chunk 0 must be JSON
	uint32_t Chunk0Length = 0, Chunk0Type = 0;
	if (!ReadU32(Chunk0Length) || !ReadU32(Chunk0Type))
		return false;
	if (Chunk0Type != GLBChunkType_JSON)
		return false;
	if (Cursor + Chunk0Length > FileBytes.size())
		return false;

	std::string JsonText(reinterpret_cast<const char*>(FileBytes.data() + Cursor), (size_t)Chunk0Length);
	Cursor += Chunk0Length;

	std::string ParseErr;
	if (!ParseGLTFJson(JsonText, RootOut, &ParseErr))
		return false;

	// Establish the directory containing the .glb file so any external buffer URIs
	// (uncommon for GLB but still permitted by the spec for buffers > 0) can be
	// resolved relative to it.
	std::filesystem::path GLBPath(Path);
	RootOut.RootPath = GLBPath.parent_path().string();

	BuffersOut.clear();
	BuffersOut.resize(RootOut.buffers.size());

	// Chunk 1 (BIN) is optional per the spec, but if present, it provides the
	// payload for buffers[0] (which must have no URI).
	if (Cursor + 8 <= FileBytes.size()) {
		uint32_t Chunk1Length = 0, Chunk1Type = 0;
		if (ReadU32(Chunk1Length) && ReadU32(Chunk1Type)) {
			if (Chunk1Type == GLBChunkType_BIN && Cursor + Chunk1Length <= FileBytes.size()) {
				if (!RootOut.buffers.empty()) {
					std::vector<uint8_t>& Dst = BuffersOut[0];
					Dst.assign(FileBytes.data() + Cursor, FileBytes.data() + Cursor + Chunk1Length);
				}
				Cursor += Chunk1Length;
			}
		}
	}

	// Best-effort load any non-GLB buffers via their URIs (slot 0 may already be
	// populated by the BIN chunk above; only fill empty slots from URI here).
	for (size_t i = 0; i < RootOut.buffers.size(); ++i) {
		if (!BuffersOut[i].empty())
			continue;
		(void)resolve_external_buffer(RootOut.RootPath, RootOut.buffers[i], BuffersOut[i]);
	}

	return true;
}


bool GS::GLBReader::ReadGLBToDenseMesh(
	const std::string& Path,
	DenseMesh& MeshOut,
	const ReadOptions& Options)
{
	Root R;
	std::vector<std::vector<uint8_t>> Buffers;
	if (!ReadGLB(Path, R, Buffers, Options))
		return false;

	GS::GLTFToDenseMeshOptions ConvOpts;
	ConvOpts.bIgnoreNormals = !Options.bNormals;
	ConvOpts.bIgnoreUVs = !Options.bUVs;
	GS::GLTFFormatDataToDenseMesh(R, Buffers, MeshOut, ConvOpts);
	return true;
}
