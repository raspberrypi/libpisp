/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * bcm2712.cpp - BCM2712 PiSP variant configuration
 */
#include "variant.hpp"

namespace libpisp {

const PiSPVariant BCM2712_C0 {
	.version_ = 0x02252700,
	.numFrontEnds_ = 2,
	.numBackEnds_ = 1,
	.numFrontEndBranches_ = { 2, 2 },
	.frontEndMaxWidth_ = { 6144, 6144 },
	.frontEndDownscaler_ = {{ { true, true }, { true, true } }},
	.frontEndDownscalerMaxWidth_ = {{ { 6144, 4096 }, { 6144, 4096 } }},
	.backEndMaxTileWidth_ = 640,
	.numBackEndBranches_ = { 2 },
	.backEndIntegralImage_ = {{ { false, false } }},
	.backEndDownscaler_ = {{ { true, true } }}
};

const PiSPVariant BCM2712_D0 {
	.version_ = 0x02251301,
	.numFrontEnds_ = 2,
	.numBackEnds_ = 1,
	.numFrontEndBranches_ = { 2, 2 },
	.frontEndMaxWidth_ = { 6144, 6144 },
	.frontEndDownscaler_ = {{ { true, true }, { true, true } }},
	.frontEndDownscalerMaxWidth_ = {{ { 6144, 4096 }, { 6144, 4096 } }},
	.backEndMaxTileWidth_ = 320,
	.numBackEndBranches_ = { 2 },
	.backEndIntegralImage_ = {{ { false, false } }},
	.backEndDownscaler_ = {{ { false, true } }}
};

} // namespace libpisp
