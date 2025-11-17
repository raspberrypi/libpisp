/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>

#include "bitfield.hpp"
#include "pisp_be_config.h"

namespace libpisp
{

enum class RgbEnableValues : uint32_t {
	None = 		0,
	Input = 		(1 << 0),
	Dedistort = 	(1 << 1),
	Ccm = 			(1 << 2),
	Sat_control = 	(1 << 3),
	Ycbcr = 		(1 << 4),
	False_colour = (1 << 5),
	Sharpen = 		(1 << 6),
	Ycbcr_inverse = (1 << 7),
	Pre_csc0 = 	(1 << 8),
	Pre_csc1 = 	(1 << 9),
	Gamma0 = 		(1 << 10),
	Gamma1 = 		(1 << 11),
	Post_csc0 = 	(1 << 12),
	Post_csc1 = 	(1 << 13),
	Downscale0 = 	(1 << 14),
	Downscale1 = 	(1 << 15),
	Resample0 = 	(1 << 16),
	Resample1 = 	(1 << 17),
	Output0 = 		(1 << 18),
	Output1 = 		(1 << 19),
	Compress0 = 	(1 << 20),
	Compress1 = 	(1 << 21),
};

class RgbEnables : public Bitfield<RgbEnableValues>
{
public:
	using Bitfield::Bitfield; // inherit constructors	
};

enum class BayerEnableValues: uint32_t {
	None =			0,
	Input = 			(1 << 0),
	Decompress = 		(1 << 1),
	Dpc = 				(1 << 2),
	Geq = 				(1 << 3),
	Tdn_input = 		(1 << 4),
	Tdn_decompress = 	(1 << 5),
	Tdn = 				(1 << 6),
	Tdn_compress = 		(1 << 7),
	Tdn_output = 		(1 << 8),
	Sdn = 				(1 << 9),
	Blc = 				(1 << 10),
	Stitch_input = 		(1 << 11),
	Stitch_decompress = (1 << 12),
	Stitch = 			(1 << 13),
	Stitch_compress = 	(1 << 14),
	Stitch_output = 	(1 << 15),
	Wbg = 				(1 << 16),
	Cdn = 				(1 << 17),
	Lsc = 				(1 << 18),
	Tonemap = 			(1 << 19),
	Cac = 				(1 << 20),
	Debin = 			(1 << 21),
	Demosaic = 			(1 << 22),
	Tonemap_input = 	(1 << 23),
	Transpose = 		(1 << 24),
};

class BayerEnables : public Bitfield<BayerEnableValues>
{
public:
	using Bitfield::Bitfield; // inherit constructors
};

class BackEnd;

class BackendConfigAdapter
{
public:
	BackendConfigAdapter(BackEnd *be)
		: be_ (be)
	{
	}

	struct Global
	{
		BayerEnables bayer_enables;
		RgbEnables rgb_enables;
		uint8_t bayer_order;
		BackendConfigAdapter const *adapter;
	};

	uint32_t RgbEnablesToHw(RgbEnables rgb_enables) const;
	RgbEnables HwToRgbEnables(uint32_t hw_enables) const;
	uint32_t BayerEnablesToHw(BayerEnables bayer_enables) const;
	BayerEnables HwToBayerEnables(uint32_t hw_enables) const;
	Global HwtoGlobal(const pisp_be_global_config &hw_global) const;
	pisp_be_global_config GlobaltoHw(const Global &global) const;

private:
	BackEnd *be_;
};

} // namespace libpisp

