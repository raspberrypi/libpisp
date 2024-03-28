/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_utils.cpp - PiSP buffer helper utilities
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

#include "backend/pisp_be_config.h"

#include "pisp_common.h"
#include "logging.hpp"

namespace libpisp
{

uint32_t computeXOffset(uint32_t /* pisp_image_format */ format, int x)
{
	PISP_ASSERT(x >= 0 && x < 65536);
	PISP_ASSERT(!PISP_IMAGE_FORMAT_HOG(format)); // HOG is deprecated everywhere

	uint32_t xOffset = 0;

	// HoG features are slightly different from the rest.
	if (PISP_IMAGE_FORMAT_HOG(format))
	{
		 // x here is in units of cells.
		 // Output 16-bit word samples per bin. This is then packed to:
		 // 32-bytes for an unsigned histogram cell.
		 // 48-bytes for a signed histogram cell.
		xOffset = x * ((format & PISP_IMAGE_FORMAT_HOG_UNSIGNED) ? 32 : 48);
	}
	else if (format & (PISP_IMAGE_FORMAT_INTEGRAL_IMAGE | PISP_IMAGE_FORMAT_BPP_32))
	{
		// 32-bit words per sample.
		xOffset = x * 4;
	}
	else
	{
		// First, convert from columns to samples.
		if (PISP_IMAGE_FORMAT_THREE_CHANNEL(format) && PISP_IMAGE_FORMAT_INTERLEAVED(format))
			x *= (PISP_IMAGE_FORMAT_SAMPLING_422(format)) ? 2 : 3;

		// For the benefit of stride calculations, round up to the next byte-packing boundary.
		switch (format & PISP_IMAGE_FORMAT_BPS_MASK)
		{
		case PISP_IMAGE_FORMAT_BPS_8:
			xOffset = x;
			break;
		case PISP_IMAGE_FORMAT_BPS_10:
			// NOTE: We don't currently define any 10bps multi-channel format that are NOT wallpaper,
			// and this function is not currently called for wallpaper. It's conceivable that we might
			// add 10bps multi-channel non-wallpaper formats in future, but we'd then need to specify
			// which are tightly-packed (MIPI-style) and which are loosely-packed (wallpaper-style).
			// Let's not pre-empt those decisions now. 10-bit raw Bayer is always tightly-packed.
			PISP_ASSERT(PISP_IMAGE_FORMAT_SINGLE_CHANNEL(format));
			xOffset = ((x + 3) >> 2) * 5;
			break;
		case PISP_IMAGE_FORMAT_BPS_12:
			xOffset = ((x + 1) >> 1) * 3;
			break;
		case PISP_IMAGE_FORMAT_BPS_14:
			xOffset = ((x + 3) >> 2) * 7;
			break;
		default:
			xOffset = 2 * x;
		}
	}

	return xOffset;
}

void computeStrideAlign(pisp_image_format_config &config, int align, bool preserveSubsampleRatio)
{
	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format))
	{
		config.stride2 = config.stride = config.height * PISP_WALLPAPER_WIDTH;
		if (PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			config.stride2 /= 2;
		return;
	}

	uint16_t width = config.width;
	if (PISP_IMAGE_FORMAT_COMPRESSED(config.format))
		width = (width + 7) & ~7; // compression uses blocks of 8 samples

	int32_t computedStride = computeXOffset(config.format, width);
	if (!config.stride || config.stride < computedStride)
		config.stride = computedStride;
	config.stride2 = 0;

	switch (config.format & PISP_IMAGE_FORMAT_PLANARITY_MASK)
	{
	case PISP_IMAGE_FORMAT_PLANARITY_PLANAR:
		if (PISP_IMAGE_FORMAT_SAMPLING_422(config.format) || PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			config.stride2 = config.stride >> 1;
		else if (PISP_IMAGE_FORMAT_THREE_CHANNEL(config.format))
			config.stride2 = config.stride;
		break;
	case PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR:
		PISP_ASSERT(PISP_IMAGE_FORMAT_SAMPLING_422(config.format) || PISP_IMAGE_FORMAT_SAMPLING_420(config.format));
		config.stride2 = config.stride;
		break;
	}

	// image in memory must be sufficiently aligned
	config.stride = (config.stride + align - 1) & ~(align - 1);
	config.stride2 = (config.stride2 + align - 1) & ~(align - 1);

	// For YUV420/422 formats, ensure the stride ratio matches the subample ratio for the planes.
	if (preserveSubsampleRatio && PISP_IMAGE_FORMAT_PLANAR(config.format) &&
		(PISP_IMAGE_FORMAT_SAMPLING_422(config.format) || PISP_IMAGE_FORMAT_SAMPLING_420(config.format)))
		config.stride = config.stride2 << 1;

	// image in memory must be sufficiently aligned
	config.stride = (config.stride + align - 1) & ~(align - 1);
	config.stride2 = (config.stride2 + align - 1) & ~(align - 1);
}

void computeStride(pisp_image_format_config &config, bool preserveSubsampleRatio)
{
	// Our preferred alignment is really 64 bytes, though 16 should work too. Use 16 for now, as it gives better test coverage.
	computeStrideAlign(config, PISP_BACK_END_OUTPUT_MIN_ALIGN, preserveSubsampleRatio);
}

void computeOptimalStride(pisp_image_format_config &config, bool preserveSubsampleRatio)
{
	// Use our preferred alignment of 64 bytes.
	computeStrideAlign(config, PISP_BACK_END_OUTPUT_MAX_ALIGN, preserveSubsampleRatio);
}

void computeOptimalStride(pisp_image_format_config &config)
{
	computeOptimalStride(config, false);
}

void computeAddrOffset(const pisp_image_format_config &config, int x, int y, uint32_t *addrOffset,
						 uint32_t *addrOffset2)
{
	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format))
	{
		int pixelsInRoll =
			PISP_IMAGE_FORMAT_BPS_8(config.format)
				? PISP_WALLPAPER_WIDTH
				: (PISP_IMAGE_FORMAT_BPS_16(config.format) ? PISP_WALLPAPER_WIDTH / 2 : PISP_WALLPAPER_WIDTH / 4 * 3);
		int pixelOffsetInRoll = x % pixelsInRoll;
		int pixelOffsetInBytes;

		if (PISP_IMAGE_FORMAT_BPS_8(config.format))
			pixelOffsetInBytes = pixelOffsetInRoll;
		else if (PISP_IMAGE_FORMAT_BPS_16(config.format))
			pixelOffsetInBytes = pixelOffsetInRoll * 2;
		else
		{
			// 10-bit format. Whinge if not a multiple of 3 into the roll.
			PISP_ASSERT(pixelOffsetInRoll % 3 == 0);
			pixelOffsetInBytes = pixelOffsetInRoll / 3 * 4;
		}

		int numRolls = x / pixelsInRoll;
		*addrOffset = numRolls * config.stride + y * PISP_WALLPAPER_WIDTH + pixelOffsetInBytes;
		if (PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			*addrOffset2 = numRolls * config.stride2 + y / 2 * PISP_WALLPAPER_WIDTH + pixelOffsetInBytes;
		else
			*addrOffset2 = *addrOffset;

		return;
	}

	uint32_t xByteOffset = computeXOffset(config.format, x);
	*addrOffset = y * config.stride + xByteOffset;
	if (addrOffset2 && !PISP_IMAGE_FORMAT_INTERLEAVED(config.format))
	{
		if (PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			y /= 2;

		if (PISP_IMAGE_FORMAT_PLANAR(config.format) && !PISP_IMAGE_FORMAT_SAMPLING_444(config.format))
			xByteOffset /= 2;

		*addrOffset2 = y * config.stride2 + xByteOffset;
	}
}

int numPlanes(pisp_image_format format)
{
	int planes = 1;

	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(format))
	{
		switch (format & PISP_IMAGE_FORMAT_PLANARITY_MASK)
		{
		case PISP_IMAGE_FORMAT_PLANARITY_INTERLEAVED:
			planes = 1;
			break;
		case PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR:
			planes = 2;
			break;
		case PISP_IMAGE_FORMAT_PLANARITY_PLANAR:
			planes = 3;
			break;
		}
	}

	return planes;
}

std::size_t getPlaneSize(const pisp_image_format_config &config, int plane)
{
	uint64_t stride = std::abs(plane ? config.stride2 : config.stride); // in case vflipped?
	uint64_t planeSize = 0;

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format))
	{
		int pixelsInRoll =
			PISP_IMAGE_FORMAT_BPS_8(config.format)
				? PISP_WALLPAPER_WIDTH
				: (PISP_IMAGE_FORMAT_BPS_16(config.format) ? PISP_WALLPAPER_WIDTH / 2 : PISP_WALLPAPER_WIDTH / 4 * 3);
		std::size_t numRolls = (config.width + pixelsInRoll - 1) / pixelsInRoll;
		planeSize = numRolls * stride;
	}
	else
	{
		std::size_t height = plane && PISP_IMAGE_FORMAT_SAMPLING_420(config.format) ? config.height >> 1
																					: config.height;
		planeSize = height * stride;
	}

	return planeSize >= (1ULL << 32) ? 0 : planeSize;
}

static const std::map<std::string, uint32_t> &formatsTable()
{
	// Note that alternate names and plane orderings are not defined to keep a 1:1 mapping.
	static const std::map<std::string, uint32_t> formats = {
		{ "YUV444P", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_444 +
						PISP_IMAGE_FORMAT_PLANARITY_PLANAR },
		{ "YUV422P", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_422 +
						PISP_IMAGE_FORMAT_PLANARITY_PLANAR },
		{ "YUV420P", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_420 +
						PISP_IMAGE_FORMAT_PLANARITY_PLANAR },
		{ "NV12", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_420 +
					  PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR },
		{ "NV21", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_420 +
					  PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR + PISP_IMAGE_FORMAT_ORDER_SWAPPED },
		{ "YUYV", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_422 +
					  PISP_IMAGE_FORMAT_PLANARITY_INTERLEAVED },
		{ "UYVY", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_422 +
					  PISP_IMAGE_FORMAT_PLANARITY_INTERLEAVED + PISP_IMAGE_FORMAT_ORDER_SWAPPED },
		{ "NV16", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_422 +
					  PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR },
		{ "NV61", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_8 + PISP_IMAGE_FORMAT_SAMPLING_422 +
					  PISP_IMAGE_FORMAT_PLANARITY_SEMI_PLANAR + PISP_IMAGE_FORMAT_ORDER_SWAPPED },
		{ "RGB888", PISP_IMAGE_FORMAT_THREE_CHANNEL },
		{ "RGBX8888", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPP_32 },
		{ "RGB161616", PISP_IMAGE_FORMAT_THREE_CHANNEL + PISP_IMAGE_FORMAT_BPS_16 },
		{ "BAYER16", PISP_IMAGE_FORMAT_BPS_16 + PISP_IMAGE_FORMAT_UNCOMPRESSED },
		{ "PISP_COMP1", PISP_IMAGE_FORMAT_COMPRESSION_MODE_1 },
		{ "PISP_COMP2", PISP_IMAGE_FORMAT_COMPRESSION_MODE_2 },
	};

	return formats;
}

unsigned int getPispImageFormat(const std::string &format)
{
	auto it = formatsTable().find(format);
	if (it == formatsTable().end())
		return 0;

	return it->second;
}

std::string getPispImageFormat(uint32_t format)
{
	// Remove shift from the format assignment, its value does not change format.
	format = format & ~PISP_IMAGE_FORMAT_SHIFT_MASK;

	const auto &fmts = formatsTable();
	auto it = std::find_if(fmts.begin(), fmts.end(), [format](const auto &f) { return f.second == format; });
	if (it == fmts.end())
		return {};

	return it->first;
}

} // namespace libpisp
