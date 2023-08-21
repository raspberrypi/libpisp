
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * frontend.cpp - PiSP Front End implementation
 */
#include "frontend.hpp"

#include "common/logging.hpp"
#include "common/utils.hpp"

using namespace libpisp;

namespace
{

struct config_param
{
	uint32_t dirty_flag;
	uint32_t dirty_flag_extra;
	std::size_t offset;
	std::size_t size;
};

const config_param config_map[] = {
	// *_dirty_flag_extra types
	{ 0, PISP_FE_DIRTY_GLOBAL,     offsetof(pisp_fe_config, global),           sizeof(pisp_fe_global_config)         },
	{ 0, PISP_FE_DIRTY_FLOATING,   offsetof(pisp_fe_config, floating_stats),   sizeof(pisp_fe_floating_stats_config) },
	{ 0, PISP_FE_DIRTY_OUTPUT_AXI, offsetof(pisp_fe_config, output_axi),       sizeof(pisp_fe_output_axi_config)     },
	// *_dirty_flag types
	{ PISP_FE_ENABLE_INPUT, 0, offsetof(pisp_fe_config, input), sizeof(pisp_fe_input_config)                         },
	{ PISP_FE_ENABLE_DECOMPRESS, 0, offsetof(pisp_fe_config, decompress), sizeof(pisp_decompress_config)             },
	{ PISP_FE_ENABLE_DECOMPAND, 0, offsetof(pisp_fe_config, decompand), sizeof(pisp_fe_decompand_config)             },
	{ PISP_FE_ENABLE_BLA, 0, offsetof(pisp_fe_config, bla), sizeof(pisp_bla_config)                                  },
	{ PISP_FE_ENABLE_DPC, 0, offsetof(pisp_fe_config, dpc), sizeof(pisp_fe_dpc_config)                               },
	{ PISP_FE_ENABLE_STATS_CROP, 0, offsetof(pisp_fe_config, stats_crop), sizeof(pisp_fe_crop_config)                },
	{ PISP_FE_ENABLE_BLC, 0, offsetof(pisp_fe_config, blc), sizeof(pisp_bla_config)                                  },
	{ PISP_FE_ENABLE_CDAF_STATS, 0, offsetof(pisp_fe_config, cdaf_stats), sizeof(pisp_fe_cdaf_stats_config)          },
	{ PISP_FE_ENABLE_AWB_STATS, 0, offsetof(pisp_fe_config, awb_stats), sizeof(pisp_fe_awb_stats_config)             },
	{ PISP_FE_ENABLE_RGBY, 0, offsetof(pisp_fe_config, rgby), sizeof(pisp_fe_rgby_config)                            },
	{ PISP_FE_ENABLE_LSC, 0, offsetof(pisp_fe_config, lsc), sizeof(pisp_fe_lsc_config)                               },
	{ PISP_FE_ENABLE_AGC_STATS, 0, offsetof(pisp_fe_config, agc_stats), sizeof(pisp_agc_statistics)                  },
	{ PISP_FE_ENABLE_CROP0, 0, offsetof(pisp_fe_config, ch[0].crop), sizeof(pisp_fe_crop_config)                     },
	{ PISP_FE_ENABLE_DOWNSCALE0, 0, offsetof(pisp_fe_config, ch[0].downscale), sizeof(pisp_fe_downscale_config)      },
	{ PISP_FE_ENABLE_COMPRESS0, 0, offsetof(pisp_fe_config, ch[0].compress), sizeof(pisp_compress_config)            },
	{ PISP_FE_ENABLE_OUTPUT0, 0, offsetof(pisp_fe_config, ch[0].output), sizeof(pisp_fe_output_config)               },
	{ PISP_FE_ENABLE_CROP1, 0, offsetof(pisp_fe_config, ch[1].crop), sizeof(pisp_fe_crop_config)                     },
	{ PISP_FE_ENABLE_DOWNSCALE1, 0, offsetof(pisp_fe_config, ch[1].downscale), sizeof(pisp_fe_downscale_config)      },
	{ PISP_FE_ENABLE_COMPRESS1, 0, offsetof(pisp_fe_config, ch[1].compress), sizeof(pisp_compress_config)            },
	{ PISP_FE_ENABLE_OUTPUT1, 0, offsetof(pisp_fe_config, ch[1].output), sizeof(pisp_fe_output_config)               },
};

inline uint32_t block_enable(uint32_t block, unsigned int branch)
{
	return block << (4 * branch);
}

void finalise_lsc(pisp_fe_lsc_config &lsc, uint16_t width, uint16_t height)
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

void finalise_agc(pisp_fe_agc_stats_config &agc, uint16_t width, uint16_t height)
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

void finalise_awb(pisp_fe_awb_stats_config &awb, uint16_t width, uint16_t height)
{
	 // Just a warning that ACLS algorithms might want the size calculations
	 // here to match the Back End LSC.
	if (awb.size_x == 0)
		awb.size_x = std::max(2, ((width - 2 * awb.offset_x + PISP_AWB_STATS_SIZE - 1) / PISP_AWB_STATS_SIZE));
	awb.size_x += (awb.size_x & 1);

	if (awb.size_y == 0)
		awb.size_y = std::max(2, ((height - 2 * awb.offset_y + PISP_AWB_STATS_SIZE - 1) / PISP_AWB_STATS_SIZE));
	awb.size_y += (awb.size_y & 1);
}

void finalise_cdaf(pisp_fe_cdaf_stats_config &cdaf, uint16_t width, uint16_t height)
{
	if (cdaf.size_x == 0)
		cdaf.size_x = std::max(2, ((width - 2 * cdaf.offset_x) / PISP_CDAF_STATS_SIZE) & ~1);
	if (cdaf.size_y == 0)
		cdaf.size_y = std::max(2, ((height - 2 * cdaf.offset_y) / PISP_CDAF_STATS_SIZE) & ~1);
}

void finalise_downscale(pisp_fe_downscale_config &downscale, uint16_t width, uint16_t height)
{
	downscale.output_width = (((width >> 1) * downscale.xout) / downscale.xin) * 2;
	downscale.output_height = (((height >> 1) * downscale.yout) / downscale.yin) * 2;
}

void finalise_compression(pisp_fe_config const &fe_config, int i)
{
	uint32_t fmt = fe_config.ch[i].output.format.format;
	uint32_t enables = fe_config.global.enables;

	if (PISP_IMAGE_FORMAT_compressed(fmt) && !(enables & block_enable(PISP_FE_ENABLE_COMPRESS0, i)))
		PISP_LOG(fatal, "FrontEnd::finalise: output compressed but compression not enabled");

	if (!PISP_IMAGE_FORMAT_compressed(fmt) && (enables & block_enable(PISP_FE_ENABLE_COMPRESS0, i)))
		PISP_LOG(fatal, "FrontEnd::finalise: output uncompressed but compression enabled");

	if ((enables & block_enable(PISP_FE_ENABLE_COMPRESS0, i)) && !PISP_IMAGE_FORMAT_bps_8(fmt))
		PISP_LOG(fatal, "FrontEnd::finalise: compressed output is not 8 bit");
}

void decimate_config(pisp_fe_config &fe_config)
{
	if (fe_config.global.enables & PISP_FE_ENABLE_LSC)
	{
		fe_config.lsc.centre_x >>= 1;
		fe_config.lsc.centre_y >>= 1;
	}

	if (fe_config.global.enables & PISP_FE_ENABLE_CDAF_STATS)
	{
		fe_config.cdaf_stats.offset_x >>= 1;
		fe_config.cdaf_stats.offset_y >>= 1;
		fe_config.cdaf_stats.size_x >>= 1;
		fe_config.cdaf_stats.size_y >>= 1;
		fe_config.cdaf_stats.skip_x >>= 1;
		fe_config.cdaf_stats.skip_y >>= 1;
	}

	if (fe_config.global.enables & PISP_FE_ENABLE_AWB_STATS)
	{
		fe_config.awb_stats.offset_x >>= 1;
		fe_config.awb_stats.offset_y >>= 1;
		fe_config.awb_stats.size_x >>= 1;
		fe_config.awb_stats.size_y >>= 1;
	}

	if (fe_config.global.enables & PISP_FE_ENABLE_AGC_STATS)
	{
		fe_config.agc_stats.offset_x >>= 1;
		fe_config.agc_stats.offset_y >>= 1;
		fe_config.agc_stats.size_x >>= 1;
		fe_config.agc_stats.size_y >>= 1;
	}

	for (unsigned int i = 0; i < PISP_FLOATING_STATS_NUM_ZONES; i++)
	{
		pisp_fe_floating_stats_region &region = fe_config.floating_stats.regions[i];
		region.offset_x >>= 1;
		region.offset_y >>= 1;
		region.size_x >>= 1;
		region.size_y >>= 1;
	}
}

} // namespace

FrontEnd::FrontEnd(bool streaming, PiSPVariant const &variant, int align) : variant_(variant), align_(align)
{
	pisp_fe_input_config input;

	memset(&fe_config_, 0, sizeof(fe_config_));
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
		pisp_fe_output_axi_config output_axi;
		output_axi.maxlen_flags = 0xaf;
		output_axi.cache_prot = 0x32;
		output_axi.qos = 0x8410;
		output_axi.thresh = 0x0140;
		output_axi.throttle = 0x4100;
		SetOutputAXI(output_axi);
	}

	pisp_fe_global_config global;
	GetGlobal(global);
	global.enables |= PISP_FE_ENABLE_INPUT;
	SetGlobal(global);
	SetInput(input);
}

FrontEnd::~FrontEnd()
{
}

void FrontEnd::SetGlobal(pisp_fe_global_config const &global)
{
	// label anything that has become enabled as dirty
	fe_config_.dirty_flags |= (global.enables & ~fe_config_.global.enables);
	fe_config_.global = global;
	fe_config_.dirty_flags_extra |= PISP_FE_DIRTY_GLOBAL;
}

void FrontEnd::GetGlobal(pisp_fe_global_config &global) const
{
	global = fe_config_.global;
}

void FrontEnd::SetInput(pisp_fe_input_config const &input)
{
	fe_config_.input = input;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_INPUT;
}

void FrontEnd::SetDecompress(pisp_decompress_config const &decompress)
{
	fe_config_.decompress = decompress;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_DECOMPRESS;
}

void FrontEnd::SetDecompand(pisp_fe_decompand_config const &decompand)
{
	fe_config_.decompand = decompand;
	fe_config_.decompand.pad = 0;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_DECOMPAND;
}

void FrontEnd::SetDpc(pisp_fe_dpc_config const &dpc)
{
	fe_config_.dpc = dpc;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_DPC;
}

void FrontEnd::SetBla(pisp_bla_config const &bla)
{
	fe_config_.bla = bla;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_BLA;
}

void FrontEnd::SetStatsCrop(pisp_fe_crop_config const &stats_crop)
{
	fe_config_.stats_crop = stats_crop;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_STATS_CROP;
}

void FrontEnd::SetBlc(pisp_bla_config const &blc)
{
	fe_config_.blc = blc;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_BLC;
}

void FrontEnd::SetLsc(pisp_fe_lsc_config const &lsc)
{
	fe_config_.lsc = lsc;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_LSC;
}

void FrontEnd::SetRGBY(pisp_fe_rgby_config const &rgby)
{
	fe_config_.rgby = rgby;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_RGBY;
}

void FrontEnd::SetAgcStats(pisp_fe_agc_stats_config const &agc_stats)
{
	fe_config_.agc_stats = agc_stats;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_AGC_STATS;
}

void FrontEnd::GetAgcStats(pisp_fe_agc_stats_config &agc_stats)
{
	agc_stats = fe_config_.agc_stats;
}

void FrontEnd::SetAwbStats(pisp_fe_awb_stats_config const &awb_stats)
{
	fe_config_.awb_stats = awb_stats;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_AWB_STATS;
}

void FrontEnd::GetAwbStats(pisp_fe_awb_stats_config &awb_stats)
{
	awb_stats = fe_config_.awb_stats;
}

void FrontEnd::SetFloatingStats(pisp_fe_floating_stats_config const &floating_stats)
{
	fe_config_.floating_stats = floating_stats;
	fe_config_.dirty_flags_extra |= PISP_FE_DIRTY_FLOATING;
}

void FrontEnd::SetCdafStats(pisp_fe_cdaf_stats_config const &cdaf_stats)
{
	fe_config_.cdaf_stats = cdaf_stats;
	fe_config_.dirty_flags |= PISP_FE_ENABLE_CDAF_STATS;
}

void FrontEnd::GetCdafStats(pisp_fe_cdaf_stats_config &cdaf_stats)
{
	cdaf_stats = fe_config_.cdaf_stats;
}

void FrontEnd::SetCrop(unsigned int output_num, pisp_fe_crop_config const &crop)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	fe_config_.ch[output_num].crop = crop;
	fe_config_.dirty_flags |= block_enable(PISP_FE_ENABLE_CROP0, output_num);
}

void FrontEnd::SetDownscale(unsigned int output_num, pisp_fe_downscale_config const &downscale)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));
	PISP_ASSERT(variant_.FrontEndDownscalerAvailable(0, output_num));

	fe_config_.ch[output_num].downscale = downscale;
	fe_config_.dirty_flags |= block_enable(PISP_FE_ENABLE_DOWNSCALE0, output_num);
}

void FrontEnd::SetCompress(unsigned int output_num, pisp_compress_config const &compress)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	fe_config_.ch[output_num].compress = compress;
	fe_config_.dirty_flags |= block_enable(PISP_FE_ENABLE_COMPRESS0, output_num);
}

void FrontEnd::SetOutputFormat(unsigned int output_num, pisp_image_format_config const &output_format)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	fe_config_.ch[output_num].output.format = output_format;
	fe_config_.dirty_flags |= block_enable(PISP_FE_ENABLE_OUTPUT0, output_num);
}

void FrontEnd::SetOutputIntrLines(unsigned int output_num, int ilines)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	fe_config_.ch[output_num].output.ilines = ilines;
	fe_config_.dirty_flags |= block_enable(PISP_FE_ENABLE_OUTPUT0, output_num);
}

void FrontEnd::SetOutputBuffer(unsigned int output_num, pisp_fe_output_buffer_config const &output_buffer)
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	fe_config_.output_buffer[output_num] = output_buffer;
	// Assume these always get written.
}

void FrontEnd::SetOutputAXI(pisp_fe_output_axi_config const &output_axi)
{
	fe_config_.output_axi = output_axi;
	fe_config_.dirty_flags_extra |= PISP_FE_DIRTY_OUTPUT_AXI;
}

void FrontEnd::MergeConfig(const pisp_fe_config &config)
{
	for (auto const &param : config_map)
	{
		if ((param.dirty_flag & config.dirty_flags) || (param.dirty_flag_extra & config.dirty_flags_extra))
		{
			const uint8_t *src = reinterpret_cast<const uint8_t *>(&config) + param.offset;
			uint8_t *dest = reinterpret_cast<uint8_t *>(&fe_config_) + param.offset;

			memcpy(dest, src, param.size);
			fe_config_.dirty_flags |= param.dirty_flag;
			fe_config_.dirty_flags_extra |= param.dirty_flag_extra;
		}
	}
}

void FrontEnd::Prepare(pisp_fe_config *config)
{
	// Only finalise blocks that are dirty *and* enabled.
	uint32_t dirty_flags = fe_config_.dirty_flags & fe_config_.global.enables;
	uint16_t width = fe_config_.input.format.width, height = fe_config_.input.format.height;

	if (fe_config_.global.enables & PISP_FE_ENABLE_STATS_CROP)
	{
		width = fe_config_.stats_crop.width;
		height = fe_config_.stats_crop.height;
	}

	if (fe_config_.global.enables & PISP_FE_ENABLE_DECIMATE)
	{
		width = ((width + 2) & ~3) >> 1;
		height = 2 * (height >> 2) + ((height & 3) ? 1 : 0);
	}

	if (dirty_flags & PISP_FE_ENABLE_LSC)
		finalise_lsc(fe_config_.lsc, width, height);
	if (dirty_flags & PISP_FE_ENABLE_AGC_STATS)
		finalise_agc(fe_config_.agc_stats, width, height);
	if (dirty_flags & PISP_FE_ENABLE_AWB_STATS)
		finalise_awb(fe_config_.awb_stats, width, height);
	if (dirty_flags & PISP_FE_ENABLE_CDAF_STATS)
		finalise_cdaf(fe_config_.cdaf_stats, width, height);

	width = fe_config_.input.format.width, height = fe_config_.input.format.height;
	for (int i = 0; i < PISP_FE_NUM_OUTPUTS; i++)
	{
		if (dirty_flags & block_enable(PISP_FE_ENABLE_DOWNSCALE0, i))
		{
			int cwidth = width, cheight = height;

			if (fe_config_.global.enables & block_enable(PISP_FE_ENABLE_CROP0, i))
				cwidth = fe_config_.ch[i].crop.width, cheight = fe_config_.ch[i].crop.height;

			finalise_downscale(fe_config_.ch[i].downscale, cwidth, cheight);
		}

		if (dirty_flags & (block_enable(PISP_FE_ENABLE_OUTPUT0, i) | block_enable(PISP_FE_ENABLE_COMPRESS0, i)))
			finalise_compression(fe_config_, i);

		if (dirty_flags & block_enable(PISP_FE_ENABLE_OUTPUT0, i))
		{
			pisp_image_format_config &image_config = fe_config_.ch[i].output.format;

			getOutputSize(i, image_config.width, image_config.height);
			if (!image_config.stride)
				compute_stride_align(image_config, align_);
		}
	}

	*config = fe_config_;

	// Fixup any grid offsets/sizes if stats decimation is enabled.
	if (config->global.enables & PISP_FE_ENABLE_DECIMATE)
		decimate_config(*config);

	fe_config_.dirty_flags = fe_config_.dirty_flags_extra = 0;
}

void FrontEnd::getOutputSize(unsigned int output_num, uint16_t &width, uint16_t &height) const
{
	PISP_ASSERT(output_num < variant_.FrontEndNumBranches(0));

	width = height = 0;

	if (fe_config_.global.enables & block_enable(PISP_FE_ENABLE_OUTPUT0, output_num))
	{
		width = fe_config_.input.format.width;
		height = fe_config_.input.format.height;

		if (fe_config_.global.enables & block_enable(PISP_FE_ENABLE_CROP0, output_num))
		{
			width = fe_config_.ch[output_num].crop.width;
			height = fe_config_.ch[output_num].crop.height;
		}

		if (fe_config_.global.enables & block_enable(PISP_FE_ENABLE_DOWNSCALE0, output_num))
		{
			width = fe_config_.ch[output_num].downscale.output_width;
			height = fe_config_.ch[output_num].downscale.output_height;
		}
	}
}
