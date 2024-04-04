
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * frontend.cpp - PiSP Front End implementation
 */
#include "frontend.hpp"

#include <cstddef>
#include <type_traits>

#include "common/logging.hpp"
#include "common/utils.hpp"

using namespace libpisp;

namespace
{

inline uint32_t blockEnable(uint32_t block, unsigned int branch)
{
	return block << (4 * branch);
}

void finaliseLsc(pisp_fe_lsc_config &lsc, uint16_t width, uint16_t height)
{
	if (lsc.centre_x == 0)
		lsc.centre_x = width / 2;

	if (lsc.centre_y == 0)
		lsc.centre_y = height / 2;

	if (lsc.scale == 0)
	{
		uint16_t max_dx = std::max<int>(width - lsc.centre_x, lsc.centre_x);
		uint16_t max_dy = std::max<int>(height - lsc.centre_y, lsc.centre_y);
		uint32_t max_r2 = max_dx * (uint32_t)max_dx + max_dy * (uint32_t)max_dy;

		// spec requires r^2 to fit 31 bits
		PISP_ASSERT(max_r2 < (1u << 31));

		lsc.shift = 0;
		while (max_r2 >= 2 * ((PISP_FE_LSC_LUT_SIZE - 1) << FrontEnd::InterpPrecision))
		{
			max_r2 >>= 1;
			lsc.shift++;
		}

		lsc.scale =
			((1 << FrontEnd::ScalePrecision) * ((PISP_FE_LSC_LUT_SIZE - 1) << FrontEnd::InterpPrecision) - 1) / max_r2;
		if (lsc.scale >= (1 << FrontEnd::ScalePrecision))
			lsc.scale = (1 << FrontEnd::ScalePrecision) - 1;
	}
}

void finaliseAgc(pisp_fe_agc_stats_config &agc, uint16_t width, uint16_t height)
{
	if (agc.size_x == 0)
		agc.size_x = std::max(2, ((width - 2 * agc.offset_x) / PISP_AGC_STATS_SIZE) & ~1);
	if (agc.size_y == 0)
		agc.size_y = std::max(2, ((height - 2 * agc.offset_y) / PISP_AGC_STATS_SIZE) & ~1);
	if (agc.row_size_x == 0)
		agc.row_size_x = std::max(2, (width - 2 * agc.row_offset_x) & ~1);
	if (agc.row_size_y == 0)
		agc.row_size_y = std::max(2, ((height - 2 * agc.row_offset_y) / PISP_AGC_STATS_NUM_ROW_SUMS) & ~1);
}

void finaliseAwb(pisp_fe_awb_stats_config &awb, uint16_t width, uint16_t height)
{
	 // Just a warning that ACLS algorithms might want the size calculations
	 // here to match the Back End LSC. Here we round the cell width and height
	// to the nearest even number.
	if (awb.size_x == 0)
		awb.size_x = 2 * std::max(1, ((width - 2 * awb.offset_x + PISP_AWB_STATS_SIZE) / (2 * PISP_AWB_STATS_SIZE)));

	if (awb.size_y == 0)
		awb.size_y = 2 * std::max(1, ((height - 2 * awb.offset_y + PISP_AWB_STATS_SIZE) / (2 * PISP_AWB_STATS_SIZE)));
}

void finaliseCdaf(pisp_fe_cdaf_stats_config &cdaf, uint16_t width, uint16_t height)
{
	if (cdaf.size_x == 0)
		cdaf.size_x = std::max(2, ((width - 2 * cdaf.offset_x) / PISP_CDAF_STATS_SIZE) & ~1);
	if (cdaf.size_y == 0)
		cdaf.size_y = std::max(2, ((height - 2 * cdaf.offset_y) / PISP_CDAF_STATS_SIZE) & ~1);
}

void finaliseDownscale(pisp_fe_downscale_config &downscale, uint16_t width, uint16_t height)
{
	if (!(downscale.flags & DOWNSCALE_GENERAL))
	{
		// Old style. We specified a ratio, from which output size is computed.
		downscale.output_width = (((width >> 1) * downscale.xout) / downscale.xin) * 2;
		downscale.output_height = (((height >> 1) * downscale.yout) / downscale.yin) * 2;
	}
	else
	{
		// New style. We specified an output size, from which scaling is derived.
		int s, w = width >> 1;

		for(s = 0; w > (1<<s); ++s) /* empty */ ;
		downscale.shift = s;
		downscale.recip = ((16384 << s) + (w >> 1)) / w;
		downscale.input_width = width;
		downscale.input_height = height;
	}
}

void finaliseCompression(pisp_fe_config const &feConfig, int i)
{
	uint32_t fmt = feConfig.ch[i].output.format.format;
	uint32_t enables = feConfig.global.enables;

	if (PISP_IMAGE_FORMAT_COMPRESSED(fmt) && !(enables & blockEnable(PISP_FE_ENABLE_COMPRESS0, i)))
		PISP_LOG(fatal, "FrontEnd::finalise: output compressed but compression not enabled");

	if (!PISP_IMAGE_FORMAT_COMPRESSED(fmt) && (enables & blockEnable(PISP_FE_ENABLE_COMPRESS0, i)))
		PISP_LOG(fatal, "FrontEnd::finalise: output uncompressed but compression enabled");

	if ((enables & blockEnable(PISP_FE_ENABLE_COMPRESS0, i)) && !PISP_IMAGE_FORMAT_BPS_8(fmt))
		PISP_LOG(fatal, "FrontEnd::finalise: compressed output is not 8 bit");
}

template <typename T>
std::enable_if_t<std::is_integral_v<T>> inline div2RoundE(T &val)
{
	// Divide by 2 and round to the next even number.
	val = ((val + 2) & ~3) >> 1;
}

void decimateConfig(pisp_fe_config &feConfig)
{
	if (feConfig.global.enables & PISP_FE_ENABLE_LSC)
	{
		div2RoundE(feConfig.lsc.centre_x);
		div2RoundE(feConfig.lsc.centre_y);
	}

	if (feConfig.global.enables & PISP_FE_ENABLE_CDAF_STATS)
	{
		div2RoundE(feConfig.cdaf_stats.offset_x);
		div2RoundE(feConfig.cdaf_stats.offset_y);
		div2RoundE(feConfig.cdaf_stats.size_x);
		div2RoundE(feConfig.cdaf_stats.size_y);
		div2RoundE(feConfig.cdaf_stats.skip_x);
		div2RoundE(feConfig.cdaf_stats.skip_y);
	}

	if (feConfig.global.enables & PISP_FE_ENABLE_AWB_STATS)
	{
		div2RoundE(feConfig.awb_stats.offset_x);
		div2RoundE(feConfig.awb_stats.offset_y);
		div2RoundE(feConfig.awb_stats.size_x);
		div2RoundE(feConfig.awb_stats.size_y);
	}

	if (feConfig.global.enables & PISP_FE_ENABLE_AGC_STATS)
	{
		div2RoundE(feConfig.agc_stats.offset_x);
		div2RoundE(feConfig.agc_stats.offset_y);
		div2RoundE(feConfig.agc_stats.size_x);
		div2RoundE(feConfig.agc_stats.size_y);
		div2RoundE(feConfig.agc_stats.row_offset_x);
		div2RoundE(feConfig.agc_stats.row_offset_y);
		div2RoundE(feConfig.agc_stats.row_size_x);
		div2RoundE(feConfig.agc_stats.row_size_y);
	}

	for (unsigned int i = 0; i < PISP_FLOATING_STATS_NUM_ZONES; i++)
	{
		pisp_fe_floating_stats_region &region = feConfig.floating_stats.regions[i];
		div2RoundE(region.offset_x);
		div2RoundE(region.offset_y);
		div2RoundE(region.size_x);
		div2RoundE(region.size_y);
	}
}

} // namespace

FrontEnd::FrontEnd(bool streaming, PiSPVariant const &variant, int align) : variant_(variant), align_(align)
{
	pisp_fe_input_config input;

	memset(&feConfig_, 0, sizeof(feConfig_));
	memset(&input, 0, sizeof(input));

	input.streaming = !!streaming;

	// Configure some plausible default AXI reader settings.
	if (!input.streaming)
	{
		input.axi.maxlen_flags = PISP_AXI_FLAG_ALIGN | 7;
		input.axi.cache_prot = 0x33;
		input.axi.qos = 0;
		input.holdoff = 0;
	}
	else
	{
		feConfig_.output_axi.maxlen_flags = 0xaf;
		feConfig_.output_axi.cache_prot = 0x32;
		feConfig_.output_axi.qos = 0x8410;
		feConfig_.output_axi.thresh = 0x0140;
		feConfig_.output_axi.throttle = 0x4100;
		feConfig_.dirty_flags_extra |= PISP_FE_DIRTY_OUTPUT_AXI;
	}

	pisp_fe_global_config global;
	getGlobal(global);
	global.enables |= PISP_FE_ENABLE_INPUT;
	setGlobal(global);
	setInput(input);
}

FrontEnd::~FrontEnd()
{
}

void FrontEnd::setGlobal(pisp_fe_global_config const &global)
{
	// label anything that has become enabled as dirty
	feConfig_.dirty_flags |= (global.enables & ~feConfig_.global.enables);
	feConfig_.global = global;
	feConfig_.dirty_flags_extra |= PISP_FE_DIRTY_GLOBAL;
}

void FrontEnd::getGlobal(pisp_fe_global_config &global) const
{
	global = feConfig_.global;
}

void FrontEnd::setInput(pisp_fe_input_config const &input)
{
	feConfig_.input = input;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_INPUT;
}

void FrontEnd::setDecompress(pisp_decompress_config const &decompress)
{
	feConfig_.decompress = decompress;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_DECOMPRESS;
}

void FrontEnd::setDecompand(pisp_fe_decompand_config const &decompand)
{
	feConfig_.decompand = decompand;
	feConfig_.decompand.pad = 0;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_DECOMPAND;
}

void FrontEnd::setDpc(pisp_fe_dpc_config const &dpc)
{
	feConfig_.dpc = dpc;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_DPC;
}

void FrontEnd::setBla(pisp_bla_config const &bla)
{
	feConfig_.bla = bla;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_BLA;
}

void FrontEnd::setStatsCrop(pisp_fe_crop_config const &statsCrop)
{
	feConfig_.stats_crop = statsCrop;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_STATS_CROP;
}

void FrontEnd::setBlc(pisp_bla_config const &blc)
{
	feConfig_.blc = blc;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_BLC;
}

void FrontEnd::setLsc(pisp_fe_lsc_config const &lsc)
{
	feConfig_.lsc = lsc;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_LSC;
}

void FrontEnd::setRgby(pisp_fe_rgby_config const &rgby)
{
	feConfig_.rgby = rgby;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_RGBY;
}

void FrontEnd::setAgcStats(pisp_fe_agc_stats_config const &agcStats)
{
	feConfig_.agc_stats = agcStats;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_AGC_STATS;
}

void FrontEnd::getAgcStats(pisp_fe_agc_stats_config &agcStats) const
{
	agcStats = feConfig_.agc_stats;
}

void FrontEnd::setAwbStats(pisp_fe_awb_stats_config const &awbStats)
{
	feConfig_.awb_stats = awbStats;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_AWB_STATS;
}

void FrontEnd::getAwbStats(pisp_fe_awb_stats_config &awbStats) const
{
	awbStats = feConfig_.awb_stats;
}

void FrontEnd::setFloatingStats(pisp_fe_floating_stats_config const &floatingStats)
{
	feConfig_.floating_stats = floatingStats;
	feConfig_.dirty_flags_extra |= PISP_FE_DIRTY_FLOATING;
}

void FrontEnd::setCdafStats(pisp_fe_cdaf_stats_config const &cdafStats)
{
	feConfig_.cdaf_stats = cdafStats;
	feConfig_.dirty_flags |= PISP_FE_ENABLE_CDAF_STATS;
}

void FrontEnd::getCdafStats(pisp_fe_cdaf_stats_config &cdafStats) const
{
	cdafStats = feConfig_.cdaf_stats;
}

void FrontEnd::setCrop(unsigned int outputNum, pisp_fe_crop_config const &crop)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	feConfig_.ch[outputNum].crop = crop;
	feConfig_.dirty_flags |= blockEnable(PISP_FE_ENABLE_CROP0, outputNum);
}

void FrontEnd::setDownscale(unsigned int outputNum, pisp_fe_downscale_config const &downscale)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	PISP_ASSERT(variant_.frontEndDownscalerAvailable(0, outputNum));

	feConfig_.ch[outputNum].downscale = downscale;
	feConfig_.dirty_flags |= blockEnable(PISP_FE_ENABLE_DOWNSCALE0, outputNum);
}

void FrontEnd::setCompress(unsigned int outputNum, pisp_compress_config const &compress)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	feConfig_.ch[outputNum].compress = compress;
	feConfig_.dirty_flags |= blockEnable(PISP_FE_ENABLE_COMPRESS0, outputNum);
}

void FrontEnd::setOutputFormat(unsigned int outputNum, pisp_image_format_config const &outputFormat)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	feConfig_.ch[outputNum].output.format = outputFormat;
	feConfig_.dirty_flags |= blockEnable(PISP_FE_ENABLE_OUTPUT0, outputNum);
}

void FrontEnd::setOutputIntrLines(unsigned int outputNum, int ilines)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	feConfig_.ch[outputNum].output.ilines = ilines;
	feConfig_.dirty_flags |= blockEnable(PISP_FE_ENABLE_OUTPUT0, outputNum);
}

void FrontEnd::setOutputBuffer(unsigned int outputNum, pisp_fe_output_buffer_config const &outputBuffer)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	feConfig_.output_buffer[outputNum] = outputBuffer;
	// Assume these always get written.
}

void FrontEnd::getInput(pisp_fe_input_config &input) const
{
	input = feConfig_.input;
}

void FrontEnd::getInputBuffer(pisp_fe_input_buffer_config &inputBuffer) const
{
	inputBuffer = feConfig_.input_buffer;
}

void FrontEnd::getDecompress(pisp_decompress_config &decompress) const
{
	decompress = feConfig_.decompress;
}

void FrontEnd::getDecompand(pisp_fe_decompand_config &decompand) const
{
	decompand = feConfig_.decompand;
}

void FrontEnd::getDpc(pisp_fe_dpc_config &dpc) const
{
	dpc = feConfig_.dpc;
}

void FrontEnd::getBla(pisp_bla_config &bla) const
{
	bla = feConfig_.bla;
}

void FrontEnd::getStatsCrop(pisp_fe_crop_config &statsCrop) const
{
	statsCrop = feConfig_.stats_crop;
}

void FrontEnd::getBlc(pisp_bla_config &blc) const
{
	blc = feConfig_.blc;
}

void FrontEnd::getRgby(pisp_fe_rgby_config &rgby) const
{
	rgby = feConfig_.rgby;
}

void FrontEnd::getLsc(pisp_fe_lsc_config &lsc) const
{
	lsc = feConfig_.lsc;
}

void FrontEnd::getFloatingStats(pisp_fe_floating_stats_config &floatingStats) const
{
	floatingStats = feConfig_.floating_stats;
}

void FrontEnd::getCrop(unsigned int outputNum, pisp_fe_crop_config &crop) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	crop = feConfig_.ch[outputNum].crop;
}

void FrontEnd::getDownscale(unsigned int outputNum, pisp_fe_downscale_config &downscale) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	downscale = feConfig_.ch[outputNum].downscale;
}

void FrontEnd::getCompress(unsigned int outputNum, pisp_compress_config &compress) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	compress = feConfig_.ch[outputNum].compress;
}

void FrontEnd::getOutputFormat(unsigned int outputNum, pisp_image_format_config &outputFormat) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	outputFormat = feConfig_.ch[outputNum].output.format;
}

void FrontEnd::getOutputBuffer(unsigned int outputNum, pisp_fe_output_buffer_config &outputBuffer) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	outputBuffer = feConfig_.output_buffer[outputNum];
}

int FrontEnd::getOutputIntrLines(unsigned int outputNum) const
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));
	return feConfig_.ch[outputNum].output.ilines;
}

void FrontEnd::prepare(pisp_fe_config *config)
{
	// Only finalise blocks that are dirty *and* enabled.
	uint32_t dirtyFlags = feConfig_.dirty_flags & feConfig_.global.enables;
	uint16_t width = feConfig_.input.format.width, height = feConfig_.input.format.height;

	if (feConfig_.global.enables & PISP_FE_ENABLE_STATS_CROP)
	{
		width = feConfig_.stats_crop.width;
		height = feConfig_.stats_crop.height;
	}

	if (dirtyFlags & PISP_FE_ENABLE_LSC)
		finaliseLsc(feConfig_.lsc, width, height);
	if (dirtyFlags & PISP_FE_ENABLE_AGC_STATS)
		finaliseAgc(feConfig_.agc_stats, width, height);
	if (dirtyFlags & PISP_FE_ENABLE_AWB_STATS)
		finaliseAwb(feConfig_.awb_stats, width, height);
	if (dirtyFlags & PISP_FE_ENABLE_CDAF_STATS)
		finaliseCdaf(feConfig_.cdaf_stats, width, height);

	width = feConfig_.input.format.width, height = feConfig_.input.format.height;
	for (int i = 0; i < PISP_FE_NUM_OUTPUTS; i++)
	{
		if (dirtyFlags & blockEnable(PISP_FE_ENABLE_DOWNSCALE0, i))
		{
			int cwidth = width, cheight = height;

			if (feConfig_.global.enables & blockEnable(PISP_FE_ENABLE_CROP0, i))
				cwidth = feConfig_.ch[i].crop.width, cheight = feConfig_.ch[i].crop.height;

			finaliseDownscale(feConfig_.ch[i].downscale, cwidth, cheight);
		}

		if (dirtyFlags & (blockEnable(PISP_FE_ENABLE_OUTPUT0, i) | blockEnable(PISP_FE_ENABLE_COMPRESS0, i)))
			finaliseCompression(feConfig_, i);

		if (dirtyFlags & blockEnable(PISP_FE_ENABLE_OUTPUT0, i))
		{
			pisp_image_format_config &imageConfig = feConfig_.ch[i].output.format;

			fixOutputSize(i);
			if (!imageConfig.stride)
				computeStrideAlign(imageConfig, align_);
		}
	}

	*config = feConfig_;

	// Fixup any grid offsets/sizes if stats decimation is enabled.
	if (config->global.enables & PISP_FE_ENABLE_DECIMATE)
		decimateConfig(*config);

	feConfig_.dirty_flags = feConfig_.dirty_flags_extra = 0;
}

void FrontEnd::fixOutputSize(unsigned int outputNum)
{
	PISP_ASSERT(outputNum < variant_.frontEndNumBranches(0));

	pisp_image_format_config &imageConfig = feConfig_.ch[outputNum].output.format;

	imageConfig.width = imageConfig.height = 0;

	if (feConfig_.global.enables & blockEnable(PISP_FE_ENABLE_OUTPUT0, outputNum))
	{
		imageConfig.width = feConfig_.input.format.width;
		imageConfig.height = feConfig_.input.format.height;

		if (feConfig_.global.enables & blockEnable(PISP_FE_ENABLE_CROP0, outputNum))
		{
			imageConfig.width = feConfig_.ch[outputNum].crop.width;
			imageConfig.width = feConfig_.ch[outputNum].crop.height;
		}

		if (feConfig_.global.enables & blockEnable(PISP_FE_ENABLE_DOWNSCALE0, outputNum))
		{
			imageConfig.width = feConfig_.ch[outputNum].downscale.output_width;
			imageConfig.height = feConfig_.ch[outputNum].downscale.output_height;
		}
	}
}
