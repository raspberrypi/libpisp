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

const PiSPVariant BCM2712_C0 {
	.name = "BCM2712_C0",
	.id = 0x0001,
	.fe_version = 0x00114666,
	.be_version = 0x02252700,
	.num_fe = 2,
	.num_be = 1,
	.num_fe_branches = { 2, 2 },
	.fe_stats_max_width = { 6144, 6144 },
	.fe_downscaler = { { { true, true }, { true, true } } },
	.fe_downscaler_max_width = { { { 6144, 4096 }, { 6144, 4096 } } },
	.be_max_tile_width = 640,
	.num_be_branches = { 2 },
	.be_integral_images = { { { false, false } } },
	.be_downscaler = { { { false, true } } },
	.be_rgb32_support = false,
};

const PiSPVariant BCM2712_D0 {
	.name = "BCM2712_D0",
	.id = 0x0002,	
	.fe_version = 0x00114666,
	.be_version = 0x02252701,
	.num_fe = 2,
	.num_be = 1,
	.num_fe_branches = { 2, 2 },
	.fe_stats_max_width = { 6144, 6144 },
	.fe_downscaler = { { { true, true }, { true, true } } },
	.fe_downscaler_max_width = { { { 6144, 4096 }, { 6144, 4096 } } },
	.be_max_tile_width = 640,
	.num_be_branches = { 2 },
	.be_integral_images = { { { false, false } } },
	.be_downscaler = { { { false, true } } },
	.be_rgb32_support = true,
};

const PiSPVariant BERYL {
	.name = "BERYL",
	.id = 0x0003,
	.fe_version = 0x00114666,
	.be_version = 0x00352320,
	.num_fe = 2,
	.num_be = 1,
	.num_fe_branches = { 2, 2 },
	.fe_stats_max_width = { 6144, 6144 },
	.fe_downscaler = { { { true, true }, { true, true } } },
	.fe_downscaler_max_width = { { { 6144, 4096 }, { 6144, 4096 } } },
	.be_max_tile_width = 9126,
	.num_be_branches = { 2 },
	.be_integral_images = { { { false, false } } },
	.be_downscaler = { { { true, true } } },
	.be_rgb32_support = true,
};

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
				   [fe_version, be_version](const auto &hw) { return hw.front_end_version() == fe_version &&
																	 hw.back_end_version() == be_version; });

	if (it == variants.end())
		return {};

	return *it;
}

} // namespace libpisp
