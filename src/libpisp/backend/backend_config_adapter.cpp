/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */

#include "backend_config_adapter.hpp"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <variant>

using namespace libpisp;

namespace
{

struct enable_mapping
{
	uint32_t v1;
	uint32_t v2;
};

const std::map<RgbEnables, enable_mapping> rgb_enable_map =
{
	{ RgbEnables::Input,			{ PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT } },
	{ RgbEnables::Dedistort,		{ 0,								PISP_BE_RGB_ENABLE2_DEDISTORT } },
	{ RgbEnables::Ccm,				{ PISP_BE_RGB_ENABLE_CCM,			PISP_BE_RGB_ENABLE2_CCM } },
	{ RgbEnables::Sat_control,		{ PISP_BE_RGB_ENABLE_SAT_CONTROL,	PISP_BE_RGB_ENABLE2_SAT_CONTROL } },
	{ RgbEnables::Ycbcr,			{ PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR } },
	{ RgbEnables::False_colour,		{ PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR } },
	{ RgbEnables::Sharpen,			{ PISP_BE_RGB_ENABLE_SHARPEN,		PISP_BE_RGB_ENABLE2_SHARPEN } },
	{ RgbEnables::Ycbcr_inverse,	{ PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE } },
	{ RgbEnables::Pre_csc0,			{ PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_PRE_CSC0 } },
	{ RgbEnables::Pre_csc1,			{ PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_PRE_CSC1 } },
	{ RgbEnables::Gamma0,			{ PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 } },
	{ RgbEnables::Gamma1,			{ 0,								PISP_BE_RGB_ENABLE2_GAMMA1 } },
	{ RgbEnables::Post_csc0,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC0 } },
	{ RgbEnables::Post_csc1,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC1 } },
	{ RgbEnables::Downscale0,		{ PISP_BE_RGB_ENABLE_DOWNSCALE0,	PISP_BE_RGB_ENABLE2_DOWNSCALE0 } },
	{ RgbEnables::Downscale1,		{ PISP_BE_RGB_ENABLE_DOWNSCALE1,	PISP_BE_RGB_ENABLE2_DOWNSCALE1 } },
	{ RgbEnables::Resample0,		{ PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 } },
	{ RgbEnables::Resample1,		{ PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 } },
	{ RgbEnables::Output0,			{ PISP_BE_RGB_ENABLE_OUTPUT0,		PISP_BE_RGB_ENABLE2_OUTPUT0 } },
	{ RgbEnables::Output1,			{ PISP_BE_RGB_ENABLE_OUTPUT1,		PISP_BE_RGB_ENABLE2_OUTPUT1 } },
	{ RgbEnables::Compress0,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS0 } },
	{ RgbEnables::Compress1,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS1 } },
};

const std::map<BayerEnables, enable_mapping> bayer_enable_map =
{
	{ BayerEnables::Input,				{ PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT } },
	{ BayerEnables::Decompress,			{ PISP_BE_BAYER_ENABLE_DECOMPRESS,			PISP_BE_BAYER_ENABLE_DECOMPRESS } },
	{ BayerEnables::Dpc,				{ PISP_BE_BAYER_ENABLE_DPC,					PISP_BE_BAYER_ENABLE_DPC } },
	{ BayerEnables::Geq,				{ PISP_BE_BAYER_ENABLE_GEQ,					PISP_BE_BAYER_ENABLE_GEQ } },
	{ BayerEnables::Tdn_input,			{ PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT } },
	{ BayerEnables::Tdn_decompress,		{ PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS } },
	{ BayerEnables::Tdn,				{ PISP_BE_BAYER_ENABLE_TDN,					PISP_BE_BAYER_ENABLE_TDN } },
	{ BayerEnables::Tdn_compress,		{ PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS } },
	{ BayerEnables::Tdn_output,			{ PISP_BE_BAYER_ENABLE_TDN_OUTPUT,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT } },
	{ BayerEnables::Sdn,				{ PISP_BE_BAYER_ENABLE_SDN,					PISP_BE_BAYER_ENABLE_SDN } },
	{ BayerEnables::Blc,				{ PISP_BE_BAYER_ENABLE_BLC,					PISP_BE_BAYER_ENABLE_BLC } },
	{ BayerEnables::Stitch_input,		{ PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT } },
	{ BayerEnables::Stitch_decompress,	{ PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS } },
	{ BayerEnables::Stitch,				{ PISP_BE_BAYER_ENABLE_STITCH,				PISP_BE_BAYER_ENABLE_STITCH } },
	{ BayerEnables::Stitch_compress,	{ PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS } },
	{ BayerEnables::Stitch_output,		{ PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT } },
	{ BayerEnables::Wbg,				{ PISP_BE_BAYER_ENABLE_WBG,					PISP_BE_BAYER_ENABLE_WBG } },
	{ BayerEnables::Cdn,				{ PISP_BE_BAYER_ENABLE_CDN,					PISP_BE_BAYER_ENABLE_CDN } },
	{ BayerEnables::Lsc,				{ PISP_BE_BAYER_ENABLE_LSC,					PISP_BE_BAYER_ENABLE_LSC } },
	{ BayerEnables::Tonemap,			{ PISP_BE_BAYER_ENABLE_TONEMAP,				PISP_BE_BAYER_ENABLE_TONEMAP } },
	{ BayerEnables::Cac,				{ PISP_BE_BAYER_ENABLE_CAC,					PISP_BE_BAYER_ENABLE_CAC } },
	{ BayerEnables::Debin,				{ PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN } },
	{ BayerEnables::Demosaic,			{ PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC } },
	{ BayerEnables::Tonemap_input,		{ PISP_BE_BAYER_ENABLE_TONEMAP_INPUT,		PISP_BE_BAYER_ENABLE_TONEMAP_INPUT } },
	{ BayerEnables::Transpose,			{ PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE } },
};

} // namespace

void SetRgbEnable(BackEndConfig config, RgbEnables rgb_enables, bool dirty)
{
	std::visit(
		[rgb_enables](auto *cfg)
		{
			const bool is_v1 = std::is_same_v<decltype(cfg), pisp_be_config *>;
			
			for (const auto &[enable_flag, mapping] : rgb_enable_map)
			{
				if (rgb_enables & enable_flag)
				{
					uint32_t hw_flag = is_v1 ? mapping.v1 : mapping.v2;
					if (hw_flag != 0)
						cfg->global.rgb_enables |= hw_flag;
				}
			}
		},
		config);
}

void SetBayerEnable(BackEndConfig config, BayerEnables bayer_enables, bool dirty)
{
	std::visit(
		[bayer_enables](auto *cfg)
		{
			const bool is_v1 = std::is_same_v<decltype(cfg), pisp_be_config *>;
			
			for (const auto &[enable_flag, mapping] : bayer_enable_map)
			{
				if (bayer_enables & enable_flag)
				{
					uint32_t hw_flag = is_v1 ? mapping.v1 : mapping.v2;
					if (hw_flag != 0)
						cfg->global.bayer_enables |= hw_flag;
				}
			}
		},
		config);
}
