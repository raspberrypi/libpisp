/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 *
 * config_types.hpp - Common type aliases for PiSP configuration structures
 */
#pragma once

#include "pisp_common.h"

namespace libpisp
{

// Shared type aliases used by both frontend and backend
using ImageFormatConfig = struct pisp_image_format_config;
using DecompressConfig = struct pisp_decompress_config;
using CompressConfig = struct pisp_compress_config;
using BlaConfig = struct pisp_bla_config;

} // namespace libpisp
