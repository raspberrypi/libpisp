/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_common.h - Raspberry Pi PiSP common configuration definitions
 */
#ifndef _PISP_COMMON_H_
#define _PISP_COMMON_H_

#include <linux/types.h>

#include "pisp_types.h"

typedef struct {
	uint16_t black_level_r;
	uint16_t black_level_gr;
	uint16_t black_level_gb;
	uint16_t black_level_b;
	uint16_t output_black_level;
	uint8_t pad[2];
} pisp_bla_config;

typedef struct {
	uint16_t gain_r;
	uint16_t gain_g;
	uint16_t gain_b;
	uint8_t pad[2];
} pisp_wbg_config;

typedef struct {
	/* value subtracted from incoming data */
	uint16_t offset;
	uint8_t pad;
	/* 1 => Companding; 2 => Delta (recommended); 3 => Combined (for HDR) */
	uint8_t mode;
} pisp_compress_config;

typedef struct {
	/* value added to reconstructed data */
	uint16_t offset;
	uint8_t pad;
	/* 1 => Companding; 2 => Delta (recommended); 3 => Combined (for HDR) */
	uint8_t mode;
} pisp_decompress_config;

typedef enum {
	/* round down bursts to end at a 32-byte boundary, to align following bursts */
	PISP_AXI_FLAG_ALIGN = 128,
	 /* for FE writer: force WSTRB high, to pad output to 16-byte boundary */
	PISP_AXI_FLAG_PAD = 64,
	/* for FE writer: Use Output FIFO level to trigger "panic" */
	PISP_AXI_FLAG_PANIC = 32
} pisp_axi_flags;

typedef struct {
	/* burst length minus one, which must be in the range 0:15; OR'd with flags */
	uint8_t maxlen_flags;
	/* { prot[2:0], cache[3:0] } fields, echoed on AXI bus */
	uint8_t cache_prot;
	/* QoS field(s) (4x4 bits for FE writer; 4 bits for other masters) */
	uint16_t qos;
} pisp_axi_config;

/* The metadata format identifier for BE configuration buffers. */
#define V4L2_META_FMT_RPI_BE_CFG v4l2_fourcc('R', 'P', 'B', 'C')

/* The metadata format identifier for FE configuration buffers. */
#define V4L2_META_FMT_RPI_FE_CFG v4l2_fourcc('R', 'P', 'F', 'C')

/* The metadata format identifier for FE configuration buffers. */
#define V4L2_META_FMT_RPI_FE_STATS v4l2_fourcc('R', 'P', 'F', 'S')

#endif /* _PISP_COMMON_H_ */
