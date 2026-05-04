// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspaceIOPlatform.h"
#include "Core/buffer_view.h"
#include "Image/GSImage.h"

namespace GS
{

enum class EReadImageResult : uint8_t
{
	Ok                 = 0,
	Error              = 1,    // unspecified failure
	InvalidInput       = 2,    // null/empty buffer, or buffer too large for the underlying decoder
	NotRequestedFormat = 3,    // signature did not match the requested format (e.g. not a PNG)
	CorruptData        = 4,    // image data is malformed
	OutOfMemory        = 5,    // decoder failed to allocate working memory
	TooLarge           = 6,    // image dimensions exceed the decoder's safe limit
};

enum class EReadImageFormat : uint8_t
{
	Unknown = 0,
	PNG     = 1,
	JPEG    = 2,
};

// Decode a PNG from the given memory buffer into ImageOut as 8-bit RGBA
// (interpreted as sRGB). On failure ImageOut is left in its prior state.
GRADIENTSPACEIO_API
EReadImageResult ReadPNG(const_buffer_view<uint8_t> Buffer, Image4b& ImageOut);

// Decode a JPEG from the given memory buffer into ImageOut as 8-bit RGBA
// (interpreted as sRGB; alpha is set to 255). On failure ImageOut is left
// in its prior state.
GRADIENTSPACEIO_API
EReadImageResult ReadJPG(const_buffer_view<uint8_t> Buffer, Image4b& ImageOut);

// Detect the format of the given memory buffer (by signature) and dispatch
// to the appropriate decoder. FormatOut is set to the detected format on
// success, or to Unknown if the signature did not match any supported
// format (in which case NotRequestedFormat is returned). On any other
// failure FormatOut still reflects the detected format.
GRADIENTSPACEIO_API
EReadImageResult ReadImage(
	const_buffer_view<uint8_t> Buffer,
	Image4b& ImageOut,
	EReadImageFormat& FormatOut);

}  // end namespace GS
