/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */

#include "backend_config_adapter.hpp"

#include <cstring>
#include <map>
#include <stdexcept>

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
					if ((rgbEnables & enable) && map.v1)
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
					if ((bayerEnables & enable) && map.v1)
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
	RgbEnables enables = RgbEnable::None;

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
	BayerEnables enables = BayerEnable::None;

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

void BackendConfigAdapter::setGlobal(const BeGlobal &global)
{
	visit([&global, this](auto *cfg) {
		cfg->global.bayer_enables = bayerEnablesToHw(global.bayerEnables);
		cfg->global.rgb_enables = rgbEnablesToHw(global.rgbEnables);
		cfg->global.bayer_order = global.bayerOrder;
		cfg->global.pad[0] = cfg->global.pad[1] = cfg->global.pad[2] = 0;
	});
}

BeGlobal BackendConfigAdapter::getGlobal() const
{
	BeGlobal global;

	visit([&global, this](auto *cfg) {
		global.bayerEnables = hwToBayerEnables(cfg->global.bayer_enables);
		global.rgbEnables = hwToRgbEnables(cfg->global.rgb_enables);
		global.bayerOrder = cfg->global.bayer_order;
	});

	return global;
}

// Common field implementations (same type in both variants)
void BackendConfigAdapter::setInputFormat(const ImageFormatConfig &inputFormat)
{
	visit([&inputFormat](auto *cfg) { cfg->input_format = inputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->input_format; });
}

void BackendConfigAdapter::setDecompress(const DecompressConfig &decompress)
{
	visit([&decompress](auto *cfg) { cfg->decompress = decompress; });
}

DecompressConfig BackendConfigAdapter::getDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->decompress; });
}

void BackendConfigAdapter::setDpc(const BeDpcConfig &dpc)
{
	visit([&dpc](auto *cfg) {
		cfg->dpc = dpc;
		cfg->dpc.pad = 0;
	});
}

BeDpcConfig BackendConfigAdapter::getDpc() const
{
	return visit([](auto *cfg) -> BeDpcConfig { return cfg->dpc; });
}

void BackendConfigAdapter::setGeq(const BeGeqConfig &geq)
{
	visit([&geq](auto *cfg) { cfg->geq = geq; });
}

BeGeqConfig BackendConfigAdapter::getGeq() const
{
	return visit([](auto *cfg) -> BeGeqConfig { return cfg->geq; });
}

void BackendConfigAdapter::setTdnInputFormat(const ImageFormatConfig &tdnInputFormat)
{
	visit([&tdnInputFormat](auto *cfg) { cfg->tdn_input_format = tdnInputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getTdnInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_input_format; });
}

void BackendConfigAdapter::setTdnDecompress(const DecompressConfig &tdnDecompress)
{
	visit([&tdnDecompress](auto *cfg) { cfg->tdn_decompress = tdnDecompress; });
}

DecompressConfig BackendConfigAdapter::getTdnDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->tdn_decompress; });
}

void BackendConfigAdapter::setTdnCompress(const CompressConfig &tdnCompress)
{
	visit([&tdnCompress](auto *cfg) { cfg->tdn_compress = tdnCompress; });
}

CompressConfig BackendConfigAdapter::getTdnCompress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->tdn_compress; });
}

void BackendConfigAdapter::setTdnOutputFormat(const ImageFormatConfig &tdnOutputFormat)
{
	visit([&tdnOutputFormat](auto *cfg) { cfg->tdn_output_format = tdnOutputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getTdnOutputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_output_format; });
}

void BackendConfigAdapter::setSdn(const BeSdnConfig &sdn)
{
	visit([&sdn](auto *cfg) {
		cfg->sdn = sdn;
		cfg->sdn.pad = 0;
	});
}

BeSdnConfig BackendConfigAdapter::getSdn() const
{
	return visit([](auto *cfg) -> BeSdnConfig { return cfg->sdn; });
}

void BackendConfigAdapter::setBlc(const BlaConfig &blc)
{
	visit([&blc](auto *cfg) {
		cfg->blc = blc;
		cfg->blc.pad[0] = cfg->blc.pad[1] = 0;
	});
}

BlaConfig BackendConfigAdapter::getBlc() const
{
	return visit([](auto *cfg) -> BlaConfig { return cfg->blc; });
}

void BackendConfigAdapter::setStitchInputFormat(const ImageFormatConfig &stitchInputFormat)
{
	visit([&stitchInputFormat](auto *cfg) { cfg->stitch_input_format = stitchInputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getStitchInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_input_format; });
}

void BackendConfigAdapter::setStitchDecompress(const DecompressConfig &stitchDecompress)
{
	visit([&stitchDecompress](auto *cfg) { cfg->stitch_decompress = stitchDecompress; });
}

DecompressConfig BackendConfigAdapter::getStitchDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->stitch_decompress; });
}

void BackendConfigAdapter::setStitchCompress(const CompressConfig &stitchCompress)
{
	visit([&stitchCompress](auto *cfg) { cfg->stitch_compress = stitchCompress; });
}

CompressConfig BackendConfigAdapter::getStitchCompress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->stitch_compress; });
}

void BackendConfigAdapter::setStitchOutputFormat(const ImageFormatConfig &stitchOutputFormat)
{
	visit([&stitchOutputFormat](auto *cfg) { cfg->stitch_output_format = stitchOutputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getStitchOutputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_output_format; });
}

void BackendConfigAdapter::setLsc(const BeLscConfig &lsc)
{
	visit([&lsc](auto *cfg) { 
		cfg->lsc.grid_step_x = lsc.grid_step_x;
		cfg->lsc.grid_step_y = lsc.grid_step_y;
		std::memcpy(cfg->lsc.lut_packed, lsc.lut_packed.data(), sizeof(cfg->lsc.lut_packed));
	});

	// Store the extras as well!
	extras_->lsc = lsc;
}

BeLscConfig BackendConfigAdapter::getLsc() const
{
	return extras_->lsc;
}

void BackendConfigAdapter::setWbg(const WbgConfig &wbg)
{
	visit([&wbg](auto *cfg) {
		cfg->wbg = wbg;
		cfg->wbg.pad[0] = cfg->wbg.pad[1] = 0;
	});
}

WbgConfig BackendConfigAdapter::getWbg() const
{
	return visit([](auto *cfg) -> WbgConfig { return cfg->wbg; });
}

void BackendConfigAdapter::setCdn(const BeCdnConfig &cdn)
{
	visit([&cdn](auto *cfg) { cfg->cdn = cdn; });
}

BeCdnConfig BackendConfigAdapter::getCdn() const
{
	return visit([](auto *cfg) -> BeCdnConfig { return cfg->cdn; });
}

void BackendConfigAdapter::setCac(const BeCacConfig &cac)
{
	visit([&cac](auto *cfg) { 
		cfg->cac.grid_step_x = cac.grid_step_x;
		cfg->cac.grid_step_y = cac.grid_step_y;
		std::memcpy(cfg->cac.lut, cac.lut.data(), sizeof(cfg->cac.lut));
	});

	// Store the extras as well!
	extras_->cac = cac;
}

BeCacConfig BackendConfigAdapter::getCac() const
{
	return extras_->cac;
}

void BackendConfigAdapter::setDebin(const BeDebinConfig &debin)
{
	visit([&debin](auto *cfg) {
		cfg->debin = debin;
		cfg->debin.pad[0] = cfg->debin.pad[1] = 0;
	});
}

BeDebinConfig BackendConfigAdapter::getDebin() const
{
	return visit([](auto *cfg) -> BeDebinConfig { return cfg->debin; });
}

void BackendConfigAdapter::setCcm(const BeCcmConfig &ccm)
{
	visit([&ccm](auto *cfg) {
		cfg->ccm = ccm;
		cfg->ccm.pad[0] = cfg->ccm.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getCcm() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ccm; });
}

void BackendConfigAdapter::setSatControl(const BeSatControlConfig &satControl)
{
	visit([&satControl](auto *cfg) {
		cfg->sat_control = satControl;
		cfg->sat_control.pad = 0;
	});
}

BeSatControlConfig BackendConfigAdapter::getSatControl() const
{
	return visit([](auto *cfg) -> BeSatControlConfig { return cfg->sat_control; });
}

void BackendConfigAdapter::setYcbcr(const BeCcmConfig &ycbcr)
{
	visit([&ycbcr](auto *cfg) {
		cfg->ycbcr = ycbcr;
		cfg->ycbcr.pad[0] = cfg->ycbcr.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getYcbcr() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr; });
}

void BackendConfigAdapter::setFalseColour(const BeFalseColourConfig &falseColour)
{
	visit([&falseColour](auto *cfg) {
		cfg->false_colour = falseColour;
		cfg->false_colour.pad[0] = cfg->false_colour.pad[1] = cfg->false_colour.pad[2] = 0;
	});
}

BeFalseColourConfig BackendConfigAdapter::getFalseColour() const
{
	return visit([](auto *cfg) -> BeFalseColourConfig { return cfg->false_colour; });
}

void BackendConfigAdapter::setSharpen(const BeSharpenConfig &sharpen)
{
	visit([&sharpen](auto *cfg) {
		cfg->sharpen = sharpen;
		cfg->sharpen.pad0[0] = cfg->sharpen.pad0[1] = cfg->sharpen.pad0[2] = 0;
		cfg->sharpen.pad1[0] = cfg->sharpen.pad1[1] = cfg->sharpen.pad1[2] = 0;
		cfg->sharpen.pad2[0] = cfg->sharpen.pad2[1] = cfg->sharpen.pad2[2] = 0;
		cfg->sharpen.pad3[0] = cfg->sharpen.pad3[1] = cfg->sharpen.pad3[2] = 0;
		cfg->sharpen.pad4[0] = cfg->sharpen.pad4[1] = cfg->sharpen.pad4[2] = 0;
		cfg->sharpen.pad5 = cfg->sharpen.pad6 = cfg->sharpen.pad7 = cfg->sharpen.pad8 = cfg->sharpen.pad9 = 0;
	});
}

BeSharpenConfig BackendConfigAdapter::getSharpen() const
{
	return visit([](auto *cfg) -> BeSharpenConfig { return cfg->sharpen; });
}

void BackendConfigAdapter::setShFcCombine(const BeShFcCombineConfig &shFcCombine)
{
	visit([&shFcCombine](auto *cfg) {
		cfg->sh_fc_combine = shFcCombine;
		cfg->sh_fc_combine.pad = 0;
	});
}

BeShFcCombineConfig BackendConfigAdapter::getShFcCombine() const
{
	return visit([](auto *cfg) -> BeShFcCombineConfig { return cfg->sh_fc_combine; });
}

void BackendConfigAdapter::setYcbcrInverse(const BeCcmConfig &ycbcrInverse)
{
	visit([&ycbcrInverse](auto *cfg) {
		cfg->ycbcr_inverse = ycbcrInverse;
		cfg->ycbcr_inverse.pad[0] = cfg->ycbcr_inverse.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getYcbcrInverse() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr_inverse; });
}

void BackendConfigAdapter::setCsc(unsigned int i, const BeCcmConfig &csc)
{
	visit(overloaded {
		[&csc, i](pisp_be_config *cfg) {
			cfg->csc[i] = csc;
			cfg->csc[i].pad[0] = cfg->csc[i].pad[1] = 0;
		},
		[&csc, i](pisp_be_config2 *cfg) {
			cfg->post_csc[i] = csc;
			cfg->post_csc[i].pad[0] = cfg->post_csc[i].pad[1] = 0;
		}
	});
}

BeCcmConfig BackendConfigAdapter::getCsc(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> BeCcmConfig { return cfg->csc[i]; },
		[i](pisp_be_config2 *cfg) -> BeCcmConfig { return cfg->post_csc[i]; }
	});
}

void BackendConfigAdapter::setDownscale(unsigned int i, const BeDownscaleConfig &downscale)
{
	visit([&downscale, i](auto *cfg) {
		cfg->downscale[i].scale_factor_h = downscale.scale_factor_h;
		cfg->downscale[i].scale_factor_v = downscale.scale_factor_v;
		cfg->downscale[i].scale_recip_h = downscale.scale_recip_h;
		cfg->downscale[i].scale_recip_v = downscale.scale_recip_v;
	});

	// Store the extras as well!
	extras_->downscale[i] = downscale;
}

BeDownscaleConfig BackendConfigAdapter::getDownscale(unsigned int i) const
{
	return extras_->downscale[i];
}

void BackendConfigAdapter::setOutputFormat(unsigned int i, const BeOutputFormatConfig &outputFormat)
{
	visit(overloaded {
		[&outputFormat, i](pisp_be_config *cfg) {
			cfg->output_format[i].image = outputFormat.image;
			cfg->output_format[i].transform = outputFormat.transform;
			cfg->output_format[i].lo = outputFormat.lo;
			cfg->output_format[i].lo2 = outputFormat.lo2;
			cfg->output_format[i].hi = outputFormat.hi;
			cfg->output_format[i].hi2 = outputFormat.hi2;
			cfg->output_format[i].pad[0] = cfg->output_format[i].pad[1] = cfg->output_format[i].pad[2] = 0;
		},
		[&outputFormat, i](pisp_be_config2 *cfg) {
			cfg->output_format[i] = outputFormat;
		}
	});
}

BeOutputFormatConfig BackendConfigAdapter::getOutputFormat(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> BeOutputFormatConfig {
			BeOutputFormatConfig outputFormat {};
			outputFormat.image = cfg->output_format[i].image;
			outputFormat.transform = cfg->output_format[i].transform;
			outputFormat.lo = cfg->output_format[i].lo;
			outputFormat.lo2 = cfg->output_format[i].lo2;
			outputFormat.hi = cfg->output_format[i].hi;
			outputFormat.hi2 = cfg->output_format[i].hi2;
			return outputFormat;
		},
		[i](pisp_be_config2 *cfg) -> BeOutputFormatConfig { return cfg->output_format[i]; }
	});
}

// Variant-specific implementations
void BackendConfigAdapter::setTdn(const BeTdnConfig &tdn)
{
	visit(overloaded {
		[&tdn](pisp_be_config *cfg) {
			cfg->tdn = tdn;
			cfg->tdn.pad = 0;
		},
		[&tdn]([[maybe_unused]] pisp_be_config2 *cfg) {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
		}
	});
}

BeTdnConfig BackendConfigAdapter::getTdn() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeTdnConfig {
			return cfg->tdn;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> BeTdnConfig {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
			return BeTdnConfig{};
		}
	});
}

void BackendConfigAdapter::setStitch(const BeStitchConfig &stitch)
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

BeStitchConfig BackendConfigAdapter::getStitch() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeStitchConfig {
			return cfg->stitch;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> BeStitchConfig {
			// v2 uses pisp_be_tdn2_config, conversion not implemented
			return BeStitchConfig{};
		}
	});
}

void BackendConfigAdapter::setTonemap(const BeTonemapConfig &tonemap)
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

BeTonemapConfig BackendConfigAdapter::getTonemap() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeTonemapConfig {
			return cfg->tonemap;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> BeTonemapConfig {
			// v2 uses tonemap2, conversion not implemented
			return BeTonemapConfig{};
		}
	});
}

void BackendConfigAdapter::setDemosaic(const BeDemosaicConfig &demosaic)
{
	visit(overloaded {
		[&demosaic](pisp_be_config *cfg) {
			cfg->demosaic = demosaic;
			cfg->demosaic.pad[0] = cfg->demosaic.pad[1] = 0;
		},
		[&demosaic](pisp_be_config2 *cfg) {
			// v2 uses pisp_be_demosaic2_config - copy compatible fields
			cfg->demosaic.sharper = demosaic.sharper;
			cfg->demosaic.fc_mode = demosaic.fc_mode;
		}
	});
}

BeDemosaicConfig BackendConfigAdapter::getDemosaic() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeDemosaicConfig {
			return cfg->demosaic;
		},
		[](pisp_be_config2 *cfg) -> BeDemosaicConfig {
			// Convert from v2 to v1
			BeDemosaicConfig result{};
			result.sharper = cfg->demosaic.sharper;
			result.fc_mode = cfg->demosaic.fc_mode;
			return result;
		}
	});
}

void BackendConfigAdapter::setGamma(const BeGammaConfig &gamma)
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

BeGammaConfig BackendConfigAdapter::getGamma() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeGammaConfig {
			return cfg->gamma;
		},
		[](pisp_be_config2 *cfg) -> BeGammaConfig {
			// Convert from v2 array to v1 single
			BeGammaConfig result{};
			for (unsigned int i = 0; i < PISP_BE_GAMMA_LUT_SIZE && i < PISP_BE_GAMMA2_LUT_SIZE; i++) {
				result.lut[i] = cfg->gamma[0].lut[i];
			}
			return result;
		}
	});
}

void BackendConfigAdapter::setResample(unsigned int i, const BeResampleConfig &resample)
{
	visit(overloaded {
		[&resample, i](pisp_be_config *cfg) {
			cfg->resample[i].scale_factor_h = resample.scale_factor_h;
			cfg->resample[i].scale_factor_v = resample.scale_factor_v;
			std::memcpy(cfg->resample[i].coef, resample.coef.data(), sizeof(cfg->resample[i].coef));
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

	// Store the extras as well!
	extras_->resample[i] = resample;
}

BeResampleConfig BackendConfigAdapter::getResample(unsigned int i) const
{
	return extras_->resample[i];
}
