// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/STLWriter.h"

#include <vector>


using namespace GS;


bool GS::STLWriter::WriteSTL(
	const std::string& Filename,
	const DenseMesh& Mesh,
	const std::string& MeshName,
	bool bWriteBinary)
{
	if (bWriteBinary) {
		auto BinaryWriter = GS::FileBinaryWriter::OpenFile(Filename);
		if (!BinaryWriter)
			return false;
		return GS::STLWriter::WriteSTL(BinaryWriter, Mesh);
	}
	else {
		auto TextWriter = GS::FileTextWriter::OpenFile(Filename);
		if (!TextWriter)
			return false;
		return GS::STLWriter::WriteSTL(TextWriter, Mesh, MeshName);
	}
}


bool GS::STLWriter::WriteSTL(
	ITextWriter& TextWriter,
	const DenseMesh& Mesh,
	const std::string& MeshName )
{
	char line_buffer[1024];
	int TriCount = Mesh.GetTriangleCount();

	TextWriter.WriteToken("solid ");
	TextWriter.WriteToken(MeshName.c_str());
	TextWriter.WriteEndOfLine();

	for (int i = 0; i < TriCount; ++i) {
		Index3i tri = Mesh.GetTriangle(i);
		Vector3d A = Mesh.GetPosition(tri.A), B = Mesh.GetPosition(tri.B), C = Mesh.GetPosition(tri.C);
		Vector3d Normal = GS::Normal(A, B, C);
		snprintf(line_buffer, 1023, "facet normal %f %f %f", (float)Normal.X, (float)Normal.Y, (float)Normal.Z);
		TextWriter.WriteLine(line_buffer);
		TextWriter.WriteLine(" outer loop");
		snprintf(line_buffer, 1023, "  vertex %f %f %f", (float)A.X, (float)A.Y, (float)A.Z);
		TextWriter.WriteLine(line_buffer);
		snprintf(line_buffer, 1023, "  vertex %f %f %f", (float)B.X, (float)B.Y, (float)B.Z);
		TextWriter.WriteLine(line_buffer);
		snprintf(line_buffer, 1023, "  vertex %f %f %f", (float)C.X, (float)C.Y, (float)C.Z);
		TextWriter.WriteLine(line_buffer);
		TextWriter.WriteLine(" endloop");
		TextWriter.WriteLine("endfacet");
	}

	TextWriter.WriteToken("endsolid ");
	TextWriter.WriteToken(MeshName.c_str());
	TextWriter.WriteEndOfLine();

	return true;
}


bool GS::STLWriter::WriteSTL(
	IBinaryWriter& BinaryWriter,
	const DenseMesh& Mesh)
{
	int TriCount = Mesh.GetTriangleCount();

	char header[80];
	memset(header, 0, 80);
	snprintf(header, 80, "gradientspace_stl");
	bool bWritesOK = BinaryWriter.WriteBytes(header, 80);
	bWritesOK &= BinaryWriter.WriteBytes(&TriCount, 4);

	uint16_t attribute = 0;
	for (int i = 0; i < TriCount; ++i) {
		Index3i tri = Mesh.GetTriangle(i);
		Vector3f A = (Vector3f)Mesh.GetPosition(tri.A), B = (Vector3f)Mesh.GetPosition(tri.B), C = (Vector3f)Mesh.GetPosition(tri.C);
		Vector3f Normal = GS::Normal(A, B, C);
		bWritesOK &= BinaryWriter.WriteBytes(&Normal.X, sizeof(float) * 3);
		bWritesOK &= BinaryWriter.WriteBytes(&A.X, sizeof(float) * 3);
		bWritesOK &= BinaryWriter.WriteBytes(&B.X, sizeof(float) * 3);
		bWritesOK &= BinaryWriter.WriteBytes(&C.X, sizeof(float) * 3);
		bWritesOK &= BinaryWriter.WriteBytes(&attribute, 2);
	}

	return bWritesOK;
}
