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
	PrefColours    		= 0x00000080,
	YcbcrInverse	= 0x00000100,
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

using RgbEnables = Bitfield<RgbEnable>;

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
	Transpose		= 0x01000000,
	Tonemap2		= 0x02000000,
	TonemapInput	= 0x04000000,
};

using BayerEnables = Bitfield<BayerEnable>;

enum class DirtyFlag : uint32_t
{
	None		= 0x00000000,
	Crop		= 0x00000001,
	Global		= 0x00000002,
	ShFcCombine	= 0x00000004,
};

using DirtyFlags = Bitfield<DirtyFlag>;

template <EnumType T>
T get_flag_for_branch(const T field, unsigned int branch)
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(field) << branch);
}

// Global config is special case between variants!
struct BeGlobal
{
	BayerEnables bayer_enables;
	RgbEnables rgb_enables;
	uint8_t bayer_order;
	uint8_t quad_bayer;
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
	// v1 HW has a single set of coefficients for both directions, and uses coef_h.
	std::array<int16_t, PISP_BE_RESAMPLE_FILTER_SIZE> coef_h {};
	std::array<int16_t, PISP_BE_RESAMPLE_FILTER_SIZE> coef_v {};
	uint16_t scaled_width = 0; // Non-register field
	uint16_t scaled_height = 0; // Non-register field
	std::array<int16_t, 3> initial_phase_h {}; // Non-register field
	std::array<int16_t, 3> initial_phase_v {}; // Non-register field
};

// No hardware registers for crop directly.
struct BeCropConfig
{
	uint16_t offset_x = 0;
	uint16_t offset_y = 0;
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

	// v2 (pisp_be_tdn2_config)
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
	// TODO: make this a std::array
	int16_t lut[PISP_BE_DDX_GRID_SIZE + 1][PISP_BE_DDX_GRID_SIZE + 1]{};
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
using BePrefColoursConfig = struct pisp_be_pref_colours_config;
using BeTonemapConfig = struct pisp_be_tonemap_config;
using BeCompressConfig = struct pisp_compress_config;
using BeTonemap2Config = struct pisp_be_tonemap2_config;

// The non-register fields of the above configs, i.e. the parameters that have no HW registers but are needed
// for tiling. Everything else is read back from the HW config, so nothing is stored twice.
struct BeSwParams
{
	struct Offset
	{
		uint16_t x = 0;
		uint16_t y = 0;
	};

	struct Size
	{
		uint16_t width = 0;
		uint16_t height = 0;
	};

	Offset lsc;
	Offset cac;
	Offset dedistort;
	std::array<Size, PISP_BACK_END_NUM_OUTPUTS> downscale {};
	std::array<Size, PISP_BACK_END_NUM_OUTPUTS> resample {};
	std::array<std::array<int16_t, 3>, PISP_BACK_END_NUM_OUTPUTS> resample_initial_phase_h {};
	std::array<std::array<int16_t, 3>, PISP_BACK_END_NUM_OUTPUTS> resample_initial_phase_v {};
	std::array<BeCropConfig, PISP_BACK_END_NUM_OUTPUTS> crop {};
};

// This lives in the (possibly shared) BackEnd config storage, so must be safe to copy as raw bytes.
static_assert(std::is_standard_layout_v<BeSwParams> && std::is_trivially_copyable_v<BeSwParams>);

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
	std::array<uint16_t,  PISP_BACK_END_NUM_OUTPUTS>crop_width {};
	std::array<uint16_t,  PISP_BACK_END_NUM_OUTPUTS>crop_height {};
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
	uint32_t dedistorted_offset_x = 0;
	uint32_t dedistorted_width = 0;
	std::array<uint8_t, PISP_BACK_END_NUM_OUTPUTS> resample_start_context_x {};
	std::array<uint8_t, PISP_BACK_END_NUM_OUTPUTS> resample_start_context_y {};
};

class BackendConfigAdapter
{
public:
	// The constructor overloads only allow matching config and tile versions to be paired.
	BackendConfigAdapter(pisp_be_config *config, pisp_tile *tiles, BeSwParams *sw)
		: hw_(HwV1 { config, tiles }), sw_(sw)
	{
	}

	BackendConfigAdapter(pisp_be_config2 *config, pisp_tile2 *tiles, BeSwParams *sw)
		: hw_(HwV2 { config, tiles }), sw_(sw)
	{
	}

	void set_tile(unsigned int i, const BeTile &tile);
	BeTile get_tile(unsigned int i) const;

	void set_global(const BeGlobal &global);
	BeGlobal get_global() const;

	void set_input_format(const ImageFormatConfig &input_format);
	ImageFormatConfig get_input_format() const;

	void set_decompress(const DecompressConfig &decompress);
	DecompressConfig get_decompress() const;

	void set_dpc(const BeDpcConfig &dpc);
	BeDpcConfig get_dpc() const;

	void set_geq(const BeGeqConfig &geq);
	BeGeqConfig get_geq() const;

	void set_tdn_input_format(const ImageFormatConfig &tdn_input_format);
	ImageFormatConfig get_tdn_input_format() const;

	void set_tdn_decompress(const DecompressConfig &tdn_decompress);
	DecompressConfig get_tdn_decompress() const;

	void set_tdn_compress(const CompressConfig &tdn_compress);
	CompressConfig get_tdn_compress() const;

	void set_tdn_output_format(const ImageFormatConfig &tdn_output_format);
	ImageFormatConfig get_tdn_output_format() const;

	void set_sdn(const BeSdnConfig &sdn);
	BeSdnConfig get_sdn() const;

	void set_blc(const BlaConfig &blc);
	BlaConfig get_blc() const;

	void set_stitch_input_format(const ImageFormatConfig &stitch_input_format);
	ImageFormatConfig get_stitch_input_format() const;

	void set_stitch_decompress(const DecompressConfig &stitch_decompress);
	DecompressConfig get_stitch_decompress() const;

	void set_stitch_compress(const CompressConfig &stitch_compress);
	CompressConfig get_stitch_compress() const;

	void set_stitch_output_format(const ImageFormatConfig &stitch_output_format);
	ImageFormatConfig get_stitch_output_format() const;

	void set_lsc(const BeLscConfig &lsc);
	BeLscConfig get_lsc() const;

	void set_wbg(const WbgConfig &wbg);
	WbgConfig get_wbg() const;

	void set_cdn(const BeCdnConfig &cdn);
	BeCdnConfig get_cdn() const;

	void set_cac(const BeCacConfig &cac);
	BeCacConfig get_cac() const;

	void set_debin(const BeDebinConfig &debin);
	BeDebinConfig get_debin() const;

	void set_ccm(const BeCcmConfig &ccm);
	BeCcmConfig get_ccm() const;

	void set_sat_control(const BeSatControlConfig &sat_control);
	BeSatControlConfig get_sat_control() const;

	void set_ycbcr(const BeCcmConfig &ycbcr);
	BeCcmConfig get_ycbcr() const;

	void set_false_colour(const BeFalseColourConfig &false_colour);
	BeFalseColourConfig get_false_colour() const;

	void set_sharpen(const BeSharpenConfig &sharpen);
	BeSharpenConfig get_sharpen() const;

	void set_sh_fc_combine(const BeShFcCombineConfig &sh_fc_combine);
	BeShFcCombineConfig get_sh_fc_combine() const;

	void set_pref_colours(const BePrefColoursConfig &pref_colours);
	BePrefColoursConfig get_pref_colours() const;

	void set_ycbcr_inverse(const BeCcmConfig &ycbcr_inverse);
	BeCcmConfig get_ycbcr_inverse() const;

	void set_pre_csc(unsigned int i, const BeCcmConfig &csc);
	BeCcmConfig get_pre_csc(unsigned int i) const;

	void set_post_csc(unsigned int i, const BeCcmConfig &csc);
	BeCcmConfig get_post_csc(unsigned int i) const;

	void set_downscale(unsigned int i, const BeDownscaleConfig &downscale);
	BeDownscaleConfig get_downscale(unsigned int i) const;

	void set_output_format(unsigned int i, const BeOutputFormatConfig &output_format);
	BeOutputFormatConfig get_output_format(unsigned int i) const;

	void set_tdn(const BeTdnConfig &tdn);
	BeTdnConfig get_tdn() const;

	void set_stitch(const BeStitchConfig &stitch);
	BeStitchConfig get_stitch() const;

	void set_tonemap(const BeTonemapConfig &tonemap);
	BeTonemapConfig get_tonemap() const;

	void set_tonemap2(const BeTonemap2Config &tonemap);
	BeTonemap2Config get_tonemap2() const;

	void set_tonemap_format(const ImageFormatConfig &format);
	ImageFormatConfig get_tonemap_format() const;

	void set_demosaic(const BeDemosaicConfig &demosaic);
	BeDemosaicConfig get_demosaic() const;

	void set_gamma(unsigned int i, const BeGammaConfig &gamma);
	BeGammaConfig get_gamma(unsigned int i) const;

	void set_resample(unsigned int i, const BeResampleConfig &resample);
	BeResampleConfig get_resample(unsigned int i) const;

	void set_crop(unsigned int i, BeCropConfig const &crop);
	BeCropConfig get_crop(unsigned int i) const;
	
	void set_dedistort(const BeDedistortionConfig &dedistort);
	BeDedistortionConfig get_dedistort() const;

	void set_output_compress(unsigned int i, const BeCompressConfig &compress);
	BeCompressConfig get_output_compress(unsigned int i) const;

private:
	template <typename Config, typename Tile>
	struct Hw
	{
		Config *cfg;
		Tile *tiles;
	};

	using HwV1 = Hw<pisp_be_config, pisp_tile>;
	using HwV2 = Hw<pisp_be_config2, pisp_tile2>;

	// True if Cfg is (a pointer to) the v1 HW config, for compile-time selection of version specific code.
	template <typename Cfg>
	static constexpr bool is_v1 = std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<Cfg>>, pisp_be_config>;

	// Call f with a pointer to the version specific HW config.
	template <typename F>
	auto visit(F &&f) const
	{
		return std::visit([&f](auto hw) { return f(hw.cfg); }, hw_);
	}

	// Call f with a pointer to the version specific HW tile array.
	template <typename F>
	auto visit_tiles(F &&f) const
	{
		return std::visit([&f](auto hw) { return f(hw.tiles); }, hw_);
	}

	std::variant<HwV1, HwV2> hw_;
	BeSwParams *sw_;
};

} // namespace libpisp
