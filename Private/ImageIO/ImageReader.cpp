// Copyright Gradientspace Corp. All Rights Reserved.
#include "ImageIO/ImageReader.h"

#include <climits>
#include <cstring>

// Use stb_image only via memory buffers; disable stdio paths and limit
// the compiled decoder to the formats we expose so unused format code is dropped.
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"


using namespace GS;


namespace
{

// stb_image only signals failure via NULL return + a thread-local short
// reason string, so this is the only way to distinguish error categories.
EReadImageResult ClassifySTBFailure(const char* Reason)
{
	if (Reason == nullptr)
		return EReadImageResult::Error;
	if (std::strcmp(Reason, "bad png sig") == 0)
		return EReadImageResult::NotRequestedFormat;
	if (std::strcmp(Reason, "no SOI") == 0)
		return EReadImageResult::NotRequestedFormat;
	if (std::strcmp(Reason, "outofmem") == 0)
		return EReadImageResult::OutOfMemory;
	if (std::strcmp(Reason, "too large") == 0)
		return EReadImageResult::TooLarge;
	return EReadImageResult::CorruptData;
}

bool HasPNGSignature(const uint8_t* Data, size_t Size)
{
	static const uint8_t Sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (Size < sizeof(Sig)) return false;
	return std::memcmp(Data, Sig, sizeof(Sig)) == 0;
}

bool HasJPEGSignature(const uint8_t* Data, size_t Size)
{
	// JPEG starts with the SOI marker FF D8, immediately followed by another
	// FF marker byte (e.g. APP0 = E0, APP1 = E1, ...).
	if (Size < 3) return false;
	return Data[0] == 0xFF && Data[1] == 0xD8 && Data[2] == 0xFF;
}

EReadImageResult DecodeRGBAFromMemory(const_buffer_view<uint8_t> Buffer, Image4b& ImageOut)
{
	int Width = 0, Height = 0, ChannelsInFile = 0;
	stbi_uc* Data = stbi_load_from_memory(
		&Buffer[0], (int)Buffer.size(),
		&Width, &Height, &ChannelsInFile, /*desired_channels=*/4);
	if (Data == nullptr)
		return ClassifySTBFailure(stbi_failure_reason());

	EImageInitResult InitResult = ImageOut.Initialize(
		Width, Height, EGSPixelFormat::RGBA8, /*bIsSRGB=*/true);
	if (InitResult == EImageInitResult::Ok || InitResult == EImageInitResult::Converted)
	{
		// Color4b is standard-layout {R,G,B,A} bytes, matching stb's RGBA stream.
		std::memcpy(ImageOut.Pixels.raw_pointer(), Data,
			(size_t)Width * (size_t)Height * 4);
	}

	stbi_image_free(Data);

	if (InitResult == EImageInitResult::InvalidConversion ||
		InitResult == EImageInitResult::InvalidDimensions)
		return EReadImageResult::Error;
	return EReadImageResult::Ok;
}

}  // anonymous namespace


namespace
{

EReadImageResult ValidateBuffer(const_buffer_view<uint8_t> Buffer)
{
	if (Buffer.is_empty())
		return EReadImageResult::InvalidInput;
	// stb_image's memory API takes a 32-bit length
	if (Buffer.size() > (size_t)INT_MAX)
		return EReadImageResult::InvalidInput;
	return EReadImageResult::Ok;
}

EReadImageFormat DetectFormat(const uint8_t* Data, size_t Size)
{
	if (HasPNGSignature(Data, Size)) return EReadImageFormat::PNG;
	if (HasJPEGSignature(Data, Size)) return EReadImageFormat::JPEG;
	return EReadImageFormat::Unknown;
}

}  // anonymous namespace


EReadImageResult GS::ReadPNG(const_buffer_view<uint8_t> Buffer, Image4b& ImageOut)
{
	if (EReadImageResult R = ValidateBuffer(Buffer); R != EReadImageResult::Ok)
		return R;
	if (!HasPNGSignature(&Buffer[0], Buffer.size()))
		return EReadImageResult::NotRequestedFormat;
	return DecodeRGBAFromMemory(Buffer, ImageOut);
}


EReadImageResult GS::ReadJPG(const_buffer_view<uint8_t> Buffer, Image4b& ImageOut)
{
	if (EReadImageResult R = ValidateBuffer(Buffer); R != EReadImageResult::Ok)
		return R;
	if (!HasJPEGSignature(&Buffer[0], Buffer.size()))
		return EReadImageResult::NotRequestedFormat;
	return DecodeRGBAFromMemory(Buffer, ImageOut);
}


EReadImageResult GS::ReadImage(
	const_buffer_view<uint8_t> Buffer,
	Image4b& ImageOut,
	EReadImageFormat& FormatOut)
{
	FormatOut = EReadImageFormat::Unknown;
	if (EReadImageResult R = ValidateBuffer(Buffer); R != EReadImageResult::Ok)
		return R;

	FormatOut = DetectFormat(&Buffer[0], Buffer.size());
	if (FormatOut == EReadImageFormat::Unknown)
		return EReadImageResult::NotRequestedFormat;

	return DecodeRGBAFromMemory(Buffer, ImageOut);
}
