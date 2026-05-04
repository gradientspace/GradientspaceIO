// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLTFWriter.h"

#include <filesystem>
#include <fstream>


using namespace GS;
using namespace GS::GLTFFormatData;


namespace
{

bool write_text_file(const std::string& Path, const std::string& Contents)
{
	std::ofstream out(Path, std::ios::binary);
	if (!out.is_open())
		return false;
	out.write(Contents.data(), (std::streamsize)Contents.size());
	return out.good();
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

// Encode raw bytes to standard base64 (RFC 4648, with '=' padding).
void encode_base64(const uint8_t* Data, size_t Size, std::string& Out)
{
	static constexpr char Alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	Out.clear();
	Out.reserve(((Size + 2) / 3) * 4);
	size_t i = 0;
	for (; i + 3 <= Size; i += 3) {
		uint32_t v = ((uint32_t)Data[i] << 16) | ((uint32_t)Data[i + 1] << 8) | (uint32_t)Data[i + 2];
		Out.push_back(Alphabet[(v >> 18) & 0x3F]);
		Out.push_back(Alphabet[(v >> 12) & 0x3F]);
		Out.push_back(Alphabet[(v >> 6) & 0x3F]);
		Out.push_back(Alphabet[v & 0x3F]);
	}
	size_t Rem = Size - i;
	if (Rem == 1) {
		uint32_t v = (uint32_t)Data[i] << 16;
		Out.push_back(Alphabet[(v >> 18) & 0x3F]);
		Out.push_back(Alphabet[(v >> 12) & 0x3F]);
		Out.push_back('=');
		Out.push_back('=');
	}
	else if (Rem == 2) {
		uint32_t v = ((uint32_t)Data[i] << 16) | ((uint32_t)Data[i + 1] << 8);
		Out.push_back(Alphabet[(v >> 18) & 0x3F]);
		Out.push_back(Alphabet[(v >> 12) & 0x3F]);
		Out.push_back(Alphabet[(v >> 6) & 0x3F]);
		Out.push_back('=');
	}
}

} // namespace


bool GS::GLTFWriter::WriteGLTF(
	const std::string& OutGLTFPath,
	const Root& InRoot,
	const std::vector<uint8_t>& BinaryBlob,
	BufferLayout Layout,
	bool bPrettyJson)
{
	Root R = InRoot;

	// Ensure a buffers[0] entry exists so we have somewhere to record the URI
	// and byteLength for the payload.
	if (R.buffers.empty()) {
		Buffer B;
		B.byteLength = (int)BinaryBlob.size();
		R.buffers.push_back(B);
	}
	R.buffers[0].byteLength = (int)BinaryBlob.size();

	if (Layout == BufferLayout::Sidecar) {
		std::filesystem::path GLTFPath(OutGLTFPath);
		std::filesystem::path BinPath = GLTFPath;
		BinPath.replace_extension(".bin");
		std::string BinFilename = BinPath.filename().string();

		if (!write_binary_file(BinPath.string(), BinaryBlob))
			return false;
		R.buffers[0].uri = BinFilename;
	}
	else { // BufferLayout::Embedded
		std::string Encoded;
		encode_base64(BinaryBlob.data(), BinaryBlob.size(), Encoded);
		R.buffers[0].uri = "data:application/octet-stream;base64," + Encoded;
	}

	std::string Json = SerializeGLTFJson(R, bPrettyJson);
	return write_text_file(OutGLTFPath, Json);
}


bool GS::GLTFWriter::WriteGLTF(
	const std::string& OutGLTFPath,
	const DenseMesh& Mesh,
	const WriteOptions& Options)
{
	Root R;
	std::vector<uint8_t> Bin;
	GS::DenseMeshToGLTFFormatData(Mesh, R, Bin, Options.bNormals, Options.bUVs);

	return WriteGLTF(OutGLTFPath, R, Bin, BufferLayout::Sidecar, Options.bPrettyJson);
}
