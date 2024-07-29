/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <algorithm>
#include <array>
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
	Tonemap2		= 0x02000000,
	Tonemap2Input	= 0x04000000,
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
	std::array<uint32_t, PISP_BE_LSC_LUT_SIZE * PISP_BE_LSC_LUT_SIZE> lut_packed {};
	uint16_t offset_x = 0; // Non-register field
	uint16_t offset_y = 0; // Non-register field
};

struct BeCacConfig
{
	uint16_t grid_step_x = 0;
	uint16_t grid_step_y = 0;
	std::array<int8_t, PISP_BE_CAC_LUT_SIZE * PISP_BE_CAC_LUT_SIZE * 2 * 2> lut {};
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
	// TODO make this a union??
	std::array<int16_t, PISP_BE_RESAMPLE_FILTER_SIZE> coef_h {};
	std::array<int16_t, PISP_BE_RESAMPLE_FILTER_SIZE> coef_v {};
	std::array<int16_t, PISP_BE_RESAMPLE_FILTER_SIZE> coef {}; 
	uint16_t scaled_width = 0; // Non-register field
	uint16_t scaled_height = 0; // Non-register field
	std::array<int16_t, 3> initial_phase_h {}; // Non-register field
	std::array<int16_t, 3> initial_phase_v {}; // Non-register field
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

// pisp_be_demosaic2_config is a superset of pisp_be_demosaic_config
using BeDemosaicConfig = struct pisp_be_demosaic2_config;

// pisp_be_tdn2_config is a superset of pisp_be_tdn_config
using BeTdnConfig = struct pisp_be_tdn2_config;

// TODO make this a union?
struct BeStitchConfig
{
	// v1
	uint16_t threshold_lo = 0;
	uint8_t threshold_diff_power = 0;
	uint16_t exposure_ratio = 0;
	uint8_t motion_threshold_256 = 0;
	uint8_t motion_threshold_recip = 0;

	// v2
	uint16_t black_level = 0;
	uint16_t ratio = 0;
	uint16_t noise_constant = 0;
	uint16_t noise_slope = 0;
	uint16_t threshold = 0;
	uint8_t reset = 0;
	uint8_t apply_threshold = 0;
	uint8_t swap_inputs = 0;
	uint16_t saturation_threshold = 0;
};

struct BeGammaConfig
{
	std::array<uint32_t, std::max<size_t>(PISP_BE_GAMMA2_LUT_SIZE, PISP_BE_GAMMA_LUT_SIZE)> lut {};
};

struct BeDedistortionConfig
{
	uint16_t grid_step_x = 0;
	uint16_t grid_step_y = 0;
	std::array<int16_t, (PISP_BE_DDX_GRID_SIZE + 1) * (PISP_BE_DDX_GRID_SIZE + 1)> lut {};
    uint16_t offset_x = 0; // Non-register field
    uint16_t offset_y = 0; // Non-register field
};

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
using BeTonemapConfig = struct pisp_be_tonemap_config;
using BeCompressConfig = struct pisp_compress_config;
using BeTonemap2Config = struct pisp_be_tonemap2_config;

struct BeConfigExtra
{
	BeLscConfig lsc {};
	BeCacConfig cac {};
	BeDedistortionConfig dedistort {};
	std::array<BeDownscaleConfig, PISP_BACK_END_NUM_OUTPUTS> downscale {};
	std::array<BeResampleConfig, PISP_BACK_END_NUM_OUTPUTS> resample {};
	std::array<BeCropConfig, PISP_BACK_END_NUM_OUTPUTS> crop {};
};

struct BeTile
{
	// 2712 only:
	uint8_t edge = 0;

	// Common:
	uint32_t input_addr_offset = 0;
	uint32_t input_addr_offset2 = 0;
	uint16_t input_offset_x = 0;
	uint16_t input_offset_y = 0;
	uint16_t input_width = 0;
	uint16_t input_height = 0;
	uint32_t tdn_input_addr_offset = 0;
	uint32_t tdn_output_addr_offset = 0;
	uint32_t stitch_input_addr_offset = 0;
	uint32_t stitch_output_addr_offset = 0;
	uint32_t lsc_grid_offset_x = 0;
	uint32_t lsc_grid_offset_y = 0;
	uint32_t cac_grid_offset_x = 0;
	uint32_t cac_grid_offset_y = 0;

	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> crop_x_start {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> crop_x_end {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> crop_y_start {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> crop_y_end {};
	// Downscale phase is per-component on 2712, and per-output on Beryl:
	std::array<uint16_t, 3 * PISP_BACK_END_NUM_OUTPUTS> downscale_phase_x {};
	std::array<uint16_t, 3 * PISP_BACK_END_NUM_OUTPUTS> downscale_phase_y {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> resample_in_width {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> resample_in_height {};
	// Resample phase is per-component on 2712, and per-output on Beryl:	
	std::array<uint16_t, 3 * PISP_BACK_END_NUM_OUTPUTS> resample_phase_x {};
	std::array<uint16_t, 3 * PISP_BACK_END_NUM_OUTPUTS> resample_phase_y {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> output_offset_x {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> output_offset_y {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> output_width {};
	std::array<uint16_t, PISP_BACK_END_NUM_OUTPUTS> output_height {};
	std::array<uint32_t, PISP_BACK_END_NUM_OUTPUTS> output_addr_offset {};
	std::array<uint32_t, PISP_BACK_END_NUM_OUTPUTS> output_addr_offset2 {};

	// Beryl only:
	uint32_t tonemap_grid_offset_x = 0;
	uint32_t tonemap_grid_offset_y = 0;
	uint32_t tonemap_lores_offset_x = 0;
	uint32_t tonemap_lores_offset_y = 0;
	uint32_t dedistort_grid_offset_x = 0;
	uint32_t dedistort_grid_offset_y = 0;
};

class BackendConfigAdapter
{
public:
	using HwConfigVariant = std::variant<pisp_be_config *,  pisp_be_config2 *>;
	using HwTileVariant = std::variant<pisp_tile *,  pisp_tile2 *>;

	BackendConfigAdapter(HwConfigVariant config, HwTileVariant tile, BeConfigExtra *extras)
		: config_(config), tile_(tile), extras_(extras)
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

	void setTile(unsigned int i, const BeTile &tile);
	BeTile getTile(unsigned int i) const;

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

	void setPreCsc(unsigned int i, const BeCcmConfig &csc);
	BeCcmConfig getPreCsc(unsigned int i) const;

	void setPostCsc(unsigned int i, const BeCcmConfig &csc);
	BeCcmConfig getPostCsc(unsigned int i) const;

	void setDownscale(unsigned int i, const BeDownscaleConfig &downscale);
	BeDownscaleConfig getDownscale(unsigned int i) const;

	void setOutputFormat(unsigned int i, const BeOutputFormatConfig &outputFormat);
	BeOutputFormatConfig getOutputFormat(unsigned int i) const;

	void setTdn(const BeTdnConfig &tdn);
	BeTdnConfig getTdn() const;

	void setStitch(const BeStitchConfig &stitch);
	BeStitchConfig getStitch() const;

	void setTonemap(const BeTonemapConfig &tonemap);
	BeTonemapConfig getTonemap() const;

	void setTonemap2(const BeTonemap2Config &tonemap);
	BeTonemap2Config getTonemap2() const;

	void setTonemap2Format(const ImageFormatConfig &format);
	ImageFormatConfig getTonemap2Format() const;

	void setDemosaic(const BeDemosaicConfig &demosaic);
	BeDemosaicConfig getDemosaic() const;

	void setGamma(unsigned int i, const BeGammaConfig &gamma);
	BeGammaConfig getGamma(unsigned int i) const;

	void setResample(unsigned int i, const BeResampleConfig &resample);
	BeResampleConfig getResample(unsigned int i) const;

	void setDedistort(const BeDedistortionConfig &dedistort);
	BeDedistortionConfig getDedistort() const;

	void setOutputCompress(unsigned int i, const BeCompressConfig &compress);
	BeCompressConfig getOutputCompress(unsigned int i) const;

	BeConfigExtra *getExtras()
	{
		return extras_;
	}

private:
	uint32_t rgbEnablesToHw(RgbEnables rgbEnables) const;
	RgbEnables hwToRgbEnables(uint32_t hwEnables) const;
	uint32_t bayerEnablesToHw(BayerEnables bayerEnables) const;
	BayerEnables hwToBayerEnables(uint32_t hwEnables) const;

	HwConfigVariant config_;
	HwTileVariant tile_;
	BeConfigExtra *extras_;
};

} // namespace libpisp
