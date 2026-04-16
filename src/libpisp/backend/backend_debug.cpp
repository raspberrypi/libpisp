
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend_prepare.cpp - PiSP Back End debug features
 */
#include "backend.hpp"

#include <nlohmann/json.hpp>
#include <string>

#include "pisp_be_config.h"

using namespace libpisp;
using json = nlohmann::ordered_json;

namespace
{

struct ConfigField
{
	std::string name;
	std::size_t offset;
	std::size_t size;
	std::size_t num;
};

struct ConfigBlock
{
	std::string name;
	std::size_t offset;
	std::vector<ConfigField> fields;
};

const std::map<std::size_t, uint32_t> mask {
	{ 1, 0x000000ff },
	{ 2, 0x0000ffff },
	{ 4, 0xffffffff },
};

#define PISP_ARRAY_SIZE(s, f) (sizeof(((s *)0)->f) / sizeof(((s *)0)->f[0]))

// clang-format off

const std::vector<ConfigBlock> be_config {
	{
		"global", offsetof(pisp_be_config, global),
		{
			{ "bayer_enables", offsetof(pisp_be_global_config, bayer_enables), sizeof(uint32_t), 1 },
			{ "rgb_enables", offsetof(pisp_be_global_config, rgb_enables), sizeof(uint32_t), 1 },
			{ "bayer_order", offsetof(pisp_be_global_config, bayer_order), sizeof(uint8_t), 1 },
		}
	},
	{
		"input_format", offsetof(pisp_be_config, input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"decompress", offsetof(pisp_be_config, decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"dpc", offsetof(pisp_be_config, dpc),
		{
			{ "coeff_level", offsetof(pisp_be_dpc_config, coeff_level), sizeof(uint8_t), 1 },
			{ "coeff_range", offsetof(pisp_be_dpc_config, coeff_range), sizeof(uint8_t), 1 },
			{ "flags", offsetof(pisp_be_dpc_config, flags), sizeof(uint8_t), 1 },
		}
	},
	{
		"geq", offsetof(pisp_be_config, geq),
		{
			{ "offset", offsetof(pisp_be_geq_config, offset), sizeof(uint16_t), 1 },
			{ "slope_sharper", offsetof(pisp_be_geq_config, slope_sharper), sizeof(uint16_t), 1 },
			{ "min", offsetof(pisp_be_geq_config, min), sizeof(uint16_t), 1 },
			{ "max", offsetof(pisp_be_geq_config, max), sizeof(uint16_t), 1 },
		}
	},
	{
		"tdn_input_format", offsetof(pisp_be_config, tdn_input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"tdn_decompress", offsetof(pisp_be_config, tdn_decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"tdn", offsetof(pisp_be_config, tdn),
		{
			{ "black_level", offsetof(pisp_be_tdn_config, black_level), sizeof(uint16_t), 1 },
			{ "ratio", offsetof(pisp_be_tdn_config, ratio), sizeof(uint16_t), 1 },
			{ "noise_constant", offsetof(pisp_be_tdn_config, noise_constant), sizeof(uint16_t), 1 },
			{ "noise_slope", offsetof(pisp_be_tdn_config, noise_slope), sizeof(uint16_t), 1 },
			{ "threshold", offsetof(pisp_be_tdn_config, threshold), sizeof(uint16_t), 1 },
			{ "reset", offsetof(pisp_be_tdn_config, reset), sizeof(uint8_t), 1 },
		}
	},
	{
		"tdn_compress", offsetof(pisp_be_config, tdn_compress),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"tdn_output_format", offsetof(pisp_be_config, tdn_output_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"sdn", offsetof(pisp_be_config, sdn),
		{
			{ "black_level", offsetof(pisp_be_sdn_config, black_level), sizeof(uint16_t), 1 },
			{ "leakage", offsetof(pisp_be_sdn_config, leakage), sizeof(uint8_t), 1 },
			{ "noise_constant", offsetof(pisp_be_sdn_config, noise_constant), sizeof(uint16_t), 1 },
			{ "noise_slope", offsetof(pisp_be_sdn_config, noise_slope), sizeof(uint16_t), 1 },
			{ "noise_constant2", offsetof(pisp_be_sdn_config, noise_constant2), sizeof(uint16_t), 1 },
			{ "noise_slope2", offsetof(pisp_be_sdn_config, noise_slope2), sizeof(uint16_t), 1 },
		}
	},
	{
		"blc", offsetof(pisp_be_config, blc),
		{
			{ "black_level_r", offsetof(pisp_bla_config, black_level_r), sizeof(uint16_t), 1 },
			{ "black_level_gr", offsetof(pisp_bla_config, black_level_gr), sizeof(uint16_t), 1 },
			{ "black_level_gb", offsetof(pisp_bla_config, black_level_gb), sizeof(uint16_t), 1 },
			{ "black_level_b", offsetof(pisp_bla_config, black_level_b), sizeof(uint16_t), 1 },
			{ "output_black_level", offsetof(pisp_bla_config, output_black_level), sizeof(uint16_t), 1 },
		}
	},
	{
		"stitch_compress", offsetof(pisp_be_config, stitch_compress),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"stitch_output_format", offsetof(pisp_be_config, stitch_output_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"stitch_input_format", offsetof(pisp_be_config, stitch_input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"stitch_decompress", offsetof(pisp_be_config, stitch_decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"stitch", offsetof(pisp_be_config, stitch),
		{
			{ "threshold_lo", offsetof(pisp_be_stitch_config, threshold_lo), sizeof(uint16_t), 1 },
			{ "threshold_diff_power", offsetof(pisp_be_stitch_config, threshold_diff_power), sizeof(uint8_t), 1 },
			{ "exposure_ratio", offsetof(pisp_be_stitch_config, exposure_ratio), sizeof(uint16_t), 1 },
			{ "motion_threshold_256", offsetof(pisp_be_stitch_config, motion_threshold_256), sizeof(uint8_t), 1 },
			{ "motion_threshold_recip", offsetof(pisp_be_stitch_config, motion_threshold_recip), sizeof(uint8_t), 1 },
		}
	},
	{
		"lsc", offsetof(pisp_be_config, lsc),
		{
			{ "grid_step_x", offsetof(pisp_be_lsc_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_lsc_config, grid_step_y), sizeof(uint16_t), 1 },
			{ "lut_packed", offsetof(pisp_be_lsc_config, lut_packed), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_be_lsc_config, lut_packed) },
		}
	},
	{
		"wbg", offsetof(pisp_be_config, wbg),
		{
			{ "gain_r", offsetof(pisp_wbg_config, gain_r), sizeof(uint16_t), 1 },
			{ "gain_g", offsetof(pisp_wbg_config, gain_g), sizeof(uint16_t), 1 },
			{ "gain_b", offsetof(pisp_wbg_config, gain_b), sizeof(uint16_t), 1 },
		}
	},
	{
		"cdn", offsetof(pisp_be_config, cdn),
		{
			{ "thresh", offsetof(pisp_be_cdn_config, thresh), sizeof(uint16_t), 1 },
			{ "iir_strength", offsetof(pisp_be_cdn_config, iir_strength), sizeof(uint8_t), 1 },
			{ "g_adjust", offsetof(pisp_be_cdn_config, g_adjust), sizeof(uint8_t), 1 },
		}
	},
	{
		"cac", offsetof(pisp_be_config, cac),
		{
			{ "grid_step_x", offsetof(pisp_be_cac_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_cac_config, grid_step_y), sizeof(uint8_t), 1 },
			{ "lut", offsetof(pisp_be_cac_config, lut), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_cac_config, lut) },
		}
	},
	{
		"debin", offsetof(pisp_be_config, debin),
		{
			{ "coeffs", offsetof(pisp_be_debin_config, coeffs), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_debin_config, coeffs) },
			{ "h_enable", offsetof(pisp_be_debin_config, h_enable), sizeof(int8_t), 1 },
			{ "v_enable", offsetof(pisp_be_debin_config, v_enable), sizeof(int8_t), 1 },
		}
	},
	{
		"tonemap", offsetof(pisp_be_config, tonemap),
		{
			{ "detail_constant", offsetof(pisp_be_tonemap_config, detail_constant), sizeof(uint16_t), 1 },
			{ "detail_slope", offsetof(pisp_be_tonemap_config, detail_slope), sizeof(uint16_t), 1 },
			{ "iir_strength", offsetof(pisp_be_tonemap_config, iir_strength), sizeof(uint16_t), 1 },
			{ "strength", offsetof(pisp_be_tonemap_config, strength), sizeof(uint16_t), 1 },
			{ "lut", offsetof(pisp_be_tonemap_config, lut), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_be_tonemap_config, lut) },
		}
	},
	{
		"demosaic", offsetof(pisp_be_config, demosaic),
		{
			{ "sharper", offsetof(pisp_be_demosaic_config, sharper), sizeof(uint8_t), 1 },
			{ "fc_mode", offsetof(pisp_be_demosaic_config, fc_mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"ccm", offsetof(pisp_be_config, ccm),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"sat_control", offsetof(pisp_be_config, sat_control),
		{
			{ "shift_r", offsetof(pisp_be_sat_control_config, shift_r), sizeof(uint8_t), 1 },
			{ "shift_g", offsetof(pisp_be_sat_control_config, shift_g), sizeof(uint8_t), 1 },
			{ "shift_b", offsetof(pisp_be_sat_control_config, shift_b), sizeof(uint8_t), 1 },
		}
	},
	{
		"ycbcr", offsetof(pisp_be_config, ycbcr),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"sharpen", offsetof(pisp_be_config, sharpen),
		{
			{ "kernel0", offsetof(pisp_be_sharpen_config, kernel0), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel0) },
			{ "kernel1", offsetof(pisp_be_sharpen_config, kernel1), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel1) },
			{ "kernel2", offsetof(pisp_be_sharpen_config, kernel2), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel2) },
			{ "kernel3", offsetof(pisp_be_sharpen_config, kernel3), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel3) },
			{ "kernel4", offsetof(pisp_be_sharpen_config, kernel4), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel4) },
			{ "threshold_offset0", offsetof(pisp_be_sharpen_config, threshold_offset0), sizeof(uint16_t), 1 },
			{ "threshold_slope0", offsetof(pisp_be_sharpen_config, threshold_slope0), sizeof(uint16_t), 1 },
			{ "threshold_offset1", offsetof(pisp_be_sharpen_config, threshold_offset1), sizeof(uint16_t), 1 },
			{ "threshold_slope1", offsetof(pisp_be_sharpen_config, threshold_slope1), sizeof(uint16_t), 1 },
			{ "threshold_offset2", offsetof(pisp_be_sharpen_config, threshold_offset2), sizeof(uint16_t), 1 },
			{ "threshold_slope2", offsetof(pisp_be_sharpen_config, threshold_slope2), sizeof(uint16_t), 1 },
			{ "threshold_offset3", offsetof(pisp_be_sharpen_config, threshold_offset3), sizeof(uint16_t), 1 },
			{ "threshold_slope3", offsetof(pisp_be_sharpen_config, threshold_slope3), sizeof(uint16_t), 1 },
			{ "threshold_offset4", offsetof(pisp_be_sharpen_config, threshold_offset4), sizeof(uint16_t), 1 },
			{ "threshold_slope4", offsetof(pisp_be_sharpen_config, threshold_slope4), sizeof(uint16_t), 1 },
			{ "positive_strength", offsetof(pisp_be_sharpen_config, positive_strength), sizeof(uint16_t), 1 },
			{ "positive_pre_limit", offsetof(pisp_be_sharpen_config, positive_pre_limit), sizeof(uint16_t), 1 },
			{ "positive_func", offsetof(pisp_be_sharpen_config, positive_func), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, positive_func) },
			{ "positive_limit", offsetof(pisp_be_sharpen_config, positive_limit), sizeof(uint16_t), 1 },
			{ "negative_strength", offsetof(pisp_be_sharpen_config, negative_strength), sizeof(uint16_t), 1 },
			{ "negative_pre_limit", offsetof(pisp_be_sharpen_config, negative_pre_limit), sizeof(uint16_t), 1 },
			{ "negative_func", offsetof(pisp_be_sharpen_config, negative_func), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, negative_func) },
			{ "negative_limit", offsetof(pisp_be_sharpen_config, negative_limit), sizeof(uint16_t), 1 },
			{ "enables", offsetof(pisp_be_sharpen_config, enables), sizeof(uint8_t), 1 },
			{ "white", offsetof(pisp_be_sharpen_config, white), sizeof(uint8_t), 1 },
			{ "black", offsetof(pisp_be_sharpen_config, black), sizeof(uint8_t), 1 },
			{ "grey", offsetof(pisp_be_sharpen_config, grey), sizeof(uint8_t), 1 },
		}
	},
	{
		"false_colour", offsetof(pisp_be_config, false_colour),
		{
			{ "distance", offsetof(pisp_be_false_colour_config, distance), sizeof(uint8_t), 1 },
		}
	},
	{
		"sh_fc_combine", offsetof(pisp_be_config, sh_fc_combine),
		{
			{ "y_factor", offsetof(pisp_be_sh_fc_combine_config, y_factor), sizeof(uint8_t), 1 },
			{ "c1_factor", offsetof(pisp_be_sh_fc_combine_config, c1_factor), sizeof(uint8_t), 1 },
			{ "c2_factor", offsetof(pisp_be_sh_fc_combine_config, c2_factor), sizeof(uint8_t), 1 },
		}
	},
	{
		"ycbcr_inverse", offsetof(pisp_be_config, ycbcr_inverse),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"gamma", offsetof(pisp_be_config, gamma),
		{
			{ "lut", offsetof(pisp_be_gamma_config, lut), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_be_gamma_config, lut) },
		}
	},
	{
		"csc0", offsetof(pisp_be_config, csc[0]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"csc1", offsetof(pisp_be_config, csc[1]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"downscale0", offsetof(pisp_be_config, downscale[0]),
		{
			{ "scale_factor_h", offsetof(pisp_be_downscale_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_downscale_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "scale_recip_h", offsetof(pisp_be_downscale_config, scale_recip_h), sizeof(uint16_t), 1 },
			{ "scale_recip_v", offsetof(pisp_be_downscale_config, scale_recip_v), sizeof(uint16_t), 1 },
		}
	},
	{
		"downscale1", offsetof(pisp_be_config, downscale[1]),
		{
			{ "scale_factor_h", offsetof(pisp_be_downscale_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_downscale_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "scale_recip_h", offsetof(pisp_be_downscale_config, scale_recip_h), sizeof(uint16_t), 1 },
			{ "scale_recip_v", offsetof(pisp_be_downscale_config, scale_recip_v), sizeof(uint16_t), 1 },
		}
	},
	{
		"resample0", offsetof(pisp_be_config, resample[0]),
		{
			{ "scale_factor_h", offsetof(pisp_be_resample_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_resample_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "coef", offsetof(pisp_be_resample_config, coef), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample_config, coef) },
		}
	},
	{
		"resample1", offsetof(pisp_be_config, resample[1]),
		{
			{ "scale_factor_h", offsetof(pisp_be_resample_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_resample_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "coef", offsetof(pisp_be_resample_config, coef), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample_config, coef) },
		}
	},
	{
		"output_format0", offsetof(pisp_be_config, output_format[0]),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
			{ "transform", offsetof(pisp_be_output_format_config, transform), sizeof(uint8_t), 1 },
			{ "lo", offsetof(pisp_be_output_format_config, lo), sizeof(uint8_t), 1 },
			{ "hi", offsetof(pisp_be_output_format_config, hi), sizeof(uint8_t), 1 },
			{ "lo2", offsetof(pisp_be_output_format_config, lo2), sizeof(uint8_t), 1 },
			{ "hi2", offsetof(pisp_be_output_format_config, hi2), sizeof(uint8_t), 1 },
		}
	},
	{
		"output_format1", offsetof(pisp_be_config, output_format[1]),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
			{ "transform", offsetof(pisp_be_output_format_config, transform), sizeof(uint8_t), 1 },
			{ "lo", offsetof(pisp_be_output_format_config, lo), sizeof(uint8_t), 1 },
			{ "hi", offsetof(pisp_be_output_format_config, hi), sizeof(uint8_t), 1 },
			{ "lo2", offsetof(pisp_be_output_format_config, lo2), sizeof(uint8_t), 1 },
			{ "hi2", offsetof(pisp_be_output_format_config, hi2), sizeof(uint8_t), 1 },
		}
	}
	/* HoG removed */
	/* The "axi" field here is never configured or used; the real BE_AXI register is not part of the config */
};

const std::vector<ConfigBlock> be_config2 {
	{
		"global", offsetof(pisp_be_config2, global),
		{
			{ "bayer_enables", offsetof(pisp_be_global_config, bayer_enables), sizeof(uint32_t), 1 },
			{ "rgb_enables", offsetof(pisp_be_global_config, rgb_enables), sizeof(uint32_t), 1 },
			{ "bayer_order", offsetof(pisp_be_global_config, bayer_order), sizeof(uint8_t), 1 },
		}
	},
	{
		"input_format", offsetof(pisp_be_config2, input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"decompress", offsetof(pisp_be_config2, decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"dpc", offsetof(pisp_be_config2, dpc),
		{
			{ "coeff_level", offsetof(pisp_be_dpc_config, coeff_level), sizeof(uint8_t), 1 },
			{ "coeff_range", offsetof(pisp_be_dpc_config, coeff_range), sizeof(uint8_t), 1 },
			{ "flags", offsetof(pisp_be_dpc_config, flags), sizeof(uint8_t), 1 },
		}
	},
	{
		"geq", offsetof(pisp_be_config2, geq),
		{
			{ "offset", offsetof(pisp_be_geq_config, offset), sizeof(uint16_t), 1 },
			{ "slope_sharper", offsetof(pisp_be_geq_config, slope_sharper), sizeof(uint16_t), 1 },
			{ "min", offsetof(pisp_be_geq_config, min), sizeof(uint16_t), 1 },
			{ "max", offsetof(pisp_be_geq_config, max), sizeof(uint16_t), 1 },
		}
	},
	{
		"tdn_input_format", offsetof(pisp_be_config2, tdn_input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"tdn_decompress", offsetof(pisp_be_config2, tdn_decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"tdn", offsetof(pisp_be_config2, tdn),
		{
			{ "black_level", offsetof(pisp_be_tdn2_config, black_level), sizeof(uint16_t), 1 },
			{ "ratio", offsetof(pisp_be_tdn2_config, ratio), sizeof(uint16_t), 1 },
			{ "noise_constant", offsetof(pisp_be_tdn2_config, noise_constant), sizeof(uint16_t), 1 },
			{ "noise_slope", offsetof(pisp_be_tdn2_config, noise_slope), sizeof(uint16_t), 1 },
			{ "threshold", offsetof(pisp_be_tdn2_config, threshold), sizeof(uint16_t), 1 },
			{ "reset", offsetof(pisp_be_tdn2_config, reset), sizeof(uint8_t), 1 },
			{ "apply_threshold", offsetof(pisp_be_tdn2_config, apply_threshold), sizeof(uint8_t), 1 },
			{ "swap_inputs", offsetof(pisp_be_tdn2_config, swap_inputs), sizeof(uint8_t), 1 },
			{ "saturation_threshold", offsetof(pisp_be_tdn2_config, saturation_threshold), sizeof(uint16_t), 1 },
		}
	},
	{
		"tdn_compress", offsetof(pisp_be_config2, tdn_compress),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"tdn_output_format", offsetof(pisp_be_config2, tdn_output_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"sdn", offsetof(pisp_be_config2, sdn),
		{
			{ "black_level", offsetof(pisp_be_sdn_config, black_level), sizeof(uint16_t), 1 },
			{ "leakage", offsetof(pisp_be_sdn_config, leakage), sizeof(uint8_t), 1 },
			{ "noise_constant", offsetof(pisp_be_sdn_config, noise_constant), sizeof(uint16_t), 1 },
			{ "noise_slope", offsetof(pisp_be_sdn_config, noise_slope), sizeof(uint16_t), 1 },
			{ "noise_constant2", offsetof(pisp_be_sdn_config, noise_constant2), sizeof(uint16_t), 1 },
			{ "noise_slope2", offsetof(pisp_be_sdn_config, noise_slope2), sizeof(uint16_t), 1 },
		}
	},
	{
		"blc", offsetof(pisp_be_config2, blc),
		{
			{ "black_level_r", offsetof(pisp_bla_config, black_level_r), sizeof(uint16_t), 1 },
			{ "black_level_gr", offsetof(pisp_bla_config, black_level_gr), sizeof(uint16_t), 1 },
			{ "black_level_gb", offsetof(pisp_bla_config, black_level_gb), sizeof(uint16_t), 1 },
			{ "black_level_b", offsetof(pisp_bla_config, black_level_b), sizeof(uint16_t), 1 },
			{ "output_black_level", offsetof(pisp_bla_config, output_black_level), sizeof(uint16_t), 1 },
		}
	},
	{
		"stitch_compress", offsetof(pisp_be_config2, stitch_compress),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"stitch_output_format", offsetof(pisp_be_config2, stitch_output_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"stitch_input_format", offsetof(pisp_be_config2, stitch_input_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"stitch_decompress", offsetof(pisp_be_config2, stitch_decompress),
		{
			{ "offset", offsetof(pisp_decompress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_decompress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"stitch", offsetof(pisp_be_config2, stitch),
		{
			{ "black_level", offsetof(pisp_be_tdn2_config, black_level), sizeof(uint16_t), 1 },
			{ "ratio", offsetof(pisp_be_tdn2_config, ratio), sizeof(uint16_t), 1 },
			{ "noise_constant", offsetof(pisp_be_tdn2_config, noise_constant), sizeof(uint16_t), 1 },
			{ "noise_slope", offsetof(pisp_be_tdn2_config, noise_slope), sizeof(uint16_t), 1 },
			{ "threshold", offsetof(pisp_be_tdn2_config, threshold), sizeof(uint16_t), 1 },
			{ "reset", offsetof(pisp_be_tdn2_config, reset), sizeof(uint8_t), 1 },
			{ "apply_threshold", offsetof(pisp_be_tdn2_config, apply_threshold), sizeof(uint8_t), 1 },
			{ "swap_inputs", offsetof(pisp_be_tdn2_config, swap_inputs), sizeof(uint8_t), 1 },
			{ "saturation_threshold", offsetof(pisp_be_tdn2_config, saturation_threshold), sizeof(uint16_t), 1 },
		}
	},
	{
		"lsc", offsetof(pisp_be_config2, lsc),
		{
			{ "grid_step_x", offsetof(pisp_be_lsc_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_lsc_config, grid_step_y), sizeof(uint16_t), 1 },
			{ "lut_packed", offsetof(pisp_be_lsc_config, lut_packed), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_be_lsc_config, lut_packed) },
		}
	},
	{
		"wbg", offsetof(pisp_be_config2, wbg),
		{
			{ "gain_r", offsetof(pisp_wbg_config, gain_r), sizeof(uint16_t), 1 },
			{ "gain_g", offsetof(pisp_wbg_config, gain_g), sizeof(uint16_t), 1 },
			{ "gain_b", offsetof(pisp_wbg_config, gain_b), sizeof(uint16_t), 1 },
		}
	},
	{
		"cdn", offsetof(pisp_be_config2, cdn),
		{
			{ "thresh", offsetof(pisp_be_cdn_config, thresh), sizeof(uint16_t), 1 },
			{ "iir_strength", offsetof(pisp_be_cdn_config, iir_strength), sizeof(uint8_t), 1 },
			{ "g_adjust", offsetof(pisp_be_cdn_config, g_adjust), sizeof(uint8_t), 1 },
		}
	},
	{
		"cac", offsetof(pisp_be_config2, cac),
		{
			{ "grid_step_x", offsetof(pisp_be_cac_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_cac_config, grid_step_y), sizeof(uint8_t), 1 },
			{ "lut", offsetof(pisp_be_cac_config, lut), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_cac_config, lut) },
		}
	},
	{
		"debin", offsetof(pisp_be_config2, debin),
		{
			{ "coeffs", offsetof(pisp_be_debin_config, coeffs), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_debin_config, coeffs) },
			{ "h_enable", offsetof(pisp_be_debin_config, h_enable), sizeof(int8_t), 1 },
			{ "v_enable", offsetof(pisp_be_debin_config, v_enable), sizeof(int8_t), 1 },
		}
	},
	{
		"tonemap2_format", offsetof(pisp_be_config2, tonemap2_format),
		{
			{ "width", offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
		}
	},
	{
		"tonemap2", offsetof(pisp_be_config2, tonemap2),
		{
			{ "grid_step_x", offsetof(pisp_be_tonemap2_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_tonemap2_config, grid_step_y), sizeof(uint16_t), 1 },
			{ "lores_step_x", offsetof(pisp_be_tonemap2_config, lores_step_x), sizeof(uint16_t), 1 },
			{ "lores_step_y", offsetof(pisp_be_tonemap2_config, lores_step_y), sizeof(uint16_t), 1 },
			{ "sd_constant", offsetof(pisp_be_tonemap2_config, sd_constant), sizeof(uint16_t), 1 },
			{ "sd_slope", offsetof(pisp_be_tonemap2_config, sd_slope), sizeof(uint16_t), 1 },
			{ "dx_sd_recip", offsetof(pisp_be_tonemap2_config, dx_sd_recip), sizeof(uint16_t), 1 },
			{ "dy_sd_recip", offsetof(pisp_be_tonemap2_config, dy_sd_recip), sizeof(uint16_t), 1 },
			{ "centre_weight", offsetof(pisp_be_tonemap2_config, centre_weight), sizeof(uint8_t), 1 },
			{ "spatial_gains", offsetof(pisp_be_tonemap2_config, spatial_gains), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_tonemap2_config, spatial_gains) * PISP_ARRAY_SIZE(pisp_be_tonemap2_config, spatial_gains[0]) },
			{ "spatial_gain_knee", offsetof(pisp_be_tonemap2_config, spatial_gain_knee), sizeof(uint16_t), 1 },
			{ "spatial_gain_slope", offsetof(pisp_be_tonemap2_config, spatial_gain_slope), sizeof(uint16_t), 1 },
			{ "level_gain_base", offsetof(pisp_be_tonemap2_config, level_gain_base), sizeof(uint16_t), 1 },
			{ "level_gain_boost", offsetof(pisp_be_tonemap2_config, level_gain_boost), sizeof(uint16_t), 1 },
			{ "level_gain_log", offsetof(pisp_be_tonemap2_config, level_gain_log), sizeof(uint8_t), 1 },
			{ "gamma", offsetof(pisp_be_tonemap2_config, gamma), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_tonemap2_config, gamma) },
			{ "boost_neg", offsetof(pisp_be_tonemap2_config, boost_neg), sizeof(uint16_t), 1 },
			{ "boost_pos", offsetof(pisp_be_tonemap2_config, boost_pos), sizeof(uint16_t), 1 },
		}
	},
	{
		"demosaic", offsetof(pisp_be_config2, demosaic),
		{
			{ "sharper", offsetof(pisp_be_demosaic2_config, sharper), sizeof(uint8_t), 1 },
			{ "fc_mode", offsetof(pisp_be_demosaic2_config, fc_mode), sizeof(uint8_t), 1 },
			{ "grad_scale", offsetof(pisp_be_demosaic2_config, grad_scale), sizeof(uint8_t), 1 },
			{ "extra_scale", offsetof(pisp_be_demosaic2_config, extra_scale), sizeof(uint8_t), 1 },
			{ "colour_scale1", offsetof(pisp_be_demosaic2_config, colour_scale1), sizeof(uint8_t), 1 },
			{ "colour_scale2", offsetof(pisp_be_demosaic2_config, colour_scale2), sizeof(uint8_t), 1 },
			{ "min_scale", offsetof(pisp_be_demosaic2_config, min_scale), sizeof(uint8_t), 1 },
			{ "max_scale", offsetof(pisp_be_demosaic2_config, max_scale), sizeof(uint8_t), 1 },
			{ "threshold_scale", offsetof(pisp_be_demosaic2_config, threshold_scale), sizeof(uint8_t), 1 },
		}
	},
	{
		"dedistort", offsetof(pisp_be_config2, dedistort),
		{
			{ "grid_step_x", offsetof(pisp_be_dedistort_config, grid_step_x), sizeof(uint16_t), 1 },
			{ "grid_step_y", offsetof(pisp_be_dedistort_config, grid_step_y), sizeof(uint16_t), 1 },
			{ "lut", offsetof(pisp_be_dedistort_config, lut), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_dedistort_config, lut) * PISP_ARRAY_SIZE(pisp_be_dedistort_config, lut[0]) },
		}
	},
	{
		"ccm", offsetof(pisp_be_config2, ccm),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"sat_control", offsetof(pisp_be_config2, sat_control),
		{
			{ "shift_r", offsetof(pisp_be_sat_control_config, shift_r), sizeof(uint8_t), 1 },
			{ "shift_g", offsetof(pisp_be_sat_control_config, shift_g), sizeof(uint8_t), 1 },
			{ "shift_b", offsetof(pisp_be_sat_control_config, shift_b), sizeof(uint8_t), 1 },
		}
	},
	{
		"ycbcr", offsetof(pisp_be_config2, ycbcr),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"sharpen", offsetof(pisp_be_config2, sharpen),
		{
			{ "kernel0", offsetof(pisp_be_sharpen_config, kernel0), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel0) },
			{ "kernel1", offsetof(pisp_be_sharpen_config, kernel1), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel1) },
			{ "kernel2", offsetof(pisp_be_sharpen_config, kernel2), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel2) },
			{ "kernel3", offsetof(pisp_be_sharpen_config, kernel3), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel3) },
			{ "kernel4", offsetof(pisp_be_sharpen_config, kernel4), sizeof(int8_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, kernel4) },
			{ "threshold_offset0", offsetof(pisp_be_sharpen_config, threshold_offset0), sizeof(uint16_t), 1 },
			{ "threshold_slope0", offsetof(pisp_be_sharpen_config, threshold_slope0), sizeof(uint16_t), 1 },
			{ "threshold_offset1", offsetof(pisp_be_sharpen_config, threshold_offset1), sizeof(uint16_t), 1 },
			{ "threshold_slope1", offsetof(pisp_be_sharpen_config, threshold_slope1), sizeof(uint16_t), 1 },
			{ "threshold_offset2", offsetof(pisp_be_sharpen_config, threshold_offset2), sizeof(uint16_t), 1 },
			{ "threshold_slope2", offsetof(pisp_be_sharpen_config, threshold_slope2), sizeof(uint16_t), 1 },
			{ "threshold_offset3", offsetof(pisp_be_sharpen_config, threshold_offset3), sizeof(uint16_t), 1 },
			{ "threshold_slope3", offsetof(pisp_be_sharpen_config, threshold_slope3), sizeof(uint16_t), 1 },
			{ "threshold_offset4", offsetof(pisp_be_sharpen_config, threshold_offset4), sizeof(uint16_t), 1 },
			{ "threshold_slope4", offsetof(pisp_be_sharpen_config, threshold_slope4), sizeof(uint16_t), 1 },
			{ "positive_strength", offsetof(pisp_be_sharpen_config, positive_strength), sizeof(uint16_t), 1 },
			{ "positive_pre_limit", offsetof(pisp_be_sharpen_config, positive_pre_limit), sizeof(uint16_t), 1 },
			{ "positive_func", offsetof(pisp_be_sharpen_config, positive_func), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, positive_func) },
			{ "positive_limit", offsetof(pisp_be_sharpen_config, positive_limit), sizeof(uint16_t), 1 },
			{ "negative_strength", offsetof(pisp_be_sharpen_config, negative_strength), sizeof(uint16_t), 1 },
			{ "negative_pre_limit", offsetof(pisp_be_sharpen_config, negative_pre_limit), sizeof(uint16_t), 1 },
			{ "negative_func", offsetof(pisp_be_sharpen_config, negative_func), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_sharpen_config, negative_func) },
			{ "negative_limit", offsetof(pisp_be_sharpen_config, negative_limit), sizeof(uint16_t), 1 },
			{ "enables", offsetof(pisp_be_sharpen_config, enables), sizeof(uint8_t), 1 },
			{ "white", offsetof(pisp_be_sharpen_config, white), sizeof(uint8_t), 1 },
			{ "black", offsetof(pisp_be_sharpen_config, black), sizeof(uint8_t), 1 },
			{ "grey", offsetof(pisp_be_sharpen_config, grey), sizeof(uint8_t), 1 },
		}
	},
	{
		"false_colour", offsetof(pisp_be_config2, false_colour),
		{
			{ "distance", offsetof(pisp_be_false_colour_config, distance), sizeof(uint8_t), 1 },
		}
	},
	{
		"sh_fc_combine", offsetof(pisp_be_config2, sh_fc_combine),
		{
			{ "y_factor", offsetof(pisp_be_sh_fc_combine_config, y_factor), sizeof(uint8_t), 1 },
			{ "c1_factor", offsetof(pisp_be_sh_fc_combine_config, c1_factor), sizeof(uint8_t), 1 },
			{ "c2_factor", offsetof(pisp_be_sh_fc_combine_config, c2_factor), sizeof(uint8_t), 1 },
		}
	},
	{
		"ycbcr_inverse", offsetof(pisp_be_config2, ycbcr_inverse),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"pre_csc0", offsetof(pisp_be_config2, pre_csc[0]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"pre_csc1", offsetof(pisp_be_config2, pre_csc[1]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"gamma0", offsetof(pisp_be_config2, gamma[0]),
		{
			{ "lut", offsetof(pisp_be_gamma2_config, lut), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_gamma2_config, lut) },
		}
	},
	{
		"gamma1", offsetof(pisp_be_config2, gamma[1]),
		{
			{ "lut", offsetof(pisp_be_gamma2_config, lut), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_be_gamma2_config, lut) },
		}
	},
	{
		"post_csc0", offsetof(pisp_be_config2, post_csc[0]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"post_csc1", offsetof(pisp_be_config2, post_csc[1]),
		{
			{ "coeffs", offsetof(pisp_be_ccm_config, coeffs), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, coeffs) },
			{ "offsets", offsetof(pisp_be_ccm_config, offsets), sizeof(int32_t), PISP_ARRAY_SIZE(pisp_be_ccm_config, offsets) },
		}
	},
	{
		"downscale0", offsetof(pisp_be_config2, downscale[0]),
		{
			{ "scale_factor_h", offsetof(pisp_be_downscale_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_downscale_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "scale_recip_h", offsetof(pisp_be_downscale_config, scale_recip_h), sizeof(uint16_t), 1 },
			{ "scale_recip_v", offsetof(pisp_be_downscale_config, scale_recip_v), sizeof(uint16_t), 1 },
		}
	},
	{
		"downscale1", offsetof(pisp_be_config2, downscale[1]),
		{
			{ "scale_factor_h", offsetof(pisp_be_downscale_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_downscale_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "scale_recip_h", offsetof(pisp_be_downscale_config, scale_recip_h), sizeof(uint16_t), 1 },
			{ "scale_recip_v", offsetof(pisp_be_downscale_config, scale_recip_v), sizeof(uint16_t), 1 },
		}
	},
	{
		"resample0", offsetof(pisp_be_config2, resample[0]),
		{
			{ "scale_factor_h", offsetof(pisp_be_resample2_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_resample2_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "coef_h", offsetof(pisp_be_resample2_config, coef_h), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample2_config, coef_h) },
			{ "coef_v", offsetof(pisp_be_resample2_config, coef_v), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample2_config, coef_v) },
		}
	},
	{
		"resample1", offsetof(pisp_be_config2, resample[1]),
		{
			{ "scale_factor_h", offsetof(pisp_be_resample2_config, scale_factor_h), sizeof(uint16_t), 1 },
			{ "scale_factor_v", offsetof(pisp_be_resample2_config, scale_factor_v), sizeof(uint16_t), 1 },
			{ "coef_h", offsetof(pisp_be_resample2_config, coef_h), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample2_config, coef_h) },
			{ "coef_v", offsetof(pisp_be_resample2_config, coef_v), sizeof(int16_t), PISP_ARRAY_SIZE(pisp_be_resample2_config, coef_v) },
		}
	},
	{
		"output_format0", offsetof(pisp_be_config2, output_format[0]),
		{
			{ "width", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
			{ "transform", offsetof(pisp_be_output_format_config2, transform), sizeof(uint8_t), 1 },
			{ "rebayer", offsetof(pisp_be_output_format_config2, rebayer), sizeof(uint8_t), 1 },
			{ "bayer_order", offsetof(pisp_be_output_format_config2, bayer_order), sizeof(uint8_t), 1 },
			{ "chroma_pos", offsetof(pisp_be_output_format_config2, chroma_pos), sizeof(uint8_t), 1 },
			{ "lo", offsetof(pisp_be_output_format_config2, lo), sizeof(uint16_t), 1 },
			{ "hi", offsetof(pisp_be_output_format_config2, hi), sizeof(uint16_t), 1 },
			{ "lo2", offsetof(pisp_be_output_format_config2, lo2), sizeof(uint16_t), 1 },
			{ "hi2", offsetof(pisp_be_output_format_config2, hi2), sizeof(uint16_t), 1 },
		}
	},
	{
		"output_format1", offsetof(pisp_be_config2, output_format[1]),
		{
			{ "width", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, width), sizeof(uint16_t), 1 },
			{ "height", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, height), sizeof(uint16_t), 1 },
			{ "format", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, format), sizeof(uint32_t), 1 },
			{ "stride", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, stride), sizeof(int32_t), 1 },
			{ "stride2", offsetof(pisp_be_output_format_config2, image) + offsetof(pisp_image_format_config, stride2), sizeof(int32_t), 1 },
			{ "transform", offsetof(pisp_be_output_format_config2, transform), sizeof(uint8_t), 1 },
			{ "rebayer", offsetof(pisp_be_output_format_config2, rebayer), sizeof(uint8_t), 1 },
			{ "bayer_order", offsetof(pisp_be_output_format_config2, bayer_order), sizeof(uint8_t), 1 },
			{ "chroma_pos", offsetof(pisp_be_output_format_config2, chroma_pos), sizeof(uint8_t), 1 },
			{ "lo", offsetof(pisp_be_output_format_config2, lo), sizeof(uint16_t), 1 },
			{ "hi", offsetof(pisp_be_output_format_config2, hi), sizeof(uint16_t), 1 },
			{ "lo2", offsetof(pisp_be_output_format_config2, lo2), sizeof(uint16_t), 1 },
			{ "hi2", offsetof(pisp_be_output_format_config2, hi2), sizeof(uint16_t), 1 },
		}
	},
	{
		"compress0", offsetof(pisp_be_config2, compress[0]),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	},
	{
		"compress1", offsetof(pisp_be_config2, compress[1]),
		{
			{ "offset", offsetof(pisp_compress_config, offset), sizeof(uint16_t), 1 },
			{ "mode", offsetof(pisp_compress_config, mode), sizeof(uint8_t), 1 },
		}
	}
};

const ConfigBlock tiles_config {
	"tiles", 0,
	{
		{ "edge", offsetof(pisp_tile, edge), sizeof(uint8_t), 1 },
		{ "input_addr_offset", offsetof(pisp_tile, input_addr_offset), sizeof(uint32_t), 1 },
		{ "input_addr_offset2", offsetof(pisp_tile, input_addr_offset2), sizeof(uint32_t), 1 },
		{ "input_offset_x", offsetof(pisp_tile, input_offset_x), sizeof(uint16_t), 1 },
		{ "input_offset_y", offsetof(pisp_tile, input_offset_y), sizeof(uint16_t), 1 },
		{ "input_width", offsetof(pisp_tile, input_width), sizeof(uint16_t), 1 },
		{ "input_height", offsetof(pisp_tile, input_width), sizeof(uint16_t), 1 },
		{ "tdn_input_addr_offset", offsetof(pisp_tile, tdn_input_addr_offset), sizeof(uint32_t), 1 },
		{ "tdn_output_addr_offset", offsetof(pisp_tile, tdn_output_addr_offset), sizeof(uint32_t), 1 },
		{ "stitch_input_addr_offset", offsetof(pisp_tile, stitch_input_addr_offset), sizeof(uint32_t), 1 },
		{ "stitch_output_addr_offset", offsetof(pisp_tile, stitch_output_addr_offset), sizeof(uint32_t), 1 },
		{ "lsc_grid_offset_x", offsetof(pisp_tile, lsc_grid_offset_x), sizeof(uint32_t), 1 },
		{ "lsc_grid_offset_y", offsetof(pisp_tile, lsc_grid_offset_y), sizeof(uint32_t), 1 },
		{ "cac_grid_offset_x", offsetof(pisp_tile, cac_grid_offset_x), sizeof(uint32_t), 1 },
		{ "cac_grid_offset_y", offsetof(pisp_tile, cac_grid_offset_y), sizeof(uint32_t), 1 },
		{ "crop_x_start", offsetof(pisp_tile, crop_x_start), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, crop_x_start) },
		{ "crop_x_end", offsetof(pisp_tile, crop_x_end), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, crop_x_end) },
		{ "crop_y_start", offsetof(pisp_tile, crop_y_start), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, crop_y_start) },
		{ "crop_y_end", offsetof(pisp_tile, crop_y_end), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, crop_y_end) },
		{ "downscale_phase_x", offsetof(pisp_tile, downscale_phase_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, downscale_phase_x) },
		{ "downscale_phase_y", offsetof(pisp_tile, downscale_phase_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, downscale_phase_y) },
		{ "resample_in_width", offsetof(pisp_tile, resample_in_width), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, resample_in_width) },
		{ "resample_in_height", offsetof(pisp_tile, resample_in_height), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, resample_in_height) },
		{ "resample_phase_x", offsetof(pisp_tile, resample_phase_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, resample_phase_x) },
		{ "resample_phase_y", offsetof(pisp_tile, resample_phase_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, resample_phase_y) },
		{ "output_offset_x", offsetof(pisp_tile, output_offset_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, output_offset_x) },
		{ "output_offset_y", offsetof(pisp_tile, output_offset_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, output_offset_y) },
		{ "output_width", offsetof(pisp_tile, output_width), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, output_width) },
		{ "output_height", offsetof(pisp_tile, output_height), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile, output_height) },
		{ "output_addr_offset", offsetof(pisp_tile, output_addr_offset), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_tile, output_addr_offset) },
		{ "output_addr_offset2", offsetof(pisp_tile, output_addr_offset2), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_tile, output_addr_offset2) },
		{ "output_hog_addr_offset", offsetof(pisp_tile, output_hog_addr_offset), sizeof(uint32_t), 1 },
	}
};

const ConfigBlock tiles_config2 {
	"tiles", 0,
	{
		{ "input_addr_offset", offsetof(pisp_tile2, input_addr_offset), sizeof(uint32_t), 1 },
		{ "input_addr_offset2", offsetof(pisp_tile2, input_addr_offset2), sizeof(uint32_t), 1 },
		{ "input_offset_x", offsetof(pisp_tile2, input_offset_x), sizeof(uint16_t), 1 },
		{ "input_offset_y", offsetof(pisp_tile2, input_offset_y), sizeof(uint16_t), 1 },
		{ "input_width", offsetof(pisp_tile2, input_width), sizeof(uint16_t), 1 },
		{ "input_height", offsetof(pisp_tile2, input_height), sizeof(uint16_t), 1 },
		{ "tdn_input_addr_offset", offsetof(pisp_tile2, tdn_input_addr_offset), sizeof(uint32_t), 1 },
		{ "tdn_output_addr_offset", offsetof(pisp_tile2, tdn_output_addr_offset), sizeof(uint32_t), 1 },
		{ "stitch_input_addr_offset", offsetof(pisp_tile2, stitch_input_addr_offset), sizeof(uint32_t), 1 },
		{ "stitch_output_addr_offset", offsetof(pisp_tile2, stitch_output_addr_offset), sizeof(uint32_t), 1 },
		{ "lsc_grid_offset_x", offsetof(pisp_tile2, lsc_grid_offset_x), sizeof(uint32_t), 1 },
		{ "lsc_grid_offset_y", offsetof(pisp_tile2, lsc_grid_offset_y), sizeof(uint32_t), 1 },
		{ "cac_grid_offset_x", offsetof(pisp_tile2, cac_grid_offset_x), sizeof(uint32_t), 1 },
		{ "cac_grid_offset_y", offsetof(pisp_tile2, cac_grid_offset_y), sizeof(uint32_t), 1 },
		{ "tonemap_grid_offset_x", offsetof(pisp_tile2, tonemap_grid_offset_x), sizeof(uint32_t), 1 },
		{ "tonemap_grid_offset_y", offsetof(pisp_tile2, tonemap_grid_offset_y), sizeof(uint32_t), 1 },
		{ "tonemap_lores_offset_x", offsetof(pisp_tile2, tonemap_lores_offset_x), sizeof(uint32_t), 1 },
		{ "tonemap_lores_offset_y", offsetof(pisp_tile2, tonemap_lores_offset_y), sizeof(uint32_t), 1 },
		{ "dedistort_grid_offset_x", offsetof(pisp_tile2, dedistort_grid_offset_x), sizeof(uint32_t), 1 },
		{ "dedistort_grid_offset_y", offsetof(pisp_tile2, dedistort_grid_offset_y), sizeof(uint32_t), 1 },
		{ "dedistorted_offset_x", offsetof(pisp_tile2, dedistorted_offset_x), sizeof(uint16_t), 1 },
		{ "dedistorted_width", offsetof(pisp_tile2, dedistorted_width), sizeof(uint16_t), 1 },
		{ "crop_x_start", offsetof(pisp_tile2, crop_x_start), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, crop_x_start) },
		{ "crop_y_start", offsetof(pisp_tile2, crop_y_start), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, crop_y_start) },
		{ "crop_width", offsetof(pisp_tile2, crop_width), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, crop_width) },
		{ "crop_height", offsetof(pisp_tile2, crop_height), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, crop_height) },
		{ "downscale_phase_x", offsetof(pisp_tile2, downscale_phase_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, downscale_phase_x) },
		{ "downscale_phase_y", offsetof(pisp_tile2, downscale_phase_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, downscale_phase_y) },
		{ "resample_in_width", offsetof(pisp_tile2, resample_in_width), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, resample_in_width) },
		{ "resample_in_height", offsetof(pisp_tile2, resample_in_height), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, resample_in_height) },
		{ "resample_phase_x", offsetof(pisp_tile2, resample_phase_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, resample_phase_x) },
		{ "resample_phase_y", offsetof(pisp_tile2, resample_phase_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, resample_phase_y) },
		{ "resample_start_context_x", offsetof(pisp_tile2, resample_start_context_x), sizeof(uint8_t), PISP_ARRAY_SIZE(pisp_tile2, resample_start_context_x) },
		{ "resample_start_context_y", offsetof(pisp_tile2, resample_start_context_y), sizeof(uint8_t), PISP_ARRAY_SIZE(pisp_tile2, resample_start_context_y) },
		{ "output_offset_x", offsetof(pisp_tile2, output_offset_x), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, output_offset_x) },
		{ "output_offset_y", offsetof(pisp_tile2, output_offset_y), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, output_offset_y) },
		{ "output_width", offsetof(pisp_tile2, output_width), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, output_width) },
		{ "output_height", offsetof(pisp_tile2, output_height), sizeof(uint16_t), PISP_ARRAY_SIZE(pisp_tile2, output_height) },
		{ "output_addr_offset", offsetof(pisp_tile2, output_addr_offset), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_tile2, output_addr_offset) },
		{ "output_addr_offset2", offsetof(pisp_tile2, output_addr_offset2), sizeof(uint32_t), PISP_ARRAY_SIZE(pisp_tile2, output_addr_offset2) },
	}
};

uint32_t read_val(uint8_t *ptr, uint32_t offset, uint32_t size)
{
	uint32_t val = 0;

	for (unsigned int i = 0; i < size; i++)
		val |= (*(ptr + offset + i)) << (i * 8);

	return val & mask.at(size);
}

void write_val(uint8_t *ptr, uint32_t offset, uint32_t size, uint32_t val)
{
	for (unsigned int i = 0; i < size; i++)
		*(ptr + offset + i) = (val >> (i * 8)) & 0xff;
}

} // namespace

std::string BackEnd::get_json_config(pisp_be_tiles_config *config)
{
	bool v2 = variant_ >= BERYL;
	const auto &cfg = v2 ? be_config2 : be_config;
	const auto &tcfg = v2 ? tiles_config2 : tiles_config;

	json j = { {"version", v2 ? 2.0 : 1.0}, {"be_revision", variant_.back_end_version()} };

	uint8_t *config_base = v2 ? (uint8_t *)&config->v2.config : (uint8_t *)&config->config;
	for (auto const &param : cfg)
	{
		json b;
		for (auto const &field : param.fields)
		{
			if (field.num == 1)
			{
				b[param.name][field.name] = read_val(config_base, param.offset + field.offset, field.size);
			}
			else
			{
				for (unsigned int i = 0; i < field.num; i++)
					b[param.name][field.name].push_back(read_val(config_base, param.offset + field.offset + i * field.size, field.size));
			}
		}
		j["config"].push_back(b);
	}

	unsigned int num_tiles = v2 ? config->v2.num_tiles : config->num_tiles;
	for (unsigned int t = 0; t < num_tiles; t++)
	{
		uint8_t *tile_base = v2 ? (uint8_t *)&config->v2.tiles[t] : (uint8_t *)&config->tiles[t];
		json b;
		for (auto const &field : tcfg.fields)
		{
			if (field.num == 1)
			{
				b[field.name] = read_val(tile_base, field.offset, field.size);
			}
			else
			{
				for (unsigned int i = 0; i < field.num; i++)
					b[field.name].push_back(read_val(tile_base, field.offset + i * field.size, field.size));
			}
		}
		j["tiles"].push_back(b);
	}

	return j.dump(4);
}

void BackEnd::set_json_config(const std::string &json_str)
{
	json j = json::parse(json_str);

	bool v2 = variant_ >= BERYL;
	const auto &cfg = v2 ? be_config2 : be_config;

	uint8_t *config = v2 ? (uint8_t *)&pipe_->hw_config_v2 : (uint8_t *)&pipe_->hw_config;
	for (auto const &param : cfg)
	{
		for (auto const &field : param.fields)
		{
			if (field.num == 1)
			{
				uint32_t value = j["config"][param.name][field.name].get<uint32_t>();
				write_val(config, param.offset + field.offset, field.size, value);
			}
			else
			{
				std::vector<uint32_t> values = j["config"][param.name][field.name].get<std::vector<uint32_t>>();
				for (unsigned int i = 0; i < values.size(); i++)
					write_val(config, param.offset + field.offset + i * field.size, field.size, values[i]);
			}
		}
	}

	// Clear any dirty flags so no reconfiguration happens on the next Prepare() call.
	pipe_->dirty_flags_bayer = BayerEnable::None;
	pipe_->dirty_flags_rgb = RgbEnable::None;
	pipe_->dirty_flags_extra = DirtyFlag::None;

	// But do retile the pipeline to get the tile structures setup correctly.
	pipe_->retile = true;
}

// clang-format on
