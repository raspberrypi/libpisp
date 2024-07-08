/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_utils.cpp - PiSP buffer helper utilities
 */
#include <cmath>
#include <cstdint>

#include "backend/pisp_be_config.h"

#include "pisp_common.h"
#include "logging.hpp"

namespace libpisp
{

uint32_t compute_x_offset(uint32_t /* pisp_image_format */ format, int x)
{
	PISP_ASSERT(x >= 0 && x < 65536);
	uint32_t x_offset = 0;
	uint32_t bps = format & PISP_IMAGE_FORMAT_BPS_MASK;

	// HoG features are slightly different from the rest.
	if (PISP_IMAGE_FORMAT_HOG(format))
	{
		 // x here is in units of cells.
		 // Output 16-bit word samples per bin. This is then packed to:
		 // 32-bytes for an unsigned histogram cell.
		 // 48-bytes for a signed histogram cell.
		x_offset = x * ((format & PISP_IMAGE_FORMAT_HOG_UNSIGNED) ? 32 : 48);
	}
	else if (format & (PISP_IMAGE_FORMAT_INTEGRAL_IMAGE | PISP_IMAGE_FORMAT_BPP_32))
	{
		// 32-bit words per sample.
		x_offset = x * 4;
	}
	else
	{
		if (bps == PISP_IMAGE_FORMAT_BPS_16)
			x_offset = x * 2;
		else if (bps == PISP_IMAGE_FORMAT_BPS_12)
			x_offset = (x * 3 + 1) / 2;
		else if (bps == PISP_IMAGE_FORMAT_BPS_10)
			x_offset = (x / 3) * 4;
		else
			x_offset = x;

		if ((format & PISP_IMAGE_FORMAT_THREE_CHANNEL) && PISP_IMAGE_FORMAT_INTERLEAVED(format))
		{
			if (PISP_IMAGE_FORMAT_SAMPLING_422(format))
				x_offset *= 2;
			else
				x_offset *= 3;
		}
	}
	return x_offset;
}

void compute_stride_align(pisp_image_format_config &config, int align)
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

	int32_t computed_stride = compute_x_offset(config.format, width);
	if (!config.stride || config.stride < computed_stride)
		config.stride = computed_stride;
	config.stride2 = 0;

	if (!PISP_IMAGE_FORMAT_HOG(config.format))
	{
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
	}
}

void compute_stride(pisp_image_format_config &config)
{
	// Our preferred alignment is really 64 bytes, though 16 should work too. Use 16 for now, as it gives better test coverage.
	compute_stride_align(config, PISP_BACK_END_OUTPUT_MIN_ALIGN);
}

void compute_optimal_stride(pisp_image_format_config &config)
{
	// Use our preferred alignment of 64 bytes.
	compute_stride_align(config, PISP_BACK_END_OUTPUT_MAX_ALIGN);
}

void compute_addr_offset(const pisp_image_format_config &config, int x, int y, uint32_t *addr_offset,
						 uint32_t *addr_offset2)
{
	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format))
	{
		int pixels_in_roll =
			PISP_IMAGE_FORMAT_BPS_8(config.format)
				? PISP_WALLPAPER_WIDTH
				: (PISP_IMAGE_FORMAT_BPS_16(config.format) ? PISP_WALLPAPER_WIDTH / 2 : PISP_WALLPAPER_WIDTH / 4 * 3);
		int pixel_offset_in_roll = x % pixels_in_roll;
		int pixel_offset_in_bytes;

		if (PISP_IMAGE_FORMAT_BPS_8(config.format))
			pixel_offset_in_bytes = pixel_offset_in_roll;
		else if (PISP_IMAGE_FORMAT_BPS_16(config.format))
			pixel_offset_in_bytes = pixel_offset_in_roll * 2;
		else
		{
			// 10-bit format. Whinge if not a multiple of 3 into the roll.
			PISP_ASSERT(pixel_offset_in_roll % 3 == 0);
			pixel_offset_in_bytes = pixel_offset_in_roll / 3 * 4;
		}

		int num_rolls = x / pixels_in_roll;
		*addr_offset = num_rolls * config.stride + y * PISP_WALLPAPER_WIDTH + pixel_offset_in_bytes;
		if (PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			*addr_offset2 = num_rolls * config.stride2 + y / 2 * PISP_WALLPAPER_WIDTH + pixel_offset_in_bytes;
		else
			*addr_offset2 = *addr_offset;

		return;
	}

	uint32_t x_byte_offset = compute_x_offset(config.format, x);
	*addr_offset = y * config.stride + x_byte_offset;
	if (addr_offset2 && !PISP_IMAGE_FORMAT_INTERLEAVED(config.format))
	{
		if (PISP_IMAGE_FORMAT_SAMPLING_420(config.format))
			y /= 2;

		if (PISP_IMAGE_FORMAT_PLANAR(config.format) && !PISP_IMAGE_FORMAT_SAMPLING_444(config.format))
			x_byte_offset /= 2;

		*addr_offset2 = y * config.stride2 + x_byte_offset;
	}
}

int num_planes(pisp_image_format format)
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

std::size_t get_plane_size(const pisp_image_format_config &config, int plane)
{
	uint64_t stride = std::abs(plane ? config.stride2 : config.stride); // in case vflipped?
	uint64_t plane_size = 0;

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format))
	{
		int pixels_in_roll =
			PISP_IMAGE_FORMAT_BPS_8(config.format)
				? PISP_WALLPAPER_WIDTH
				: (PISP_IMAGE_FORMAT_BPS_16(config.format) ? PISP_WALLPAPER_WIDTH / 2 : PISP_WALLPAPER_WIDTH / 4 * 3);
		std::size_t num_rolls = (config.width + pixels_in_roll - 1) / pixels_in_roll;
		plane_size = num_rolls * stride;
	}
	else
	{
		std::size_t height = plane && PISP_IMAGE_FORMAT_SAMPLING_420(config.format) ? config.height >> 1
																					: config.height;
		plane_size = height * stride;
	}

	return plane_size >= (1ULL << 32) ? 0 : plane_size;
}

} // namespace libpisp
