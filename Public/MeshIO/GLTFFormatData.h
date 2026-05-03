// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Mesh/DenseMesh.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


// glTF 2.0 reference: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
//
// Type definitions in this namespace mirror the JSON schema of glTF 2.0.
// The top-level GLTFFormatData::Root struct is the in-memory representation
// of a parsed .gltf file (binary buffer payloads are passed alongside as
// std::vector<uint8_t> per Root.buffers entry).
//
// Unsupported features (matching the geometry3Sharp reference):
//   - Animation, Skin, or Camera data
//   - Sparse Accessors
//   - Extension or Extra fields
namespace GS::GLTFFormatData
{

enum class EComponentType : int
{
	SignedByte = 5120,
	UnsignedByte = 5121,
	SignedShort = 5122,
	UnsignedShort = 5123,
	UnsignedInt = 5125,
	Float = 5126
};

enum class EElementType : int
{
	Scalar = 0,
	Vec2 = 1,
	Vec3 = 2,
	Vec4 = 3,
	Mat2 = 4,
	Mat3 = 5,
	Mat4 = 6
};

enum class EPrimitiveMode : int
{
	Points = 0,
	Lines = 1,
	LineLoop = 2,
	LineStrip = 3,
	Triangles = 4,
	TriangleStrip = 5,
	TriangleFan = 6
};

enum class EBufferViewTarget : int
{
	ARRAY_BUFFER = 34962,
	ELEMENT_ARRAY_BUFFER = 34963
};


// Standard glTF mesh attribute names
constexpr const char* MeshAttribute_Position = "POSITION";
constexpr const char* MeshAttribute_Normal = "NORMAL";
constexpr const char* MeshAttribute_Tangent = "TANGENT";
constexpr const char* MeshAttribute_UVPrefix = "TEXCOORD_";
constexpr const char* MeshAttribute_ColorPrefix = "COLOR_";


// Helpers
GRADIENTSPACEIO_API int GetComponentByteCount(EComponentType ComponentType);
GRADIENTSPACEIO_API int GetElementComponentCount(EElementType ElementType);
GRADIENTSPACEIO_API const char* GetElementTypeString(EElementType ElementType);
// Returns true on success and writes the parsed enum to OutType. False on unknown string.
GRADIENTSPACEIO_API bool ParseElementType(const std::string& Str, EElementType& OutType);


struct GRADIENTSPACEIO_API Asset
{
	std::optional<std::string> copyright;
	std::optional<std::string> generator;
	std::string version = "2.0";
};


// Sparse accessor info is parsed-but-not-implemented; presence triggers a warning.
struct GRADIENTSPACEIO_API SparseAccessorInfo
{
	int count = 0;
};

struct GRADIENTSPACEIO_API Accessor
{
	int bufferView = 0;
	int byteOffset = 0;
	EComponentType componentType = EComponentType::Float;
	int count = 0;
	std::string type = "VEC3";

	std::optional<std::vector<float>> min;
	std::optional<std::vector<float>> max;
	std::optional<SparseAccessorInfo> sparse;

	int GetComponentByteCount() const { return GLTFFormatData::GetComponentByteCount(componentType); }
	// Throws std::out_of_range if `type` is unrecognized.
	EElementType GetElementType() const;
	int GetElementByteCount() const;
};


struct GRADIENTSPACEIO_API BufferView
{
	std::optional<std::string> name;
	int buffer = 0;
	int byteLength = 0;
	int byteOffset = 0;
	int byteStride = 0;
	std::optional<EBufferViewTarget> target;
};


struct GRADIENTSPACEIO_API Buffer
{
	std::optional<std::string> name;
	std::optional<std::string> uri;
	int byteLength = 0;
};


struct GRADIENTSPACEIO_API Image
{
	std::optional<std::string> name;
	std::optional<int> bufferView;
	std::optional<std::string> uri;
	std::optional<std::string> mimeType;
};


struct GRADIENTSPACEIO_API Texture
{
	std::optional<std::string> name;
	std::optional<int> source;
	std::optional<int> sampler;
};


enum class ESamplerMagFilter : int { NEAREST = 9728, LINEAR = 9729 };
enum class ESamplerMinFilter : int
{
	NEAREST = 9728, LINEAR = 9729,
	NEAREST_MIPMAP_NEAREST = 9984, LINEAR_MIPMAP_NEAREST = 9985,
	NEAREST_MIPMAP_LINEAR = 9986, LINEAR_MIPMAP_LINEAR = 9987
};
enum class EWrapMode : int { CLAMP_TO_EDGE = 33071, MIRRORED_REPEAT = 33648, REPEAT = 10497 };

struct GRADIENTSPACEIO_API Sampler
{
	std::optional<std::string> name;
	std::optional<int> magFilter;
	std::optional<int> minFilter;
	std::optional<int> wrapS;
	std::optional<int> wrapT;
};


struct GRADIENTSPACEIO_API TextureInfo
{
	int index = -1;
	int texCoord = 0;
};

struct GRADIENTSPACEIO_API NormalTextureInfo
{
	int index = -1;
	int texCoord = 0;
	std::optional<double> scale;
};

struct GRADIENTSPACEIO_API OcclusionTextureInfo
{
	int index = -1;
	int texCoord = 0;
	std::optional<double> strength;
};

struct GRADIENTSPACEIO_API PBRMetallicRoughness
{
	std::optional<std::vector<double>> baseColorFactor;
	std::optional<TextureInfo> baseColorTexture;
	std::optional<double> metallicFactor;
	std::optional<double> roughnessFactor;
	std::optional<TextureInfo> metallicRoughnessTexture;
};

struct GRADIENTSPACEIO_API Material
{
	std::optional<std::string> name;
	std::optional<PBRMetallicRoughness> pbrMetallicRoughness;
	std::optional<NormalTextureInfo> normalTexture;
	std::optional<OcclusionTextureInfo> occlusionTexture;
	std::optional<TextureInfo> emissiveTexture;
	std::optional<std::vector<double>> emissiveFactor;
	std::optional<std::string> alphaMode;
	std::optional<double> alphaCutoff;
	bool doubleSided = false;
};


struct GRADIENTSPACEIO_API Primitive
{
	std::optional<int> indices;
	std::optional<int> material;
	EPrimitiveMode mode = EPrimitiveMode::Triangles;
	std::unordered_map<std::string, int> attributes;
};


struct GRADIENTSPACEIO_API Mesh
{
	std::optional<std::string> name;
	std::vector<Primitive> primitives;
};


struct GRADIENTSPACEIO_API Node
{
	std::optional<std::string> name;
	std::optional<int> mesh;
	// Either a 4x4 matrix (column-major, per spec) OR a TRS combination, never both.
	std::optional<std::array<double, 16>> matrix;
	std::optional<std::array<double, 3>> translation;
	std::optional<std::array<double, 4>> rotation; // quaternion (x,y,z,w)
	std::optional<std::array<double, 3>> scale;
	std::vector<int> children;
};


struct GRADIENTSPACEIO_API Scene
{
	std::optional<std::string> name;
	std::vector<int> nodes;
};


struct GRADIENTSPACEIO_API Root
{
	std::vector<std::string> extensionsUsed;
	std::vector<std::string> extensionsRequired;

	Asset asset;

	std::vector<Buffer> buffers;
	std::vector<BufferView> bufferViews;
	std::vector<Accessor> accessors;

	std::vector<Mesh> meshes;

	std::vector<Image> images;
	std::vector<Texture> textures;
	std::vector<Sampler> samplers;
	std::vector<Material> materials;

	std::vector<Node> nodes;
	std::vector<Scene> scenes;
	std::optional<int> scene;

	// Populated by the reader, not part of the JSON. Used to resolve relative buffer URIs.
	std::string RootPath;
};


// GLB binary container constants (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification)
constexpr uint32_t GLBMagicNumber    = 0x46546C67u; // 'glTF'
constexpr uint32_t GLBChunkType_JSON = 0x4E4F534Au; // 'JSON'
constexpr uint32_t GLBChunkType_BIN  = 0x004E4942u; // 'BIN\0'

struct GRADIENTSPACEIO_API GLBHeader
{
	uint32_t magic = 0;
	uint32_t version = 0;
	uint32_t length = 0;
};


// Parse glTF JSON text into a Root structure. Returns true on success.
// On failure, returns false and (if non-null) writes an error message to OutError.
GRADIENTSPACEIO_API
bool ParseGLTFJson(const std::string& JsonText, Root& OutRoot, std::string* OutError = nullptr);

// Serialize a Root to glTF JSON text. If bPretty is true, the output is indented.
GRADIENTSPACEIO_API
std::string SerializeGLTFJson(const Root& Root, bool bPretty = true);


} // namespace GS::GLTFFormatData


namespace GS
{

// =============================================================================
// DenseMesh <-> GLTFFormatData converters (parallels DenseMeshToOBJFormatData).
// =============================================================================

// Convert a DenseMesh to a GLTFFormatData::Root + matching binary buffer.
// Produces a single mesh with a single primitive in a single scene.
// The binary buffer holds (positions, normals?, uvs?, indices) packed back-to-back;
// the Root references these via a single Buffer entry whose `uri` is left unset
// (caller decides whether to write it as a sibling .bin or pack into a GLB).
GRADIENTSPACEIO_API
void DenseMeshToGLTFFormatData(
	const DenseMesh& Mesh,
	GLTFFormatData::Root& RootOut,
	std::vector<uint8_t>& BinaryBufferOut,
	bool bIncludeNormals = true,
	bool bIncludeUVs = true);


struct GRADIENTSPACEIO_API GLTFToDenseMeshOptions
{
	bool bIgnoreUVs = false;
	bool bIgnoreNormals = false;
};

// Extract a single combined DenseMesh from a parsed GLTFFormatData::Root and its
// resolved per-Buffer binary blobs (Buffers[i] corresponds to Root.buffers[i]).
// Walks the active scene graph, applying node transforms, and accumulates all
// triangle primitives into MeshOut.
GRADIENTSPACEIO_API
void GLTFFormatDataToDenseMesh(
	const GLTFFormatData::Root& Root,
	const std::vector<std::vector<uint8_t>>& Buffers,
	DenseMesh& MeshOut,
	const GLTFToDenseMeshOptions& Options = GLTFToDenseMeshOptions());


} // namespace GS
