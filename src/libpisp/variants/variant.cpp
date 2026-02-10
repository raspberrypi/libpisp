/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_variantPiSP variant configuration definitions
 */

#include "variant.hpp"

#include <algorithm>

namespace libpisp
{

// Must make sure the .id fields are incremented correctly below!

const PiSPVariant BCM2712_C0(
	"BCM2712_C0",						// name
	0x0001,								// id
	0x00114666,							// fe_version (front-end version)
	0x02252700,							// be_version (back-end version)
	2,									// num_fe (number of front-ends)
	1,									// num_be (number of back-ends)
	{ 2, 2 },							// num_fe_branches (branches per front-end)
	{ 6144, 6144 },						// fe_stats_max_width (max stats width per front-end)
	{ { { true, true }, { true, true } } },	// fe_downscaler (downscaler available per FE/branch)
	{ { { 6144, 4096 }, { 6144, 4096 } } },	// fe_downscaler_max_width (max width per FE/branch)
	640,								// be_max_tile_width (max tile width for back-end)
	{ 2 },								// num_be_branches (branches per back-end)
	{ { { false, false } } },			// be_integral_images (integral images available per BE/branch)
	{ { { false, true } } },			// be_downscaler (downscaler available per BE/branch)
	false								// be_rgb32_support (RGB32 format support)
);

const PiSPVariant BCM2712_D0(
	"BCM2712_D0",						// name
	0x0002,								// id
	0x00114666,							// fe_version (front-end version)
	0x02252701,							// be_version (back-end version)
	2,									// num_fe (number of front-ends)
	1,									// num_be (number of back-ends)
	{ 2, 2 },							// num_fe_branches (branches per front-end)
	{ 6144, 6144 },						// fe_stats_max_width (max stats width per front-end)
	{ { { true, true }, { true, true } } },	// fe_downscaler (downscaler available per FE/branch)
	{ { { 6144, 4096 }, { 6144, 4096 } } },	// fe_downscaler_max_width (max width per FE/branch)
	640,								// be_max_tile_width (max tile width for back-end)
	{ 2 },								// num_be_branches (branches per back-end)
	{ { { false, false } } },			// be_integral_images (integral images available per BE/branch)
	{ { { false, true } } },			// be_downscaler (downscaler available per BE/branch)
	true								// be_rgb32_support (RGB32 format support)
);

const PiSPVariant BERYL(
	"BERYL",							// name
	0x0003,								// id
	0x00114666,							// fe_version (front-end version)
	0x00352320,							// be_version (back-end version)
	2,									// num_fe (number of front-ends)
	1,									// num_be (number of back-ends)
	{ 2, 2 },							// num_fe_branches (branches per front-end)
	{ 6144, 6144 },						// fe_stats_max_width (max stats width per front-end)
	{ { { true, true }, { true, true } } },  // fe_downscaler (downscaler available per FE/branch)
	{ { { 6144, 4096 }, { 6144, 4096 } } }, // fe_downscaler_max_width (max width per FE/branch)
	1024,								// be_max_tile_width (max tile width for back-end)
	{ 2 },								// num_be_branches (branches per back-end)
	{ { { false, false } } },			// be_integral_images (integral images available per BE/branch)
	{ { { true, true } } },				// be_downscaler (downscaler available per BE/branch)
	true								// be_rgb32_support (RGB32 format support)
);

const std::vector<PiSPVariant> &get_variants()
{
	static const std::vector<PiSPVariant> variants {
		BCM2712_C0,
		BCM2712_D0,
		BERYL,
	};

	return variants;
}

std::optional<PiSPVariant> get_variant(unsigned int fe_version, unsigned int be_version)
{
	const std::vector<PiSPVariant> &variants = get_variants();

	auto it = std::find_if(variants.begin(), variants.end(),
						   [fe_version, be_version](const auto &hw)
						   {
								return hw.front_end_version() == fe_version && hw.back_end_version() == be_version;
						   });

	if (it == variants.end())
		return {};

	return *it;
}

} // namespace libpisp
