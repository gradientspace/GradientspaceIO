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

} // namespace


bool GS::GLTFWriter::WriteGLTF(
	const std::string& OutGLTFPath,
	const Root& InRoot,
	const std::vector<uint8_t>& BinaryBlob,
	bool bWriteBinarySidecar,
	bool bPrettyJson)
{
	Root R = InRoot;

	if (bWriteBinarySidecar) {
		std::filesystem::path GLTFPath(OutGLTFPath);
		std::filesystem::path BinPath = GLTFPath;
		BinPath.replace_extension(".bin");
		std::string BinFilename = BinPath.filename().string();

		if (!write_binary_file(BinPath.string(), BinaryBlob))
			return false;

		// Point the first buffer at the sidecar file. If no buffer entry exists
		// (e.g. an empty mesh), create one.
		if (R.buffers.empty()) {
			Buffer B;
			B.byteLength = (int)BinaryBlob.size();
			R.buffers.push_back(B);
		}
		R.buffers[0].uri = BinFilename;
		R.buffers[0].byteLength = (int)BinaryBlob.size();
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

	return WriteGLTF(OutGLTFPath, R, Bin, /*bWriteBinarySidecar=*/true, Options.bPrettyJson);
}
