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

const std::map<RgbEnableValues, enable_mapping> rgb_enable_map =
{
	{ RgbEnableValues::Input,			{ PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT } },
	{ RgbEnableValues::Dedistort,		{ 0,								PISP_BE_RGB_ENABLE2_DEDISTORT } },
	{ RgbEnableValues::Ccm,				{ PISP_BE_RGB_ENABLE_CCM,			PISP_BE_RGB_ENABLE2_CCM } },
	{ RgbEnableValues::Sat_control,		{ PISP_BE_RGB_ENABLE_SAT_CONTROL,	PISP_BE_RGB_ENABLE2_SAT_CONTROL } },
	{ RgbEnableValues::Ycbcr,			{ PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR } },
	{ RgbEnableValues::False_colour,	{ PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR } },
	{ RgbEnableValues::Sharpen,			{ PISP_BE_RGB_ENABLE_SHARPEN,		PISP_BE_RGB_ENABLE2_SHARPEN } },
	{ RgbEnableValues::Ycbcr_inverse,	{ PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE } },
	{ RgbEnableValues::Pre_csc0,		{ PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_PRE_CSC0 } },
	{ RgbEnableValues::Pre_csc1,		{ PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_PRE_CSC1 } },
	{ RgbEnableValues::Gamma0,			{ PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 } },
	{ RgbEnableValues::Gamma1,			{ 0,								PISP_BE_RGB_ENABLE2_GAMMA1 } },
	{ RgbEnableValues::Post_csc0,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC0 } },
	{ RgbEnableValues::Post_csc1,		{ 0,								PISP_BE_RGB_ENABLE2_POST_CSC1 } },
	{ RgbEnableValues::Downscale0,		{ PISP_BE_RGB_ENABLE_DOWNSCALE0,	PISP_BE_RGB_ENABLE2_DOWNSCALE0 } },
	{ RgbEnableValues::Downscale1,		{ PISP_BE_RGB_ENABLE_DOWNSCALE1,	PISP_BE_RGB_ENABLE2_DOWNSCALE1 } },
	{ RgbEnableValues::Resample0,		{ PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 } },
	{ RgbEnableValues::Resample1,		{ PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 } },
	{ RgbEnableValues::Output0,			{ PISP_BE_RGB_ENABLE_OUTPUT0,		PISP_BE_RGB_ENABLE2_OUTPUT0 } },
	{ RgbEnableValues::Output1,			{ PISP_BE_RGB_ENABLE_OUTPUT1,		PISP_BE_RGB_ENABLE2_OUTPUT1 } },
	{ RgbEnableValues::Compress0,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS0 } },
	{ RgbEnableValues::Compress1,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS1 } },
};

const std::map<BayerEnableValues, enable_mapping> bayer_enable_map =
{
	{ BayerEnableValues::Input,				{ PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT } },
	{ BayerEnableValues::Decompress,			{ PISP_BE_BAYER_ENABLE_DECOMPRESS,			PISP_BE_BAYER_ENABLE_DECOMPRESS } },
	{ BayerEnableValues::Dpc,				{ PISP_BE_BAYER_ENABLE_DPC,					PISP_BE_BAYER_ENABLE_DPC } },
	{ BayerEnableValues::Geq,				{ PISP_BE_BAYER_ENABLE_GEQ,					PISP_BE_BAYER_ENABLE_GEQ } },
	{ BayerEnableValues::Tdn_input,			{ PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT } },
	{ BayerEnableValues::Tdn_decompress,		{ PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS } },
	{ BayerEnableValues::Tdn,				{ PISP_BE_BAYER_ENABLE_TDN,					PISP_BE_BAYER_ENABLE_TDN } },
	{ BayerEnableValues::Tdn_compress,		{ PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS } },
	{ BayerEnableValues::Tdn_output,			{ PISP_BE_BAYER_ENABLE_TDN_OUTPUT,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT } },
	{ BayerEnableValues::Sdn,				{ PISP_BE_BAYER_ENABLE_SDN,					PISP_BE_BAYER_ENABLE_SDN } },
	{ BayerEnableValues::Blc,				{ PISP_BE_BAYER_ENABLE_BLC,					PISP_BE_BAYER_ENABLE_BLC } },
	{ BayerEnableValues::Stitch_input,		{ PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT } },
	{ BayerEnableValues::Stitch_decompress,	{ PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS } },
	{ BayerEnableValues::Stitch,				{ PISP_BE_BAYER_ENABLE_STITCH,				PISP_BE_BAYER_ENABLE_STITCH } },
	{ BayerEnableValues::Stitch_compress,	{ PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS } },
	{ BayerEnableValues::Stitch_output,		{ PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT } },
	{ BayerEnableValues::Wbg,				{ PISP_BE_BAYER_ENABLE_WBG,					PISP_BE_BAYER_ENABLE_WBG } },
	{ BayerEnableValues::Cdn,				{ PISP_BE_BAYER_ENABLE_CDN,					PISP_BE_BAYER_ENABLE_CDN } },
	{ BayerEnableValues::Lsc,				{ PISP_BE_BAYER_ENABLE_LSC,					PISP_BE_BAYER_ENABLE_LSC } },
	{ BayerEnableValues::Tonemap,			{ PISP_BE_BAYER_ENABLE_TONEMAP,				PISP_BE_BAYER_ENABLE_TONEMAP } },
	{ BayerEnableValues::Cac,				{ PISP_BE_BAYER_ENABLE_CAC,					PISP_BE_BAYER_ENABLE_CAC } },
	{ BayerEnableValues::Debin,				{ PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN } },
	{ BayerEnableValues::Demosaic,			{ PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC } },
	{ BayerEnableValues::Tonemap_input,		{ PISP_BE_BAYER_ENABLE_TONEMAP_INPUT,		PISP_BE_BAYER_ENABLE_TONEMAP_INPUT } },
	{ BayerEnableValues::Transpose,			{ PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE } },
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

BackendConfigAdapter::Global BackendConfigAdapter::HwtoGlobal(const pisp_be_global_config &hw_global) const
{
	Global global;

	global.bayer_order = hw_global.bayer_order;
	global.bayer_enables = HwToBayerEnables(hw_global.bayer_enables);
	global.rgb_enables = HwToRgbEnables(hw_global.rgb_enables);

	return global;
}

pisp_be_global_config BackendConfigAdapter::GlobaltoHw(const BackendConfigAdapter::Global &global) const
{
	pisp_be_global_config config{};

	config.bayer_order = global.bayer_order;
	config.bayer_enables = BayerEnablesToHw(global.bayer_enables);
	config.rgb_enables = RgbEnablesToHw(global.rgb_enables);

	return config;
}
