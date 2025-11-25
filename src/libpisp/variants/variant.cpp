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
	.feVersion = 0x00114666,
	.beVersion = 0x02252700,
	.numFe = 2,
	.numBe = 1,
	.numFeBranches = { 2, 2 },
	.feStatsMaxWidth = { 6144, 6144 },
	.feDownscaler = { { { true, true }, { true, true } } },
	.feDownscalerMaxWidth = { { { 6144, 4096 }, { 6144, 4096 } } },
	.beMaxTileWidth = 640,
	.numBeBranches = { 2 },
	.beIntegralImages = { { { false, false } } },
	.beDownscaler = { { { false, true } } },
	.beRgb32Support = false,
};

const PiSPVariant BCM2712_D0 {
	.name = "BCM2712_D0",
	.id = 0x0002,	
	.feVersion = 0x00114666,
	.beVersion = 0x02252701,
	.numFe = 2,
	.numBe = 1,
	.numFeBranches = { 2, 2 },
	.feStatsMaxWidth = { 6144, 6144 },
	.feDownscaler = { { { true, true }, { true, true } } },
	.feDownscalerMaxWidth = { { { 6144, 4096 }, { 6144, 4096 } } },
	.beMaxTileWidth = 640,
	.numBeBranches = { 2 },
	.beIntegralImages = { { { false, false } } },
	.beDownscaler = { { { false, true } } },
	.beRgb32Support = true,
};

const std::vector<PiSPVariant> &getVariants()
{
	static const std::vector<PiSPVariant> variants {
		BCM2712_C0,
		BCM2712_D0
	};

	return variants;
}

std::optional<PiSPVariant> getVariant(unsigned int feVersion, unsigned int beVersion)
{
	const std::vector<PiSPVariant> &variants = getVariants();

	auto it = std::find_if(variants.begin(), variants.end(),
				   [feVersion, beVersion](const auto &hw) { return hw.frontEndVersion() == feVersion &&
																	 hw.backEndVersion() == beVersion; });

	if (it == variants.end())
		return {};

	return *it;
}

} // namespace libpisp
