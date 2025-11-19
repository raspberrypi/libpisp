/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <variant>

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
	SatControl		= 0x00000008,
	Ycbcr			= 0x00000010,
	FalseColour		= 0x00000020,
	Sharpen			= 0x00000040,
	YcbcrInverse	= 0x00000080,
	PreCsc0			= 0x00000400,
	PreCsc1			= 0x00000800,
	Gamma0			= 0x00001000,
	Gamma1			= 0x00002000,
	PostCsc0		= 0x00004000,
	PostCsc1		= 0x00008000,
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
	TdnInput		= 0x00000010,
	TdnDecompress	= 0x00000020,
	Tdn				= 0x00000040,
	TdnCompress		= 0x00000080,
	TdnOutput		= 0x00000100,
	Sdn				= 0x00000200,
	Blc				= 0x00000400,
	StitchInput		= 0x00000800,
	StitchDecompress = 0x00001000,
	Stitch			= 0x00002000,
	StitchCompress	= 0x00004000,
	StitchOutput	= 0x00008000,
	Wbg				= 0x00010000,
	Cdn				= 0x00020000,
	Lsc				= 0x00040000,
	Tonemap			= 0x00080000,
	Cac				= 0x00100000,
	Debin			= 0x00200000,
	Demosaic		= 0x00400000,
	TonemapInput	= 0x00800000,
	Transpose		= 0x01000000,
};

CREATE_BITFIELD(BayerEnables, BayerEnable);

enum class DirtyFlag : uint32_t
{
	None		= 0x00000000,
	Crop		= 0x00000001,
	Global		= 0x00000002,
	ShFcCombine	= 0x00000004,
};

CREATE_BITFIELD(DirtyFlags, DirtyFlag);

template <typename T>
concept EnumType = requires {
	requires std::is_enum_v<T>;
};

template <EnumType T>
T getFlagForBranch(const T field, unsigned int branch)
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

	struct Config
	{
		std::variant<pisp_be_config *, pisp_be_config2 *> config;

		struct Pointer
		{
			void *ptr;
			// Called by -> on the helper
			template <typename T>
			operator T *() const
			{
				return static_cast<T *>(ptr);
			}
		};

		struct Proxy
		{
			std::variant<pisp_be_config *, pisp_be_config2 *> *variant;

			// First operator-> returns a helper object
			auto operator->() const
			{
				return std::visit([](auto *ptr) { return Pointer { ptr }; }, *variant);
			}
		};

		// Entry point for ->
		Proxy operator->() { return Proxy { &config }; }
	};

	struct Global
	{
		BayerEnables bayerEnables;
		RgbEnables rgbEnables;
		uint8_t bayerOrder;
		BackendConfigAdapter const *adapter;
	};

	uint32_t rgbEnablesToHw(RgbEnables rgbEnables) const;
	RgbEnables hwToRgbEnables(uint32_t hwEnables) const;
	uint32_t bayerEnablesToHw(BayerEnables bayerEnables) const;
	BayerEnables hwToBayerEnables(uint32_t hwEnables) const;
	Global hwToGlobal(const pisp_be_global_config &hwGlobal) const;
	pisp_be_global_config globalToHw(const Global &global) const;

private:
	BackEnd *be_;
};

} // namespace libpisp

