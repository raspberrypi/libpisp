/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_utils.hpp - PiSP buffer helper utilities
 */
#pragma once

#include "pisp_common.h"

namespace PiSP
{

void compute_stride(pisp_image_format_config &config);
void compute_stride_align(pisp_image_format_config &config, int align);
void compute_addr_offset(const pisp_image_format_config &config, int x, int y, uint32_t *addr_offset,
						 uint32_t *addr_offset2);
int num_planes(pisp_image_format format);
std::size_t get_plane_size(const pisp_image_format_config &config, int plane);

} // namespace PiSP
