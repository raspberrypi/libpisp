/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>
#include <type_traits>

#include "bitfield.hpp"
#include "pisp_be_config.h"

namespace libpisp
{

enum class RgbEnable : uint32_t
{
	None			= 0x00000000,
	Input			= 0x00000001,
	Dedistort		= 0x00000002,
	Ccm				= 0x00000004,
	Sat_control		= 0x00000008,
	Ycbcr			= 0x00000010,
	False_colour	= 0x00000020,
	Sharpen			= 0x00000040,
	Ycbcr_inverse	= 0x00000080,
	Pre_csc0		= 0x00000400,
	Pre_csc1		= 0x00000800,
	Gamma0			= 0x00001000,
	Gamma1			= 0x00002000,
	Post_csc0		= 0x00004000,
	Post_csc1		= 0x00008000,
	Downscale0		= 0x00010000,
	Downscale1		= 0x00020000,
	Resample0		= 0x00040000,
	Resample1		= 0x00080000,
	Output0			= 0x00100000,
	Output1			= 0x00200000,
	Compress0		= 0x00400000,
	Compress1		= 0x00800000,
};

CREATE_BITFIELD(RgbEnables, RgbEnable);

enum class BayerEnable : uint32_t
{
	None 			= 0x00000000,
	Input			= 0x00000001,
	Decompress		= 0x00000002,
	Dpc				= 0x00000004,
	Geq				= 0x00000008,
	Tdn_input		= 0x00000010,
	Tdn_decompress	= 0x00000020,
	Tdn				= 0x00000040,
	Tdn_compress	= 0x00000080,
	Tdn_output		= 0x00000100,
	Sdn				= 0x00000200,
	Blc				= 0x00000400,
	Stitch_input	= 0x00000800,
	Stitch_decompress = 0x00001000,
	Stitch			= 0x00002000,
	Stitch_compress	= 0x00004000,
	Stitch_output	= 0x00008000,
	Wbg				= 0x00010000,
	Cdn				= 0x00020000,
	Lsc				= 0x00040000,
	Tonemap			= 0x00080000,
	Cac				= 0x00100000,
	Debin			= 0x00200000,
	Demosaic		= 0x00400000,
	Tonemap_input	= 0x00800000,
	Transpose		= 0x01000000,
};

CREATE_BITFIELD(BayerEnables, BayerEnable);

enum class DirtyFlag : uint32_t
{
	None = 0,
	Crop = (1 << 0),
	Global = (1 << 1),
	SH_FC_Combine = (1 << 2),
};

CREATE_BITFIELD(DirtyFlags, DirtyFlag);

template <typename T>
concept EnumType = requires {
	requires std::is_enum_v<T>;
};

template <EnumType T>
T get_flag_for_branch(const T field, unsigned int branch)
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(field) << branch);
}

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

