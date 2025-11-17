/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */

#include "backend_config_adapter.hpp"
#include "backend.hpp"

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
	{ RgbEnableInput,			{ PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT } },
	{ RgbEnableDedistort,		{ 0,								PISP_BE_RGB_ENABLE2_DEDISTORT } },
	{ RgbEnableCcm,				{ PISP_BE_RGB_ENABLE_CCM,			PISP_BE_RGB_ENABLE2_CCM } },
	{ RgbEnableSat_control,		{ PISP_BE_RGB_ENABLE_SAT_CONTROL,	PISP_BE_RGB_ENABLE2_SAT_CONTROL } },
	{ RgbEnableYcbcr,			{ PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR } },
	{ RgbEnableFalse_colour,	{ PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR } },
	{ RgbEnableSharpen,			{ PISP_BE_RGB_ENABLE_SHARPEN,		PISP_BE_RGB_ENABLE2_SHARPEN } },
	{ RgbEnableYcbcr_inverse,	{ PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE } },
	{ RgbEnablePre_csc0,		{ PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_PRE_CSC0 } },
	{ RgbEnablePre_csc1,		{ PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_PRE_CSC1 } },
	{ RgbEnableGamma0,			{ PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 } },
	{ RgbEnableGamma1,			{ 0,								PISP_BE_RGB_ENABLE2_GAMMA1 } },
	{ RgbEnablePost_csc0,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC0 } },
	{ RgbEnablePost_csc1,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC1 } },
	{ RgbEnableDownscale0,		{ PISP_BE_RGB_ENABLE_DOWNSCALE0,	PISP_BE_RGB_ENABLE2_DOWNSCALE0 } },
	{ RgbEnableDownscale1,		{ PISP_BE_RGB_ENABLE_DOWNSCALE1,	PISP_BE_RGB_ENABLE2_DOWNSCALE1 } },
	{ RgbEnableResample0,		{ PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 } },
	{ RgbEnableResample1,		{ PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 } },
	{ RgbEnableOutput0,			{ PISP_BE_RGB_ENABLE_OUTPUT0,		PISP_BE_RGB_ENABLE2_OUTPUT0 } },
	{ RgbEnableOutput1,			{ PISP_BE_RGB_ENABLE_OUTPUT1,		PISP_BE_RGB_ENABLE2_OUTPUT1 } },
	{ RgbEnableCompress0,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS0 } },
	{ RgbEnableCompress1,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS1 } },
};

const std::map<BayerEnables, enable_mapping> bayer_enable_map =
{
	{ BayerEnableInput,				{ PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT } },
	{ BayerEnableDecompress,			{ PISP_BE_BAYER_ENABLE_DECOMPRESS,			PISP_BE_BAYER_ENABLE_DECOMPRESS } },
	{ BayerEnableDpc,				{ PISP_BE_BAYER_ENABLE_DPC,					PISP_BE_BAYER_ENABLE_DPC } },
	{ BayerEnableGeq,				{ PISP_BE_BAYER_ENABLE_GEQ,					PISP_BE_BAYER_ENABLE_GEQ } },
	{ BayerEnableTdn_input,			{ PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT } },
	{ BayerEnableTdn_decompress,		{ PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS } },
	{ BayerEnableTdn,				{ PISP_BE_BAYER_ENABLE_TDN,					PISP_BE_BAYER_ENABLE_TDN } },
	{ BayerEnableTdn_compress,		{ PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS } },
	{ BayerEnableTdn_output,			{ PISP_BE_BAYER_ENABLE_TDN_OUTPUT,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT } },
	{ BayerEnableSdn,				{ PISP_BE_BAYER_ENABLE_SDN,					PISP_BE_BAYER_ENABLE_SDN } },
	{ BayerEnableBlc,				{ PISP_BE_BAYER_ENABLE_BLC,					PISP_BE_BAYER_ENABLE_BLC } },
	{ BayerEnableStitch_input,		{ PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT } },
	{ BayerEnableStitch_decompress,	{ PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS } },
	{ BayerEnableStitch,				{ PISP_BE_BAYER_ENABLE_STITCH,				PISP_BE_BAYER_ENABLE_STITCH } },
	{ BayerEnableStitch_compress,	{ PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS } },
	{ BayerEnableStitch_output,		{ PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT } },
	{ BayerEnableWbg,				{ PISP_BE_BAYER_ENABLE_WBG,					PISP_BE_BAYER_ENABLE_WBG } },
	{ BayerEnableCdn,				{ PISP_BE_BAYER_ENABLE_CDN,					PISP_BE_BAYER_ENABLE_CDN } },
	{ BayerEnableLsc,				{ PISP_BE_BAYER_ENABLE_LSC,					PISP_BE_BAYER_ENABLE_LSC } },
	{ BayerEnableTonemap,			{ PISP_BE_BAYER_ENABLE_TONEMAP,				PISP_BE_BAYER_ENABLE_TONEMAP } },
	{ BayerEnableCac,				{ PISP_BE_BAYER_ENABLE_CAC,					PISP_BE_BAYER_ENABLE_CAC } },
	{ BayerEnableDebin,				{ PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN } },
	{ BayerEnableDemosaic,			{ PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC } },
	{ BayerEnableTonemap_input,		{ PISP_BE_BAYER_ENABLE_TONEMAP_INPUT,		PISP_BE_BAYER_ENABLE_TONEMAP_INPUT } },
	{ BayerEnableTranspose,			{ PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE } },
};

// From https://en.cppreference.com/w/cpp/utility/variant/visit2.html
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

} // namespace

uint32_t BackendConfigAdapter::RgbEnablesToHw(RgbEnables rgb_enables) const
{
	uint32_t hw_enables = 0;

	std::visit(
		overloaded
		{
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgb_enable_map)
					if ((rgb_enables) & enable && map.v1)
						hw_enables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgb_enable_map)
					if ((rgb_enables & enable) && map.v2)
						hw_enables |= map.v2;
			}
		},
		be_->config_);

	return hw_enables;
}

uint32_t BackendConfigAdapter::BayerEnablesToHw(BayerEnables bayer_enables) const
{
	uint32_t hw_enables = 0;

	std::visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayer_enable_map)
					if ((bayer_enables) & enable && map.v1)
						hw_enables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayer_enable_map)
					if ((bayer_enables & enable) && map.v2)
						hw_enables |= map.v2;
			}
		},
		be_->config_);

	return hw_enables;
}

RgbEnables BackendConfigAdapter::HwToRgbEnables(uint32_t hw_enables) const
{
	RgbEnables enables = static_cast<RgbEnables>(0);

	std::visit(
		overloaded
		{
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgb_enable_map)
					if ((hw_enables & map.v1) && map.v1)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgb_enable_map)
					if ((hw_enables & map.v2) && map.v2)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		},
		be_->config_);

	return enables;
}

BayerEnables BackendConfigAdapter::HwToBayerEnables(uint32_t hw_enables) const
{
	BayerEnables enables = static_cast<BayerEnables>(0);

	std::visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayer_enable_map)
					if ((hw_enables & map.v1) && map.v1)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayer_enable_map)
					if ((hw_enables & map.v2) && map.v2)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		},
		be_->config_);

	return enables;
}
