
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
constexpr unsigned int ResamplePrecision = 10;
constexpr unsigned int NumPhases = 16;
constexpr unsigned int NumTaps = 6;

void checkStride(ImageFormatConfig const &config)
{
	if (config.stride % PISP_BACK_END_OUTPUT_MIN_ALIGN || config.stride2 % PISP_BACK_END_OUTPUT_MIN_ALIGN)
		throw std::runtime_error("Output stride values not sufficiently aligned");

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.format) && (config.stride % 128 || config.stride2 % 128))
		throw std::runtime_error("Wallpaper format should have 128-byte aligned rolls");

	ImageFormatConfig check = config;
	computeStrideAlign(check, PISP_BACK_END_OUTPUT_MIN_ALIGN);
	if (check.stride > config.stride || check.stride2 > config.stride2)
	{
		PISP_LOG(fatal, "Strides should be at least " << check.stride << " and " << check.stride2 << " but are "
													  << config.stride << " and " << config.stride2);
	}
}

// TDN and Stitch I/O dimensions must match the input, though the format may differ.
static void checkRawioFormat(ImageFormatConfig &fmt, uint16_t w, uint16_t h)
{
	if (fmt.width == 0 || fmt.height == 0)
	{
		fmt.width = w;
		fmt.height = h;
	}
	else if (fmt.width != w || fmt.height != h)
		throw std::runtime_error("BackEnd::finalise: Image dimensions do not match input");

	if (fmt.stride == 0)
		computeStride(fmt);
	else
		checkStride(fmt);
}
unsigned int getPixelAlignment(uint32_t format, int byteAlignment)
{
	int alignmentPixels = byteAlignment; // for 8bpp formats

	if (PISP_IMAGE_FORMAT_BPS_16(format))
		alignmentPixels = byteAlignment / 2;
	else if (PISP_IMAGE_FORMAT_BPS_10(format))
		alignmentPixels = byteAlignment * 3 / 4;
	else if (PISP_IMAGE_FORMAT_BPP_32(format))
		alignmentPixels = byteAlignment / 4;

	if (PISP_IMAGE_FORMAT_PLANAR(format) && !PISP_IMAGE_FORMAT_SAMPLING_444(format))
		alignmentPixels *= 2; // the UV planes in fully planar 420/422 output will have half the width
	else if (PISP_IMAGE_FORMAT_INTERLEAVED(format) &&
			 (PISP_IMAGE_FORMAT_SAMPLING_422(format) || PISP_IMAGE_FORMAT_SAMPLING_420(format)))
		alignmentPixels /= 2; // YUYV type outputs need only 8 pixels to make 16 bytes

	return alignmentPixels;
}

unsigned int lcm(int a, int b)
{
	int origA = a, origB = b, tmp;
	while (b)
		tmp = a % b, a = b, b = tmp;
	return origA / a * origB;
}

static tiling::Length2 calculateOutputAlignment(uint32_t format, int align = PISP_BACK_END_OUTPUT_MAX_ALIGN)
{
	int yAlignment = PISP_IMAGE_FORMAT_SAMPLING_420(format) ? 2 : 1;

	return tiling::Length2(getPixelAlignment(format, align), yAlignment);
}

void calculateInputAddrOffset(int x, int y, ImageFormatConfig const &inputFormat, uint32_t *addrOffset,
								 uint32_t *addrOffset2 = nullptr)
{
	uint32_t offset2 = 0;

	computeAddrOffset(inputFormat, x, y, addrOffset, &offset2);
	if (addrOffset2)
		*addrOffset2 = offset2;
}

} // namespace

void BackEnd::finaliseBayerRgbInputs() const 
{
	const ImageFormatConfig &config = config_->getInputFormat();

	if (config.width < PISP_BACK_END_MIN_TILE_WIDTH || config.height < PISP_BACK_END_MIN_TILE_HEIGHT)
		throw std::runtime_error("finaliseBayerRgbInputs: input image too small");
}

void BackEnd::finaliseInputs() const
{
	const ImageFormatConfig &input_format = config_->getInputFormat();

	// Not so much finalising, just checking that input dimensions and strides are OK.
	if (config_->getGlobal().bayerEnables & BayerEnable::Input)
	{
		if ((input_format.width & 1) || (input_format.height & 1))
			throw std::runtime_error("finaliseInputs: Bayer pipe image dimensions must be even");
		if (input_format.stride & 15)
			throw std::runtime_error("finaliseInputs: input stride should be at least 16-byte aligned");
	}
	else if (config_->getGlobal().rgbEnables & RgbEnable::Input)
	{
		if (PISP_IMAGE_FORMAT_SAMPLING_420(input_format.format) && (input_format.width & 1))
			throw std::runtime_error("finaliseInputs: 420 input height must be even");
		else if ((PISP_IMAGE_FORMAT_SAMPLING_420(input_format.format) ||
				  PISP_IMAGE_FORMAT_SAMPLING_422(input_format.format)) &&
				 (input_format.width & 1))
			throw std::runtime_error("finaliseInputs: 420/422 input width must be even");
		if (PISP_IMAGE_FORMAT_WALLPAPER(input_format.format))
		{
			if ((input_format.stride & 127) || (input_format.stride2 & 127))
				throw std::runtime_error("finaliseInputs: wallpaper format strides must be at least 128-byte aligned");
		}
		else if ((input_format.stride & 15) || (input_format.stride2 & 15))
			throw std::runtime_error("finaliseInputs: input strides must be at least 16-byte aligned");
	}
}

void BackEnd::finaliseLsc()
{
	BeLscConfig lsc = config_->getLsc();
	uint16_t width = config_->getInputFormat().width, height = config_->getInputFormat().height;

	// Just a warning that ACLS algorithms might want the grid calculations here to match the AWB/ACLS stats.
	static const int P = PISP_BE_LSC_STEP_PRECISION;

	if (lsc.grid_step_x == 0)
		lsc.grid_step_x = (PISP_BE_LSC_GRID_SIZE << P) / width;
	if (lsc.grid_step_y == 0)
		lsc.grid_step_y = (PISP_BE_LSC_GRID_SIZE << P) / height;

	PISP_ASSERT(lsc.grid_step_x * (width + lsc.offset_x - 1) < (PISP_BE_LSC_GRID_SIZE << P));
	PISP_ASSERT(lsc.grid_step_y * (height + lsc.offset_y - 1) < (PISP_BE_LSC_GRID_SIZE << P));
	
	config_->setLsc(lsc);
}

void BackEnd::finaliseCac()
{
	BeCacConfig cac = config_->getCac();
	uint16_t width = config_->getInputFormat().width, height = config_->getInputFormat().height;

	static const int P = PISP_BE_CAC_STEP_PRECISION;

	if (cac.grid_step_x == 0)
		cac.grid_step_x = (PISP_BE_CAC_GRID_SIZE << P) / width;
	if (cac.grid_step_y == 0)
		cac.grid_step_y = (PISP_BE_CAC_GRID_SIZE << P) / height;

	PISP_ASSERT(cac.grid_step_x * (width + cac.offset_x - 1) < (PISP_BE_CAC_GRID_SIZE << P));
	PISP_ASSERT(cac.grid_step_y * (height + cac.offset_y - 1) < (PISP_BE_CAC_GRID_SIZE << P));
	
	config_->setCac(cac);
}

void BackEnd::finaliseResample(unsigned int index, uint16_t width, uint16_t height)
{
	BeResampleConfig resample = config_->getResample(index);
	const BeResampleExtra &resampleExtra = pipe_->configExtra.resample[index];

	uint32_t scaleFactorH = ((width - 1) << ScalePrecision) / (resampleExtra.scaled_width - 1);
	uint32_t scaleFactorV = ((height - 1) << ScalePrecision) / (resampleExtra.scaled_height - 1);

	if ((scaleFactorH < UnityScale / 16 || scaleFactorH >= 16 * UnityScale) ||
		(scaleFactorV < UnityScale / 16 || scaleFactorV >= 16 * UnityScale))
		throw std::runtime_error("finaliseResample: Invalid scaling factors (must be < 16x down/upscale).");

	resample.scale_factor_h = scaleFactorH;
	resample.scale_factor_v = scaleFactorV;
	// If the filter coefficients are unset we should probably copy in our "default ones".
	
	config_->setResample(index, resample);
}

void BackEnd::finaliseDownscale(unsigned int index, uint16_t width, uint16_t height)
{
	BeDownscaleConfig downscale = config_->getDownscale(index);
	const BeDownscaleExtra &downscaleExtra = pipe_->configExtra.downscale[index];

	PISP_LOG(debug, "width " << width << " scaled_width " << downscaleExtra.scaled_width);
	PISP_LOG(debug, "height " << height << " scaled_height " << downscaleExtra.scaled_height);

	uint32_t scaleFactorH = (width << ScalePrecision) / (downscaleExtra.scaled_width);
	uint32_t scaleFactorV = (height << ScalePrecision) / (downscaleExtra.scaled_height);

	if ((scaleFactorH != UnityScale && (scaleFactorH < 2 * UnityScale || scaleFactorH > 8 * UnityScale)) ||
		(scaleFactorV != UnityScale && (scaleFactorV < 2 * UnityScale || scaleFactorV > 8 * UnityScale)))
		throw std::runtime_error("finaliseDownscale: Invalid scaling factors (must be 1x or >= 2x && <= 8x).");

	downscale.scale_factor_h = scaleFactorH;
	downscale.scale_factor_v = scaleFactorV;
	downscale.scale_recip_h = (downscaleExtra.scaled_width << ScalePrecision) / (width);
	downscale.scale_recip_v = (downscaleExtra.scaled_height << ScalePrecision) / (height);
	
	config_->setDownscale(index, downscale);

	PISP_LOG(debug, "scale_factor_h " << downscale.scale_factor_h << " scale_factor_v " << downscale.scale_factor_v);
	PISP_LOG(debug, "scale_recip_h " << downscale.scale_recip_h << " scale_recip_v " << downscale.scale_recip_v);
}

void BackEnd::finaliseDecompression() const
{
	uint32_t fmt = config_->getInputFormat().format;
	BayerEnables bayerEnables = config_->getGlobal().bayerEnables;

	if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !(bayerEnables & BayerEnable::Decompress))
		throw std::runtime_error("BackEnd::finalise: input compressed but decompression not enabled");

	if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && (bayerEnables & BayerEnable::Decompress))
		throw std::runtime_error("BackEnd::finalise: input uncompressed but decompression enabled");

	if ((bayerEnables & BayerEnable::Decompress) && !PISP_IMAGE_FORMAT_BPS_8(fmt))
		throw std::runtime_error("BackEnd::finalise: compressed input is not 8bpp");
}

void BackEnd::finaliseTdn()
{
	const ImageFormatConfig &inputFormat = config_->getInputFormat();
	const BeTdnConfig &tdn = config_->getTdn();
	ImageFormatConfig tdnInputFormat = config_->getTdnInputFormat();
	ImageFormatConfig tdnOutputFormat = config_->getTdnOutputFormat();
	const BayerEnables bayerEnables = config_->getGlobal().bayerEnables;
	
	int tdnEnabled = bayerEnables & BayerEnable::Tdn;
	int tdnInputEnabled = bayerEnables & BayerEnable::TdnInput;
	int tdnDecompressEnabled = bayerEnables & BayerEnable::TdnDecompress;
	int tdnCompressEnabled = bayerEnables & BayerEnable::TdnCompress;
	int tdnOutputEnabled = bayerEnables & BayerEnable::TdnOutput;
	uint32_t fmt = tdnOutputFormat.format;

	if (tdnEnabled && !tdnOutputEnabled)
		throw std::runtime_error("finaliseTdn: TDN output not enabled when TDN enabled");

	if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !tdnCompressEnabled)
		throw std::runtime_error("finaliseTdn: TDN output compressed but compression not enabled");

	if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && tdnCompressEnabled)
		throw std::runtime_error("finaliseTdn: TDN output uncompressed but compression enabled");

	if (tdnCompressEnabled && !PISP_IMAGE_FORMAT_BPS_8(fmt))
		throw std::runtime_error("finaliseTdn: TDN output does not match compression mode");

	if (tdnOutputEnabled)
          checkRawioFormat(tdnOutputFormat, inputFormat.width, inputFormat.height);

	if (tdnInputEnabled)
		checkRawioFormat(tdnOutputFormat, inputFormat.width, inputFormat.height);

	if (!tdnEnabled)
	{
		if (tdnInputEnabled)
			throw std::runtime_error("finaliseTdn: TDN input enabled but TDN not enabled");
		// I suppose there is a weird (and entirely pointless) case where TDN is not enabled but TDN output is, which we allow.
	}
	else if (tdn.reset)
	{
		if (tdnInputEnabled)
			throw std::runtime_error("finaliseTdn: TDN input enabled but TDN being reset");
	}
	else
	{
		if (!tdnInputEnabled)
			throw std::runtime_error("finaliseTdn: TDN input not enabled but TDN not being reset");
		// Make the TDN input match the output if it's unset. Usually this will be the sensible thing to do.
		if (tdnInputFormat.width == 0 && tdnInputFormat.height == 0)
			tdnInputFormat = tdnOutputFormat;
		if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !tdnDecompressEnabled)
			throw std::runtime_error("finaliseTdn: TDN input compressed but decompression not enabled");
		if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && tdnDecompressEnabled)
			throw std::runtime_error("finaliseTdn: TDN input uncompressed but decompression enabled");
		if (tdnCompressEnabled && !PISP_IMAGE_FORMAT_BPS_8(fmt))
			throw std::runtime_error("finaliseTdn: TDN output does not match compression mode");
	}
	
	config_->setTdnInputFormat(tdnInputFormat);
	config_->setTdnOutputFormat(tdnOutputFormat);
}

void BackEnd::finaliseStitch()
{
	const ImageFormatConfig &inputFormat = config_->getInputFormat();
	BeStitchConfig stitch = config_->getStitch();
	ImageFormatConfig stitchInputFormat = config_->getStitchInputFormat();
	ImageFormatConfig stitchOutputFormat = config_->getStitchOutputFormat();
	const BayerEnables bayerEnables = config_->getGlobal().bayerEnables;
	
	bool stitchEnabled = bayerEnables & BayerEnable::Stitch;
	bool stitchInputEnabled = bayerEnables & BayerEnable::StitchInput;
	bool stitchDecompressEnabled = bayerEnables & BayerEnable::StitchDecompress;
	bool stitchCompressEnabled = bayerEnables & BayerEnable::StitchCompress;
	bool stitchOutputEnabled = bayerEnables & BayerEnable::StitchOutput;
	uint32_t inputFmt = stitchInputFormat.format;
	uint32_t outputFmt = stitchOutputFormat.format;

	if (stitchEnabled != stitchInputEnabled)
		throw std::runtime_error("BackEnd::finalise: stitch and stitch_input should be enabled/disabled together");
	if (stitchInputEnabled && PISP_IMAGE_FORMAT_COMPRESSED(inputFmt) && !stitchDecompressEnabled)
		throw std::runtime_error("BackEnd::finalise: stitch output compressed but decompression not enabled");
	if (stitchInputEnabled && !PISP_IMAGE_FORMAT_COMPRESSED(inputFmt) && stitchDecompressEnabled)
		throw std::runtime_error("BackEnd::finalise: stitch output uncompressed but decompression enabled");
	if (stitchOutputEnabled && PISP_IMAGE_FORMAT_COMPRESSED(outputFmt) && !stitchCompressEnabled)
		throw std::runtime_error("BackEnd::finalise: stitch output compressed but compression not enabled");
	if (stitchOutputEnabled && !PISP_IMAGE_FORMAT_COMPRESSED(outputFmt) && stitchCompressEnabled)
		throw std::runtime_error("BackEnd::finalise: stitch output uncompressed but compression enabled");
	if (stitchDecompressEnabled && !PISP_IMAGE_FORMAT_BPS_8(inputFmt))
		throw std::runtime_error("BackEnd::finalise: stitch input does not match compression mode");
	if (stitchCompressEnabled && !PISP_IMAGE_FORMAT_BPS_8(outputFmt))
		throw std::runtime_error("BackEnd::finalise: stitch output does not match compression mode");

	if (stitchOutputEnabled)
		checkRawioFormat(stitchOutputFormat, inputFormat.width, inputFormat.height);

	if (stitchInputEnabled)
		checkRawioFormat(stitchInputFormat, inputFormat.width, inputFormat.height);

	// Compute the motion_threshold reciprocal if it hasn't been done.
	if (stitch.motion_threshold_recip == 0)
	{
		if (stitch.motion_threshold_256 == 0)
			stitch.motion_threshold_recip = 255;
		else
			// We round the result up where possible as the block may work (ever so slightly) better like this.
			stitch.motion_threshold_recip =
				std::min(255, (256 + (int)stitch.motion_threshold_256 - 1) / stitch.motion_threshold_256);
	}
	
	config_->setStitchInputFormat(stitchInputFormat);
	config_->setStitchOutputFormat(stitchOutputFormat);
	config_->setStitch(stitch);
}

void BackEnd::finaliseOutput(unsigned int index)
{
	BeOutputFormatConfig config = config_->getOutputFormat(index);

	// If the high clipping bound is zero assume it wasn't set and the intention is that no clipping occurs.
	if (config.hi == 0)
		config.hi = 65535;
	if (config.hi2 == 0)
		config.hi2 = 65535;

	// Do some checking on output image dimensions and strides.
	if (config.image.width < PISP_BACK_END_MIN_TILE_WIDTH || config.image.height < PISP_BACK_END_MIN_TILE_HEIGHT)
		throw std::runtime_error("finaliseOutput: output image too small");

	if (PISP_IMAGE_FORMAT_SAMPLING_420(config.image.format) && (config.image.height & 1))
		throw std::runtime_error("finaliseOutput: 420 image height should be even");

	if ((PISP_IMAGE_FORMAT_SAMPLING_420(config.image.format) || PISP_IMAGE_FORMAT_SAMPLING_422(config.image.format)) &&
		!PISP_IMAGE_FORMAT_INTERLEAVED(config.image.format) && (config.image.width & 1))
		throw std::runtime_error("finaliseOutput: 420/422 image width should be even");
	
	config_->setOutputFormat(index, config);

	if (PISP_IMAGE_FORMAT_WALLPAPER(config.image.format))
	{
		if ((config.image.stride & 127) || (config.image.stride2 & 127))
			throw std::runtime_error("finaliseOutput: wallpaper image stride should be at least 128-byte aligned");
	}
	else if ((config.image.stride & 15) || (config.image.stride2 & 15))
		throw std::runtime_error("finaliseOutput: image stride should be at least 16-byte aligned");
}

void BackEnd::checkTiles(const TilingConfig &tilingConfig)
{
	for (unsigned int tileNum = 0; tileNum < pipe_->numTilesX * pipe_->numTilesY; tileNum++)
	{
		const pisp_tile &tile = pipe_->tiles[tileNum];

		PISP_ASSERT(tile.input_width && tile.input_height); // zero inputs shouldn't be possible

		if (tile.input_width < PISP_BACK_END_MIN_TILE_WIDTH || tile.input_height < PISP_BACK_END_MIN_TILE_HEIGHT)
			throw std::runtime_error("Tile too small at input");

		for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		{
			if ((config_->getGlobal().rgbEnables & getFlagForBranch(RgbEnable::Output0, i)) == 0)
				continue;

			unsigned int widthAfterCrop = tile.input_width - tile.crop_x_start[i] - tile.crop_x_end[i];
			unsigned int heightAfterCrop = tile.input_height - tile.crop_y_start[i] - tile.crop_y_end[i];

			// A tile that gets cropped away completely can't produce output, and vice versa.
			PISP_ASSERT((widthAfterCrop * heightAfterCrop == 0) ==
						(tile.output_width[i] * tile.output_height[i] == 0));

			// A zero-sized tile is legitimate meaning "no output", but otherwise minimum tile sizes must be respected.
			if (widthAfterCrop && heightAfterCrop)
			{
				bool rh_edge = tile.output_offset_x[i] + tile.output_width[i] == tilingConfig.outputImageSize[i].dx;

				if (widthAfterCrop < PISP_BACK_END_MIN_TILE_WIDTH)
				{
					PISP_LOG(warning, "Tile narrow after crop: tile " << tileNum << " output " << i
						 << " InputWidth " << tile.input_width << " after_crop " << widthAfterCrop
						 << " crop start " << tile.crop_x_start[i] << " end " << tile.crop_x_end[i]);
					if (!rh_edge)
						throw std::runtime_error("Tile width too small after crop");
				}
				if (heightAfterCrop < PISP_BACK_END_MIN_TILE_HEIGHT)
					throw std::runtime_error("Tile height too small after crop");

				if (tile.resample_in_width[i] < PISP_BACK_END_MIN_TILE_WIDTH)
				{
					PISP_LOG(warning, "Tile narrow after downscale: tile " << tileNum << " output " << i
						 << " InputWidth " << tile.input_width << " after_crop " << widthAfterCrop
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
		tileNum++;
	}
}


tiling::Length2 BackEnd::calculateInputAlignment() const
{
	const ImageFormatConfig &inputFormat = config_->getInputFormat();
	const ImageFormatConfig &tdnInputFormat = config_->getTdnInputFormat();
	const ImageFormatConfig &tdnOutputFormat = config_->getTdnOutputFormat();
	const ImageFormatConfig &stitchInputFormat = config_->getStitchInputFormat();
	const ImageFormatConfig &stitchOutputFormat = config_->getStitchOutputFormat();
	BayerEnables bayerEnables = config_->getGlobal().bayerEnables;

	if (config_->getGlobal().rgbEnables & RgbEnable::Input)
	{
		PISP_LOG(debug, "RGB input enabled");
		// Need 4 byte alignment AND even number of pixels. Height must be 2 row aligned only for 420 input.
		return tiling::Length2(lcm(getPixelAlignment(inputFormat.format, PISP_BACK_END_INPUT_ALIGN), 2),
							   PISP_IMAGE_FORMAT_SAMPLING_420(inputFormat.format) ? 2 : 1);
	}

	// For starters, we need 4 *byte* alignment (this automatically cover 2 *pixel* alignment for all the raw formats).
	int pixelAlignment = getPixelAlignment(inputFormat.format, PISP_BACK_END_INPUT_ALIGN);

	// If any input is compressed, we need 8 *pixel* alignment.
	if (PISP_IMAGE_FORMAT_COMPRESSED(inputFormat.format) ||
		((bayerEnables & BayerEnable::TdnInput) &&
		 PISP_IMAGE_FORMAT_COMPRESSED(tdnInputFormat.format)) ||
		((bayerEnables & BayerEnable::StitchInput) &&
		 PISP_IMAGE_FORMAT_COMPRESSED(stitchInputFormat.format)))
		pixelAlignment = lcm(pixelAlignment, PISP_BACK_END_COMPRESSED_ALIGN);

	// If any of the Bayer outputs are enabled, those need 16 *byte* alignment. (This already covers the outputs being compressed.)
	if (bayerEnables & BayerEnable::TdnOutput)
		pixelAlignment =
			lcm(pixelAlignment, getPixelAlignment(tdnOutputFormat.format, PISP_BACK_END_OUTPUT_MIN_ALIGN));
	if (bayerEnables & BayerEnable::StitchOutput)
		pixelAlignment = lcm(pixelAlignment,
							 getPixelAlignment(stitchOutputFormat.format, PISP_BACK_END_OUTPUT_MIN_ALIGN));

	return tiling::Length2(pixelAlignment, 2); // Bayer input rows always in pairs
}

void BackEnd::finaliseConfig()
{
	BayerEnables bayerEnables = config_->getGlobal().bayerEnables;
	RgbEnables rgbEnables = config_->getGlobal().rgbEnables;

	BayerEnables dirty_flags_bayer = pipe_->dirtyFlagsBayer & bayerEnables; // only finalise blocks that are dirty *and* enabled
	RgbEnables dirty_flags_rgb = pipe_->dirtyFlagsRgb & rgbEnables; // only finalise blocks that are dirty *and* enabled

	if ((dirty_flags_bayer & BayerEnable::Input) || (dirty_flags_rgb & RgbEnable::Input))
		finaliseBayerRgbInputs();

	if (dirty_flags_bayer & BayerEnable::Input)
		finaliseInputs();

	if (dirty_flags_bayer & (BayerEnable::Input | BayerEnable::Decompress))
		finaliseDecompression();

	if ((pipe_->dirtyFlagsBayer &
		 (BayerEnable::Tdn | BayerEnable::TdnInput | BayerEnable::TdnDecompress |
		  BayerEnable::TdnCompress | BayerEnable::TdnOutput)))
	{
		finaliseTdn();
	}

	if (pipe_->dirtyFlagsBayer &
		(BayerEnable::Stitch | BayerEnable::StitchInput | BayerEnable::StitchDecompress |
		 BayerEnable::StitchCompress | BayerEnable::StitchOutput))
	{
		finaliseStitch();
	}

	if (dirty_flags_bayer & BayerEnable::Lsc)
		finaliseLsc();

	if (dirty_flags_bayer & BayerEnable::Cac)
		finaliseCac();

	const ImageFormatConfig &inputFormat = config_->getInputFormat();

	for (unsigned int j = 0; j < variant_.backEndNumBranches(0); j++)
	{
		bool enabled = rgbEnables & getFlagForBranch(RgbEnable::Output0, j);

		if (enabled)
		{
			// crop is enabled when it contains non-zero width/height
			uint16_t w = pipe_->configExtra.crop[j].width ? pipe_->configExtra.crop[j].width : inputFormat.width;
			uint16_t h = pipe_->configExtra.crop[j].width ? pipe_->configExtra.crop[j].height : inputFormat.height;

			if (dirty_flags_rgb & getFlagForBranch(RgbEnable::Downscale0, j))
			{
				if (variant_.backEndDownscalerAvailable(0, j))
					finaliseDownscale(j, w, h);
				else
					throw std::runtime_error("Downscale is not available in output branch " + std::to_string(j));
			}

			if (rgbEnables & getFlagForBranch(RgbEnable::Downscale0, j))
			{
				// If the downscale is enabled, we update the input width/height for the resample stage.
				w = pipe_->configExtra.downscale[j].scaled_width;
				h = pipe_->configExtra.downscale[j].scaled_height;
			}

			if (dirty_flags_rgb & getFlagForBranch(RgbEnable::Resample0, j))
				finaliseResample(j, w, h);

			if (dirty_flags_rgb & getFlagForBranch(RgbEnable::Output0, j))
				finaliseOutput(j);
		}
	}
	// Finally check for a sane collection of enable bits.
	if (!((bayerEnables & BayerEnable::Input) || (bayerEnables== BayerEnable::None)))
		throw std::runtime_error("BackEnd::finalise: Bayer input disabled but Bayer pipe active");

	if (!(bayerEnables & BayerEnable::Input) && !(rgbEnables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::finalise: exactly one of Bayer and RGB inputs should be enabled");

	BayerEnables bayer_output_enables = bayerEnables & (BayerEnable::TdnOutput | BayerEnable::StitchOutput);
	RgbEnables rgb_output_enables = RgbEnable::None;
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		rgb_output_enables |= rgbEnables & getFlagForBranch(RgbEnable::Output0, i);

	if (rgb_output_enables == RgbEnable::None && bayer_output_enables == BayerEnable::None)
		throw std::runtime_error("BackEnd::finalise: PiSP not configured to do anything");
}

void BackEnd::updateSmartResize()
{
	BeGlobal global = config_->getGlobal();
	std::string filter;

	// Look through the output branches adjusting the scaling blocks where "smart resizing"
	// has been requested.
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
	{
		const ImageFormatConfig &inputFormat = config_->getInputFormat();

		// First get the size of the input to the rescalers. The crops are zero when not in use.
		uint16_t InputWidth = pipe_->configExtra.crop[i].width;
		if (!InputWidth)
			InputWidth = inputFormat.width;
		uint16_t inputHeight = pipe_->configExtra.crop[i].height;
		if (!inputHeight)
			inputHeight = inputFormat.height;

		if ((pipe_->smartResizeDirty & (1 << i)) || (pipe_->dirtyFlagsExtra & PISP_BE_DIRTY_CROP))
		{
			if (pipe_->smartResize[i].width && pipe_->smartResize[i].height)
			{
				uint16_t resamplerInputWidth = InputWidth;
				uint16_t resamplerInputHeight = inputHeight;
				uint16_t resamplerOutputWidth = pipe_->smartResize[i].width;
				uint16_t resamplerOutputHeight = pipe_->smartResize[i].height;

				PISP_LOG(debug, "Smart resize branch " << i << " input size " << InputWidth << " x " << inputHeight
													   << " output size " << pipe_->smartResize[i].width << " x "
													   << pipe_->smartResize[i].height);

				// We're doing to use the downscaler if it's available and we're downscaling
				// by more than 2x.
				// \todo - increase this "2x" threshold by using different resampler kernels.
				if (variant_.backEndDownscalerAvailable(0, i) &&
					(resamplerOutputWidth * 2 < InputWidth || resamplerOutputHeight * 2 < inputHeight))
				{
					uint16_t downscalerOutputWidth = InputWidth;
					uint16_t downscalerOutputHeight = inputHeight;

					// We treat the width and height the same. Look at the width first.
					if (resamplerOutputWidth * 2 < InputWidth)
					{
						// Try to put 2x downscale into the resampler, everything else into
						// the downscaler. But remember that it must do *at least* 2x, and no
						// more than 8x (being careful to round that limit up)..
						downscalerOutputWidth = std::clamp(resamplerOutputWidth * 2,
															 (InputWidth + 7) / 8,
															 InputWidth / 2);
					}
					// Now the same for the height.
					if (resamplerOutputHeight * 2 < inputHeight)
					{
						// Try to put 2x downscale into the resampler, everything else into
						// the downscaler. But remember that it must do *at least* 2x and no
						// more than 8x (being careful to round that limit up)..
						downscalerOutputHeight = std::clamp(resamplerOutputHeight * 2,
															  (inputHeight + 7) / 8,
															  inputHeight / 2);
					}

					PISP_LOG(debug, "Using downscaler, output size "
							 << downscalerOutputWidth << " x " <<  downscalerOutputHeight);

					// Now program up the downscaler.
					pisp_be_downscale_extra downscale = {};
					downscale.scaled_width = downscalerOutputWidth;
					downscale.scaled_height = downscalerOutputHeight;
					setDownscale(i, downscale);
					global.rgbEnables |= getFlagForBranch(RgbEnable::Downscale0, i);

					// Now adjust the input dimensions that the code below (which
					// programs the resampler) will see, so that it can, if necessary,
					// do its PPF coefficient adjustment.
					resamplerInputWidth = downscalerOutputWidth;
					resamplerInputHeight = downscalerOutputHeight;
				}
				else
				{
					global.rgbEnables &= ~getFlagForBranch(RgbEnable::Downscale0, i);
				}

				// Don't resample by unity: it needlessly reduces bit-depth, and can over-sharpen
				if (resamplerInputWidth == resamplerOutputWidth &&
					resamplerInputHeight == resamplerOutputHeight)
				{
					global.rgbEnables &= ~getFlagForBranch(RgbEnable::Resample0, i);
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
				double scaleFactorX = (double)(resamplerInputWidth - 1) / (resamplerOutputWidth - 1);
				double scaleFactorY = (double)(resamplerInputHeight - 1) / (resamplerOutputHeight - 1);
				pisp_be_resample_config resample = {};
				pisp_be_resample_extra resample_extra = {};

				if (scaleFactorX > 2.1 &&
					scaleFactorX < scaleFactorY * 1.1 && scaleFactorY < scaleFactorX * 1.1)
				{
					PISP_LOG(debug, "Setting the PPF as a trapezoidal filter");

					scaleFactorX = std::min<double>(scaleFactorX, NumTaps - 1);

					for (unsigned int p = 0; p < NumPhases; p++)
					{
						// Initial phase for the current pixel (offset 2 in the filter)
						// is calculated as 1 - p / NumPhases
						resample.coef[p * NumTaps + 0] =
							(1 << ResamplePrecision) - (p << ResamplePrecision) / NumPhases;

						resample.coef[p * NumTaps + 0] /= scaleFactorX;

						double scale = scaleFactorX - (1.0 - (double)p / NumPhases);
						for (unsigned int t = 1; t < 1 + std::ceil(scaleFactorX); t++)
						{
							double s = std::min(1.0, scale);
							resample.coef[p * NumTaps + t] = s * (1 << ResamplePrecision) / scaleFactorX;
							scale -= s;
						}
					}

					// resample_extra will be filled in below.
					setResample(i, resample, resample_extra);
				}
				else
				{
					// Let's choose a resampling filter based on the scaling factor.
					// The selection mapping is defined in the config json file.
					initialiseResample(resample, scaleFactorX);
				}

				// Last thing is to set the output dimensions.
				resample_extra.scaled_width = resamplerOutputWidth;
				resample_extra.scaled_height = resamplerOutputHeight;
				setResample(i, resample_extra);
				global.rgbEnables |= getFlagForBranch(RgbEnable::Resample0, i);
			}
		}
	}

	setGlobal(global);
	pipe_->smartResizeDirty = 0;
}

void BackEnd::updateTiles()
{
	if (pipe_->retile)
	{
		TilingConfig tilingConfig;
		const BeConfigExtra &ce = pipe_->configExtra;
		const ImageFormatConfig &inputFormat = config_->getInputFormat();
		RgbEnables rgbEnables = config_->getGlobal().rgbEnables;

		pipe_->retile = false;
		tilingConfig.inputAlignment = calculateInputAlignment();

		PISP_LOG(debug, "Input alignments are " << tilingConfig.inputAlignment << " pixels");

		tilingConfig.inputImageSize = tiling::Length2(inputFormat.width, inputFormat.height);

		for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		{
			const BeOutputFormatConfig &outputFormat = config_->getOutputFormat(i);
			const BeDownscaleConfig &downscale = config_->getDownscale(i);
			const BeResampleConfig &resample = config_->getResample(i);

			tilingConfig.crop[i] = tiling::Interval2(tiling::Interval(ce.crop[i].offset_x, ce.crop[i].width),
													  tiling::Interval(ce.crop[i].offset_y, ce.crop[i].height));

			if (tilingConfig.crop[i].x.length == 0 || tilingConfig.crop[i].y.length == 0)
				tilingConfig.crop[i] = tiling::Interval2(tiling::Interval(0, inputFormat.width),
														  tiling::Interval(0, inputFormat.height));

			tilingConfig.outputHMirror[i] = outputFormat.transform & PISP_BE_TRANSFORM_HFLIP;
			tilingConfig.downscaleFactor[i] =
				tiling::Length2(downscale.scale_factor_h, downscale.scale_factor_v);
			tilingConfig.resampleFactor[i] =
				tiling::Length2(resample.scale_factor_h, resample.scale_factor_v);
			tilingConfig.downscaleImageSize[i] =
				tiling::Length2(ce.downscale[i].scaled_width, ce.downscale[i].scaled_height);
			tilingConfig.outputImageSize[i] =
				tiling::Length2(outputFormat.image.width, outputFormat.image.height);
			tilingConfig.outputMaxAlignment[i] =
				calculateOutputAlignment(outputFormat.image.format, PISP_BACK_END_OUTPUT_MAX_ALIGN);
			tilingConfig.outputMinAlignment[i] =
				calculateOutputAlignment(outputFormat.image.format, PISP_BACK_END_OUTPUT_MIN_ALIGN);
		}

		tilingConfig.maxTileSize.dx = sw_.maxTileWidth ? sw_.maxTileWidth : variant_.backEndMaxTileWidth(0);
		tilingConfig.maxTileSize.dy = sw_.maxStripeHeight ? sw_.maxStripeHeight : MaxStripeHeight;
		tilingConfig.minTileSize = tiling::Length2(PISP_BACK_END_MIN_TILE_WIDTH, PISP_BACK_END_MIN_TILE_HEIGHT);

		tilingConfig.resampleEnables = tilingConfig.downscaleEnables = 0;
		if (rgbEnables & RgbEnable::Resample0)
			tilingConfig.resampleEnables = 1;
		if (rgbEnables & RgbEnable::Resample1)
			tilingConfig.resampleEnables |= (1 << 1);
		if (rgbEnables & RgbEnable::Downscale0)
			tilingConfig.downscaleEnables = 1;
		if (rgbEnables & RgbEnable::Downscale1)
			tilingConfig.downscaleEnables |= (1 << 1);

		// Set compressed_input to false as otherwise the tiling would pad tiles up to multiples of 8 pixels even when these lie
		// outside the actual image width (and we've chosen not to handle compression like that).
		tilingConfig.compressedInput = false;
		retilePipeline(tilingConfig);
		checkTiles(tilingConfig);
		pipe_->finaliseTiling = true;
	}

	if (pipe_->finaliseTiling)
	{
		finaliseTiling();
		pipe_->finaliseTiling = false;
	}
}

void BackEnd::retilePipeline(TilingConfig const &tilingConfig)
{
	// The tiling library provides tiles in a SW Tile structure.
	Tile tiles[PISP_BACK_END_NUM_TILES];
	tiling::Length2 grid;

	tilePipeline(tilingConfig, tiles, PISP_BACK_END_NUM_TILES, &grid);
	pipe_->numTilesX = grid.dx;
	pipe_->numTilesY = grid.dy;

	// Finally convert the Tiles into pisp_tiles.
	for (unsigned int i = 0; i < pipe_->numTilesX * pipe_->numTilesY; i++)
	{
		RgbEnables rgbEnables = config_->getGlobal().rgbEnables;

		pisp_tile &t = pipe_->tiles[i];

		memset(&t, 0, sizeof(pisp_tile));
		t.edge = 0;
		if (i < pipe_->numTilesX)
			t.edge |= PISP_TOP_EDGE;
		if (i >= pipe_->numTilesX * (pipe_->numTilesY - 1))
			t.edge |= PISP_BOTTOM_EDGE;
		if (i % pipe_->numTilesX == 0)
			t.edge |= PISP_LEFT_EDGE;
		if ((i + 1) % pipe_->numTilesX == 0)
			t.edge |= PISP_RIGHT_EDGE;

		t.input_offset_x = tiles[i].input.input.x.offset;
		t.input_offset_y = tiles[i].input.input.y.offset;
		t.input_width = tiles[i].input.input.x.length;
		t.input_height = tiles[i].input.input.y.length;

		if (tiles[i].input.output != tiles[i].input.input)
			throw std::runtime_error("BackEnd::retilePipeline: tiling error in Bayer pipe");

		for (unsigned int j = 0; j < variant_.backEndNumBranches(0); j++)
		{
			bool enabled = (rgbEnables & getFlagForBranch(RgbEnable::Output0, j));
			if (enabled && (tiles[i].output[j].output.x.length == 0 || tiles[i].output[j].output.y.length == 0))
			{
				// If a tile produces no output there's no point sending anything down this branch, so ensure the crop
				// "eats" everything and set everything else to zero.
				t.crop_x_start[j] = t.input_width;
				t.crop_x_end[j] = 0;
				t.crop_y_start[j] = t.input_height;
				t.crop_y_end[j] = 0;
				t.resample_in_width[j] = 0;
				t.resample_in_height[j] = 0;
				t.output_offset_x[j] = 0;
				t.output_offset_y[j] = 0;
				t.output_width[j] = 0;
				t.output_height[j] = 0;
				continue;
			}

			tiling::Crop2 downscaleCrop;
			tiling::Interval2 resample_size = tiles[i].crop[j].output;
			resample_size.x = resample_size.x - tiles[i].resample[j].crop.x;
			resample_size.y = resample_size.y - tiles[i].resample[j].crop.y;

			// When a resize stage is disabled, the tile size after the stage is found from the input of the
			// next block. Also there will be no extra crop necessary for the resize operation.
			if (rgbEnables & getFlagForBranch(RgbEnable::Downscale0, j))
			{
				downscaleCrop = tiles[i].downscale[j].crop + tiles[i].crop[j].crop;
				// Size of the tile going into the resample block needs to be set here.
				resample_size = tiles[i].downscale[j].output;
			}
			else if (rgbEnables & getFlagForBranch(RgbEnable::Resample0, j))
			{
				downscaleCrop = tiles[i].resample[j].crop + tiles[i].crop[j].crop;
			}
			else
			{
				downscaleCrop = tiles[i].output[j].crop + tiles[i].crop[j].crop;
			}

			t.crop_x_start[j] = downscaleCrop.x.start;
			t.crop_x_end[j] = downscaleCrop.x.end;
			t.crop_y_start[j] = downscaleCrop.y.start;
			t.crop_y_end[j] = downscaleCrop.y.end;
			t.resample_in_width[j] = resample_size.x.length;
			t.resample_in_height[j] = resample_size.y.length;
			t.output_offset_x[j] = tiles[i].output[j].output.x.offset;
			t.output_offset_y[j] = tiles[i].output[j].output.y.offset;
			t.output_width[j] = tiles[i].output[j].output.x.length;
			t.output_height[j] = tiles[i].output[j].output.y.length;

			for (int p = 0; p < 3; p++)
			{
				// Calculate x/y initial downsampler/resampler phases per-plane.
				if (rgbEnables & getFlagForBranch(RgbEnable::Downscale0, j))
				{
					const BeDownscaleConfig &downscale = config_->getDownscale(j);
					unsigned int frac_x = (resample_size.x.offset * downscale.scale_factor_h) &
										  ((1 << ScalePrecision) - 1);
					unsigned int frac_y = (resample_size.y.offset * downscale.scale_factor_v) &
										  ((1 << ScalePrecision) - 1);
					// Fractional component of the input required to generate the output pixel.
					t.downscale_phase_x[p * variant_.backEndNumBranches(0) + j] = (UnityPhase - frac_x);
					t.downscale_phase_y[p * variant_.backEndNumBranches(0) + j] = (UnityPhase - frac_y);
				}

				if (rgbEnables & getFlagForBranch(RgbEnable::Resample0, j))
				{
					const BeResampleConfig &resample = config_->getResample(j);
					// Location of the output pixel in the interpolated (input) image.
					unsigned int interpolated_pix_x =
						(t.output_offset_x[j] * NumPhases * resample.scale_factor_h) >> ScalePrecision;
					unsigned int interpolated_pix_y =
						(t.output_offset_y[j] * NumPhases * resample.scale_factor_v) >> ScalePrecision;
					// Phase of the interpolated input pixel.
					t.resample_phase_x[p * variant_.backEndNumBranches(0) + j] =
						((interpolated_pix_x % NumPhases) << ScalePrecision) / NumPhases;
					t.resample_phase_y[p * variant_.backEndNumBranches(0) + j] =
						((interpolated_pix_y % NumPhases) << ScalePrecision) / NumPhases;
					// Account for any user defined initial phase - this could be negative!
					t.resample_phase_x[p * variant_.backEndNumBranches(0) + j] +=
						pipe_->configExtra.resample[j].initial_phase_h[p];
					t.resample_phase_y[p * variant_.backEndNumBranches(0) + j] +=
						pipe_->configExtra.resample[j].initial_phase_v[p];
					// Have to be within this range, else some calculation went wrong.
					PISP_ASSERT(t.resample_phase_x[p * variant_.backEndNumBranches(0) + j] <= (2 * UnityPhase - 1));
					PISP_ASSERT(t.resample_phase_y[p * variant_.backEndNumBranches(0) + j] <= (2 * UnityPhase - 1));
				}
			}

			// Phase difference between planes cannot be > 0.5 pixels on the output dimenstions.
			if (rgbEnables & getFlagForBranch(RgbEnable::Resample0, j))
			{
				const BeResampleConfig &resample = config_->getResample(j);
				int phase_max = (resample.scale_factor_h * UnityPhase / 2) >> ScalePrecision;
				if (std::abs(t.resample_phase_x[0 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_x[1 * variant_.backEndNumBranches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_x[1 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_x[2 * variant_.backEndNumBranches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_x[0 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_x[2 * variant_.backEndNumBranches(0) + j]) > phase_max)
				{
					throw std::runtime_error("Resample phase x for tile is > 0.5 pixels on the output dimensions.");
				}
				phase_max = (resample.scale_factor_v * UnityPhase / 2) >> ScalePrecision;
				if (std::abs(t.resample_phase_y[0 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_y[1 * variant_.backEndNumBranches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_y[1 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_y[2 * variant_.backEndNumBranches(0) + j]) > phase_max ||
					std::abs(t.resample_phase_y[0 * variant_.backEndNumBranches(0) + j] -
							 t.resample_phase_y[2 * variant_.backEndNumBranches(0) + j]) > phase_max)
				{
					throw std::runtime_error("Resample phase y for tile is > 0.5 pixels on the output dimensions.");
				}
			}
		}
	}
}

void BackEnd::finaliseTiling()
{
	// Update tile parameters (offsets/strides) from on the BE pipeline configuration.
	for (unsigned int tileNum = 0; tileNum < pipe_->numTilesX * pipe_->numTilesY; tileNum++)
	{
		const BeLscConfig &lsc = config_->getLsc();
		const BeCacConfig &cac = config_->getCac();

		pisp_tile &t = pipe_->tiles[tileNum];

		calculateInputAddrOffset(t.input_offset_x, t.input_offset_y, config_->getInputFormat(),
									&t.input_addr_offset, &t.input_addr_offset2);
		calculateInputAddrOffset(t.input_offset_x, t.input_offset_y, config_->getTdnInputFormat(),
									&t.tdn_input_addr_offset);
		calculateInputAddrOffset(t.input_offset_x, t.input_offset_y, config_->getTdnOutputFormat(),
									&t.tdn_output_addr_offset);
		calculateInputAddrOffset(t.input_offset_x, t.input_offset_y, config_->getStitchInputFormat(),
									&t.stitch_input_addr_offset);
		calculateInputAddrOffset(t.input_offset_x, t.input_offset_y, config_->getStitchOutputFormat(),
									&t.stitch_output_addr_offset);
		PISP_LOG(debug, "Input offsets " << t.input_offset_x << "," << t.input_offset_y << " address offsets "
										 << t.input_addr_offset << " and " << t.input_addr_offset2);

		if (config_->getGlobal().bayerEnables & BayerEnable::Lsc)
		{
			t.lsc_grid_offset_x = (t.input_offset_x + pipe_->configExtra.lsc.offset_x) * lsc.grid_step_x;
			t.lsc_grid_offset_y = (t.input_offset_y + pipe_->configExtra.lsc.offset_y) * lsc.grid_step_y;
		}

		if (config_->getGlobal().bayerEnables & BayerEnable::Cac)
		{
			t.cac_grid_offset_x = (t.input_offset_x + pipe_->configExtra.cac.offset_x) * cac.grid_step_x;
			t.cac_grid_offset_y = (t.input_offset_y + pipe_->configExtra.cac.offset_y) * cac.grid_step_y;
		}

		for (unsigned int j = 0; j < variant_.backEndNumBranches(0); j++)
		{
			const BeOutputFormatConfig &outputFormat = config_->getOutputFormat(j);
			int output_offset_x_unflipped = t.output_offset_x[j], output_offset_y_unflipped = t.output_offset_y[j];

			if (outputFormat.transform & PISP_BE_TRANSFORM_HFLIP)
				t.output_offset_x[j] =
					outputFormat.image.width - output_offset_x_unflipped - t.output_width[j];

			if (outputFormat.transform & PISP_BE_TRANSFORM_VFLIP)
				t.output_offset_y[j] = outputFormat.image.height - output_offset_y_unflipped - 1;

			computeAddrOffset(outputFormat.image, t.output_offset_x[j], t.output_offset_y[j], &t.output_addr_offset[j],
							  &t.output_addr_offset2[j]);

			PISP_LOG(debug, "Branch " << j << " output offsets " << t.output_offset_x[j] << "," << t.output_offset_y[j]
									  << " address offsets " << t.output_addr_offset[j] << " and "
									  << t.output_addr_offset2[j]);
		}
	}
}

void BackEnd::getOutputSize(int i, uint16_t *width, uint16_t *height, ImageFormatConfig const &ifmt) const
{
	// This internal version doesn't check if the output is enabled
	if (pipe_->smartResize[i].width && pipe_->smartResize[i].height)
		*width = pipe_->smartResize[i].width, *height = pipe_->smartResize[i].height;
	else if (config_->getGlobal().rgbEnables & getFlagForBranch(RgbEnable::Resample0, i))
		*width = pipe_->configExtra.resample[i].scaled_width, *height = pipe_->configExtra.resample[i].scaled_height;
	else if (config_->getGlobal().rgbEnables & getFlagForBranch(RgbEnable::Downscale0, i))
		*width = pipe_->configExtra.downscale[i].scaled_width, *height = pipe_->configExtra.downscale[i].scaled_height;
	else if (pipe_->configExtra.crop[i].width) // crop width and height will be zero when crop disabled
		*width = pipe_->configExtra.crop[i].width, *height = pipe_->configExtra.crop[i].height;
	else
		*width = ifmt.width, *height = ifmt.height;
}

bool BackEnd::computeOutputImageFormat(unsigned int i, ImageFormatConfig &fmt,
									   ImageFormatConfig const &ifmt) const
{
	PISP_ASSERT(i < PISP_BACK_END_NUM_OUTPUTS);

	BeOutputFormatConfig outputFormat = config_->getOutputFormat(i);
	if (&fmt != &outputFormat.image)
		fmt = outputFormat.image;

	if (config_->getGlobal().rgbEnables & getFlagForBranch(RgbEnable::Output0, i))
{
		getOutputSize(i, &fmt.width, &fmt.height, ifmt);
		if (!fmt.stride)
			computeStride(fmt);
		else
			checkStride(fmt);
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
	BayerEnables bayerEnables = config_->getGlobal().bayerEnables;
	RgbEnables rgbEnables = config_->getGlobal().rgbEnables;

	PISP_LOG(debug, "New frame!");

	// On every start-of-frame we:
	// 1. Check the input configuration appears sensible.
	if (!(bayerEnables & BayerEnable::Input) && !(rgbEnables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::preFrameUpdate: neither Bayer nor RGB inputs are enabled");
	else if ((bayerEnables & BayerEnable::Input) && (rgbEnables & RgbEnable::Input))
		throw std::runtime_error("BackEnd::preFrameUpdate: both Bayer and RGB inputs are enabled");

	// 2. Also check the output configuration is all filled in and looks sensible. Again, addresses must be
	// left to the HAL.
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
	{
		BeOutputFormatConfig outputFormat = config_->getOutputFormat(i);
		computeOutputImageFormat(i, outputFormat.image, config_->getInputFormat());
		config_->setOutputFormat(i, outputFormat);

		if (outputFormat.image.format & PISP_IMAGE_FORMAT_INTEGRAL_IMAGE)
		{
			throw std::runtime_error("Integral images are not supported.");
		}
	}

	// 3. Fill in any other missing bits of config, and update the tiling if necessary.
	updateSmartResize();
	finaliseConfig();
	updateTiles();

	if (config)
	{
		// Allow passing of empty pointer, if only pipe_->hwConfig should be filled
		// 4. Write the config and tiles to the provided buffer to send to the hardware.
		config->num_tiles = pipe_->numTilesX * pipe_->numTilesY;
		memcpy(config->tiles, pipe_->tiles.data(), config->num_tiles * sizeof(pisp_tile));
		config->config = pipe_->hwConfig;

		// 5. Clear any dirty flags for the next configuration update.
		pipe_->dirtyFlagsBayer = BayerEnable::None;
		pipe_->dirtyFlagsRgb = RgbEnable::None;
		pipe_->dirtyFlagsExtra = DirtyFlag::None;
	}
}
