/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>

#include "pisp_be_config.h"

namespace libpisp
{

enum RgbEnables {
	RgbEnableInput = 		(1 << 0),
	RgbEnableDedistort = 	(1 << 1),
	RgbEnableCcm = 			(1 << 2),
	RgbEnableSat_control = 	(1 << 3),
	RgbEnableYcbcr = 		(1 << 4),
	RgbEnableFalse_colour = (1 << 5),
	RgbEnableSharpen = 		(1 << 6),
	RgbEnableYcbcr_inverse = (1 << 7),
	RgbEnablePre_csc0 = 	(1 << 8),
	RgbEnablePre_csc1 = 	(1 << 9),
	RgbEnableGamma0 = 		(1 << 10),
	RgbEnableGamma1 = 		(1 << 11),
	RgbEnablePost_csc0 = 	(1 << 12),
	RgbEnablePost_csc1 = 	(1 << 13),
	RgbEnableDownscale0 = 	(1 << 14),
	RgbEnableDownscale1 = 	(1 << 15),
	RgbEnableResample0 = 	(1 << 16),
	RgbEnableResample1 = 	(1 << 17),
	RgbEnableOutput0 = 		(1 << 18),
	RgbEnableOutput1 = 		(1 << 19),
	RgbEnableCompress0 = 	(1 << 20),
	RgbEnableCompress1 = 	(1 << 21),
};

enum BayerEnables {
	BayerEnableInput = 			(1 << 0),
	BayerEnableDecompress = 		(1 << 1),
	BayerEnableDpc = 				(1 << 2),
	BayerEnableGeq = 				(1 << 3),
	BayerEnableTdn_input = 		(1 << 4),
	BayerEnableTdn_decompress = 	(1 << 5),
	BayerEnableTdn = 				(1 << 6),
	BayerEnableTdn_compress = 		(1 << 7),
	BayerEnableTdn_output = 		(1 << 8),
	BayerEnableSdn = 				(1 << 9),
	BayerEnableBlc = 				(1 << 10),
	BayerEnableStitch_input = 		(1 << 11),
	BayerEnableStitch_decompress = (1 << 12),
	BayerEnableStitch = 			(1 << 13),
	BayerEnableStitch_compress = 	(1 << 14),
	BayerEnableStitch_output = 	(1 << 15),
	BayerEnableWbg = 				(1 << 16),
	BayerEnableCdn = 				(1 << 17),
	BayerEnableLsc = 				(1 << 18),
	BayerEnableTonemap = 			(1 << 19),
	BayerEnableCac = 				(1 << 20),
	BayerEnableDebin = 			(1 << 21),
	BayerEnableDemosaic = 			(1 << 22),
	BayerEnableTonemap_input = 	(1 << 23),
	BayerEnableTranspose = 		(1 << 24),
};

class BackEnd;

class BackendConfigAdapter
{
public:
	BackendConfigAdapter(BackEnd *be)
		: be_ (be)
	{
	}

	uint32_t RgbEnablesToHw(RgbEnables rgb_enables) const;
	uint32_t BayerEnablesToHw(BayerEnables bayer_enables) const;

	struct Global
	{
		BayerEnables bayer_enables;
		RgbEnables rgb_enables;
		uint8_t bayer_order;

		operator pisp_be_global_config() const;
	};

private:
	BackEnd *be_;
};

} // namespace libpisp

