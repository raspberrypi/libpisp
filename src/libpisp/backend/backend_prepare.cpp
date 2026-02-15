
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend_prepare.cpp - PiSP Back End configuration generation
 */
#include "backend.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "common/logging.hpp"
#include "common/utils.hpp"
#include "tiling/types.hpp"

using namespace libpisp;

namespace
{

// Limit this to a sensible size
constexpr unsigned int MaxStripeHeight = 3072;
// Precision for the scaler blocks
constexpr unsigned int ScalePrecision = 12;
constexpr unsigned int PhasePrecision = 12;
constexpr unsigned int UnityScale = 1 << ScalePrecision;
constexpr unsigned int UnityPhase = 1 << PhasePrecision;
// PPF properties
constexpr unsigned int ScaleStartContext = 2;
constexpr unsigned int ResamplePrecision = 10;
constexpr unsigned int NumPhases = 16;
constexpr unsigned int NumTaps = 6;

void check_stride(ImageFormatConfig const &config)
{
	if (config.stride % PISP_BACK_END_OUTPUT_MIN_ALIGN || config.stride2 % PISP_BACK_END_OUTPUT_MIN_ALIGN)
		throw std::runtime_error("Output stride values not sufficiently aligned");

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format) && (config.stride % 128 || config.stride2 % 128))
		throw std::runtime_error("Wallpaper format should have 128-byte aligned rolls");

	ImageFormatConfig check = config;
	compute_stride_align(check, PISP_BACK_END_OUTPUT_MIN_ALIGN);
	if (check.stride > config.stride || check.stride2 > config.stride2)
	{
		PISP_LOG(fatal, "Strides should be at least " << check.stride << " and " << check.stride2 << " but are "
													  << config.stride << " and " << config.stride2);
	}
}

// TDN and Stitch I/O dimensions must match the input, though the format may differ.
static void check_raw_io_format(ImageFormatConfig &fmt, uint16_t w, uint16_t h)
{
	if (fmt.width == 0 || fmt.height == 0)
	{
		fmt.width = w;
		fmt.height = h;
	}
	else if (fmt.width != w || fmt.height != h)
		throw std::runtime_error("BackEnd::finalise: Image dimensions do not match input");

	if (fmt.stride == 0)
		compute_stride(fmt);
	else
		check_stride(fmt);
}
unsigned int get_pixel_alignment(uint32_t format, int byte_alignment)
{
	int alignment_pixels;

	if (PISP_IMAGE_FORMAT_BPP_32(format))
		alignment_pixels = byte_alignment / 4;
	else if (PISP_IMAGE_FORMAT_BPS_16(format))
		alignment_pixels = byte_alignment / 2;
	else if (PISP_IMAGE_FORMAT_BPS_8(format))
		alignment_pixels = byte_alignment;
	else
	{
		int p = 3, b = 4; // for 10-bit wallpaper, 3 pixels occupy 4 bytes
		if (PISP_IMAGE_FORMAT_BPS_14(format))
			p = 4, b = 7;
		else if (PISP_IMAGE_FORMAT_BPS_12(format))
			p = 2, b = 3;
		else if (PISP_IMAGE_FORMAT_BPS_10(format) && PISP_IMAGE_FORMAT_SINGLE_CHANNEL(format))
			p = 4, b = 5;
		else if (!PISP_IMAGE_FORMAT_BPS_10(format) || !PISP_IMAGE_FORMAT_WALLPAPER(format) ||
				 !PISP_IMAGE_FORMAT_SEMIPLANAR(format))
			throw std::runtime_error("Undefined image format");

		alignment_pixels = byte_alignment * p;
		if (alignment_pixels % b == 0)
			alignment_pixels /= b;
	}

	if (PISP_IMAGE_FORMAT_PLANAR(format) && !PISP_IMAGE_FORMAT_SAMPLING_444(format))
		alignment_pixels *= 2; // the UV planes in fully planar 420/422 output will have half the width
	else if (PISP_IMAGE_FORMAT_INTERLEAVED(format) && PISP_IMAGE_FORMAT_SAMPLING_422(format) && !(alignment_pixels & 1))
		alignment_pixels >>= 1; // YUYV type outputs need only 8 pixels to make 16 bytes

	return alignment_pixels;
}

unsigned int lcm(int a, int b)
{
	int orig_a = a, orig_b = b, tmp;
	while (b)
		tmp = a % b, a = b, b = tmp;
	return orig_a / a * orig_b;
}

static tiling::Length2 calculate_output_alignment(uint32_t format, int align = PISP_BACK_END_OUTPUT_MAX_ALIGN)
{
	int y_alignment = PISP_IMAGE_FORMAT_SAMPLING_420(format) ? 2 : 1;

	return tiling::Length2(get_pixel_alignment(format, align), y_alignment);
}

void calculate_input_addr_offset(int x, int y, ImageFormatConfig const &input_format, uint32_t *addr_offset,
								 uint32_t *addr_offset2 = nullptr)
{
	uint32_t offset2 = 0;

	compute_addr_offset(input_format, x, y, addr_offset, &offset2);
	if (addr_offset2)
		*addr_offset2 = offset2;
}

} // namespace

void BackEnd::finalise_bayer_rgb_inputs() const 
{
	const ImageFormatConfig &config = config_->get_input_format();

	if (config.width < PISP_BACK_END_MIN_TILE_WIDTH || config.height < PISP_BACK_END_MIN_TILE_HEIGHT)
		throw std::runtime_error("finalise_bayer_rgb_inputs: input image too small");
}

void BackEnd::finalise_inputs() const
{
	const ImageFormatConfig &input_format = config_->get_input_format();

	// Not so much finalising, just checking that input dimensions and strides are OK.
	if (config_->get_global().bayer_enables & BayerEnable::Input)
	{
		if ((input_format.width & 1) || (input_format.height & 1))
			throw std::runtime_error("finalise_inputs: Bayer pipe image dimensions must be even");
		if (input_format.stride & 15)
			throw std::runtime_error("finalise_inputs: input stride should be at least 16-byte aligned");
	}
	else if (config_->get_global().rgb_enables & RgbEnable::Input)
	{
		if (PISP_IMAGE_FORMAT_SAMPLING_420(input_format.format) && (input_format.height & 1))
			throw std::runtime_error("finalise_inputs: 420 input height must be even");
		else if ((PISP_IMAGE_FORMAT_SAMPLING_420(input_format.format) ||
				  PISP_IMAGE_FORMAT_SAMPLING_422(input_format.format)) &&
				 (input_format.width & 1))
			throw std::runtime_error("finalise_inputs: 420/422 input width must be even");
		if (PISP_IMAGE_FORMAT_WALLPAPER(input_format.format))
		{
			if ((input_format.stride & 127) || (input_format.stride2 & 127))
				throw std::runtime_error("finalise_inputs: wallpaper format strides must be at least 128-byte aligned");
		}
		else if ((input_format.stride & 15) || (input_format.stride2 & 15))
			throw std::runtime_error("finalise_inputs: input strides must be at least 16-byte aligned");
	}
}

void BackEnd::finalise_lsc()
{
	BeLscConfig lsc = config_->get_lsc();
	uint16_t width = post_transpose_width(), height = post_transpose_height();

	// Just a warning that ACLS algorithms might want the grid calculations here to match the AWB/ACLS stats.
	static const int P = PISP_BE_LSC_STEP_PRECISION;

	if (lsc.grid_step_x == 0)
		lsc.grid_step_x = (PISP_BE_LSC_GRID_SIZE << P) / width;
	if (lsc.grid_step_y == 0)
		lsc.grid_step_y = (PISP_BE_LSC_GRID_SIZE << P) / height;

	PISP_ASSERT(lsc.grid_step_x * (width + lsc.offset_x - 1) < (PISP_BE_LSC_GRID_SIZE << P));
	PISP_ASSERT(lsc.grid_step_y * (height + lsc.offset_y - 1) < (PISP_BE_LSC_GRID_SIZE << P));
	
	config_->set_lsc(lsc);
}

void BackEnd::finalise_cac()
{
	BeCacConfig cac = config_->get_cac();
	uint16_t width = post_transpose_width(), height = post_transpose_height();

	static const int P = PISP_BE_CAC_STEP_PRECISION;

	if (cac.grid_step_x == 0)
		cac.grid_step_x = (PISP_BE_CAC_GRID_SIZE << P) / width;
	if (cac.grid_step_y == 0)
		cac.grid_step_y = (PISP_BE_CAC_GRID_SIZE << P) / height;

	PISP_ASSERT(cac.grid_step_x * (width + cac.offset_x - 1) < (PISP_BE_CAC_GRID_SIZE << P));
	PISP_ASSERT(cac.grid_step_y * (height + cac.offset_y - 1) < (PISP_BE_CAC_GRID_SIZE << P));
	
	config_->set_cac(cac);
}

void BackEnd::finalise_dedistort()
{
	BeDedistortionConfig dd = config_->get_dedistort();
	uint16_t width = post_transpose_width(), height = post_transpose_height();

	static constexpr int P = PISP_BE_DDX_STEP_PRECISION;

	if (dd.grid_step_x == 0)
		dd.grid_step_x = (PISP_BE_DDX_GRID_SIZE << P) / width;
	if (dd.grid_step_y == 0)
		dd.grid_step_y = (PISP_BE_DDX_GRID_SIZE << P) / height;

	PISP_ASSERT(dd.grid_step_x * (width + dd.offset_x - 1) < (PISP_BE_DDX_GRID_SIZE << P));
	PISP_ASSERT(dd.grid_step_y * (height + dd.offset_y - 1) < (PISP_BE_DDX_GRID_SIZE << P));

	config_->set_dedistort(dd);
}

void BackEnd::finalise_tonemap2()
{
	const ImageFormatConfig &format = config_->get_tonemap2_format();
	BeTonemap2Config tonemap = config_->get_tonemap2();
	uint16_t width = post_transpose_width(), height = post_transpose_height();

	static const int P = PISP_BE_TONEMAP2_STEP_PRECISION;

	if (tonemap.grid_step_x == 0)
		tonemap.grid_step_x = (PISP_BE_TONEMAP2_GRID_SIZE << P) / width;
	if (tonemap.grid_step_y == 0)
		tonemap.grid_step_y = (PISP_BE_TONEMAP2_GRID_SIZE << P) / height;

	if (tonemap.lores_step_x == 0 && width != 0)
		tonemap.lores_step_x = std::max(1, (format.width << PISP_BE_TONEMAP2_LORES_PRECISION) / width);
	if (tonemap.lores_step_y == 0 && height != 0)
		tonemap.lores_step_y = std::max(1, (format.height << PISP_BE_TONEMAP2_LORES_PRECISION) / height);

	if (tonemap.spatial_gain_slope == 0 && tonemap.spatial_gain_knee != 0)
	{
		if (65535 - tonemap.spatial_gain_knee < 32768)
			throw std::runtime_error("finalise_tonemap: spatial_gain_knee too high");
		tonemap.spatial_gain_slope = (1UL << 31) / (65535 - tonemap.spatial_gain_knee);
	}

	config_->set_tonemap2(tonemap);
}

void BackEnd::finalise_resample(unsigned int index, uint16_t width, uint16_t height)
{
	BeResampleConfig resample = config_->get_resample(index);

	uint32_t scale_factor_h = ((width - 1) << ScalePrecision) / (resample.scaled_width - 1);
	uint32_t scale_factor_v = ((height - 1) << ScalePrecision) / (resample.scaled_height - 1);

	if ((scale_factor_h < UnityScale / 16 || scale_factor_h >= 16 * UnityScale) ||
		(scale_factor_v < UnityScale / 16 || scale_factor_v >= 16 * UnityScale))
		throw std::runtime_error("finalise_resample: Invalid scaling factors (must be < 16x down/upscale).");

	resample.scale_factor_h = scale_factor_h;
	resample.scale_factor_v = scale_factor_v;
	// If the filter coefficients are unset we should probably copy in our "default ones".
	
	config_->set_resample(index, resample);
}

void BackEnd::finalise_downscale(unsigned int index, uint16_t width, uint16_t height)
{
	BeDownscaleConfig downscale = config_->get_downscale(index);

	PISP_LOG(debug, "width " << width << " scaled_width " << downscale.scaled_width);
	PISP_LOG(debug, "height " << height << " scaled_height " << downscale.scaled_height);

	uint32_t scale_factor_h = (width << ScalePrecision) / (downscale.scaled_width);
	uint32_t scale_factor_v = (height << ScalePrecision) / (downscale.scaled_height);

	if ((scale_factor_h != UnityScale && (scale_factor_h < 2 * UnityScale || scale_factor_h > 8 * UnityScale)) ||
		(scale_factor_v != UnityScale && (scale_factor_v < 2 * UnityScale || scale_factor_v > 8 * UnityScale)))
		throw std::runtime_error("finalise_downscale: Invalid scaling factors (must be 1x or >= 2x && <= 8x).");

	downscale.scale_factor_h = scale_factor_h;
	downscale.scale_factor_v = scale_factor_v;
	downscale.scale_recip_h = (downscale.scaled_width << ScalePrecision) / (width);
	downscale.scale_recip_v = (downscale.scaled_height << ScalePrecision) / (height);
	
	config_->set_downscale(index, downscale);

	PISP_LOG(debug, "scale_factor_h " << downscale.scale_factor_h << " scale_factor_v " << downscale.scale_factor_v);
	PISP_LOG(debug, "scale_recip_h " << downscale.scale_recip_h << " scale_recip_v " << downscale.scale_recip_v);
}

void BackEnd::finalise_decompression() const
{
	uint32_t fmt = config_->get_input_format().format;
	BayerEnables bayer_enables = config_->get_global().bayer_enables;

	if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !(bayer_enables & BayerEnable::Decompress))
		throw std::runtime_error("BackEnd::finalise: input compressed but decompression not enabled");

	if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && (bayer_enables & BayerEnable::Decompress))
		throw std::runtime_error("BackEnd::finalise: input uncompressed but decompression enabled");

	if ((bayer_enables & BayerEnable::Decompress) && !PISP_IMAGE_FORMAT_BPS_8(fmt))
		throw std::runtime_error("BackEnd::finalise: compressed input is not 8bpp");
}

void BackEnd::finalise_tdn()
{
	const BeTdnConfig &tdn = config_->get_tdn();
	ImageFormatConfig tdn_input_format = config_->get_tdn_input_format();
	ImageFormatConfig tdn_output_format = config_->get_tdn_output_format();
	const BayerEnables bayer_enables = config_->get_global().bayer_enables;
	
	bool tdn_enabled = (bayer_enables & BayerEnable::Tdn);
	int tdn_input_enabled = bayer_enables & BayerEnable::TdnInput;
	int tdn_decompress_enabled = bayer_enables & BayerEnable::TdnDecompress;
	int tdn_compress_enabled = bayer_enables & BayerEnable::TdnCompress;
	int tdn_output_enabled = bayer_enables & BayerEnable::TdnOutput;
	uint32_t fmt = tdn_output_format.format;

	if (tdn_enabled && !tdn_output_enabled)
		throw std::runtime_error("finalise_tdn: TDN output not enabled when TDN enabled");

	if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !tdn_compress_enabled)
		throw std::runtime_error("finalise_tdn: TDN output compressed but compression not enabled");

	if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && tdn_compress_enabled)
		throw std::runtime_error("finalise_tdn: TDN output uncompressed but compression enabled");

	if (tdn_compress_enabled && !PISP_IMAGE_FORMAT_BPS_8(fmt))
		throw std::runtime_error("finalise_tdn: TDN output does not match compression mode");

	if (tdn_output_enabled)
        check_raw_io_format(tdn_output_format, post_transpose_width(), post_transpose_height());

	if (tdn_input_enabled)
		check_raw_io_format(tdn_output_format, post_transpose_width(), post_transpose_height());

	if (!tdn_enabled)
	{
		if (tdn_input_enabled)
			throw std::runtime_error("finalise_tdn: TDN input enabled but TDN not enabled");
		// I suppose there is a weird (and entirely pointless) case where TDN is not enabled but TDN output is, which we allow.
	}
	else if (tdn.reset)
	{
		if (tdn_input_enabled)
			throw std::runtime_error("finalise_tdn: TDN input enabled but TDN being reset");
	}
	else
	{
		if (!tdn_input_enabled)
			throw std::runtime_error("finalise_tdn: TDN input not enabled but TDN not being reset");
		// Make the TDN input match the output if it's unset. Usually this will be the sensible thing to do.
		if (tdn_input_format.width == 0 && tdn_input_format.height == 0)
			tdn_input_format = tdn_output_format;
		if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !tdn_decompress_enabled)
			throw std::runtime_error("finalise_tdn: TDN input compressed but decompression not enabled");
		if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && tdn_decompress_enabled)
			throw std::runtime_error("finalise_tdn: TDN input uncompressed but decompression enabled");
		if (tdn_compress_enabled && !PISP_IMAGE_FORMAT_BPS_8(fmt))
			throw std::runtime_error("finalise_tdn: TDN output does not match compression mode");
	}
	
	config_->set_tdn_input_format(tdn_input_format);
	config_->set_tdn_output_format(tdn_output_format);
}

void BackEnd::finalise_stitch()
{
	BeStitchConfig stitch = config_->get_stitch();
	ImageFormatConfig stitch_input_format = config_->get_stitch_input_format();
	ImageFormatConfig stitch_output_format = config_->get_stitch_output_format();
	const BayerEnables bayer_enables = config_->get_global().bayer_enables;
	
	bool stitch_enabled = bayer_enables & BayerEnable::Stitch;
	bool stitch_input_enabled = bayer_enables & BayerEnable::StitchInput;
	bool stitch_decompress_enabled = bayer_enables & BayerEnable::StitchDecompress;
	bool stitch_compress_enabled = bayer_enables & BayerEnable::StitchCompress;
	bool stitch_output_enabled = bayer_enables & BayerEnable::StitchOutput;
	uint32_t input_fmt = stitch_input_format.format;
	uint32_t output_fmt = stitch_output_format.format;

	if (stitch_enabled != stitch_input_enabled)
		throw std::runtime_error("BackEnd::finalise: stitch and stitch_input should be enabled/disabled together");
	if (stitch_input_enabled && PISP_IMAGE_FORMAT_COMPRESSED(input_fmt) && !stitch_decompress_enabled)
		throw std::runtime_error("BackEnd::finalise: stitch output compressed but decompression not enabled");
	if (stitch_input_enabled && !PISP_IMAGE_FORMAT_COMPRESSED(input_fmt) && stitch_decompress_enabled)
		throw std::runtime_error("BackEnd::finalise: stitch output uncompressed but decompression enabled");
	if (stitch_output_enabled && PISP_IMAGE_FORMAT_COMPRESSED(output_fmt) && !stitch_compress_enabled)
		throw std::runtime_error("BackEnd::finalise: stitch output compressed but compression not enabled");
	if (stitch_output_enabled && !PISP_IMAGE_FORMAT_COMPRESSED(output_fmt) && stitch_compress_enabled)
		throw std::runtime_error("BackEnd::finalise: stitch output uncompressed but compression enabled");
	if (stitch_decompress_enabled && !PISP_IMAGE_FORMAT_BPS_8(input_fmt))
		throw std::runtime_error("BackEnd::finalise: stitch input does not match compression mode");
	if (stitch_compress_enabled && !PISP_IMAGE_FORMAT_BPS_8(output_fmt))
		throw std::runtime_error("BackEnd::finalise: stitch output does not match compression mode");

	if (stitch_output_enabled)
		check_raw_io_format(stitch_output_format, post_transpose_width(), post_transpose_height());

	if (stitch_input_enabled)
		check_raw_io_format(stitch_input_format, post_transpose_width(), post_transpose_height());

	// Compute the motion_threshold reciprocal if it hasn't been done.
	if (variant_ <= BCM2712_D0 && stitch.motion_threshold_recip == 0)
	{
		if (stitch.motion_threshold_256 == 0)
			stitch.motion_threshold_recip = 255;
		else
			// We round the result up where possible as the block may work (ever so slightly) better like this.
			stitch.motion_threshold_recip =
				std::min(255, (256 + (int)stitch.motion_threshold_256 - 1) / stitch.motion_threshold_256);
	}

	config_->set_stitch_input_format(stitch_input_format);
	config_->set_stitch_output_format(stitch_output_format);
	config_->set_stitch(stitch);
}

void BackEnd::finalise_output(unsigned int index)
{
	BeOutputFormatConfig config = config_->get_output_format(index);
	BeCompressConfig compress = config_->get_output_compress(index);

	// If the high clipping bound is zero assume it wasn't set and the intention is that no clipping occurs.
	if (config.hi == 0)
		config.hi = 65535;
	if (config.hi2 == 0)
		config.hi2 = 65535;

	// Do some checking on output image dimensions and strides.
	if (config.image.width < PISP_BACK_END_MIN_TILE_WIDTH || config.image.height < PISP_BACK_END_MIN_TILE_HEIGHT)
		throw std::runtime_error("finalise_output: output image too small");

	if (PISP_IMAGE_FORMAT_SAMPLING_420(config.image.format) && (config.image.height & 1))
		throw std::runtime_error("finalise_output: 420 image height should be even");

	if ((PISP_IMAGE_FORMAT_SAMPLING_420(config.image.format) || PISP_IMAGE_FORMAT_SAMPLING_422(config.image.format)) &&
		!PISP_IMAGE_FORMAT_INTERLEAVED(config.image.format) && (config.image.width & 1))
		throw std::runtime_error("finalise_output: 420/422 image width should be even");

	if (PISP_IMAGE_FORMAT_COMPRESSED(config.image.format))
	{
		// If compression mode was not configured, assume it's the same as the image format
		if (compress.mode == 0)
			compress.mode = ((config.image.format & PISP_IMAGE_FORMAT_COMPRESSION_MODE_1) ? 1 : 0) +
							((config.image.format & PISP_IMAGE_FORMAT_COMPRESSION_MODE_2) ? 2 : 0);

		if (config.image.format & ~(PISP_IMAGE_FORMAT_COMPRESSION_MODE_1 | PISP_IMAGE_FORMAT_COMPRESSION_MODE_2))
			throw std::runtime_error("finalise_output: compressed image must be simple 8bpp");
		if (!(config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Compress0, index)))
			throw std::runtime_error("finalise_output: compressed output does not enable compression");
		
		config_->set_output_compress(index, compress);
	}
	else if (config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Compress0, index))
		throw std::runtime_error("finalise_output: compression enabled for non-compressed output");

	config_->set_output_format(index, config);

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.image.format))
	{
		if ((config.image.stride & 127) || (config.image.stride2 & 127))
			throw std::runtime_error("finalise_output: wallpaper image stride should be at least 128-byte aligned");
	}
	else if ((config.image.stride & 15) || (config.image.stride2 & 15))
		throw std::runtime_error("finalise_output: image stride should be at least 16-byte aligned");
}

void BackEnd::check_tiles(const TilingConfig &tiling_config)
{
	for (unsigned int tile_num = 0; tile_num < pipe_->num_tiles_x * pipe_->num_tiles_y; tile_num++)
	{
		const BeTile &tile = config_->get_tile(tile_num);

		// zero inputs shouldn't be possible
		PISP_ASSERT(tile.input_width && tile.input_height);
		if (variant_ >= BERYL)
			PISP_ASSERT(tile.dedistorted_width);

		if (tile.input_width < PISP_BACK_END_MIN_TILE_WIDTH || tile.input_height < PISP_BACK_END_MIN_TILE_HEIGHT)
			throw std::runtime_error("Tile too small at input");

		if (variant_ >= BERYL && tile.dedistorted_width < PISP_BACK_END_MIN_TILE_WIDTH)
			throw std::runtime_error("Tile too small after dedistort");

		for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
		{
			if ((config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Output0, i)) == 0)
				continue;

			unsigned int width_after_crop = (variant_ >= BERYL) ? tile.crop_width[i] : (tile.input_width - tile.crop_x_start[i] - tile.crop_x_end[i]);
			unsigned int height_after_crop = (variant_ >= BERYL) ? tile.crop_height[i] : (tile.input_height - tile.crop_y_start[i] - tile.crop_y_end[i]);

			// A tile that gets cropped away completely can't produce output, and vice versa.
			PISP_ASSERT((width_after_crop * height_after_crop == 0) ==
						(tile.output_width[i] * tile.output_height[i] == 0));

			// A zero-sized tile is legitimate meaning "no output", but otherwise minimum tile sizes must be respected.
			if (width_after_crop && height_after_crop)
			{
				bool rh_edge = tile.output_offset_x[i] + tile.output_width[i] == tiling_config.output_image_size[i].dx;

				if (width_after_crop < PISP_BACK_END_MIN_TILE_WIDTH)
				{
					PISP_LOG(warning, "Tile narrow after crop: tile " << tile_num << " output " << i
						 << " input_width " << (variant_ >= BERYL ? tile.dedistorted_width : tile.input_width)
						 << " after_crop " << width_after_crop
						 << " crop start " << tile.crop_x_start[i]
						 << " width " << (variant_ >= BERYL ? tile.crop_width[i] : tile.crop_x_end[i]));
					if (!rh_edge)
						throw std::runtime_error("Tile width too small after crop");
				}
				if (height_after_crop < PISP_BACK_END_MIN_TILE_HEIGHT)
					throw std::runtime_error("Tile height too small after crop");

				if (tile.resample_in_width[i] < PISP_BACK_END_MIN_TILE_WIDTH)
				{
					PISP_LOG(warning, "Tile narrow after downscale: tile " << tile_num << " output " << i
						 << " input_width " << tile.dedistorted_width << " after_crop " << width_after_crop
						 << " after downscale " << tile.resample_in_width[i]);
					if (!rh_edge)
						throw std::runtime_error("Tile width too small after downscale");
				}

				if (tile.resample_in_height[i] < PISP_BACK_END_MIN_TILE_HEIGHT)
					throw std::runtime_error("Tile height too small after downscale");

				if (!rh_edge && tile.output_width[i] < PISP_BACK_END_MIN_TILE_WIDTH)
					throw std::runtime_error("Tile width too small at output");
				if (tile.output_height[i] < PISP_BACK_END_MIN_TILE_HEIGHT)
					throw std::runtime_error("Tile height too small at output");
			}
		}
		tile_num++;
	}
}

tiling::Length2 BackEnd::calculate_input_alignment() const
{
	const ImageFormatConfig &input_format = config_->get_input_format();
	const ImageFormatConfig &tdn_input_format = config_->get_tdn_input_format();
	const ImageFormatConfig &tdn_output_format = config_->get_tdn_output_format();
	const ImageFormatConfig &stitch_input_format = config_->get_stitch_input_format();
	const ImageFormatConfig &stitch_output_format = config_->get_stitch_output_format();
	BayerEnables bayer_enables = config_->get_global().bayer_enables;

	if (config_->get_global().rgb_enables & RgbEnable::Input)
	{
		PISP_LOG(debug, "RGB input enabled");
		// Need 4 byte alignment AND even number of pixels. Height must be 2 row aligned only for 420 input.
		return tiling::Length2(lcm(get_pixel_alignment(input_format.format, PISP_BACK_END_INPUT_ALIGN), 2),
							   PISP_IMAGE_FORMAT_SAMPLING_420(input_format.format) ? 2 : 1);
	}

	// For starters, we need 4 *byte* alignment (this automatically cover 2 *pixel* alignment for all the raw formats).
	int pixel_alignment = get_pixel_alignment(input_format.format, PISP_BACK_END_INPUT_ALIGN);

	// If any input is compressed, we need 8 *pixel* alignment. (Ignore input when transposing, we deal with that later.)
	if ((PISP_IMAGE_FORMAT_COMPRESSED(input_format.format) && !(bayer_enables & BayerEnable::Transpose)) ||
		((bayer_enables & BayerEnable::TdnInput) && PISP_IMAGE_FORMAT_COMPRESSED(tdn_input_format.format)) ||
		((bayer_enables & BayerEnable::StitchInput) && PISP_IMAGE_FORMAT_COMPRESSED(stitch_input_format.format)))
		pixel_alignment = lcm(pixel_alignment, PISP_BACK_END_COMPRESSED_ALIGN);

	// If any of the Bayer outputs are enabled, those need 16 *byte* alignment. (This already covers the outputs being compressed.)
	if (bayer_enables & BayerEnable::TdnOutput)
		pixel_alignment =
			lcm(pixel_alignment, get_pixel_alignment(tdn_output_format.format, PISP_BACK_END_OUTPUT_MIN_ALIGN));
	if (bayer_enables & BayerEnable::StitchOutput)
		pixel_alignment =
			lcm(pixel_alignment, get_pixel_alignment(stitch_output_format.format, PISP_BACK_END_OUTPUT_MIN_ALIGN));

	// Bayer input rows always in pairs; but when transposing, the input-in-memory should be highly aligned
	return tiling::Length2(pixel_alignment, (bayer_enables & BayerEnable::Transpose) ? 64 : 2);
}

void BackEnd::finalise_config()
{
	BayerEnables bayer_enables = config_->get_global().bayer_enables;
	RgbEnables rgb_enables = config_->get_global().rgb_enables;

	BayerEnables dirty_flags_bayer = pipe_->dirty_flags_bayer & bayer_enables; // only finalise blocks that are dirty *and* enabled
	RgbEnables dirty_flags_rgb = pipe_->dirty_flags_rgb & rgb_enables; // only finalise blocks that are dirty *and* enabled

	if ((dirty_flags_bayer & BayerEnable::Input) || (dirty_flags_rgb & RgbEnable::Input))
		finalise_bayer_rgb_inputs();

	if (dirty_flags_bayer & BayerEnable::Input)
		finalise_inputs();

	if (dirty_flags_bayer & (BayerEnable::Input | BayerEnable::Decompress))
		finalise_decompression();

	if ((pipe_->dirty_flags_bayer &
		 (BayerEnable::Tdn | BayerEnable::TdnInput | BayerEnable::TdnDecompress |
		  BayerEnable::TdnCompress | BayerEnable::TdnOutput)))
	{
		finalise_tdn();
	}

	if (pipe_->dirty_flags_bayer &
		(BayerEnable::Stitch | BayerEnable::StitchInput | BayerEnable::StitchDecompress |
		 BayerEnable::StitchCompress | BayerEnable::StitchOutput))
	{
		finalise_stitch();
	}

	if (dirty_flags_bayer & BayerEnable::Lsc)
		finalise_lsc();

	if (dirty_flags_bayer & BayerEnable::Cac)
		finalise_cac();

	if (dirty_flags_rgb & RgbEnable::Dedistort)
		finalise_dedistort();
		
	if (dirty_flags_bayer & BayerEnable::Tonemap2)
		finalise_tonemap2();

	for (unsigned int j = 0; j < variant_.back_end_num_branches(0); j++)
	{
		bool enabled = rgb_enables & get_flag_for_branch(RgbEnable::Output0, j);

		if (enabled)
		{
			// crop is enabled when it contains non-zero width/height
			uint16_t w = config_->get_crop(j).width ? config_->get_crop(j).width : post_transpose_width();
			uint16_t h = config_->get_crop(j).width ? config_->get_crop(j).height : post_transpose_height();

			if (dirty_flags_rgb & get_flag_for_branch(RgbEnable::Downscale0, j))
			{
				if (variant_.back_end_downscaler_available(0, j))
					finalise_downscale(j, w, h);
				else
					throw std::runtime_error("Downscale is not available in output branch " + std::to_string(j));
			}

			if (rgb_enables & get_flag_for_branch(RgbEnable::Downscale0, j))
			{
				// If the downscale is enabled, we update the input width/height for the resample stage.
				w = config_->get_downscale(j).scaled_width;
				h = config_->get_downscale(j).scaled_height;
			}

			if (dirty_flags_rgb & get_flag_for_branch(RgbEnable::Resample0, j))
				finalise_resample(j, w, h);

			if (dirty_flags_rgb & get_flag_for_branch(RgbEnable::Output0, j))
				finalise_output(j);
		}
	}
	// Finally check for a sane collection of enable bits.
	if (!((bayer_enables & BayerEnable::Input) || (bayer_enables== BayerEnable::None)))
		throw std::runtime_error("BackEnd::finalise: Bayer input disabled but Bayer pipe active");

	if (!(bayer_enables & BayerEnable::Input) && !(rgb_enables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::finalise: exactly one of Bayer and RGB inputs should be enabled");

	BayerEnables bayer_outputs_enabled = bayer_enables & (BayerEnable::TdnOutput | BayerEnable::StitchOutput);
	RgbEnables rgb_outputs_enabled = RgbEnable::None;
	for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
		rgb_outputs_enabled |= rgb_enables & get_flag_for_branch(RgbEnable::Output0, i);

	if (rgb_outputs_enabled == RgbEnable::None && bayer_outputs_enabled == BayerEnable::None)
		throw std::runtime_error("BackEnd::finalise: PiSP not configured to do anything");
}

void BackEnd::update_smart_resize()
{
	BeGlobal global = config_->get_global();
	std::string filter;

	// Look through the output branches adjusting the scaling blocks where "smart resizing"
	// has been requested.
	for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
	{
		// First get the size of the input to the rescalers. The crops are zero when not in use.
		// Although dedistort can change tile width, assume it does not change full-image dimensions.
		uint16_t input_width = config_->get_crop(i).width;
		if (!input_width)
			input_width = post_transpose_width();
		uint16_t input_height = config_->get_crop(i).height;
		if (!input_height)
			input_height = post_transpose_height();

		if ((pipe_->smart_resize_dirty & (1 << i)) || (pipe_->dirty_flags_extra & DirtyFlag::Crop))
		{
			if (pipe_->smart_resize[i].width && pipe_->smart_resize[i].height)
			{
			uint16_t resampler_input_width = input_width;
			uint16_t resampler_input_height = input_height;
				uint16_t resampler_output_width = pipe_->smart_resize[i].width;
				uint16_t resampler_output_height = pipe_->smart_resize[i].height;

				PISP_LOG(debug, "Smart resize branch " << i << " input size " << input_width << " x " << input_height
													   << " output size " << pipe_->smart_resize[i].width << " x "
													   << pipe_->smart_resize[i].height);

				// We're doing to use the downscaler if it's available and we're downscaling
				// by more than 2x.
				// \todo - increase this "2x" threshold by using different resampler kernels.
				if (variant_.back_end_downscaler_available(0, i) &&
					(resampler_output_width * 2 < input_width || resampler_output_height * 2 < input_height))
				{
					uint16_t downscaler_output_width = input_width;
					uint16_t downscaler_output_height = input_height;

					// We treat the width and height the same. Look at the width first.
					if (resampler_output_width * 2 < input_width)
					{
						// Try to put 2x downscale into the resampler, everything else into
						// the downscaler. But remember that it must do *at least* 2x, and no
						// more than 8x (being careful to round that limit up)..
						downscaler_output_width = std::clamp(resampler_output_width * 2,
															 (input_width + 7) / 8,
															 input_width / 2);
					}
					// Now the same for the height.
					if (resampler_output_height * 2 < input_height)
					{
						// Try to put 2x downscale into the resampler, everything else into
						// the downscaler. But remember that it must do *at least* 2x and no
						// more than 8x (being careful to round that limit up)..
						downscaler_output_height = std::clamp(resampler_output_height * 2,
															  (input_height + 7) / 8,
															  input_height / 2);
					}

					PISP_LOG(debug, "Using downscaler, output size "
							 << downscaler_output_width << " x " <<  downscaler_output_height);

					// Now program up the downscaler.
					BeDownscaleConfig downscale = config_->get_downscale(i);
					downscale.scaled_width = downscaler_output_width;
					downscale.scaled_height = downscaler_output_height;
					config_->set_downscale(i, downscale);
					global.rgb_enables |= get_flag_for_branch(RgbEnable::Downscale0, i);

					// Now adjust the input dimensions that the code below (which
					// programs the resampler) will see, so that it can, if necessary,
					// do its PPF coefficient adjustment.
					resampler_input_width = downscaler_output_width;
					resampler_input_height = downscaler_output_height;
				}
				else
				{
					global.rgb_enables &= ~get_flag_for_branch(RgbEnable::Downscale0, i);
				}

				// Don't resample by unity: it needlessly reduces bit-depth, and can over-sharpen
				if (resampler_input_width == resampler_output_width &&
					resampler_input_height == resampler_output_height)
				{
					global.rgb_enables &= ~get_flag_for_branch(RgbEnable::Resample0, i);
					continue;
				}

				// Finally program up the resampler block.
				// If the following conditions are met:
				//
				// - The x and y scale factors are the same,
				// - We are downscaling in both directions by a factor of > 2,
				// - The downscale factor is *smaller* than the number of filter taps - 1,
				//
				// then we can use the PPF as a trapezoidal downscaler by setting up
				// the filter coefficients (per used phase) correctly. This improves
				// on image quality for larger downscale factors.
				double scale_factor_x = (double)(resampler_input_width - 1) / (resampler_output_width - 1);
				double scale_factor_y = (double)(resampler_input_height - 1) / (resampler_output_height - 1);
				BeResampleConfig resample = config_->get_resample(i);

				if (scale_factor_x > 2.1 &&
					scale_factor_x < scale_factor_y * 1.1 && scale_factor_y < scale_factor_x * 1.1)
				{
					PISP_LOG(debug, "Setting the PPF as a trapezoidal filter");

					scale_factor_x = std::min<double>(scale_factor_x, NumTaps - 1);
					// Zero initialise the coefficient optional and return the array reference.
					auto &coef = resample.coef.emplace();

					for (unsigned int p = 0; p < NumPhases; p++)
					{
						// Initial phase for the current pixel (offset 2 in the filter)
						// is calculated as 1 - p / NumPhases
						coef[p * NumTaps + 0] = (1 << ResamplePrecision) - (p << ResamplePrecision) / NumPhases;
						coef[p * NumTaps + 0] /= scale_factor_x;

						double scale = scale_factor_x - (1.0 - (double)p / NumPhases);
						for (unsigned int t = 1; t < 1 + std::ceil(scale_factor_x); t++)
						{
							double s = std::min(1.0, scale);
							coef[p * NumTaps + t] = s * (1 << ResamplePrecision) / scale_factor_x;
							scale -= s;
						}
					}
				}
				else
				{
					// Let's choose a resampling filter based on the scaling factor.
					// The selection mapping is defined in the config json file.
					// TODO is this variant check needed?
					initialise_resample(resample, scale_factor_x);
				}

				// Last thing is to set the output dimensions.
				resample.scaled_width = resampler_output_width;
				resample.scaled_height = resampler_output_height;
				set_resample(i, resample);
				global.rgb_enables |= get_flag_for_branch(RgbEnable::Resample0, i);
			}
		}
	}

	set_global(global);
	pipe_->smart_resize_dirty = 0;
}

void BackEnd::update_tiles()
{
	if (pipe_->retile)
	{
		TilingConfig tiling_config;
		RgbEnables rgb_enables = config_->get_global().rgb_enables;

		pipe_->retile = false;
		tiling_config.input_alignment = calculate_input_alignment();

		PISP_LOG(debug, "Input alignments are " << tiling_config.input_alignment << " pixels");

		tiling_config.input_image_size = tiling::Length2(post_transpose_width(), post_transpose_height());

		for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
		{
			const BeOutputFormatConfig &output_format = config_->get_output_format(i);
			const BeDownscaleConfig &downscale = config_->get_downscale(i);
			const BeResampleConfig &resample = config_->get_resample(i);
			const BeCropConfig &crop = config_->get_crop(i);			

			tiling_config.crop[i] = tiling::Interval2(tiling::Interval(crop.offset_x, crop.width),
													  tiling::Interval(crop.offset_y, crop.height));

			if (tiling_config.crop[i].x.length == 0 || tiling_config.crop[i].y.length == 0)
				tiling_config.crop[i] = tiling::Interval2(tiling::Interval(0, post_transpose_width()),
														  tiling::Interval(0, post_transpose_height()));

			tiling_config.output_h_mirror[i] = output_format.transform & PISP_BE_TRANSFORM_HFLIP;
			tiling_config.downscale_factor[i] =
				tiling::Length2(downscale.scale_factor_h, downscale.scale_factor_v);
			tiling_config.resample_factor[i] =
				tiling::Length2(resample.scale_factor_h, resample.scale_factor_v);
			tiling_config.downscale_image_size[i] =
				tiling::Length2(downscale.scaled_width, downscale.scaled_height);
			tiling_config.output_image_size[i] =
				tiling::Length2(output_format.image.width, output_format.image.height);
			tiling_config.output_max_alignment[i] =
				calculate_output_alignment(output_format.image.format, PISP_BACK_END_OUTPUT_MAX_ALIGN);
			tiling_config.output_min_alignment[i] =
				calculate_output_alignment(output_format.image.format, PISP_BACK_END_OUTPUT_MIN_ALIGN);
		}

		tiling_config.max_tile_size.dx = sw_.max_tile_width ? sw_.max_tile_width : variant_.back_end_max_tile_width(0);
		tiling_config.max_tile_size.dy = sw_.max_stripe_height ? sw_.max_stripe_height : MaxStripeHeight;
		tiling_config.min_tile_size = tiling::Length2(PISP_BACK_END_MIN_TILE_WIDTH, PISP_BACK_END_MIN_TILE_HEIGHT);

		tiling_config.resample_enables = tiling_config.downscale_enables = 0;
		if (rgb_enables & RgbEnable::Resample0)
			tiling_config.resample_enables = 1;
		if (rgb_enables & RgbEnable::Resample1)
			tiling_config.resample_enables |= (1 << 1);
		if (rgb_enables & RgbEnable::Downscale0)
			tiling_config.downscale_enables = 1;
		if (rgb_enables & RgbEnable::Downscale1)
			tiling_config.downscale_enables |= (1 << 1);

		// For dedistortion, look up the most -ve and most +ve source for a given output X coordinate
		BeDedistortionConfig dd = config_->get_dedistort();
		if ((rgb_enables & RgbEnable::Dedistort) && dd.grid_step_x)
		{
			for (unsigned i = 0; i <= PISP_BE_DDX_GRID_SIZE; ++i)
			{
				int xpos = (i << PISP_BE_DDX_STEP_PRECISION) / (double)dd.grid_step_x - dd.offset_x;
				double left = xpos + 1024.0;
				double right = xpos - 1024.0;
				for (unsigned j = 0; j <= PISP_BE_DDX_GRID_SIZE; ++j)
				{
					// Since we're ignoring context, assume sub-pixel accuracy is not needed either!
					double ipos = xpos + 0.0625 * dd.lut[j][i];
					ipos = std::clamp(ipos, 0.0, 0.0 + post_transpose_width());
					if (ipos < left)
						left = ipos;
					if (ipos > right)
						right = ipos;
				}
				tiling_config.dedistort_pwls[0].append(xpos, left);  // output->input min (left)
				tiling_config.dedistort_pwls[1].append(xpos, right); // output->input max (right)
				tiling_config.dedistort_pwls[2].append(right, xpos); // input->output min (right)
			}
		}

		// We need context both before and after dedistortion, and in any case the previous fixed amount was ungenerous.
		// Add a fixed allowance for each of the Bayer and RGB pipelines (up to the split), if any stage requires it.
		BayerEnables bayer_enables = config_->get_global().bayer_enables;
		tiling_config.has_bayer_context =
			!!(bayer_enables & (BayerEnable::Dpc | BayerEnable::Geq | BayerEnable::Tdn | BayerEnable::Sdn |
								BayerEnable::Stitch | BayerEnable::Cdn | BayerEnable::Lsc | BayerEnable::Tonemap2 |
								BayerEnable::Cac | BayerEnable::Debin | BayerEnable::Demosaic));
		tiling_config.has_rgb_context = !!(rgb_enables & (RgbEnable::FalseColour | RgbEnable::Sharpen));

		// Set compressed_input to false as otherwise the tiling would pad tiles up to multiples of 8 pixels even when these lie
		// outside the actual image width (and we've chosen not to handle compression like that).
		tiling_config.compressed_input = false;
		retile_pipeline(tiling_config);
		check_tiles(tiling_config);
		pipe_->finalise_tiling = true;
	}

	if (pipe_->finalise_tiling)
	{
		finalise_tiling();
		pipe_->finalise_tiling = false;
	}
}

void BackEnd::retile_pipeline(TilingConfig const &tiling_config)
{
	// The tiling library provides tiles in a SW Tile structure.
	Tile tiles[PISP_BACK_END_NUM_TILES];
	tiling::Length2 grid;

	tile_pipeline(tiling_config, tiles, PISP_BACK_END_NUM_TILES, &grid);
	pipe_->num_tiles_x = grid.dx;
	pipe_->num_tiles_y = grid.dy;

	// Finally convert the Tiles into BeTiles.
	for (unsigned int i = 0; i < pipe_->num_tiles_x * pipe_->num_tiles_y; i++)
	{
		RgbEnables rgb_enables = config_->get_global().rgb_enables;
		bool top_edge = i < pipe_->num_tiles_x;
		bool left_edge = i % pipe_->num_tiles_x == 0;

		BeTile t = config_->get_tile(i);

		if (variant_ <= BCM2712_D0)
		{
			t.edge = 0;
			if (top_edge)
				t.edge |= PISP_TOP_EDGE;
			if (i >= pipe_->num_tiles_x * (pipe_->num_tiles_y - 1))
				t.edge |= PISP_BOTTOM_EDGE;
			if (left_edge)
				t.edge |= PISP_LEFT_EDGE;
			if ((i + 1) % pipe_->num_tiles_x == 0)
				t.edge |= PISP_RIGHT_EDGE;
		}

		t.input_offset_x = tiles[i].input.input.x.offset;
		t.input_offset_y = tiles[i].input.input.y.offset;
		t.input_width = tiles[i].input.input.x.length;
		t.input_height = tiles[i].input.input.y.length;
		t.dedistorted_offset_x = tiles[i].dedistort.output.x.offset;
		t.dedistorted_width = tiles[i].dedistort.output.x.length;

		if (tiles[i].input.output != tiles[i].input.input)
			throw std::runtime_error("BackEnd::retile_pipeline: tiling error in Bayer pipe");

		for (unsigned int j = 0; j < variant_.back_end_num_branches(0); j++)
		{
			bool enabled = (rgb_enables & get_flag_for_branch(RgbEnable::Output0, j));
			if (enabled && (tiles[i].output[j].output.x.length == 0 || tiles[i].output[j].output.y.length == 0))
			{
				// If a tile produces no output there's no point sending anything down this branch, so ensure the crop
				// "eats" everything and set everything else to zero.
				if (variant_ < BERYL)
				{
					t.crop_x_start[j] = t.input_width;
					t.crop_x_end[j] = 0;
					t.crop_y_start[j] = t.input_height;
					t.crop_y_end[j] = 0;
				}
				else
				{
					t.crop_x_start[j] = 0;
					t.crop_y_start[j] = 0;
					t.crop_width[j] = 0;
					t.crop_height[j] = 0;
				}

				t.resample_in_width[j] = 0;
				t.resample_in_height[j] = 0;
				t.output_offset_x[j] = 0;
				t.output_offset_y[j] = 0;
				t.output_width[j] = 0;
				t.output_height[j] = 0;
				continue;
			}

			tiling::Crop2 downscale_crop;
			tiling::Interval2 resample_size = tiles[i].crop[j].output;
			resample_size.x = resample_size.x - tiles[i].resample[j].crop.x;
			resample_size.y = resample_size.y - tiles[i].resample[j].crop.y;

			// When a resize stage is disabled, the tile size after the stage is found from the input of the
			// next block. Also there will be no extra crop necessary for the resize operation.
			if (rgb_enables & get_flag_for_branch(RgbEnable::Downscale0, j))
			{
				downscale_crop = tiles[i].downscale[j].crop + tiles[i].crop[j].crop;
				// Size of the tile going into the resample block needs to be set here.
				resample_size = tiles[i].downscale[j].output;
			}
			else if (rgb_enables & get_flag_for_branch(RgbEnable::Resample0, j))
			{
				downscale_crop = tiles[i].resample[j].crop + tiles[i].crop[j].crop;
			}
			else
			{
				downscale_crop = tiles[i].output[j].crop + tiles[i].crop[j].crop;
			}

			t.crop_x_start[j] = downscale_crop.x.start;
			t.crop_y_start[j] = downscale_crop.y.start;
			if (variant_ < BERYL)
			{
				t.crop_x_end[j] = downscale_crop.x.end;
				t.crop_y_end[j] = downscale_crop.y.end;
			}
			else
			{
				t.crop_width[j] = t.dedistorted_width - downscale_crop.x.start - downscale_crop.x.end;
				t.crop_height[j] = t.input_height - downscale_crop.y.start - downscale_crop.y.end;
			}
			t.resample_in_width[j] = resample_size.x.length;
			t.resample_in_height[j] = resample_size.y.length;
			t.output_offset_x[j] = tiles[i].output[j].output.x.offset;
			t.output_offset_y[j] = tiles[i].output[j].output.y.offset;
			t.output_width[j] = tiles[i].output[j].output.x.length;
			t.output_height[j] = tiles[i].output[j].output.y.length;

			if (variant_ > BCM2712_D0)
			{
				t.resample_start_context_x[j] = !left_edge;
                t.resample_start_context_y[j] = !top_edge;
			}

			for (int p = 0; p < 3; p++)
			{
				// Calculate x/y initial downsampler/resampler phases per-plane.

				// Variant Beryl onwards use the same phase for all planes.
				if (p > 0 && variant_ >= BERYL)
					continue;

				if (rgb_enables & get_flag_for_branch(RgbEnable::Downscale0, j))
				{
					const BeDownscaleConfig &downscale = config_->get_downscale(j);
					unsigned int frac_x = (resample_size.x.offset * downscale.scale_factor_h) &
										  ((1 << ScalePrecision) - 1);
					unsigned int frac_y = (resample_size.y.offset * downscale.scale_factor_v) &
										  ((1 << ScalePrecision) - 1);
					// Fractional component of the input required to generate the output pixel.
					t.downscale_phase_x[p * variant_.back_end_num_branches(0) + j] = (UnityPhase - frac_x);
					t.downscale_phase_y[p * variant_.back_end_num_branches(0) + j] = (UnityPhase - frac_y);
				}

				if (rgb_enables & get_flag_for_branch(RgbEnable::Resample0, j))
				{
					const BeResampleConfig &resample = config_->get_resample(j);
					// Location of the output pixel in the interpolated (input) image.
					unsigned int interpolated_pix_x =
						(t.output_offset_x[j] * NumPhases * resample.scale_factor_h) >> ScalePrecision;
					unsigned int interpolated_pix_y =
						(t.output_offset_y[j] * NumPhases * resample.scale_factor_v) >> ScalePrecision;
					// Phase of the interpolated input pixel.
					t.resample_phase_x[p * variant_.back_end_num_branches(0) + j] =
						((interpolated_pix_x % NumPhases) << ScalePrecision) / NumPhases;
					t.resample_phase_y[p * variant_.back_end_num_branches(0) + j] =
						((interpolated_pix_y % NumPhases) << ScalePrecision) / NumPhases;
					// Account for any user defined initial phase - this could be negative!
					t.resample_phase_x[p * variant_.back_end_num_branches(0) + j] +=
						config_->get_resample(j).initial_phase_h[p];
					t.resample_phase_y[p * variant_.back_end_num_branches(0) + j] +=
						config_->get_resample(j).initial_phase_v[p];
					// Have to be within this range, else some calculation went wrong.
					PISP_ASSERT(t.resample_phase_x[p * variant_.back_end_num_branches(0) + j] <= (2 * UnityPhase - 1));
					PISP_ASSERT(t.resample_phase_y[p * variant_.back_end_num_branches(0) + j] <= (2 * UnityPhase - 1));
				}
			}

			if (variant_ < BERYL && rgb_enables & get_flag_for_branch(RgbEnable::Resample0, j))
			{
				// Phase difference between planes cannot be > 0.5 pixels on the output dimensions.
				const BeResampleConfig &resample = config_->get_resample(j);
				int phase_max = (resample.scale_factor_h * UnityPhase / 2) >> ScalePrecision;
				if (std::abs(t.resample_phase_x[0 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_x[1 * variant_.back_end_num_branches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_x[1 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_x[2 * variant_.back_end_num_branches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_x[0 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_x[2 * variant_.back_end_num_branches(0) + j]) > phase_max)
				{
					throw std::runtime_error("Resample phase x for tile is > 0.5 pixels on the output dimensions.");
				}
				phase_max = (resample.scale_factor_v * UnityPhase / 2) >> ScalePrecision;
				if (std::abs(t.resample_phase_y[0 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_y[1 * variant_.back_end_num_branches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_y[1 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_y[2 * variant_.back_end_num_branches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_y[0 * variant_.back_end_num_branches(0) + j] -
							 t.resample_phase_y[2 * variant_.back_end_num_branches(0) + j]) > phase_max)
				{
					throw std::runtime_error("Resample phase y for tile is > 0.5 pixels on the output dimensions.");
				}
			}
		}

		config_->set_tile(i, t);
	}
}

void BackEnd::finalise_tiling()
{
	// Update tile parameters (offsets/strides) from on the BE pipeline configuration.
	for (unsigned int tile_num = 0; tile_num < pipe_->num_tiles_x * pipe_->num_tiles_y; tile_num++)
	{
		const BeLscConfig &lsc = config_->get_lsc();
		const BeCacConfig &cac = config_->get_cac();
		const BeTonemap2Config &tonemap2 = config_->get_tonemap2();
		const BayerEnables bayer_enables = config_->get_global().bayer_enables;

		BeTile t = config_->get_tile(tile_num);

		calculate_input_addr_offset((bayer_enables & BayerEnable::Transpose) ? t.input_offset_y : t.input_offset_x,
								 (bayer_enables & BayerEnable::Transpose) ? t.input_offset_x : t.input_offset_y,
								 config_->get_input_format(), &t.input_addr_offset, &t.input_addr_offset2);
		calculate_input_addr_offset(t.input_offset_x, t.input_offset_y, config_->get_tdn_input_format(),
									&t.tdn_input_addr_offset);
		calculate_input_addr_offset(t.input_offset_x, t.input_offset_y, config_->get_tdn_output_format(),
									&t.tdn_output_addr_offset);
		calculate_input_addr_offset(t.input_offset_x, t.input_offset_y, config_->get_stitch_input_format(),
									&t.stitch_input_addr_offset);
		calculate_input_addr_offset(t.input_offset_x, t.input_offset_y, config_->get_stitch_output_format(),
									&t.stitch_output_addr_offset);
		PISP_LOG(debug, "Input offsets " << t.input_offset_x << "," << t.input_offset_y << " address offsets "
										 << t.input_addr_offset << " and " << t.input_addr_offset2);

		if (bayer_enables & BayerEnable::Lsc)
		{
			t.lsc_grid_offset_x = (t.input_offset_x + config_->get_lsc().offset_x) * lsc.grid_step_x;
			t.lsc_grid_offset_y = (t.input_offset_y + config_->get_lsc().offset_y) * lsc.grid_step_y;
		}

		if (bayer_enables & BayerEnable::Cac)
		{
			t.cac_grid_offset_x = (t.input_offset_x + config_->get_cac().offset_x) * cac.grid_step_x;
			t.cac_grid_offset_y = (t.input_offset_y + config_->get_cac().offset_y) * cac.grid_step_y;
		}

		if (config_->get_global().rgb_enables & RgbEnable::Dedistort)
		{
			const BeDedistortionConfig &dd = config_->get_dedistort();
			t.dedistort_grid_offset_x = (t.dedistorted_offset_x + dd.offset_x) * dd.grid_step_x;
			t.dedistort_grid_offset_y = (t.input_offset_y + dd.offset_y) * dd.grid_step_y;
		}

		if (bayer_enables & BayerEnable::Tonemap2)
		{
			t.tonemap_grid_offset_x = t.input_offset_x * tonemap2.grid_step_x;
			t.tonemap_grid_offset_y = t.input_offset_y * tonemap2.grid_step_y;
			t.tonemap_lores_offset_x = t.input_offset_x * tonemap2.lores_step_x;
			t.tonemap_lores_offset_y = t.input_offset_y * tonemap2.lores_step_y;
		}

		for (unsigned int j = 0; j < variant_.back_end_num_branches(0); j++)
		{
			const BeOutputFormatConfig &output_format = config_->get_output_format(j);
			int output_offset_x_unflipped = t.output_offset_x[j], output_offset_y_unflipped = t.output_offset_y[j];

			if (output_format.transform & PISP_BE_TRANSFORM_HFLIP)
				t.output_offset_x[j] =
					output_format.image.width - output_offset_x_unflipped - t.output_width[j];

			if (output_format.transform & PISP_BE_TRANSFORM_VFLIP)
				t.output_offset_y[j] = output_format.image.height - output_offset_y_unflipped - 1;

			compute_addr_offset(output_format.image, t.output_offset_x[j], t.output_offset_y[j], &t.output_addr_offset[j],
							  &t.output_addr_offset2[j]);

			PISP_LOG(debug, "Branch " << j << " output offsets " << t.output_offset_x[j] << "," << t.output_offset_y[j]
									  << " address offsets " << t.output_addr_offset[j] << " and "
									  << t.output_addr_offset2[j]);
		}

		config_->set_tile(tile_num, t);
	}
}

void BackEnd::get_output_size(int i, uint16_t *width, uint16_t *height, ImageFormatConfig const &ifmt) const
{
	// This internal version doesn't check if the output is enabled
	if (pipe_->smart_resize[i].width && pipe_->smart_resize[i].height)
		*width = pipe_->smart_resize[i].width, *height = pipe_->smart_resize[i].height;
	else if (config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Resample0, i))
		*width = config_->get_resample(i).scaled_width, *height = config_->get_resample(i).scaled_height;
	else if (config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Downscale0, i))
		*width = config_->get_downscale(i).scaled_width, *height = config_->get_downscale(i).scaled_height;
	else if (config_->get_crop(i).width) // crop width and height will be zero when crop disabled
		*width = config_->get_crop(i).width, *height = config_->get_crop(i).height;
	else if (config_->get_global().bayer_enables & BayerEnable::Transpose)
		*width = ifmt.height, *height = ifmt.width;
	else
		*width = ifmt.width, *height = ifmt.height;
}

uint16_t BackEnd::post_transpose_width()
{
	return (config_->get_global().bayer_enables & BayerEnable::Transpose) ? config_->get_input_format().height
																		  : config_->get_input_format().width;
}

uint16_t BackEnd::post_transpose_height()
{
	return (config_->get_global().bayer_enables & BayerEnable::Transpose) ? config_->get_input_format().width
																		  : config_->get_input_format().height;
}

bool BackEnd::compute_output_image_format(unsigned int i, ImageFormatConfig &fmt,
									   ImageFormatConfig const &ifmt) const
{
	PISP_ASSERT(i < PISP_BACK_END_NUM_OUTPUTS);

	BeOutputFormatConfig output_format = config_->get_output_format(i);
	if (&fmt != &output_format.image)
		fmt = output_format.image;

	if (config_->get_global().rgb_enables & get_flag_for_branch(RgbEnable::Output0, i))
{
		get_output_size(i, &fmt.width, &fmt.height, ifmt);
		if (!fmt.stride)
			compute_stride(fmt);
		else
			check_stride(fmt);
		return true;
	}
	else
	{
		fmt.width = 0;
		fmt.height = 0;
		fmt.stride = 0;
		fmt.stride2 = 0;
		return false;
	}
}

void BackEnd::prepare(pisp_be_tiles_config *config)
{
	BayerEnables bayer_enables = config_->get_global().bayer_enables;
	RgbEnables rgb_enables = config_->get_global().rgb_enables;

	PISP_LOG(debug, "New frame!");

	// On every start-of-frame we:
	// 1. Check the input configuration appears sensible.
	if (!(bayer_enables & BayerEnable::Input) && !(rgb_enables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::pre_frame_update: neither Bayer nor RGB inputs are enabled");
	else if ((bayer_enables & BayerEnable::Input) && (rgb_enables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::pre_frame_update: both Bayer and RGB inputs are enabled");
	else if (bayer_enables && (rgb_enables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::pre_frame_update: Bayer processing not possible with RGB input");

	// 2. Also check the output configuration is all filled in and looks sensible. Again, addresses must be
	// left to the HAL.
	for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
	{
		BeOutputFormatConfig output_format = config_->get_output_format(i);
		compute_output_image_format(i, output_format.image, config_->get_input_format());
		config_->set_output_format(i, output_format);

		if (output_format.image.format & PISP_IMAGE_FORMAT_INTEGRAL_IMAGE)
		{
			throw std::runtime_error("Integral images are not supported.");
		}
	}

	// 3. Fill in any other missing bits of config, and update the tiling if necessary.
	update_smart_resize();
	finalise_config();
	update_tiles();

	if (config)
	{
		// Allow passing of empty pointer, if only pipe_->hw_config should be filled
		// 4. Write the config and tiles to the provided buffer to send to the hardware.
		if (variant_ >= BERYL)
		{
			config->version = 2;
			config->v2.num_tiles = pipe_->num_tiles_x * pipe_->num_tiles_y;
			config->v2.config = pipe_->hw_config_v2;
			std::copy(pipe_->tiles2.begin(), pipe_->tiles2.begin() + config->v2.num_tiles, config->v2.tiles);
		}
		else
		{
			config->version = 1;
			config->num_tiles = pipe_->num_tiles_x * pipe_->num_tiles_y;
			config->config = pipe_->hw_config;
			std::copy(pipe_->tiles.begin(), pipe_->tiles.begin() + config->num_tiles, config->tiles);
		}

		// 5. Clear any dirty flags for the next configuration update.
		pipe_->dirty_flags_bayer = BayerEnable::None;
		pipe_->dirty_flags_rgb = RgbEnable::None;
		pipe_->dirty_flags_extra = DirtyFlag::None;
	}
}
