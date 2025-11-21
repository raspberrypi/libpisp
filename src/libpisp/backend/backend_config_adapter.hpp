/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <variant>

#include "bitfield.hpp"
#include "pisp_be_config.h"

namespace libpisp
{

enum class RgbEnable : uint32_t
{
	None			= 0x00000000,
	Input			= 0x00000001,
	Dedistort		= 0x00000002,
	Ccm				= 0x00000004,
	SatControl		= 0x00000008,
	Ycbcr			= 0x00000010,
	FalseColour		= 0x00000020,
	Sharpen			= 0x00000040,
	YcbcrInverse	= 0x00000080,
	PreCsc0			= 0x00000400,
	PreCsc1			= 0x00000800,
	Gamma0			= 0x00001000,
	Gamma1			= 0x00002000,
	PostCsc0		= 0x00004000,
	PostCsc1		= 0x00008000,
	Downscale0		= 0x00010000,
	Downscale1		= 0x00020000,
	Resample0		= 0x00040000,
	Resample1		= 0x00080000,
	Output0			= 0x00100000,
	Output1			= 0x00200000,
	Compress0		= 0x00400000,
	Compress1		= 0x00800000,
};

CREATE_BITFIELD(RgbEnables, RgbEnable);

enum class BayerEnable : uint32_t
{
	None 			= 0x00000000,
	Input			= 0x00000001,
	Decompress		= 0x00000002,
	Dpc				= 0x00000004,
	Geq				= 0x00000008,
	TdnInput		= 0x00000010,
	TdnDecompress	= 0x00000020,
	Tdn				= 0x00000040,
	TdnCompress		= 0x00000080,
	TdnOutput		= 0x00000100,
	Sdn				= 0x00000200,
	Blc				= 0x00000400,
	StitchInput		= 0x00000800,
	StitchDecompress = 0x00001000,
	Stitch			= 0x00002000,
	StitchCompress	= 0x00004000,
	StitchOutput	= 0x00008000,
	Wbg				= 0x00010000,
	Cdn				= 0x00020000,
	Lsc				= 0x00040000,
	Tonemap			= 0x00080000,
	Cac				= 0x00100000,
	Debin			= 0x00200000,
	Demosaic		= 0x00400000,
	TonemapInput	= 0x00800000,
	Transpose		= 0x01000000,
};

CREATE_BITFIELD(BayerEnables, BayerEnable);

enum class DirtyFlag : uint32_t
{
	None		= 0x00000000,
	Crop		= 0x00000001,
	Global		= 0x00000002,
	ShFcCombine	= 0x00000004,
};

CREATE_BITFIELD(DirtyFlags, DirtyFlag);

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType T>
T getFlagForBranch(const T field, unsigned int branch)
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(field) << branch);
}

// Global config is special case between variants!
struct BeGlobal
{
	BayerEnables bayerEnables;
	RgbEnables rgbEnables;
	uint8_t bayerOrder;
};

// LSC/CAC/Downscale/Resample have extra non-register fields:
struct BeLscConfig
{
	uint16_t grid_step_x = 0;
	uint16_t grid_step_y = 0;
	uint32_t lut_packed[PISP_BE_LSC_LUT_SIZE][PISP_BE_LSC_LUT_SIZE] {};
	uint16_t offset_x = 0; // Non-register field
	uint16_t offset_y = 0; // Non-register field
};

struct BeCacConfig
{
	uint16_t grid_step_x = 0;
	uint16_t grid_step_y = 0;
	int8_t lut[PISP_BE_CAC_LUT_SIZE][PISP_BE_CAC_LUT_SIZE][2][2] {};
	uint16_t offset_x = 0;
	uint16_t offset_y = 0;
};

struct BeDownscaleConfig
{
	uint16_t scale_factor_h = 0;
	uint16_t scale_factor_v = 0;
	uint16_t scale_recip_h = 0;
	uint16_t scale_recip_v = 0;
	uint16_t scaled_width = 0; // Non-register field
	uint16_t scaled_height = 0; // Non-register field
};

struct BeResampleConfig
{
	uint16_t scale_factor_h = 0;
	uint16_t scale_factor_v = 0;
	int16_t coef[PISP_BE_RESAMPLE_FILTER_SIZE] {};
	uint16_t scaled_width = 0; // Non-register field
	uint16_t scaled_height = 0; // Non-register field
	int16_t initial_phase_h[3] {}; // Non-register field
	int16_t initial_phase_v[3] {}; // Non-register field
};

// No hardware registers for crop directly.
struct BeCropConfig
{
	uint16_t offsetX = 0;
	uint16_t offsetY = 0;
	uint16_t width = 0;
	uint16_t height = 0;
};

// pisp_be_output_format_config2 is a superset of pisp_be_output_format_config
using BeOutputFormatConfig = struct pisp_be_output_format_config2;

// Identical to the current HW structures
using ImageFormatConfig = struct pisp_image_format_config;
using DecompressConfig = struct pisp_decompress_config;
using CompressConfig = struct pisp_compress_config;
using BeDpcConfig = struct pisp_be_dpc_config;
using BeGeqConfig = struct pisp_be_geq_config;
using BeSdnConfig = struct pisp_be_sdn_config;
using BlaConfig = struct pisp_bla_config;
using WbgConfig = struct pisp_wbg_config;
using BeCdnConfig = struct pisp_be_cdn_config;
using BeDebinConfig = struct pisp_be_debin_config;
using BeCcmConfig = struct pisp_be_ccm_config;
using BeSatControlConfig = struct pisp_be_sat_control_config;
using BeFalseColourConfig = struct pisp_be_false_colour_config;
using BeSharpenConfig = struct pisp_be_sharpen_config;
using BeShFcCombineConfig = struct pisp_be_sh_fc_combine_config;
using BeTdnConfig = struct pisp_be_tdn_config;
using BeStitchConfig = struct pisp_be_stitch_config;
using BeTonemapConfig = struct pisp_be_tonemap_config;
using BeDemosaicConfig = struct pisp_be_demosaic_config;
using BeGammaConfig = struct pisp_be_gamma_config;

struct BeConfigExtra
{
	// Non-register fields:
	BeLscConfig lsc {};
	BeCacConfig cac {};
	BeDownscaleConfig downscale[PISP_BACK_END_NUM_OUTPUTS] {};
	BeResampleConfig resample[PISP_BACK_END_NUM_OUTPUTS] {};
	BeCropConfig crop[PISP_BACK_END_NUM_OUTPUTS] {};
};

class BackendConfigAdapter
{
public:
	using HwConfigVariant = std::variant<pisp_be_config*,  pisp_be_config2*>;

	BackendConfigAdapter(HwConfigVariant config, BeConfigExtra *extras)
		: config_(config), extras_(extras)
	{
	}
	
	template <typename F>
	auto visit(F &&f)
	{
		return std::visit(std::forward<F>(f), config_);
	}

	template <typename F>
	auto visit(F &&f) const
	{
		return std::visit(std::forward<F>(f), config_);
	}

	void setGlobal(const BeGlobal &global);
	BeGlobal getGlobal() const;

	void setInputFormat(const ImageFormatConfig &inputFormat);
	ImageFormatConfig getInputFormat() const;

	void setDecompress(const DecompressConfig &decompress);
	DecompressConfig getDecompress() const;

	void setDpc(const BeDpcConfig &dpc);
	BeDpcConfig getDpc() const;

	void setGeq(const BeGeqConfig &geq);
	BeGeqConfig getGeq() const;

	void setTdnInputFormat(const ImageFormatConfig &tdnInputFormat);
	ImageFormatConfig getTdnInputFormat() const;

	void setTdnDecompress(const DecompressConfig &tdnDecompress);
	DecompressConfig getTdnDecompress() const;

	void setTdnCompress(const CompressConfig &tdnCompress);
	CompressConfig getTdnCompress() const;

	void setTdnOutputFormat(const ImageFormatConfig &tdnOutputFormat);
	ImageFormatConfig getTdnOutputFormat() const;

	void setSdn(const BeSdnConfig &sdn);
	BeSdnConfig getSdn() const;

	void setBlc(const BlaConfig &blc);
	BlaConfig getBlc() const;

	void setStitchInputFormat(const ImageFormatConfig &stitchInputFormat);
	ImageFormatConfig getStitchInputFormat() const;

	void setStitchDecompress(const DecompressConfig &stitchDecompress);
	DecompressConfig getStitchDecompress() const;

	void setStitchCompress(const CompressConfig &stitchCompress);
	CompressConfig getStitchCompress() const;

	void setStitchOutputFormat(const ImageFormatConfig &stitchOutputFormat);
	ImageFormatConfig getStitchOutputFormat() const;

	void setLsc(const BeLscConfig &lsc);
	BeLscConfig getLsc() const;

	void setWbg(const WbgConfig &wbg);
	WbgConfig getWbg() const;

	void setCdn(const BeCdnConfig &cdn);
	BeCdnConfig getCdn() const;

	void setCac(const BeCacConfig &cac);
	BeCacConfig getCac() const;

	void setDebin(const BeDebinConfig &debin);
	BeDebinConfig getDebin() const;

	void setCcm(const BeCcmConfig &ccm);
	BeCcmConfig getCcm() const;

	void setSatControl(const BeSatControlConfig &satControl);
	BeSatControlConfig getSatControl() const;

	void setYcbcr(const BeCcmConfig &ycbcr);
	BeCcmConfig getYcbcr() const;

	void setFalseColour(const BeFalseColourConfig &falseColour);
	BeFalseColourConfig getFalseColour() const;

	void setSharpen(const BeSharpenConfig &sharpen);
	BeSharpenConfig getSharpen() const;

	void setShFcCombine(const BeShFcCombineConfig &shFcCombine);
	BeShFcCombineConfig getShFcCombine() const;

	void setYcbcrInverse(const BeCcmConfig &ycbcrInverse);
	BeCcmConfig getYcbcrInverse() const;

	// Array fields
	void setCsc(unsigned int i, const BeCcmConfig &csc);
	BeCcmConfig getCsc(unsigned int i) const;

	void setDownscale(unsigned int i, const BeDownscaleConfig &downscale);
	BeDownscaleConfig getDownscale(unsigned int i) const;

	void setOutputFormat(unsigned int i, const BeOutputFormatConfig &outputFormat);
	BeOutputFormatConfig getOutputFormat(unsigned int i) const;

	// Variant-specific fields - use visit
	void setTdn(const BeTdnConfig &tdn);
	BeTdnConfig getTdn() const;

	void setStitch(const BeStitchConfig &stitch);
	BeStitchConfig getStitch() const;

	void setTonemap(const BeTonemapConfig &tonemap);
	BeTonemapConfig getTonemap() const;

	void setDemosaic(const BeDemosaicConfig &demosaic);
	BeDemosaicConfig getDemosaic() const;

	void setGamma(const BeGammaConfig &gamma);
	BeGammaConfig getGamma() const;

	void setResample(unsigned int i, const BeResampleConfig &resample);
	BeResampleConfig getResample(unsigned int i) const;

private:
	uint32_t rgbEnablesToHw(RgbEnables rgbEnables) const;
	RgbEnables hwToRgbEnables(uint32_t hwEnables) const;
	uint32_t bayerEnablesToHw(BayerEnables bayerEnables) const;
	BayerEnables hwToBayerEnables(uint32_t hwEnables) const;

	HwConfigVariant config_;
	BeConfigExtra *extras_;
};

} // namespace libpisp
