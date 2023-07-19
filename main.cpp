#define _CRT_SECURE_NO_WARNINGS // for stb

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <vector>
#include <cmath>

// Settings
static const char* c_bnImage = "bn.png";
static const char* c_splatImage = "star.png";
static const size_t c_renderSize = 512;
static const size_t c_destImageSize = 512;
static const float c_radiusInUVs = 0.03f;

// Globals. meh.
int bnw, bnh, bnc;
unsigned char* bnPixels;

int splatw, splath, splatc;
unsigned char* splatPixels;

std::vector<float> renderPixels;

inline float Lerp(float A, float B, float t)
{
	return A * (1.0f - t) + B * t;
}

inline float Fract(float x)
{
	return x - std::floor(x);
}

void Splat(float u, float v, float color)
{
	const int splatRenderSize = int(c_radiusInUVs * float(c_renderSize));
	const int splatRenderSizeHalf = splatRenderSize / 2;

	for (int splatY = 0; splatY < splatRenderSize; ++splatY)
	{
		float splatV = (float(splatY) + 0.5f) / float(splatRenderSize);
		float splatReadY = splatV * splath;
		float splatReadFractY = Fract(splatReadY);

		int destY = (int(v * float(c_renderSize)) + splatY - splatRenderSizeHalf + c_renderSize) % c_renderSize;

		for (int splatX = 0; splatX < splatRenderSize; ++splatX)
		{
			float splatU = (float(splatX) + 0.5f) / float(splatRenderSize);
			float splatReadX = splatU * splatw;
			float splatReadFractX = Fract(splatReadX);

			int destX = (int(u * float(c_renderSize)) + splatX - splatRenderSizeHalf + c_renderSize) % c_renderSize;

			// bilinear interpolation
			float splatColor00 = float(splatPixels[((int(splatReadY + 0) % splath) * splatw + (int(splatReadX + 0) % splatw)) * splatc]) / 255.0f;
			float splatColor01 = float(splatPixels[((int(splatReadY + 0) % splath) * splatw + (int(splatReadX + 1) % splatw)) * splatc]) / 255.0f;
			float splatColor10 = float(splatPixels[((int(splatReadY + 1) % splath) * splatw + (int(splatReadX + 0) % splatw)) * splatc]) / 255.0f;
			float splatColor11 = float(splatPixels[((int(splatReadY + 1) % splath) * splatw + (int(splatReadX + 1) % splatw)) * splatc]) / 255.0f;

			float splatColor0x = Lerp(splatColor00, splatColor01, splatReadFractX);
			float splatColor1x = Lerp(splatColor10, splatColor11, splatReadFractX);

			float splatColor = Lerp(splatColor0x, splatColor1x, splatReadFractY);

			renderPixels[destY * c_renderSize + destX] += splatColor * color;
		}
	}
}

int main(int argc, char** argv)
{
	// load the blue noise texture
	bnPixels = stbi_load(c_bnImage, &bnw, &bnh, &bnc, 0);
	if (!bnPixels)
		return 1;

	// load the splat image	
	splatPixels = stbi_load(c_splatImage, &splatw, &splath, &splatc, 0);
	if (!splatPixels)
		return 1;

	// splat the shapes, using the noise pixels as the color for each splat
	renderPixels.resize(c_renderSize * c_renderSize, 0.0f);
	for (int iy = 0; iy < bnh; ++iy)
	{
		float v = (float(iy) + 0.5f) / float(bnh);
		for (int ix = 0; ix < bnh; ++ix)
		{
			float u = (float(ix) + 0.5f) / float(bnw);

			float color = float(bnPixels[(iy * bnw + ix) * bnc]) / 255.0f;

			Splat(u, v, color);
		}
	}

	// normalize image
	{
		float maxValue = 0.0f;
		for (float f : renderPixels)
			maxValue = std::max(maxValue, f);

		for (float& f : renderPixels)
			f /= maxValue;
	}

	// write the output hdr image
	stbi_write_hdr("out.hdr", c_renderSize, c_renderSize, 1, renderPixels.data());

	// make U8 image
	std::vector<unsigned char> outputPixels(c_destImageSize * c_destImageSize, 0);
	for (int iy = 0; iy < c_destImageSize; ++iy)
	{
		int minRenderY = (iy * c_renderSize) / c_destImageSize;
		int maxRenderY = ((iy+1) * c_renderSize) / c_destImageSize;

		for (int ix = 0; ix < c_destImageSize; ++ix)
		{
			int minRenderX = (ix * c_renderSize) / c_destImageSize;
			int maxRenderX = ((ix + 1) * c_renderSize) / c_destImageSize;

			float value = 0.0f;
			float weight = 0.0f;
			for (int renderY = minRenderY; renderY < maxRenderY; ++renderY)
			{
				for (int renderX = minRenderX; renderX < maxRenderX; ++renderX)
				{
					value += renderPixels[renderY * c_renderSize + renderX];
					weight += 1.0f;
				}
			}

			value /= weight;
			if (value < 0.0f)
				value = 0.0f;
			if (value > 1.0f)
				value = 1.0f;

			outputPixels[iy * c_destImageSize + ix] = (unsigned char)(value * 255.0f);
		}
	}

	// write the output png image
	stbi_write_png("out.png", c_destImageSize, c_destImageSize, 1, outputPixels.data(), 0);

	stbi_image_free(splatPixels);
	stbi_image_free(bnPixels);
	return 0;
}

// Not the results i expected.