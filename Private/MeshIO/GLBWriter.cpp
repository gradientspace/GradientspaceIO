// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLBWriter.h"

#include <cstring>
#include <fstream>


using namespace GS;
using namespace GS::GLTFFormatData;


// Named (not anonymous) so the UE unity build doesn't ODR-collide our helpers
// against the same-named ones in GLTFWriter.cpp. Function-local `using namespace`
// directives in the GS::GLBWriter::* members below pull these into scope without
// polluting the global namespace for sibling .cpp files in the unity TU.
namespace glb_writer_local
{

// Pad an existing in-memory chunk payload up to a 4-byte boundary using the
// given pad byte (0x20 for the JSON chunk, 0x00 for the BIN chunk per spec).
void pad_to_4(std::vector<uint8_t>& Bytes, uint8_t PadByte)
{
	size_t Remainder = Bytes.size() % 4;
	if (Remainder != 0)
		Bytes.insert(Bytes.end(), 4 - Remainder, PadByte);
}

void append_u32(std::vector<uint8_t>& Out, uint32_t Value)
{
	Out.resize(Out.size() + 4);
	std::memcpy(Out.data() + Out.size() - 4, &Value, 4);
}

bool write_binary_file(const std::string& Path, const std::vector<uint8_t>& Bytes)
{
	std::ofstream out(Path, std::ios::binary);
	if (!out.is_open())
		return false;
	if (!Bytes.empty())
		out.write(reinterpret_cast<const char*>(Bytes.data()), (std::streamsize)Bytes.size());
	return out.good();
}

} // namespace glb_writer_local


bool GS::GLBWriter::WriteGLB(
	const std::string& OutGLBPath,
	const Root& InRoot,
	const std::vector<uint8_t>& BinaryBlob)
{
	using namespace glb_writer_local;

	// Make a working copy of Root so we can fix up buffers[0] to match GLB rules:
	// the GLB BIN chunk is implicitly buffers[0], so its `uri` must be unset and
	// its `byteLength` must equal the binary payload length.
	Root R = InRoot;
	if (R.buffers.empty()) {
		Buffer B;
		R.buffers.push_back(B);
	}
	R.buffers[0].uri.reset();
	R.buffers[0].byteLength = (int)BinaryBlob.size();

	// JSON chunk: serialize compactly to keep .glb files small. Pad with spaces
	// (0x20) so the BIN chunk header lands on a 4-byte boundary.
	std::string Json = SerializeGLTFJson(R, /*bPretty=*/false);
	std::vector<uint8_t> JsonChunk(Json.begin(), Json.end());
	pad_to_4(JsonChunk, 0x20);

	// BIN chunk: pad with zeros so the file ends on a 4-byte boundary.
	std::vector<uint8_t> BinChunk = BinaryBlob;
	pad_to_4(BinChunk, 0x00);

	// File layout:
	//   12-byte header
	//   8-byte chunk0 header + JsonChunk
	//   8-byte chunk1 header + BinChunk  (omitted if BinChunk is empty)
	const bool bHasBin = !BinChunk.empty();
	uint32_t TotalLength =
		12u +
		8u + (uint32_t)JsonChunk.size() +
		(bHasBin ? (8u + (uint32_t)BinChunk.size()) : 0u);

	std::vector<uint8_t> Out;
	Out.reserve(TotalLength);

	// Header
	append_u32(Out, GLBMagicNumber);
	append_u32(Out, 2u);            // version
	append_u32(Out, TotalLength);

	// Chunk 0: JSON
	append_u32(Out, (uint32_t)JsonChunk.size());
	append_u32(Out, GLBChunkType_JSON);
	Out.insert(Out.end(), JsonChunk.begin(), JsonChunk.end());

	// Chunk 1: BIN (only when present)
	if (bHasBin) {
		append_u32(Out, (uint32_t)BinChunk.size());
		append_u32(Out, GLBChunkType_BIN);
		Out.insert(Out.end(), BinChunk.begin(), BinChunk.end());
	}

	return write_binary_file(OutGLBPath, Out);
}


bool GS::GLBWriter::WriteGLB(
	const std::string& OutGLBPath,
	const DenseMesh& Mesh,
	const WriteOptions& Options)
{
	Root R;
	std::vector<uint8_t> Bin;
	GS::DenseMeshToGLTFFormatData(Mesh, R, Bin, Options.bNormals, Options.bUVs);

	return WriteGLB(OutGLBPath, R, Bin);
}
