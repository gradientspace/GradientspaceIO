// Copyright Gradientspace Corp. All Rights Reserved.
#include "MeshIO/GLTFFormatData.h"

#include "Math/GSMatrix3.h"
#include "Math/GSQuaternion.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <vector>

// Compile nlohmann/json without exceptions: any internal throw becomes
// std::abort(), which is a safety net for genuinely-unreachable code paths.
// This module avoids that path by using the noexcept parse API and by
// type-checking JSON nodes via sj_get<T> before extraction.
#define JSON_NOEXCEPTION
#include "thirdparty/json.hpp"


using nlohmann::json;
using namespace GS;
using namespace GS::GLTFFormatData;


// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

int GS::GLTFFormatData::GetComponentByteCount(EComponentType ComponentType)
{
	switch (ComponentType) {
		case EComponentType::SignedByte:
		case EComponentType::UnsignedByte:
			return 1;
		case EComponentType::SignedShort:
		case EComponentType::UnsignedShort:
			return 2;
		case EComponentType::UnsignedInt:
		case EComponentType::Float:
			return 4;
	}
	return 0;
}

int GS::GLTFFormatData::GetElementComponentCount(EElementType ElementType)
{
	static const int Counts[7] = { 1, 2, 3, 4, 4, 8, 16 };
	int idx = (int)ElementType;
	return (idx >= 0 && idx < 7) ? Counts[idx] : 0;
}

const char* GS::GLTFFormatData::GetElementTypeString(EElementType ElementType)
{
	static const char* Names[7] = { "SCALAR", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3", "MAT4" };
	int idx = (int)ElementType;
	return (idx >= 0 && idx < 7) ? Names[idx] : "";
}

bool GS::GLTFFormatData::ParseElementType(const std::string& Str, EElementType& OutType)
{
	for (int i = 0; i < 7; ++i) {
		if (Str == GetElementTypeString((EElementType)i)) {
			OutType = (EElementType)i;
			return true;
		}
	}
	return false;
}

EElementType Accessor::GetElementType() const
{
	// Returns SCALAR as a conservative fallback if `type` is unrecognized.
	// Callers that need to distinguish failure should use ParseElementType()
	// directly (which returns bool), or check that GetElementByteCount() > 0.
	EElementType result = EElementType::Scalar;
	(void)GLTFFormatData::ParseElementType(type, result);
	return result;
}

int Accessor::GetElementByteCount() const
{
	EElementType result;
	if (!GLTFFormatData::ParseElementType(type, result))
		return 0;
	return GetElementComponentCount(result) * GetComponentByteCount();
}


// -----------------------------------------------------------------------------
// JSON glue
// -----------------------------------------------------------------------------
//
// nlohmann/json automatically serializes enum types as their underlying integer
// value, which matches how glTF stores componentType, mode, target, etc.
//
// All field reads route through sj_get<T>(json, T&), which type-checks the JSON
// node before extraction and returns false on mismatch (leaving the output
// untouched). This is the core mechanism for noexcept JSON parsing -- since we
// never call the throwing get<T>() path, JSON_NOEXCEPTION's std::abort fallback
// stays dormant even on malformed input.

namespace
{

// --- Type-safe primitive extractors ---------------------------------------

inline bool sj_get(const json& j, bool& out)
{
	if (!j.is_boolean()) return false;
	out = j.get<bool>();
	return true;
}
inline bool sj_get(const json& j, int& out)
{
	if (j.is_number_integer() || j.is_number_unsigned()) { out = j.get<int>(); return true; }
	if (j.is_number_float()) { out = (int)j.get<double>(); return true; }
	return false;
}
inline bool sj_get(const json& j, float& out)
{
	if (!j.is_number()) return false;
	out = j.get<float>();
	return true;
}
inline bool sj_get(const json& j, double& out)
{
	if (!j.is_number()) return false;
	out = j.get<double>();
	return true;
}
inline bool sj_get(const json& j, std::string& out)
{
	if (!j.is_string()) return false;
	out = j.get<std::string>();
	return true;
}

// --- Catch-all for struct types via from_json ADL -------------------------
// Any T that has a from_json(const json&, T&) overload (i.e. our GLTFFormatData
// struct types) dispatches here. The concept also excludes arithmetic types
// and std::string so the explicit primitive overloads above stay preferred.
// Declared BEFORE the container templates below so their unqualified sj_get(e,item)
// recursive calls can find this overload via ordinary template-body lookup.

template<typename T>
concept HasFromJson = requires(const json& j, T& t) { from_json(j, t); }
	&& !std::is_same_v<T, std::string>
	&& !std::is_arithmetic_v<T>;

template<typename T>
	requires HasFromJson<T>
inline bool sj_get(const json& j, T& out)
{
	if (!j.is_object()) return false;
	from_json(j, out);
	return true;
}


// --- Container templates --------------------------------------------------

template<typename T, size_t N>
inline bool sj_get(const json& j, std::array<T, N>& out)
{
	if (!j.is_array() || j.size() != N) return false;
	for (size_t i = 0; i < N; ++i)
		if (!sj_get(j[i], out[i])) return false;
	return true;
}

template<typename T>
inline bool sj_get(const json& j, std::vector<T>& out)
{
	if (!j.is_array()) return false;
	out.clear();
	out.reserve(j.size());
	for (const auto& e : j) {
		T item{};
		if (sj_get(e, item))
			out.push_back(std::move(item));
	}
	return true;
}

template<typename V>
inline bool sj_get(const json& j, std::unordered_map<std::string, V>& out)
{
	if (!j.is_object()) return false;
	for (auto it = j.begin(); it != j.end(); ++it) {
		V val{};
		if (sj_get(it.value(), val))
			out.emplace(it.key(), std::move(val));
	}
	return true;
}


// --- Field-level read/write helpers ---------------------------------------

template<typename T>
inline void write_opt(json& j, const char* key, const std::optional<T>& v)
{
	if (v.has_value())
		j[key] = *v;
}

template<typename T>
inline void read_opt(const json& j, const char* key, std::optional<T>& v)
{
	auto it = j.find(key);
	if (it == j.end() || it->is_null()) return;
	T value{};
	if (sj_get(*it, value))
		v = std::move(value);
}

template<typename T>
inline void read_field(const json& j, const char* key, T& v)
{
	auto it = j.find(key);
	if (it == j.end() || it->is_null()) return;
	(void)sj_get(*it, v);
}

template<typename T>
inline void write_vec_if_nonempty(json& j, const char* key, const std::vector<T>& v)
{
	if (!v.empty())
		j[key] = v;
}

// --- Type-checked enum-from-int helper ------------------------------------

template<typename EnumType>
inline void read_enum_int(const json& j, const char* key, EnumType& v)
{
	auto it = j.find(key);
	if (it == j.end() || it->is_null()) return;
	int raw = 0;
	if (sj_get(*it, raw))
		v = (EnumType)raw;
}

template<typename EnumType>
inline void read_enum_int_opt(const json& j, const char* key, std::optional<EnumType>& v)
{
	auto it = j.find(key);
	if (it == j.end() || it->is_null()) return;
	int raw = 0;
	if (sj_get(*it, raw))
		v = (EnumType)raw;
}

} // namespace


namespace GS::GLTFFormatData
{

// --- Asset ---
void to_json(json& j, const Asset& a)
{
	j = json::object();
	write_opt(j, "copyright", a.copyright);
	write_opt(j, "generator", a.generator);
	j["version"] = a.version;
}
void from_json(const json& j, Asset& a)
{
	read_opt(j, "copyright", a.copyright);
	read_opt(j, "generator", a.generator);
	read_field(j, "version", a.version);
}


// --- SparseAccessorInfo ---
void to_json(json& j, const SparseAccessorInfo& s)
{
	j = json::object();
	j["count"] = s.count;
}
void from_json(const json& j, SparseAccessorInfo& s)
{
	read_field(j, "count", s.count);
}


// --- Accessor ---
void to_json(json& j, const Accessor& a)
{
	j = json::object();
	// glTF requires bufferView, byteOffset, componentType, count, type for
	// non-sparse accessors -- always emit them.
	j["bufferView"] = a.bufferView;
	j["byteOffset"] = a.byteOffset;
	j["componentType"] = (int)a.componentType;
	j["count"] = a.count;
	j["type"] = a.type;
	write_opt(j, "min", a.min);
	write_opt(j, "max", a.max);
	write_opt(j, "sparse", a.sparse);
}
void from_json(const json& j, Accessor& a)
{
	read_field(j, "bufferView", a.bufferView);
	read_field(j, "byteOffset", a.byteOffset);
	read_enum_int(j, "componentType", a.componentType);
	read_field(j, "count", a.count);
	read_field(j, "type", a.type);
	read_opt(j, "min", a.min);
	read_opt(j, "max", a.max);
	read_opt(j, "sparse", a.sparse);
}


// --- BufferView ---
void to_json(json& j, const BufferView& b)
{
	j = json::object();
	write_opt(j, "name", b.name);
	j["buffer"] = b.buffer;
	j["byteLength"] = b.byteLength;
	j["byteOffset"] = b.byteOffset;
	if (b.byteStride != 0)
		j["byteStride"] = b.byteStride;
	if (b.target.has_value())
		j["target"] = (int)*b.target;
}
void from_json(const json& j, BufferView& b)
{
	read_opt(j, "name", b.name);
	read_field(j, "buffer", b.buffer);
	read_field(j, "byteLength", b.byteLength);
	read_field(j, "byteOffset", b.byteOffset);
	read_field(j, "byteStride", b.byteStride);
	read_enum_int_opt(j, "target", b.target);
}


// --- Buffer ---
void to_json(json& j, const Buffer& b)
{
	j = json::object();
	write_opt(j, "name", b.name);
	write_opt(j, "uri", b.uri);
	j["byteLength"] = b.byteLength;
}
void from_json(const json& j, Buffer& b)
{
	read_opt(j, "name", b.name);
	read_opt(j, "uri", b.uri);
	read_field(j, "byteLength", b.byteLength);
}


// --- Image / Texture / Sampler ---
void to_json(json& j, const Image& i)
{
	j = json::object();
	write_opt(j, "name", i.name);
	write_opt(j, "bufferView", i.bufferView);
	write_opt(j, "uri", i.uri);
	write_opt(j, "mimeType", i.mimeType);
}
void from_json(const json& j, Image& i)
{
	read_opt(j, "name", i.name);
	read_opt(j, "bufferView", i.bufferView);
	read_opt(j, "uri", i.uri);
	read_opt(j, "mimeType", i.mimeType);
}

void to_json(json& j, const Texture& t)
{
	j = json::object();
	write_opt(j, "name", t.name);
	write_opt(j, "source", t.source);
	write_opt(j, "sampler", t.sampler);
}
void from_json(const json& j, Texture& t)
{
	read_opt(j, "name", t.name);
	read_opt(j, "source", t.source);
	read_opt(j, "sampler", t.sampler);
}

void to_json(json& j, const Sampler& s)
{
	j = json::object();
	write_opt(j, "name", s.name);
	write_opt(j, "magFilter", s.magFilter);
	write_opt(j, "minFilter", s.minFilter);
	write_opt(j, "wrapS", s.wrapS);
	write_opt(j, "wrapT", s.wrapT);
}
void from_json(const json& j, Sampler& s)
{
	read_opt(j, "name", s.name);
	read_opt(j, "magFilter", s.magFilter);
	read_opt(j, "minFilter", s.minFilter);
	read_opt(j, "wrapS", s.wrapS);
	read_opt(j, "wrapT", s.wrapT);
}


// --- TextureInfo / NormalTextureInfo / OcclusionTextureInfo ---
void to_json(json& j, const TextureInfo& t)
{
	j = json::object();
	j["index"] = t.index;
	if (t.texCoord != 0)
		j["texCoord"] = t.texCoord;
}
void from_json(const json& j, TextureInfo& t)
{
	read_field(j, "index", t.index);
	read_field(j, "texCoord", t.texCoord);
}

void to_json(json& j, const NormalTextureInfo& t)
{
	j = json::object();
	j["index"] = t.index;
	if (t.texCoord != 0)
		j["texCoord"] = t.texCoord;
	write_opt(j, "scale", t.scale);
}
void from_json(const json& j, NormalTextureInfo& t)
{
	read_field(j, "index", t.index);
	read_field(j, "texCoord", t.texCoord);
	read_opt(j, "scale", t.scale);
}

void to_json(json& j, const OcclusionTextureInfo& t)
{
	j = json::object();
	j["index"] = t.index;
	if (t.texCoord != 0)
		j["texCoord"] = t.texCoord;
	write_opt(j, "strength", t.strength);
}
void from_json(const json& j, OcclusionTextureInfo& t)
{
	read_field(j, "index", t.index);
	read_field(j, "texCoord", t.texCoord);
	read_opt(j, "strength", t.strength);
}


// --- PBRMetallicRoughness / Material ---
void to_json(json& j, const PBRMetallicRoughness& p)
{
	j = json::object();
	write_opt(j, "baseColorFactor", p.baseColorFactor);
	write_opt(j, "baseColorTexture", p.baseColorTexture);
	write_opt(j, "metallicFactor", p.metallicFactor);
	write_opt(j, "roughnessFactor", p.roughnessFactor);
	write_opt(j, "metallicRoughnessTexture", p.metallicRoughnessTexture);
}
void from_json(const json& j, PBRMetallicRoughness& p)
{
	read_opt(j, "baseColorFactor", p.baseColorFactor);
	read_opt(j, "baseColorTexture", p.baseColorTexture);
	read_opt(j, "metallicFactor", p.metallicFactor);
	read_opt(j, "roughnessFactor", p.roughnessFactor);
	read_opt(j, "metallicRoughnessTexture", p.metallicRoughnessTexture);
}

void to_json(json& j, const Material& m)
{
	j = json::object();
	write_opt(j, "name", m.name);
	write_opt(j, "pbrMetallicRoughness", m.pbrMetallicRoughness);
	write_opt(j, "normalTexture", m.normalTexture);
	write_opt(j, "occlusionTexture", m.occlusionTexture);
	write_opt(j, "emissiveTexture", m.emissiveTexture);
	write_opt(j, "emissiveFactor", m.emissiveFactor);
	write_opt(j, "alphaMode", m.alphaMode);
	write_opt(j, "alphaCutoff", m.alphaCutoff);
	if (m.doubleSided)
		j["doubleSided"] = m.doubleSided;
}
void from_json(const json& j, Material& m)
{
	read_opt(j, "name", m.name);
	read_opt(j, "pbrMetallicRoughness", m.pbrMetallicRoughness);
	read_opt(j, "normalTexture", m.normalTexture);
	read_opt(j, "occlusionTexture", m.occlusionTexture);
	read_opt(j, "emissiveTexture", m.emissiveTexture);
	read_opt(j, "emissiveFactor", m.emissiveFactor);
	read_opt(j, "alphaMode", m.alphaMode);
	read_opt(j, "alphaCutoff", m.alphaCutoff);
	read_field(j, "doubleSided", m.doubleSided);
}


// --- Primitive / Mesh ---
void to_json(json& j, const Primitive& p)
{
	j = json::object();
	write_opt(j, "indices", p.indices);
	write_opt(j, "material", p.material);
	j["mode"] = (int)p.mode;
	if (!p.attributes.empty())
		j["attributes"] = p.attributes;
}
void from_json(const json& j, Primitive& p)
{
	read_opt(j, "indices", p.indices);
	read_opt(j, "material", p.material);
	read_enum_int(j, "mode", p.mode);
	read_field(j, "attributes", p.attributes);
}

void to_json(json& j, const Mesh& m)
{
	j = json::object();
	write_opt(j, "name", m.name);
	if (!m.primitives.empty())
		j["primitives"] = m.primitives;
}
void from_json(const json& j, Mesh& m)
{
	read_opt(j, "name", m.name);
	read_field(j, "primitives", m.primitives);
}


// --- Node ---
void to_json(json& j, const Node& n)
{
	j = json::object();
	write_opt(j, "name", n.name);
	write_opt(j, "mesh", n.mesh);
	write_opt(j, "matrix", n.matrix);
	write_opt(j, "translation", n.translation);
	write_opt(j, "rotation", n.rotation);
	write_opt(j, "scale", n.scale);
	if (!n.children.empty())
		j["children"] = n.children;
}
void from_json(const json& j, Node& n)
{
	read_opt(j, "name", n.name);
	read_opt(j, "mesh", n.mesh);
	read_opt(j, "matrix", n.matrix);
	read_opt(j, "translation", n.translation);
	read_opt(j, "rotation", n.rotation);
	read_opt(j, "scale", n.scale);
	read_field(j, "children", n.children);
}


// --- Scene ---
void to_json(json& j, const Scene& s)
{
	j = json::object();
	write_opt(j, "name", s.name);
	if (!s.nodes.empty())
		j["nodes"] = s.nodes;
}
void from_json(const json& j, Scene& s)
{
	read_opt(j, "name", s.name);
	read_field(j, "nodes", s.nodes);
}


// --- Root ---
void to_json(json& j, const Root& r)
{
	j = json::object();
	j["asset"] = r.asset;
	write_vec_if_nonempty(j, "extensionsUsed", r.extensionsUsed);
	write_vec_if_nonempty(j, "extensionsRequired", r.extensionsRequired);
	write_vec_if_nonempty(j, "buffers", r.buffers);
	write_vec_if_nonempty(j, "bufferViews", r.bufferViews);
	write_vec_if_nonempty(j, "accessors", r.accessors);
	write_vec_if_nonempty(j, "meshes", r.meshes);
	write_vec_if_nonempty(j, "images", r.images);
	write_vec_if_nonempty(j, "textures", r.textures);
	write_vec_if_nonempty(j, "samplers", r.samplers);
	write_vec_if_nonempty(j, "materials", r.materials);
	write_vec_if_nonempty(j, "nodes", r.nodes);
	write_vec_if_nonempty(j, "scenes", r.scenes);
	write_opt(j, "scene", r.scene);
}
void from_json(const json& j, Root& r)
{
	read_field(j, "asset", r.asset);
	read_field(j, "extensionsUsed", r.extensionsUsed);
	read_field(j, "extensionsRequired", r.extensionsRequired);
	read_field(j, "buffers", r.buffers);
	read_field(j, "bufferViews", r.bufferViews);
	read_field(j, "accessors", r.accessors);
	read_field(j, "meshes", r.meshes);
	read_field(j, "images", r.images);
	read_field(j, "textures", r.textures);
	read_field(j, "samplers", r.samplers);
	read_field(j, "materials", r.materials);
	read_field(j, "nodes", r.nodes);
	read_field(j, "scenes", r.scenes);
	read_opt(j, "scene", r.scene);
}

} // namespace GS::GLTFFormatData


// -----------------------------------------------------------------------------
// Top-level parse / serialize
// -----------------------------------------------------------------------------

bool GS::GLTFFormatData::ParseGLTFJson(const std::string& JsonText, Root& OutRoot, std::string* OutError)
{
	json parsed = json::parse(JsonText, /*cb=*/nullptr, /*allow_exceptions=*/false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		if (OutError != nullptr)
			*OutError = "JSON parse failed (malformed input)";
		return false;
	}
	from_json(parsed, OutRoot);
	return true;
}

std::string GS::GLTFFormatData::SerializeGLTFJson(const Root& InRoot, bool bPretty)
{
	json j;
	to_json(j, InRoot);
	return bPretty ? j.dump(2) : j.dump();
}


// -----------------------------------------------------------------------------
// DenseMesh <-> GLTFFormatData converters
// -----------------------------------------------------------------------------

namespace
{

// glTF requires bufferView byteOffset alignments compatible with their accessor's
// componentType (e.g. 4-byte aligned for float / uint32). Pad to 4 bytes globally
// to satisfy all the types we use (positions/normals/uvs floats, indices uint32).
inline size_t align_to_4(size_t value)
{
	return (value + 3) & ~size_t(3);
}

inline void pad_to_4(std::vector<uint8_t>& Buffer)
{
	while ((Buffer.size() & 0x3) != 0)
		Buffer.push_back(0);
}

// Append `Bytes` raw bytes from `Data` to Buffer, returning the byte offset at
// which the data was written.
inline size_t append_bytes(std::vector<uint8_t>& Buffer, const void* Data, size_t Bytes)
{
	pad_to_4(Buffer);
	size_t offset = Buffer.size();
	Buffer.resize(offset + Bytes);
	if (Bytes > 0)
		std::memcpy(Buffer.data() + offset, Data, Bytes);
	return offset;
}

// Internal affine transform used for node-hierarchy traversal. Stored as a
// 3x3 linear part (rotation*scale) plus a translation vector.
struct AffineTransform
{
	GS::Matrix3d Linear = GS::Matrix3d::Identity();
	GS::Vector3d Translation = GS::Vector3d::Zero();

	static AffineTransform Identity() { return AffineTransform{}; }

	GS::Vector3d TransformPoint(const GS::Vector3d& P) const
	{
		return Linear * P + Translation;
	}

	GS::Vector3d TransformDirection(const GS::Vector3d& V) const
	{
		// Note: mathematically, normals require the inverse-transpose of Linear
		// for non-uniform-scale correctness. For uniform scale and pure rotation
		// this reduces to Linear, then renormalize. Caller should renormalize.
		return Linear * V;
	}

	// Compose two transforms: (this * Other)(p) = this(Other(p))
	AffineTransform operator*(const AffineTransform& Other) const
	{
		AffineTransform R;
		R.Linear = Linear * Other.Linear;
		R.Translation = Linear * Other.Translation + Translation;
		return R;
	}
};

// Build an AffineTransform from a Node's matrix or TRS fields.
AffineTransform NodeLocalTransform(const Node& N)
{
	if (N.matrix.has_value()) {
		// glTF stores the 4x4 matrix in column-major order. Extract the upper
		// 3x3 (linear part) and the translation column.
		const std::array<double, 16>& M = *N.matrix;
		// Column-major: M[col*4 + row], so:
		//   col0 = M[0..3], col1 = M[4..7], col2 = M[8..11], col3 = M[12..15]
		AffineTransform T;
		T.Linear = GS::Matrix3d(
			M[0], M[4], M[8],
			M[1], M[5], M[9],
			M[2], M[6], M[10]);
		T.Translation = GS::Vector3d(M[12], M[13], M[14]);
		return T;
	}

	AffineTransform T;
	GS::Matrix3d S = GS::Matrix3d::Identity();
	if (N.scale.has_value()) {
		S = GS::Matrix3d((*N.scale)[0], (*N.scale)[1], (*N.scale)[2]);
	}
	GS::Matrix3d R = GS::Matrix3d::Identity();
	if (N.rotation.has_value()) {
		const std::array<double, 4>& Q = *N.rotation;
		GS::Quaterniond Quat((double)Q[0], (double)Q[1], (double)Q[2], (double)Q[3]);
		Quat = Quat.Normalized();
		// Build rotation matrix from quaternion by using rotated basis vectors as columns.
		R = GS::Matrix3d(Quat.AxisX(), Quat.AxisY(), Quat.AxisZ(), true /*bVectorsAreColumns*/);
	}
	T.Linear = R * S;
	if (N.translation.has_value()) {
		T.Translation = GS::Vector3d((*N.translation)[0], (*N.translation)[1], (*N.translation)[2]);
	}
	return T;
}


// Resolve an accessor + its bufferView into a typed pointer + element count.
// Returns nullptr on validation failure.
template<typename ElemType>
const ElemType* GetTypedAccessorPtr(
	const Root& InRoot,
	const std::vector<std::vector<uint8_t>>& Buffers,
	const Accessor& InAccessor,
	int ExpectedComponents,    // VEC3 -> 3, VEC2 -> 2, SCALAR -> 1
	size_t& OutElementCount)
{
	OutElementCount = 0;
	if (InAccessor.bufferView < 0 || InAccessor.bufferView >= (int)InRoot.bufferViews.size())
		return nullptr;
	const BufferView& BV = InRoot.bufferViews[InAccessor.bufferView];
	if (BV.buffer < 0 || BV.buffer >= (int)Buffers.size())
		return nullptr;
	const std::vector<uint8_t>& Buf = Buffers[BV.buffer];

	int ElemBytes = InAccessor.GetElementByteCount();
	if (ElemBytes != (int)sizeof(ElemType) * ExpectedComponents)
		return nullptr;
	if (BV.byteStride != 0 && BV.byteStride != ElemBytes)
		return nullptr; // interleaved buffers not yet supported

	size_t StartOffset = (size_t)BV.byteOffset + (size_t)InAccessor.byteOffset;
	size_t TotalBytes = (size_t)InAccessor.count * (size_t)ElemBytes;
	if (StartOffset + TotalBytes > Buf.size())
		return nullptr;

	OutElementCount = (size_t)InAccessor.count;
	return reinterpret_cast<const ElemType*>(Buf.data() + StartOffset);
}


struct PrimitiveAppendBuffers
{
	GS::Vector3d* Positions = nullptr;
	GS::Vector3f* Normals = nullptr;     // optional
	GS::Vector2f* UVs = nullptr;         // optional
	GS::Index3i* Triangles = nullptr;
	int FirstVertex = 0;
	int FirstTriangle = 0;
	int VertexCount = 0;
	int TriangleCount = 0;
};


// Read positions/normals/uvs/indices for a single primitive, push vertices and
// triangles into MeshOut starting at OutVertexBase / OutTriangleBase.
// Returns the number of triangles appended (0 on any failure).
int AppendPrimitiveToDenseMesh(
	const Root& InRoot,
	const std::vector<std::vector<uint8_t>>& Buffers,
	const Primitive& Prim,
	const AffineTransform& WorldXForm,
	bool bWantNormals,
	bool bWantUVs,
	int GroupID,
	GS::DenseMesh& MeshOut,
	int& OutVertexBase,    // in/out: next free vertex slot, advanced after appending
	int& OutTriangleBase,  // in/out: next free triangle slot, advanced after appending
	std::string* OutError)
{
	auto position_it = Prim.attributes.find(MeshAttribute_Position);
	if (position_it == Prim.attributes.end()) {
		if (OutError) *OutError = "primitive missing POSITION attribute";
		return 0;
	}
	int PosAccessorIdx = position_it->second;
	if (PosAccessorIdx < 0 || PosAccessorIdx >= (int)InRoot.accessors.size())
		return 0;
	const Accessor& PosAccessor = InRoot.accessors[PosAccessorIdx];
	size_t NumVerts = 0;
	const float* PosBuf = GetTypedAccessorPtr<float>(InRoot, Buffers, PosAccessor, 3, NumVerts);
	if (PosBuf == nullptr) return 0;

	const float* NormalBuf = nullptr;
	if (bWantNormals) {
		auto normal_it = Prim.attributes.find(MeshAttribute_Normal);
		if (normal_it != Prim.attributes.end()) {
			int NAcc = normal_it->second;
			if (NAcc >= 0 && NAcc < (int)InRoot.accessors.size()) {
				size_t NN = 0;
				const float* NB = GetTypedAccessorPtr<float>(InRoot, Buffers, InRoot.accessors[NAcc], 3, NN);
				if (NB != nullptr && NN == NumVerts) NormalBuf = NB;
			}
		}
	}

	const float* UVBuf = nullptr;
	if (bWantUVs) {
		std::string UV0 = std::string(MeshAttribute_UVPrefix) + "0";
		auto uv_it = Prim.attributes.find(UV0);
		if (uv_it != Prim.attributes.end()) {
			int UAcc = uv_it->second;
			if (UAcc >= 0 && UAcc < (int)InRoot.accessors.size()) {
				size_t NU = 0;
				const float* UB = GetTypedAccessorPtr<float>(InRoot, Buffers, InRoot.accessors[UAcc], 2, NU);
				if (UB != nullptr && NU == NumVerts) UVBuf = UB;
			}
		}
	}

	if (!Prim.indices.has_value()) return 0; // non-indexed primitives unsupported v1
	int IndAccessorIdx = *Prim.indices;
	if (IndAccessorIdx < 0 || IndAccessorIdx >= (int)InRoot.accessors.size()) return 0;
	const Accessor& IndAccessor = InRoot.accessors[IndAccessorIdx];

	size_t NumIndices = 0;
	const uint32_t* IndU32 = nullptr;
	const uint16_t* IndU16 = nullptr;
	if (IndAccessor.componentType == EComponentType::UnsignedInt) {
		IndU32 = GetTypedAccessorPtr<uint32_t>(InRoot, Buffers, IndAccessor, 1, NumIndices);
	} else if (IndAccessor.componentType == EComponentType::UnsignedShort) {
		IndU16 = GetTypedAccessorPtr<uint16_t>(InRoot, Buffers, IndAccessor, 1, NumIndices);
	} else {
		if (OutError) *OutError = "indices componentType not supported (must be UNSIGNED_INT or UNSIGNED_SHORT)";
		return 0;
	}
	if ((IndU32 == nullptr && IndU16 == nullptr) || (NumIndices % 3) != 0)
		return 0;
	int NumTris = (int)(NumIndices / 3);

	int VertexBase = OutVertexBase;
	int TriBase = OutTriangleBase;
	int NumV = (int)NumVerts;

	// Append vertex positions (apply world transform).
	for (int vi = 0; vi < NumV; ++vi) {
		GS::Vector3d Local((double)PosBuf[3*vi], (double)PosBuf[3*vi+1], (double)PosBuf[3*vi+2]);
		MeshOut.SetPosition(VertexBase + vi, WorldXForm.TransformPoint(Local));
	}

	// Append triangles + per-tri attributes.
	for (int ti = 0; ti < NumTris; ++ti) {
		uint32_t a = IndU32 ? IndU32[3*ti]   : (uint32_t)IndU16[3*ti];
		uint32_t b = IndU32 ? IndU32[3*ti+1] : (uint32_t)IndU16[3*ti+1];
		uint32_t c = IndU32 ? IndU32[3*ti+2] : (uint32_t)IndU16[3*ti+2];
		GS::Index3i TriV(VertexBase + (int)a, VertexBase + (int)b, VertexBase + (int)c);
		MeshOut.SetTriangle(TriBase + ti, TriV);
		MeshOut.SetTriGroup(TriBase + ti, GroupID);

		if (NormalBuf != nullptr) {
			GS::TriVtxNormals N;
			for (int j = 0; j < 3; ++j) {
				uint32_t vid = (j == 0) ? a : (j == 1 ? b : c);
				GS::Vector3d Local((double)NormalBuf[3*vid], (double)NormalBuf[3*vid+1], (double)NormalBuf[3*vid+2]);
				GS::Vector3d World = WorldXForm.TransformDirection(Local);
				double Len = World.Length();
				if (Len > 1e-12) World /= Len;
				N[j] = (GS::Vector3f)World;
			}
			MeshOut.SetTriVtxNormals(TriBase + ti, N);
		}

		if (UVBuf != nullptr) {
			GS::TriVtxUVs UV;
			for (int j = 0; j < 3; ++j) {
				uint32_t vid = (j == 0) ? a : (j == 1 ? b : c);
				UV[j] = GS::Vector2f(UVBuf[2*vid], UVBuf[2*vid+1]);
			}
			MeshOut.SetTriVtxUVs(TriBase + ti, UV);
		}
	}

	OutVertexBase = VertexBase + NumV;
	OutTriangleBase = TriBase + NumTris;
	return NumTris;
}


// Walk the scene graph to (a) count total verts/tris and (b) collect a list of
// (mesh-index, world-transform) pairs to extract.
struct MeshInstance
{
	int MeshIndex;
	AffineTransform WorldTransform;
};

void CollectMeshInstances(
	const Root& InRoot,
	int NodeIndex,
	const AffineTransform& Parent,
	std::vector<MeshInstance>& Out)
{
	if (NodeIndex < 0 || NodeIndex >= (int)InRoot.nodes.size())
		return;
	const Node& N = InRoot.nodes[NodeIndex];
	AffineTransform World = Parent * NodeLocalTransform(N);
	if (N.mesh.has_value()) {
		int MeshIdx = *N.mesh;
		if (MeshIdx >= 0 && MeshIdx < (int)InRoot.meshes.size())
			Out.push_back(MeshInstance{ MeshIdx, World });
	}
	for (int Child : N.children)
		CollectMeshInstances(InRoot, Child, World, Out);
}

void CountPrimitiveSize(
	const Root& InRoot,
	const std::vector<std::vector<uint8_t>>& Buffers,
	const Primitive& Prim,
	int& VertCountOut,
	int& TriCountOut)
{
	VertCountOut = 0;
	TriCountOut = 0;
	auto position_it = Prim.attributes.find(MeshAttribute_Position);
	if (position_it == Prim.attributes.end()) return;
	int Acc = position_it->second;
	if (Acc < 0 || Acc >= (int)InRoot.accessors.size()) return;
	VertCountOut = InRoot.accessors[Acc].count;

	if (!Prim.indices.has_value()) return;
	int IAcc = *Prim.indices;
	if (IAcc < 0 || IAcc >= (int)InRoot.accessors.size()) return;
	int NumIndices = InRoot.accessors[IAcc].count;
	if (NumIndices % 3 != 0) return;
	TriCountOut = NumIndices / 3;
}

} // namespace


void GS::DenseMeshToGLTFFormatData(
	const GS::DenseMesh& Mesh,
	GS::GLTFFormatData::Root& RootOut,
	std::vector<uint8_t>& BinaryBufferOut,
	bool bIncludeNormals,
	bool bIncludeUVs)
{
	using namespace GS::GLTFFormatData;

	RootOut = Root{};
	RootOut.asset.generator = "GradientspaceIO::GLTFWriter";

	int NumVerts = Mesh.GetVertexCount();
	int NumTris = Mesh.GetTriangleCount();

	// Build per-vertex flat arrays.
	std::vector<float> Positions((size_t)NumVerts * 3);
	GS::Vector3f Min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	GS::Vector3f Max(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
	for (int vi = 0; vi < NumVerts; ++vi) {
		GS::Vector3d P = Mesh.GetPosition(vi);
		float x = (float)P.X, y = (float)P.Y, z = (float)P.Z;
		Positions[3*vi] = x; Positions[3*vi+1] = y; Positions[3*vi+2] = z;
		Min.X = (x < Min.X) ? x : Min.X;
		Min.Y = (y < Min.Y) ? y : Min.Y;
		Min.Z = (z < Min.Z) ? z : Min.Z;
		Max.X = (x > Max.X) ? x : Max.X;
		Max.Y = (y > Max.Y) ? y : Max.Y;
		Max.Z = (z > Max.Z) ? z : Max.Z;
	}
	if (NumVerts == 0) {
		Min = GS::Vector3f(0, 0, 0); Max = GS::Vector3f(0, 0, 0);
	}

	// DenseMesh stores per-triangle attributes; for the export we collapse to
	// per-vertex by sampling the first incident triangle for each vertex. This
	// matches the simple case used by the C# reference and discards split-vertex
	// information at shared corners (acceptable for v1).
	std::vector<float> Normals;
	std::vector<float> UVs;
	std::vector<bool> NormalSet, UVSet;
	if (bIncludeNormals) {
		Normals.assign((size_t)NumVerts * 3, 0.0f);
		NormalSet.assign((size_t)NumVerts, false);
	}
	if (bIncludeUVs) {
		UVs.assign((size_t)NumVerts * 2, 0.0f);
		UVSet.assign((size_t)NumVerts, false);
	}

	std::vector<uint32_t> Indices((size_t)NumTris * 3);
	for (int ti = 0; ti < NumTris; ++ti) {
		GS::Index3i T = Mesh.GetTriangle(ti);
		Indices[3*ti] = (uint32_t)T.A;
		Indices[3*ti+1] = (uint32_t)T.B;
		Indices[3*ti+2] = (uint32_t)T.C;

		if (bIncludeNormals) {
			GS::TriVtxNormals N = Mesh.GetTriVtxNormals(ti);
			for (int j = 0; j < 3; ++j) {
				int vid = T[j];
				if (vid >= 0 && vid < NumVerts && !NormalSet[vid]) {
					Normals[3*vid] = N[j].X;
					Normals[3*vid+1] = N[j].Y;
					Normals[3*vid+2] = N[j].Z;
					NormalSet[vid] = true;
				}
			}
		}
		if (bIncludeUVs) {
			GS::TriVtxUVs U = Mesh.GetTriVtxUVs(ti);
			for (int j = 0; j < 3; ++j) {
				int vid = T[j];
				if (vid >= 0 && vid < NumVerts && !UVSet[vid]) {
					UVs[2*vid] = U[j].X;
					UVs[2*vid+1] = U[j].Y;
					UVSet[vid] = true;
				}
			}
		}
	}

	// Pack everything into BinaryBufferOut, building BufferView/Accessor records.
	BinaryBufferOut.clear();

	auto add_view = [&](const void* data, size_t bytes, std::optional<EBufferViewTarget> target) {
		BufferView BV;
		BV.buffer = 0;
		BV.byteOffset = (int)append_bytes(BinaryBufferOut, data, bytes);
		BV.byteLength = (int)bytes;
		BV.target = target;
		int idx = (int)RootOut.bufferViews.size();
		RootOut.bufferViews.push_back(BV);
		return idx;
	};

	// positions
	int PosViewIdx = add_view(Positions.data(), Positions.size() * sizeof(float), EBufferViewTarget::ARRAY_BUFFER);
	Accessor PosAcc;
	PosAcc.bufferView = PosViewIdx;
	PosAcc.byteOffset = 0;
	PosAcc.componentType = EComponentType::Float;
	PosAcc.count = NumVerts;
	PosAcc.type = GetElementTypeString(EElementType::Vec3);
	PosAcc.min = std::vector<float>{ Min.X, Min.Y, Min.Z };
	PosAcc.max = std::vector<float>{ Max.X, Max.Y, Max.Z };
	int PosAccIdx = (int)RootOut.accessors.size();
	RootOut.accessors.push_back(PosAcc);

	int NormalAccIdx = -1;
	if (bIncludeNormals && NumVerts > 0) {
		int NV = add_view(Normals.data(), Normals.size() * sizeof(float), EBufferViewTarget::ARRAY_BUFFER);
		Accessor A;
		A.bufferView = NV;
		A.componentType = EComponentType::Float;
		A.count = NumVerts;
		A.type = GetElementTypeString(EElementType::Vec3);
		NormalAccIdx = (int)RootOut.accessors.size();
		RootOut.accessors.push_back(A);
	}

	int UVAccIdx = -1;
	if (bIncludeUVs && NumVerts > 0) {
		int UV = add_view(UVs.data(), UVs.size() * sizeof(float), EBufferViewTarget::ARRAY_BUFFER);
		Accessor A;
		A.bufferView = UV;
		A.componentType = EComponentType::Float;
		A.count = NumVerts;
		A.type = GetElementTypeString(EElementType::Vec2);
		UVAccIdx = (int)RootOut.accessors.size();
		RootOut.accessors.push_back(A);
	}

	int IdxViewIdx = add_view(Indices.data(), Indices.size() * sizeof(uint32_t), EBufferViewTarget::ELEMENT_ARRAY_BUFFER);
	Accessor IndAcc;
	IndAcc.bufferView = IdxViewIdx;
	IndAcc.componentType = EComponentType::UnsignedInt;
	IndAcc.count = (int)Indices.size();
	IndAcc.type = GetElementTypeString(EElementType::Scalar);
	int IndAccIdx = (int)RootOut.accessors.size();
	RootOut.accessors.push_back(IndAcc);

	pad_to_4(BinaryBufferOut);

	Buffer Buf;
	Buf.byteLength = (int)BinaryBufferOut.size();
	// uri intentionally left unset; caller writes buffer payload externally
	// (sibling .bin file or GLB chunk1) and may set Buf.uri before serializing.
	RootOut.buffers.push_back(Buf);

	// Qualify GLTFFormatData::Mesh explicitly: the function's `Mesh` parameter
	// (a const DenseMesh&) shadows the unqualified type name.
	GLTFFormatData::Mesh GLTFMesh;
	Primitive Prim;
	Prim.indices = IndAccIdx;
	Prim.mode = EPrimitiveMode::Triangles;
	Prim.attributes[MeshAttribute_Position] = PosAccIdx;
	if (NormalAccIdx >= 0) Prim.attributes[MeshAttribute_Normal] = NormalAccIdx;
	if (UVAccIdx >= 0) Prim.attributes[std::string(MeshAttribute_UVPrefix) + "0"] = UVAccIdx;
	GLTFMesh.primitives.push_back(std::move(Prim));
	int MeshIdx = (int)RootOut.meshes.size();
	RootOut.meshes.push_back(std::move(GLTFMesh));

	Node N;
	N.mesh = MeshIdx;
	int NodeIdx = (int)RootOut.nodes.size();
	RootOut.nodes.push_back(std::move(N));

	Scene S;
	S.nodes.push_back(NodeIdx);
	RootOut.scenes.push_back(std::move(S));
	RootOut.scene = 0;
}


void GS::GLTFFormatDataToDenseMesh(
	const GS::GLTFFormatData::Root& InRoot,
	const std::vector<std::vector<uint8_t>>& Buffers,
	GS::DenseMesh& MeshOut,
	const GS::GLTFToDenseMeshOptions& Options)
{
	using namespace GS::GLTFFormatData;

	// Choose active scene.
	if (InRoot.scenes.empty() || InRoot.meshes.empty()) {
		MeshOut.Resize(0, 0);
		return;
	}
	int SceneIdx = InRoot.scene.value_or(0);
	if (SceneIdx < 0 || SceneIdx >= (int)InRoot.scenes.size())
		SceneIdx = 0;

	// Walk scene graph, collect (mesh-index, world-transform) pairs.
	std::vector<MeshInstance> Instances;
	for (int RootNode : InRoot.scenes[SceneIdx].nodes)
		CollectMeshInstances(InRoot, RootNode, AffineTransform::Identity(), Instances);

	// Pre-count total vertex/triangle slots so we can Resize() once.
	int TotalVerts = 0;
	int TotalTris = 0;
	for (const MeshInstance& Inst : Instances) {
		const Mesh& M = InRoot.meshes[Inst.MeshIndex];
		for (const Primitive& P : M.primitives) {
			int V = 0, T = 0;
			CountPrimitiveSize(InRoot, Buffers, P, V, T);
			TotalVerts += V;
			TotalTris += T;
		}
	}

	MeshOut.Resize(TotalVerts, TotalTris);

	bool bWantNormals = !Options.bIgnoreNormals;
	bool bWantUVs = !Options.bIgnoreUVs;

	int VertCursor = 0;
	int TriCursor = 0;
	int GroupCounter = 0;
	for (const MeshInstance& Inst : Instances) {
		const Mesh& M = InRoot.meshes[Inst.MeshIndex];
		for (const Primitive& P : M.primitives) {
			AppendPrimitiveToDenseMesh(
				InRoot, Buffers, P, Inst.WorldTransform,
				bWantNormals, bWantUVs,
				GroupCounter,
				MeshOut, VertCursor, TriCursor,
				/*OutError=*/nullptr);
			GroupCounter++;
		}
	}

	// If primitives failed to fill all preallocated slots (shouldn't normally
	// happen, but possible if a primitive was rejected mid-walk), we leave the
	// trailing default-constructed entries in place. The caller can detect via
	// triangle count vs. expected.
}
