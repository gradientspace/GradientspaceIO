// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Image/GSImage.h"

#include <vector>

namespace GS
{

enum class EWriteImageResult : uint8_t
{
	Ok                = 0,
	Error             = 1,    // unspecified failure
	InvalidInput      = 2,    // image not initialized, zero-sized, or pixel format unsupported by writer
	UnsupportedFormat = 3,    // requested EWriteImageFormat is Unknown / not a writable format
	EncodingFailed    = 4,    // encoder failed internally (e.g. allocation, image too large)
};

enum class EWriteImageFormat : uint8_t
{
	Unknown = 0,
	PNG     = 1,
	JPEG    = 2,
};

// Encode Image as a PNG into BufferOut. BufferOut is cleared at entry; on
// failure it is left empty. Image must be RGBA8 (Image4b's native layout);
// the bIsSRGB flag is not signaled in the resulting PNG file.
GRADIENTSPACEIO_API
EWriteImageResult WritePNG(const Image4b& Image, std::vector<uint8_t>& BufferOut);

// Encode Image as a JPEG into BufferOut at the given Quality (1..100, clamped).
// The alpha channel is discarded by the JPEG encoder. BufferOut is cleared
// at entry; on failure it is left empty.
GRADIENTSPACEIO_API
EWriteImageResult WriteJPG(const Image4b& Image, std::vector<uint8_t>& BufferOut, int Quality = 90);

// Encode Image into BufferOut in the requested Format. Returns
// UnsupportedFormat if Format is Unknown or otherwise not writable.
// JpegQuality is honored only when Format is JPEG.
GRADIENTSPACEIO_API
EWriteImageResult WriteImage(
	const Image4b& Image,
	std::vector<uint8_t>& BufferOut,
	EWriteImageFormat Format,
	int JpegQuality = 90);

}  // end namespace GS
