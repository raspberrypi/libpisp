/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * utils.hpp - PiSP buffer helper utilities
 */
#pragma once

#include <cstdint>
#include <string>

#include "config_types.hpp"

namespace libpisp
{

void compute_stride(ImageFormatConfig &config, bool preserve_subsample_ratio = false);
void compute_optimal_stride(ImageFormatConfig &config);
void compute_optimal_stride(ImageFormatConfig &config, bool preserve_subsample_ratio);
void compute_stride_align(ImageFormatConfig &config, int align, bool preserve_subsample_ratio = false);
void compute_addr_offset(const ImageFormatConfig &config, int x, int y, uint32_t *addr_offset,
						 uint32_t *addr_offset2);
int num_planes(pisp_image_format format);
std::size_t get_plane_size(const ImageFormatConfig &config, int plane);
uint32_t get_pisp_image_format(const std::string &format);
std::string get_pisp_image_format(uint32_t format);

} // namespace libpisp
