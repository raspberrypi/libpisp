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

	visit(
		overloaded {
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
		}
	);

	return hwEnables;
}

uint32_t BackendConfigAdapter::bayerEnablesToHw(BayerEnables bayerEnables) const
{
	uint32_t hwEnables = 0;

	visit(
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
		}
	);

	return hwEnables;
}

RgbEnables BackendConfigAdapter::hwToRgbEnables(uint32_t hwEnables) const
{
	RgbEnables enables = static_cast<RgbEnables>(0);

	visit(
		overloaded {
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
		}
	);

	return enables;
}

BayerEnables BackendConfigAdapter::hwToBayerEnables(uint32_t hwEnables) const
{
	BayerEnables enables = static_cast<BayerEnables>(0);

	visit(
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
		}
	);

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

void BackendConfigAdapter::setGlobal(const Global &global)
{
	pisp_be_global_config hwGlobal = globalToHw(global);
	hwGlobal.pad[0] = hwGlobal.pad[1] = hwGlobal.pad[2] = 0;
	visit([&hwGlobal](auto *cfg) {
		cfg->global = hwGlobal;
	});
}

BackendConfigAdapter::Global BackendConfigAdapter::getGlobal() const
{
	pisp_be_global_config hwGlobal = visit([](auto *cfg) -> pisp_be_global_config {
		return cfg->global;
	});
	return hwToGlobal(hwGlobal);
}

// Common field implementations (same type in both variants)
void BackendConfigAdapter::setInputFormat(const pisp_image_format_config &inputFormat)
{
	visit([&inputFormat](auto *cfg) { cfg->input_format = inputFormat; });
}

pisp_image_format_config BackendConfigAdapter::getInputFormat() const
{
	return visit([](auto *cfg) -> pisp_image_format_config { return cfg->input_format; });
}

void BackendConfigAdapter::setDecompress(const pisp_decompress_config &decompress)
{
	visit([&decompress](auto *cfg) { cfg->decompress = decompress; });
}

pisp_decompress_config BackendConfigAdapter::getDecompress() const
{
	return visit([](auto *cfg) -> pisp_decompress_config { return cfg->decompress; });
}

void BackendConfigAdapter::setDpc(const pisp_be_dpc_config &dpc)
{
	visit([&dpc](auto *cfg) { cfg->dpc = dpc; });
}

pisp_be_dpc_config BackendConfigAdapter::getDpc() const
{
	return visit([](auto *cfg) -> pisp_be_dpc_config { return cfg->dpc; });
}

void BackendConfigAdapter::setGeq(const pisp_be_geq_config &geq)
{
	visit([&geq](auto *cfg) { cfg->geq = geq; });
}

pisp_be_geq_config BackendConfigAdapter::getGeq() const
{
	return visit([](auto *cfg) -> pisp_be_geq_config { return cfg->geq; });
}

void BackendConfigAdapter::setTdnInputFormat(const pisp_image_format_config &tdnInputFormat)
{
	visit([&tdnInputFormat](auto *cfg) { cfg->tdn_input_format = tdnInputFormat; });
}

pisp_image_format_config BackendConfigAdapter::getTdnInputFormat() const
{
	return visit([](auto *cfg) -> pisp_image_format_config { return cfg->tdn_input_format; });
}

void BackendConfigAdapter::setTdnDecompress(const pisp_decompress_config &tdnDecompress)
{
	visit([&tdnDecompress](auto *cfg) { cfg->tdn_decompress = tdnDecompress; });
}

pisp_decompress_config BackendConfigAdapter::getTdnDecompress() const
{
	return visit([](auto *cfg) -> pisp_decompress_config { return cfg->tdn_decompress; });
}

void BackendConfigAdapter::setTdnCompress(const pisp_compress_config &tdnCompress)
{
	visit([&tdnCompress](auto *cfg) { cfg->tdn_compress = tdnCompress; });
}

pisp_compress_config BackendConfigAdapter::getTdnCompress() const
{
	return visit([](auto *cfg) -> pisp_compress_config { return cfg->tdn_compress; });
}

void BackendConfigAdapter::setTdnOutputFormat(const pisp_image_format_config &tdnOutputFormat)
{
	visit([&tdnOutputFormat](auto *cfg) { cfg->tdn_output_format = tdnOutputFormat; });
}

pisp_image_format_config BackendConfigAdapter::getTdnOutputFormat() const
{
	return visit([](auto *cfg) -> pisp_image_format_config { return cfg->tdn_output_format; });
}

void BackendConfigAdapter::setSdn(const pisp_be_sdn_config &sdn)
{
	visit([&sdn](auto *cfg) { cfg->sdn = sdn; });
}

pisp_be_sdn_config BackendConfigAdapter::getSdn() const
{
	return visit([](auto *cfg) -> pisp_be_sdn_config { return cfg->sdn; });
}

void BackendConfigAdapter::setBlc(const pisp_bla_config &blc)
{
	visit([&blc](auto *cfg) { cfg->blc = blc; });
}

pisp_bla_config BackendConfigAdapter::getBlc() const
{
	return visit([](auto *cfg) -> pisp_bla_config { return cfg->blc; });
}

void BackendConfigAdapter::setStitchInputFormat(const pisp_image_format_config &stitchInputFormat)
{
	visit([&stitchInputFormat](auto *cfg) { cfg->stitch_input_format = stitchInputFormat; });
}

pisp_image_format_config BackendConfigAdapter::getStitchInputFormat() const
{
	return visit([](auto *cfg) -> pisp_image_format_config { return cfg->stitch_input_format; });
}

void BackendConfigAdapter::setStitchDecompress(const pisp_decompress_config &stitchDecompress)
{
	visit([&stitchDecompress](auto *cfg) { cfg->stitch_decompress = stitchDecompress; });
}

pisp_decompress_config BackendConfigAdapter::getStitchDecompress() const
{
	return visit([](auto *cfg) -> pisp_decompress_config { return cfg->stitch_decompress; });
}

void BackendConfigAdapter::setStitchCompress(const pisp_compress_config &stitchCompress)
{
	visit([&stitchCompress](auto *cfg) { cfg->stitch_compress = stitchCompress; });
}

pisp_compress_config BackendConfigAdapter::getStitchCompress() const
{
	return visit([](auto *cfg) -> pisp_compress_config { return cfg->stitch_compress; });
}

void BackendConfigAdapter::setStitchOutputFormat(const pisp_image_format_config &stitchOutputFormat)
{
	visit([&stitchOutputFormat](auto *cfg) { cfg->stitch_output_format = stitchOutputFormat; });
}

pisp_image_format_config BackendConfigAdapter::getStitchOutputFormat() const
{
	return visit([](auto *cfg) -> pisp_image_format_config { return cfg->stitch_output_format; });
}

void BackendConfigAdapter::setLsc(const pisp_be_lsc_config &lsc)
{
	visit([&lsc](auto *cfg) { cfg->lsc = lsc; });
}

pisp_be_lsc_config BackendConfigAdapter::getLsc() const
{
	return visit([](auto *cfg) -> pisp_be_lsc_config { return cfg->lsc; });
}

void BackendConfigAdapter::setWbg(const pisp_wbg_config &wbg)
{
	visit([&wbg](auto *cfg) { cfg->wbg = wbg; });
}

pisp_wbg_config BackendConfigAdapter::getWbg() const
{
	return visit([](auto *cfg) -> pisp_wbg_config { return cfg->wbg; });
}

void BackendConfigAdapter::setCdn(const pisp_be_cdn_config &cdn)
{
	visit([&cdn](auto *cfg) { cfg->cdn = cdn; });
}

pisp_be_cdn_config BackendConfigAdapter::getCdn() const
{
	return visit([](auto *cfg) -> pisp_be_cdn_config { return cfg->cdn; });
}

void BackendConfigAdapter::setCac(const pisp_be_cac_config &cac)
{
	visit([&cac](auto *cfg) { cfg->cac = cac; });
}

pisp_be_cac_config BackendConfigAdapter::getCac() const
{
	return visit([](auto *cfg) -> pisp_be_cac_config { return cfg->cac; });
}

void BackendConfigAdapter::setDebin(const pisp_be_debin_config &debin)
{
	visit([&debin](auto *cfg) { cfg->debin = debin; });
}

pisp_be_debin_config BackendConfigAdapter::getDebin() const
{
	return visit([](auto *cfg) -> pisp_be_debin_config { return cfg->debin; });
}

void BackendConfigAdapter::setCcm(const pisp_be_ccm_config &ccm)
{
	visit([&ccm](auto *cfg) { cfg->ccm = ccm; });
}

pisp_be_ccm_config BackendConfigAdapter::getCcm() const
{
	return visit([](auto *cfg) -> pisp_be_ccm_config { return cfg->ccm; });
}

void BackendConfigAdapter::setSatControl(const pisp_be_sat_control_config &satControl)
{
	visit([&satControl](auto *cfg) { cfg->sat_control = satControl; });
}

pisp_be_sat_control_config BackendConfigAdapter::getSatControl() const
{
	return visit([](auto *cfg) -> pisp_be_sat_control_config { return cfg->sat_control; });
}

void BackendConfigAdapter::setYcbcr(const pisp_be_ccm_config &ycbcr)
{
	visit([&ycbcr](auto *cfg) { cfg->ycbcr = ycbcr; });
}

pisp_be_ccm_config BackendConfigAdapter::getYcbcr() const
{
	return visit([](auto *cfg) -> pisp_be_ccm_config { return cfg->ycbcr; });
}

void BackendConfigAdapter::setFalseColour(const pisp_be_false_colour_config &falseColour)
{
	visit([&falseColour](auto *cfg) { cfg->false_colour = falseColour; });
}

pisp_be_false_colour_config BackendConfigAdapter::getFalseColour() const
{
	return visit([](auto *cfg) -> pisp_be_false_colour_config { return cfg->false_colour; });
}

void BackendConfigAdapter::setSharpen(const pisp_be_sharpen_config &sharpen)
{
	visit([&sharpen](auto *cfg) { cfg->sharpen = sharpen; });
}

pisp_be_sharpen_config BackendConfigAdapter::getSharpen() const
{
	return visit([](auto *cfg) -> pisp_be_sharpen_config { return cfg->sharpen; });
}

void BackendConfigAdapter::setShFcCombine(const pisp_be_sh_fc_combine_config &shFcCombine)
{
	visit([&shFcCombine](auto *cfg) { cfg->sh_fc_combine = shFcCombine; });
}

pisp_be_sh_fc_combine_config BackendConfigAdapter::getShFcCombine() const
{
	return visit([](auto *cfg) -> pisp_be_sh_fc_combine_config { return cfg->sh_fc_combine; });
}

void BackendConfigAdapter::setYcbcrInverse(const pisp_be_ccm_config &ycbcrInverse)
{
	visit([&ycbcrInverse](auto *cfg) { cfg->ycbcr_inverse = ycbcrInverse; });
}

pisp_be_ccm_config BackendConfigAdapter::getYcbcrInverse() const
{
	return visit([](auto *cfg) -> pisp_be_ccm_config { return cfg->ycbcr_inverse; });
}

void BackendConfigAdapter::setCsc(unsigned int i, const pisp_be_ccm_config &csc)
{
	visit(overloaded {
		[&csc, i](pisp_be_config *cfg) { cfg->csc[i] = csc; },
		[&csc, i](pisp_be_config2 *cfg) { cfg->post_csc[i] = csc; }
	});
}

pisp_be_ccm_config BackendConfigAdapter::getCsc(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> pisp_be_ccm_config { return cfg->csc[i]; },
		[i](pisp_be_config2 *cfg) -> pisp_be_ccm_config { return cfg->post_csc[i]; }
	});
}

void BackendConfigAdapter::setDownscale(unsigned int i, const pisp_be_downscale_config &downscale)
{
	visit([&downscale, i](auto *cfg) { cfg->downscale[i] = downscale; });
}

pisp_be_downscale_config BackendConfigAdapter::getDownscale(unsigned int i) const
{
	return visit([i](auto *cfg) -> pisp_be_downscale_config { return cfg->downscale[i]; });
}

void BackendConfigAdapter::setOutputFormat(unsigned int i, const pisp_be_output_format_config &outputFormat)
{
	visit([&outputFormat, i](auto *cfg) { cfg->output_format[i] = outputFormat; });
}

pisp_be_output_format_config BackendConfigAdapter::getOutputFormat(unsigned int i) const
{
	return visit([i](auto *cfg) -> pisp_be_output_format_config { return cfg->output_format[i]; });
}

// Variant-specific implementations
void BackendConfigAdapter::setTdn(const pisp_be_tdn_config &tdn)
{
	visit(overloaded {
		[&tdn](pisp_be_config *cfg) {
			cfg->tdn = tdn;
		},
		[&tdn]([[maybe_unused]] pisp_be_config2 *cfg) {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
		}
	});
}

pisp_be_tdn_config BackendConfigAdapter::getTdn() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> pisp_be_tdn_config {
			return cfg->tdn;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> pisp_be_tdn_config {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
			pisp_be_tdn_config result{};
			return result;
		}
	});
}

void BackendConfigAdapter::setStitch(const pisp_be_stitch_config &stitch)
{
	visit(overloaded {
		[&stitch](pisp_be_config *cfg) {
			cfg->stitch = stitch;
		},
		[&stitch]([[maybe_unused]] pisp_be_config2 *cfg) {
			// v2 uses pisp_be_tdn2_config for stitch, conversion not implemented
		}
	});
}

pisp_be_stitch_config BackendConfigAdapter::getStitch() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> pisp_be_stitch_config {
			return cfg->stitch;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> pisp_be_stitch_config {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
			pisp_be_stitch_config result{};
			return result;
		}
	});
}

void BackendConfigAdapter::setTonemap(const pisp_be_tonemap_config &tonemap)
{
	visit(overloaded {
		[&tonemap](pisp_be_config *cfg) {
			cfg->tonemap = tonemap;
		},
		[&tonemap]([[maybe_unused]] pisp_be_config2 *cfg) {
			// v2 uses tonemap2 (pisp_be_tonemap2_config), conversion not implemented
		}
	});
}

pisp_be_tonemap_config BackendConfigAdapter::getTonemap() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> pisp_be_tonemap_config {
			return cfg->tonemap;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> pisp_be_tonemap_config {
			// v2 uses tonemap2, conversion not implemented
			pisp_be_tonemap_config result{};
			return result;
		}
	});
}

void BackendConfigAdapter::setDemosaic(const pisp_be_demosaic_config &demosaic)
{
	visit(overloaded {
		[&demosaic](pisp_be_config *cfg) {
			cfg->demosaic = demosaic;
		},
		[&demosaic](pisp_be_config2 *cfg) {
			// v2 uses pisp_be_demosaic2_config - copy compatible fields
			cfg->demosaic.sharper = demosaic.sharper;
			cfg->demosaic.fc_mode = demosaic.fc_mode;
		}
	});
}

pisp_be_demosaic_config BackendConfigAdapter::getDemosaic() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> pisp_be_demosaic_config {
			return cfg->demosaic;
		},
		[](pisp_be_config2 *cfg) -> pisp_be_demosaic_config {
			// Convert from v2 to v1
			pisp_be_demosaic_config result{};
			result.sharper = cfg->demosaic.sharper;
			result.fc_mode = cfg->demosaic.fc_mode;
			return result;
		}
	});
}

void BackendConfigAdapter::setGamma(const pisp_be_gamma_config &gamma)
{
	visit(overloaded {
		[&gamma](pisp_be_config *cfg) {
			cfg->gamma = gamma;
		},
		[&gamma](pisp_be_config2 *cfg) {
			// v2 uses array of pisp_be_gamma2_config
			// Copy to first element
			for (unsigned int i = 0; i < PISP_BE_GAMMA2_LUT_SIZE && i < PISP_BE_GAMMA_LUT_SIZE; i++) {
				cfg->gamma[0].lut[i] = gamma.lut[i];
			}
		}
	});
}

pisp_be_gamma_config BackendConfigAdapter::getGamma() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> pisp_be_gamma_config {
			return cfg->gamma;
		},
		[](pisp_be_config2 *cfg) -> pisp_be_gamma_config {
			// Convert from v2 array to v1 single
			pisp_be_gamma_config result{};
			for (unsigned int i = 0; i < PISP_BE_GAMMA_LUT_SIZE && i < PISP_BE_GAMMA2_LUT_SIZE; i++) {
				result.lut[i] = cfg->gamma[0].lut[i];
			}
			return result;
		}
	});
}

void BackendConfigAdapter::setResample(unsigned int i, const pisp_be_resample_config &resample)
{
	visit(overloaded {
		[&resample, i](pisp_be_config *cfg) {
			cfg->resample[i] = resample;
		},
		[&resample, i](pisp_be_config2 *cfg) {
			// v2 uses pisp_be_resample2_config
			cfg->resample[i].scale_factor_h = resample.scale_factor_h;
			cfg->resample[i].scale_factor_v = resample.scale_factor_v;
			// Copy coefficients - v1 has single coef array, v2 has separate coef_h and coef_v
			for (unsigned int j = 0; j < PISP_BE_RESAMPLE_FILTER_SIZE; j++) {
				cfg->resample[i].coef_h[j] = resample.coef[j];
				cfg->resample[i].coef_v[j] = resample.coef[j];
			}
		}
	});
}

pisp_be_resample_config BackendConfigAdapter::getResample(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> pisp_be_resample_config {
			return cfg->resample[i];
		},
		[i](pisp_be_config2 *cfg) -> pisp_be_resample_config {
			// Convert from v2 to v1
			pisp_be_resample_config result{};
			result.scale_factor_h = cfg->resample[i].scale_factor_h;
			result.scale_factor_v = cfg->resample[i].scale_factor_v;
			// v1 has single coef array, v2 has separate coef_h and coef_v - use coef_h
			for (unsigned int j = 0; j < PISP_BE_RESAMPLE_FILTER_SIZE; j++) {
				result.coef[j] = cfg->resample[i].coef_h[j];
			}
			return result;
		}
	});
}
