/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_be_config.h - PiSP Back End HW configuration definitions
 */
#ifndef _PISP_BE_CONFIG_H_
#define _PISP_BE_CONFIG_H_

#include <linux/types.h>

#include "common/pisp_common.h"

/* byte alignment for inputs */
#define PISP_BACK_END_INPUT_ALIGN 4u
/* alignment for compressed inputs */
#define PISP_BACK_END_COMPRESSED_ALIGN 8u
/* minimum required byte alignment for outputs */
#define PISP_BACK_END_OUTPUT_MIN_ALIGN 16u
/* preferred byte alignment for outputs */
#define PISP_BACK_END_OUTPUT_MAX_ALIGN 64u

/* minimum allowed tile width anywhere in the pipeline */
#define PISP_BACK_END_MIN_TILE_WIDTH 16u
/* minimum allowed tile width anywhere in the pipeline */
#define PISP_BACK_END_MIN_TILE_HEIGHT 16u

#define PISP_BACK_END_NUM_OUTPUTS 2
#define PISP_BACK_END_HOG_OUTPUT 1

#define PISP_BACK_END_NUM_TILES 64

typedef enum {
	PISP_BE_BAYER_ENABLE_INPUT = 0x000001,
	PISP_BE_BAYER_ENABLE_DECOMPRESS = 0x000002,
	PISP_BE_BAYER_ENABLE_DPC = 0x000004,
	PISP_BE_BAYER_ENABLE_GEQ = 0x000008,
	PISP_BE_BAYER_ENABLE_TDN_INPUT = 0x000010,
	PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS = 0x000020,
	PISP_BE_BAYER_ENABLE_TDN = 0x000040,
	PISP_BE_BAYER_ENABLE_TDN_COMPRESS = 0x000080,
	PISP_BE_BAYER_ENABLE_TDN_OUTPUT = 0x000100,
	PISP_BE_BAYER_ENABLE_SDN = 0x000200,
	PISP_BE_BAYER_ENABLE_BLC = 0x000400,
	PISP_BE_BAYER_ENABLE_STITCH_INPUT = 0x000800,
	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS = 0x001000,
	PISP_BE_BAYER_ENABLE_STITCH = 0x002000,
	PISP_BE_BAYER_ENABLE_STITCH_COMPRESS = 0x004000,
	PISP_BE_BAYER_ENABLE_STITCH_OUTPUT = 0x008000,
	PISP_BE_BAYER_ENABLE_WBG = 0x010000,
	PISP_BE_BAYER_ENABLE_CDN = 0x020000,
	PISP_BE_BAYER_ENABLE_LSC = 0x040000,
	PISP_BE_BAYER_ENABLE_TONEMAP = 0x080000,
	PISP_BE_BAYER_ENABLE_CAC = 0x100000,
	PISP_BE_BAYER_ENABLE_DEBIN = 0x200000,
	PISP_BE_BAYER_ENABLE_DEMOSAIC = 0x400000,
} pisp_be_bayer_enable;

typedef enum {
	PISP_BE_RGB_ENABLE_INPUT = 0x000001,
	PISP_BE_RGB_ENABLE_CCM = 0x000002,
	PISP_BE_RGB_ENABLE_SAT_CONTROL = 0x000004,
	PISP_BE_RGB_ENABLE_YCBCR = 0x000008,
	PISP_BE_RGB_ENABLE_FALSE_COLOUR = 0x000010,
	PISP_BE_RGB_ENABLE_SHARPEN = 0x000020,
	/* Preferred colours would occupy 0x000040 */
	PISP_BE_RGB_ENABLE_YCBCR_INVERSE = 0x000080,
	PISP_BE_RGB_ENABLE_GAMMA = 0x000100,
	PISP_BE_RGB_ENABLE_CSC0 = 0x000200,
	PISP_BE_RGB_ENABLE_CSC1 = 0x000400,
	PISP_BE_RGB_ENABLE_DOWNSCALE0 = 0x001000,
	PISP_BE_RGB_ENABLE_DOWNSCALE1 = 0x002000,
	PISP_BE_RGB_ENABLE_RESAMPLE0 = 0x008000,
	PISP_BE_RGB_ENABLE_RESAMPLE1 = 0x010000,
	PISP_BE_RGB_ENABLE_OUTPUT0 = 0x040000,
	PISP_BE_RGB_ENABLE_OUTPUT1 = 0x080000,
	PISP_BE_RGB_ENABLE_HOG = 0x200000
} pisp_be_rgb_enable;

#define PISP_BE_RGB_ENABLE_CSC(i) (PISP_BE_RGB_ENABLE_CSC0 << (i))
#define PISP_BE_RGB_ENABLE_DOWNSCALE(i) (PISP_BE_RGB_ENABLE_DOWNSCALE0 << (i))
#define PISP_BE_RGB_ENABLE_RESAMPLE(i) (PISP_BE_RGB_ENABLE_RESAMPLE0 << (i))
#define PISP_BE_RGB_ENABLE_OUTPUT(i) (PISP_BE_RGB_ENABLE_OUTPUT0 << (i))

/*
 * We use the enable flags to show when blocks are "dirty", but we need some
 * extra ones too.
 */
typedef enum {
	PISP_BE_DIRTY_GLOBAL = 0x0001,
	PISP_BE_DIRTY_SH_FC_COMBINE = 0x0002,
	PISP_BE_DIRTY_CROP = 0x0004
} pisp_be_dirty;

typedef struct {
	uint32_t bayer_enables;
	uint32_t rgb_enables;
	uint8_t bayer_order;
	uint8_t pad[3];
} pisp_be_global_config;

typedef struct {
	/* low 32 bits followed by high 32 bits (for each of up to three planes) */
	uint32_t addr[3][2];
} pisp_be_input_buffer_config;

typedef struct {
	uint8_t coeff_level;
	uint8_t coeff_range;
	uint8_t pad;
#define PISP_BE_DPC_FLAG_FOLDBACK 1
	uint8_t flags;
} pisp_be_dpc_config;

typedef struct {
	uint16_t offset;
#define PISP_BE_GEQ_SHARPER (1 << 15)
#define PISP_BE_GEQ_SLOPE ((1 << 10) - 1)
	/* top bit is the "sharper" flag, slope value is bottom 10 bits */
	uint16_t slope_sharper;
	uint16_t min;
	uint16_t max;
} pisp_be_geq_config;

typedef struct {
	/* low 32 bits followed by high 32 bits */
	uint32_t addr[2];
} pisp_be_tdn_input_buffer_config;

typedef struct {
	uint16_t black_level;
	uint16_t ratio;
	uint16_t noise_constant;
	uint16_t noise_slope;
	uint16_t threshold;
	uint8_t reset;
	uint8_t pad;
} pisp_be_tdn_config;

typedef struct {
	/* low 32 bits followed by high 32 bits */
	uint32_t addr[2];
} pisp_be_tdn_output_buffer_config;

typedef struct {
	uint16_t black_level;
	uint8_t leakage;
	uint8_t pad;
	uint16_t noise_constant;
	uint16_t noise_slope;
	uint16_t noise_constant2;
	uint16_t noise_slope2;
} pisp_be_sdn_config;

typedef struct {
	/* low 32 bits followed by high 32 bits */
	uint32_t addr[2];
} pisp_be_stitch_input_buffer_config;

#define PISP_BE_STITCH_STREAMING_LONG 0x8000
#define PISP_BE_STITCH_EXPOSURE_RATIO_MASK 0x7fff

typedef struct {
	uint16_t threshold_lo;
	uint8_t threshold_diff_power;
	uint8_t pad;

	/* top bit indicates whether streaming input is the long exposure */
	uint16_t exposure_ratio;

	uint8_t motion_threshold_256;
	uint8_t motion_threshold_recip;
} pisp_be_stitch_config;

typedef struct {
	/* low 32 bits followed by high 32 bits */
	uint32_t addr[2];
} pisp_be_stitch_output_buffer_config;

typedef struct {
	uint16_t thresh;
	uint8_t iir_strength;
	uint8_t g_adjust;
} pisp_be_cdn_config;

#define PISP_BE_LSC_LOG_GRID_SIZE 5
#define PISP_BE_LSC_GRID_SIZE (1 << PISP_BE_LSC_LOG_GRID_SIZE)
#define PISP_BE_LSC_STEP_PRECISION 18

typedef struct {
	/* (1<<18) / grid_cell_width */
	uint16_t grid_step_x;
	/* (1<<18) / grid_cell_height */
	uint16_t grid_step_y;
	/* RGB gains jointly encoded in 32 bits */
	uint32_t lut_packed[PISP_BE_LSC_GRID_SIZE + 1]
			   [PISP_BE_LSC_GRID_SIZE + 1];
} pisp_be_lsc_config;

typedef struct {
	uint16_t offset_x;
	uint16_t offset_y;
} pisp_be_lsc_extra;

#define PISP_BE_CAC_LOG_GRID_SIZE 3
#define PISP_BE_CAC_GRID_SIZE (1 << PISP_BE_CAC_LOG_GRID_SIZE)
#define PISP_BE_CAC_STEP_PRECISION 20

typedef struct {
	/* (1<<20) / grid_cell_width */
	uint16_t grid_step_x;
	/* (1<<20) / grid_cell_height */
	uint16_t grid_step_y;
	/* [gridy][gridx][rb][xy] */
	int8_t lut[PISP_BE_CAC_GRID_SIZE + 1][PISP_BE_CAC_GRID_SIZE + 1][2][2];
} pisp_be_cac_config;

typedef struct {
	uint16_t offset_x;
	uint16_t offset_y;
} pisp_be_cac_extra;

#define PISP_BE_DEBIN_NUM_COEFFS 4

typedef struct {
	int8_t coeffs[PISP_BE_DEBIN_NUM_COEFFS];
	int8_t h_enable;
	int8_t v_enable;
	int8_t pad[2];
} pisp_be_debin_config;

#define PISP_BE_TONEMAP_LUT_SIZE 64

typedef struct {
	uint16_t detail_constant;
	uint16_t detail_slope;
	uint16_t iir_strength;
	uint16_t strength;
	uint32_t lut[PISP_BE_TONEMAP_LUT_SIZE];
} pisp_be_tonemap_config;

typedef struct {
	uint8_t sharper;
	uint8_t fc_mode;
	uint8_t pad[2];
} pisp_be_demosaic_config;

typedef struct {
	int16_t coeffs[9];
	uint8_t pad[2];
	int32_t offsets[3];
} pisp_be_ccm_config;

typedef struct {
	uint8_t shift_r;
	uint8_t shift_g;
	uint8_t shift_b;
	uint8_t pad;
} pisp_be_sat_control_config;

typedef struct {
	uint8_t distance;
	uint8_t pad[3];
} pisp_be_false_colour_config;

#define PISP_BE_SHARPEN_SIZE 5
#define PISP_BE_SHARPEN_FUNC_NUM_POINTS 9

typedef struct {
	int8_t kernel0[PISP_BE_SHARPEN_SIZE * PISP_BE_SHARPEN_SIZE];
	int8_t pad0[3];
	int8_t kernel1[PISP_BE_SHARPEN_SIZE * PISP_BE_SHARPEN_SIZE];
	int8_t pad1[3];
	int8_t kernel2[PISP_BE_SHARPEN_SIZE * PISP_BE_SHARPEN_SIZE];
	int8_t pad2[3];
	int8_t kernel3[PISP_BE_SHARPEN_SIZE * PISP_BE_SHARPEN_SIZE];
	int8_t pad3[3];
	int8_t kernel4[PISP_BE_SHARPEN_SIZE * PISP_BE_SHARPEN_SIZE];
	int8_t pad4[3];
	uint16_t threshold_offset0;
	uint16_t threshold_slope0;
	uint16_t scale0;
	uint16_t pad5;
	uint16_t threshold_offset1;
	uint16_t threshold_slope1;
	uint16_t scale1;
	uint16_t pad6;
	uint16_t threshold_offset2;
	uint16_t threshold_slope2;
	uint16_t scale2;
	uint16_t pad7;
	uint16_t threshold_offset3;
	uint16_t threshold_slope3;
	uint16_t scale3;
	uint16_t pad8;
	uint16_t threshold_offset4;
	uint16_t threshold_slope4;
	uint16_t scale4;
	uint16_t pad9;
	uint16_t positive_strength;
	uint16_t positive_pre_limit;
	uint16_t positive_func[PISP_BE_SHARPEN_FUNC_NUM_POINTS];
	uint16_t positive_limit;
	uint16_t negative_strength;
	uint16_t negative_pre_limit;
	uint16_t negative_func[PISP_BE_SHARPEN_FUNC_NUM_POINTS];
	uint16_t negative_limit;
	uint8_t enables;
	uint8_t white;
	uint8_t black;
	uint8_t grey;
} pisp_be_sharpen_config;

typedef struct {
	uint8_t y_factor;
	uint8_t c1_factor;
	uint8_t c2_factor;
	uint8_t pad;
} pisp_be_sh_fc_combine_config;

#define PISP_BE_GAMMA_LUT_SIZE 64

typedef struct {
	uint32_t lut[PISP_BE_GAMMA_LUT_SIZE];
} pisp_be_gamma_config;

typedef struct {
	uint16_t offset_x, offset_y;
	uint16_t width, height;
} pisp_be_crop_config;

#define PISP_BE_RESAMPLE_FILTER_SIZE 96

typedef struct {
	uint16_t scale_factor_h, scale_factor_v;
	int16_t coef[PISP_BE_RESAMPLE_FILTER_SIZE];
} pisp_be_resample_config;

typedef struct {
	uint16_t scaled_width;
	uint16_t scaled_height;
	int16_t initial_phase_h[3];
	int16_t initial_phase_v[3];
} pisp_be_resample_extra;

typedef struct {
	uint16_t scale_factor_h;
	uint16_t scale_factor_v;
	uint16_t scale_recip_h;
	uint16_t scale_recip_v;
} pisp_be_downscale_config;

typedef struct {
	uint16_t scaled_width;
	uint16_t scaled_height;
} pisp_be_downscale_extra;

typedef struct {
	uint8_t compute_signed;
	uint8_t channel_mix[3];
	uint32_t stride;
} pisp_be_hog_config;

typedef struct {
	uint8_t r_qos; /* Read QoS */
	uint8_t r_cache_prot; /* Read { prot[2:0], cache[3:0] } */
	uint8_t w_qos; /* Write QoS */
	uint8_t w_cache_prot; /* Write { prot[2:0], cache[3:0] } */
} pisp_be_axi_config;

typedef enum {
	PISP_BE_TRANSFORM_NONE = 0x0,
	PISP_BE_TRANSFORM_HFLIP = 0x1,
	PISP_BE_TRANSFORM_VFLIP = 0x2,
	PISP_BE_TRANSFORM_ROT180 =
		(PISP_BE_TRANSFORM_HFLIP | PISP_BE_TRANSFORM_VFLIP)
} pisp_be_transform;

typedef struct {
	pisp_image_format_config image;
	uint8_t transform;
	uint8_t pad[3];
	uint16_t lo;
	uint16_t hi;
	uint16_t lo2;
	uint16_t hi2;
} pisp_be_output_format_config;

typedef struct {
	/* low 32 bits followed by high 32 bits (for each of 3 planes) */
	uint32_t addr[3][2];
} pisp_be_output_buffer_config;

typedef struct {
	/* low 32 bits followed by high 32 bits */
	uint32_t addr[2];
} pisp_be_hog_buffer_config;

typedef struct {
	/* For backward compatibility */
	uint8_t pad0[112];
	/* Processing configuration: */
	pisp_be_global_config global;
	pisp_image_format_config input_format;
	pisp_decompress_config decompress;
	pisp_be_dpc_config dpc;
	pisp_be_geq_config geq;
	pisp_image_format_config tdn_input_format;
	pisp_decompress_config tdn_decompress;
	pisp_be_tdn_config tdn;
	pisp_compress_config tdn_compress;
	pisp_image_format_config tdn_output_format;
	pisp_be_sdn_config sdn;
	pisp_bla_config blc;
	pisp_compress_config stitch_compress;
	pisp_image_format_config stitch_output_format;
	pisp_image_format_config stitch_input_format;
	pisp_decompress_config stitch_decompress;
	pisp_be_stitch_config stitch;
	pisp_be_lsc_config lsc;
	pisp_wbg_config wbg;
	pisp_be_cdn_config cdn;
	pisp_be_cac_config cac;
	pisp_be_debin_config debin;
	pisp_be_tonemap_config tonemap;
	pisp_be_demosaic_config demosaic;
	pisp_be_ccm_config ccm;
	pisp_be_sat_control_config sat_control;
	pisp_be_ccm_config ycbcr;
	pisp_be_sharpen_config sharpen;
	pisp_be_false_colour_config false_colour;
	pisp_be_sh_fc_combine_config sh_fc_combine;
	pisp_be_ccm_config ycbcr_inverse;
	pisp_be_gamma_config gamma;
	pisp_be_ccm_config csc[PISP_BACK_END_NUM_OUTPUTS];
	pisp_be_downscale_config downscale[PISP_BACK_END_NUM_OUTPUTS];
	pisp_be_resample_config resample[PISP_BACK_END_NUM_OUTPUTS];
	pisp_be_output_format_config
				output_format[PISP_BACK_END_NUM_OUTPUTS];
	pisp_be_hog_config hog;
	pisp_be_axi_config axi;
	/* For backward compatibility */
	uint8_t pad1[84];
} pisp_be_config;

static_assert(sizeof(pisp_be_config) == 6476, "pisp_be_config not packed as expected");

/*
 * We also need a tile structure to describe the size of the tiles going
 * through the pipeline.
 */

typedef enum {
	PISP_LEFT_EDGE = (1 << 0),
	PISP_RIGHT_EDGE = (1 << 1),
	PISP_TOP_EDGE = (1 << 2),
	PISP_BOTTOM_EDGE = (1 << 3)
} pisp_tile_edge;

typedef struct {
	uint8_t edge; // enum pisp_tile_edge
	uint8_t pad0[3];
	// 4 bytes
	uint32_t input_addr_offset;
	uint32_t input_addr_offset2;
	uint16_t input_offset_x;
	uint16_t input_offset_y;
	uint16_t input_width;
	uint16_t input_height;
	// 20 bytes
	uint32_t tdn_input_addr_offset;
	uint32_t tdn_output_addr_offset;
	uint32_t stitch_input_addr_offset;
	uint32_t stitch_output_addr_offset;
	// 36 bytes
	uint32_t lsc_grid_offset_x;
	uint32_t lsc_grid_offset_y;
	// 44 bytes
	uint32_t cac_grid_offset_x;
	uint32_t cac_grid_offset_y;
	// 52 bytes
	uint16_t crop_x_start[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t crop_x_end[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t crop_y_start[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t crop_y_end[PISP_BACK_END_NUM_OUTPUTS];
	// 68 bytes
	/* Ordering is planes then branches */
	uint16_t downscale_phase_x[3 * PISP_BACK_END_NUM_OUTPUTS];
	uint16_t downscale_phase_y[3 * PISP_BACK_END_NUM_OUTPUTS];
	// 92 bytes
	uint16_t resample_in_width[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t resample_in_height[PISP_BACK_END_NUM_OUTPUTS];
	// 100 bytes
	/* Ordering is planes then branches */
	uint16_t resample_phase_x[3 * PISP_BACK_END_NUM_OUTPUTS];
	uint16_t resample_phase_y[3 * PISP_BACK_END_NUM_OUTPUTS];
	// 124 bytes
	uint16_t output_offset_x[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t output_offset_y[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t output_width[PISP_BACK_END_NUM_OUTPUTS];
	uint16_t output_height[PISP_BACK_END_NUM_OUTPUTS];
	// 140 bytes
	uint32_t output_addr_offset[PISP_BACK_END_NUM_OUTPUTS];
	uint32_t output_addr_offset2[PISP_BACK_END_NUM_OUTPUTS];
	// 156 bytes
	uint32_t output_hog_addr_offset;
	// 160 bytes
} pisp_tile;

static_assert(sizeof(pisp_tile) == 160, "pisp_tile not packed as expected");

typedef struct {
	pisp_be_config config;
	pisp_tile tiles[PISP_BACK_END_NUM_TILES];
	int num_tiles;
} pisp_be_tiles_config;

#endif /* _PISP_BE_CONFIG_H_ */
