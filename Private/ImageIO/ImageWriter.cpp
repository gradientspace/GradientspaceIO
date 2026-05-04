// Copyright Gradientspace Corp. All Rights Reserved.
#include "ImageIO/ImageWriter.h"

// Use stb_image_write only via the to_func memory APIs; disable stdio paths
// so the file-based variants and HDR (which requires stdio) are dropped.
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb_image_write.h"


using namespace GS;


namespace
{

// stb_image_write callback: appends written bytes to a std::vector<uint8_t>
// supplied via context. stb invokes this many times during encoding.
void AppendToVector(void* Context, void* Data, int Size)
{
	if (Size <= 0 || Data == nullptr || Context == nullptr) return;
	auto* Buffer = static_cast<std::vector<uint8_t>*>(Context);
	const uint8_t* Bytes = static_cast<const uint8_t*>(Data);
	Buffer->insert(Buffer->end(), Bytes, Bytes + Size);
}

EWriteImageResult ValidateImage(const Image4b& Image)
{
	if (!Image.bIsInitialized())
		return EWriteImageResult::InvalidInput;
	// Image4b stores Color4b (standard-layout {R,G,B,A} bytes) which matches
	// stb_image_write's expected RGBA stream — but only when Format is RGBA8.
	if (Image.Format != EGSPixelFormat::RGBA8)
		return EWriteImageResult::InvalidInput;
	return EWriteImageResult::Ok;
}

}  // anonymous namespace


EWriteImageResult GS::WritePNG(const Image4b& Image, std::vector<uint8_t>& BufferOut)
{
	BufferOut.clear();
	if (EWriteImageResult R = ValidateImage(Image); R != EWriteImageResult::Ok)
		return R;

	const int W = Image.Width();
	const int H = Image.Height();
	const int Stride = W * 4;
	int Result = stbi_write_png_to_func(
		&AppendToVector, &BufferOut,
		W, H, /*comp=*/4,
		Image.Pixels.raw_pointer(), Stride);
	if (Result == 0)
	{
		BufferOut.clear();
		return EWriteImageResult::EncodingFailed;
	}
	return EWriteImageResult::Ok;
}


EWriteImageResult GS::WriteJPG(const Image4b& Image, std::vector<uint8_t>& BufferOut, int Quality)
{
	BufferOut.clear();
	if (EWriteImageResult R = ValidateImage(Image); R != EWriteImageResult::Ok)
		return R;

	if (Quality < 1)   Quality = 1;
	if (Quality > 100) Quality = 100;

	int Result = stbi_write_jpg_to_func(
		&AppendToVector, &BufferOut,
		Image.Width(), Image.Height(), /*comp=*/4,
		Image.Pixels.raw_pointer(), Quality);
	if (Result == 0)
	{
		BufferOut.clear();
		return EWriteImageResult::EncodingFailed;
	}
	return EWriteImageResult::Ok;
}


EWriteImageResult GS::WriteImage(
	const Image4b& Image,
	std::vector<uint8_t>& BufferOut,
	EWriteImageFormat Format,
	int JpegQuality)
{
	switch (Format)
	{
	case EWriteImageFormat::PNG:
		return GS::WritePNG(Image, BufferOut);
	case EWriteImageFormat::JPEG:
		return GS::WriteJPG(Image, BufferOut, JpegQuality);
	case EWriteImageFormat::Unknown:
	default:
		BufferOut.clear();
		return EWriteImageResult::UnsupportedFormat;
	}
}
