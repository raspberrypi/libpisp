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

struct EnableMapping
{
	uint32_t v1;
	uint32_t v2;
};

const std::map<RgbEnable, EnableMapping> rgbEnableMap =
{
	{ RgbEnable::Input,			{ PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT } },
	{ RgbEnable::Dedistort,		{ 0,								PISP_BE_RGB_ENABLE2_DEDISTORT } },
	{ RgbEnable::Ccm,			{ PISP_BE_RGB_ENABLE_CCM,			PISP_BE_RGB_ENABLE2_CCM } },
	{ RgbEnable::SatControl,	{ PISP_BE_RGB_ENABLE_SAT_CONTROL,	PISP_BE_RGB_ENABLE2_SAT_CONTROL } },
	{ RgbEnable::Ycbcr,			{ PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR } },
	{ RgbEnable::FalseColour,	{ PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR } },
	{ RgbEnable::Sharpen,		{ PISP_BE_RGB_ENABLE_SHARPEN,		PISP_BE_RGB_ENABLE2_SHARPEN } },
	{ RgbEnable::YcbcrInverse,	{ PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE } },
	{ RgbEnable::PreCsc0,		{ 0,								PISP_BE_RGB_ENABLE2_PRE_CSC0 } },
	{ RgbEnable::PreCsc1,		{ 0,								PISP_BE_RGB_ENABLE2_PRE_CSC1 } },
	{ RgbEnable::Gamma0,		{ PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 } },
	{ RgbEnable::Gamma1,		{ 0,								PISP_BE_RGB_ENABLE2_GAMMA1 } },
	{ RgbEnable::PostCsc0,		{ PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_POST_CSC0 } },
	{ RgbEnable::PostCsc1,		{ PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_POST_CSC1 } },
	{ RgbEnable::Downscale0,	{ PISP_BE_RGB_ENABLE_DOWNSCALE0,	PISP_BE_RGB_ENABLE2_DOWNSCALE0 } },
	{ RgbEnable::Downscale1,	{ PISP_BE_RGB_ENABLE_DOWNSCALE1,	PISP_BE_RGB_ENABLE2_DOWNSCALE1 } },
	{ RgbEnable::Resample0,		{ PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 } },
	{ RgbEnable::Resample1,		{ PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 } },
	{ RgbEnable::Output0,		{ PISP_BE_RGB_ENABLE_OUTPUT0,		PISP_BE_RGB_ENABLE2_OUTPUT0 } },
	{ RgbEnable::Output1,		{ PISP_BE_RGB_ENABLE_OUTPUT1,		PISP_BE_RGB_ENABLE2_OUTPUT1 } },
	{ RgbEnable::Compress0,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS0 } },
	{ RgbEnable::Compress1,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS1 } },
};

const std::map<BayerEnable, EnableMapping> bayerEnableMap =
{
	{ BayerEnable::Input,				{ PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT } },
	{ BayerEnable::Decompress,			{ PISP_BE_BAYER_ENABLE_DECOMPRESS,			PISP_BE_BAYER_ENABLE_DECOMPRESS } },
	{ BayerEnable::Dpc,					{ PISP_BE_BAYER_ENABLE_DPC,					PISP_BE_BAYER_ENABLE_DPC } },
	{ BayerEnable::Geq,					{ PISP_BE_BAYER_ENABLE_GEQ,					PISP_BE_BAYER_ENABLE_GEQ } },
	{ BayerEnable::TdnInput,			{ PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT } },
	{ BayerEnable::TdnDecompress,		{ PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS } },
	{ BayerEnable::Tdn,					{ PISP_BE_BAYER_ENABLE_TDN,					PISP_BE_BAYER_ENABLE_TDN } },
	{ BayerEnable::TdnCompress,			{ PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS } },
	{ BayerEnable::TdnOutput,			{ PISP_BE_BAYER_ENABLE_TDN_OUTPUT,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT } },
	{ BayerEnable::Sdn,					{ PISP_BE_BAYER_ENABLE_SDN,					PISP_BE_BAYER_ENABLE_SDN } },
	{ BayerEnable::Blc,					{ PISP_BE_BAYER_ENABLE_BLC,					PISP_BE_BAYER_ENABLE_BLC } },
	{ BayerEnable::StitchInput,			{ PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT } },
	{ BayerEnable::StitchDecompress,	{ PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS } },
	{ BayerEnable::Stitch,				{ PISP_BE_BAYER_ENABLE_STITCH,				PISP_BE_BAYER_ENABLE_STITCH } },
	{ BayerEnable::StitchCompress,		{ PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS } },
	{ BayerEnable::StitchOutput,		{ PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT } },
	{ BayerEnable::Wbg,					{ PISP_BE_BAYER_ENABLE_WBG,					PISP_BE_BAYER_ENABLE_WBG } },
	{ BayerEnable::Cdn,					{ PISP_BE_BAYER_ENABLE_CDN,					PISP_BE_BAYER_ENABLE_CDN } },
	{ BayerEnable::Lsc,					{ PISP_BE_BAYER_ENABLE_LSC,					PISP_BE_BAYER_ENABLE_LSC } },
	{ BayerEnable::Tonemap,				{ PISP_BE_BAYER_ENABLE_TONEMAP,				PISP_BE_BAYER_ENABLE_TONEMAP } },
	{ BayerEnable::Cac,					{ PISP_BE_BAYER_ENABLE_CAC,					PISP_BE_BAYER_ENABLE_CAC } },
	{ BayerEnable::Debin,				{ PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN } },
	{ BayerEnable::Demosaic,			{ PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC } },
	{ BayerEnable::TonemapInput,		{ PISP_BE_BAYER_ENABLE_TONEMAP_INPUT,		PISP_BE_BAYER_ENABLE_TONEMAP_INPUT } },
	{ BayerEnable::Transpose,			{ PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE } },
};

// From https://en.cppreference.com/w/cpp/utility/variant/visit2.html
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

} // namespace

uint32_t BackendConfigAdapter::rgbEnablesToHw(RgbEnables rgbEnables) const
{
	uint32_t hwEnables = 0;

	std::visit(
		overloaded
		{
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((rgbEnables) & enable && map.v1)
						hwEnables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((rgbEnables & enable) && map.v2)
						hwEnables |= map.v2;
			}
		},
		be_->config_);

	return hwEnables;
}

uint32_t BackendConfigAdapter::bayerEnablesToHw(BayerEnables bayerEnables) const
{
	uint32_t hwEnables = 0;

	std::visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((bayerEnables) & enable && map.v1)
						hwEnables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((bayerEnables & enable) && map.v2)
						hwEnables |= map.v2;
			}
		},
		be_->config_);

	return hwEnables;
}

RgbEnables BackendConfigAdapter::hwToRgbEnables(uint32_t hwEnables) const
{
	RgbEnables enables = static_cast<RgbEnables>(0);

	std::visit(
		overloaded
		{
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((hwEnables & map.v1) && map.v1)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((hwEnables & map.v2) && map.v2)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		},
		be_->config_);

	return enables;
}

BayerEnables BackendConfigAdapter::hwToBayerEnables(uint32_t hwEnables) const
{
	BayerEnables enables = static_cast<BayerEnables>(0);

	std::visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((hwEnables & map.v1) && map.v1)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((hwEnables & map.v2) && map.v2)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		},
		be_->config_);

	return enables;
}

BackendConfigAdapter::Global BackendConfigAdapter::hwToGlobal(const pisp_be_global_config &hwGlobal) const
{
	Global global;

	global.bayerOrder = hwGlobal.bayer_order;
	global.bayerEnables = hwToBayerEnables(hwGlobal.bayer_enables);
	global.rgbEnables = hwToRgbEnables(hwGlobal.rgb_enables);

	return global;
}

pisp_be_global_config BackendConfigAdapter::globalToHw(const BackendConfigAdapter::Global &global) const
{
	pisp_be_global_config config{};

	config.bayer_order = global.bayerOrder;
	config.bayer_enables = bayerEnablesToHw(global.bayerEnables);
	config.rgb_enables = rgbEnablesToHw(global.rgbEnables);

	return config;
}
