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

const PiSPVariant BCM2712_C0 {
	.name_ = "BCM2712_C0",
	.fe_version_ = 0x00114666,
	.be_version_ = 0x02252700,
	.num_fe_ = 2,
	.num_be_ = 1,
	.num_fe_branches_ = { 2, 2 },
	.fe_stats_max_width_ = { 6144, 6144 },
	.fe_downscaler_ = { { { true, true }, { true, true } } },
	.fe_downscaler_max_width_ = { { { 6144, 4096 }, { 6144, 4096 } } },
	.be_max_tile_width_ = 640,
	.num_be_branches_ = { 2 },
	.be_integral_images_ = { { { false, false } } },
	.be_downscaler_ = { { { false, true } } },
	.be_rgb32_support_ = false,
};

const PiSPVariant BCM2712_D0 {
	.name_ = "BCM2712_D0",
	.fe_version_ = 0x00114666,
	.be_version_ = 0x02252701,
	.num_fe_ = 2,
	.num_be_ = 1,
	.num_fe_branches_ = { 2, 2 },
	.fe_stats_max_width_ = { 6144, 6144 },
	.fe_downscaler_ = { { { true, true }, { true, true } } },
	.fe_downscaler_max_width_ = { { { 6144, 4096 }, { 6144, 4096 } } },
	.be_max_tile_width_ = 640,
	.num_be_branches_ = { 2 },
	.be_integral_images_ = { { { false, false } } },
	.be_downscaler_ = { { { false, true } } },
	.be_rgb32_support_ = true,
};

const std::vector<PiSPVariant> &get_variants()
{
	static const std::vector<PiSPVariant> variants {
		BCM2712_C0,
		BCM2712_D0
	};

	return variants;
}

std::optional<PiSPVariant> get_variant(unsigned int fe_version, unsigned int be_version)
{
	const std::vector<PiSPVariant> &variants = get_variants();

	auto it = std::find_if(variants.begin(), variants.end(),
				   [fe_version, be_version](const auto &hw) { return hw.FrontEndVersion() == fe_version &&
																	 hw.BackEndVersion() == be_version; });

	if (it == variants.end())
		return {};

	return *it;
}

} // namespace libpisp
