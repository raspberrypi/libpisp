/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * utils.hpp - PiSP buffer helper utilities
 */
#pragma once

#include <cstdint>
#include <string>

#include "pisp_common.h"

namespace libpisp
{

void computeStride(pisp_image_format_config &config, bool preserveSubsampleRatio = false);
void computeOptimalStride(pisp_image_format_config &config);
void computeOptimalStride(pisp_image_format_config &config, bool preserveSubsampleRatio);
void computeStrideAlign(pisp_image_format_config &config, int align, bool preserveSubsampleRatio = false);
void computeAddrOffset(const pisp_image_format_config &config, int x, int y, uint32_t *addrOffset,
						 uint32_t *addrOffset2);
int numPlanes(pisp_image_format format);
std::size_t getPlaneSize(const pisp_image_format_config &config, int plane);
uint32_t getPispImageFormat(const std::string &format);
std::string getPispImageFormat(uint32_t format);

} // namespace libpisp
