/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_variant.cpp - PiSP variant configuration definitions
 */
#pragma once

#include <array>

// This header files defines functions that can be called to discover the details about the specific
// implementation of the PiSP that is being used. Whatever the build system being used is, it needs
// to select the correct C file in this directory that implements these functions.

namespace libpisp
{

struct PiSPVariant
{
	static constexpr unsigned int MaxFrontEnds = 4;
	static constexpr unsigned int MaxBackEnds = 4;
	static constexpr unsigned int MaxFrontEndBranchs = 4;
	static constexpr unsigned int MaxBackEndBranches = 4;

	unsigned int version_;
	unsigned int numFrontEnds_;
	unsigned int numBackEnds_;
	std::array<unsigned int, MaxFrontEnds> numFrontEndBranches_;
	std::array<unsigned int, MaxFrontEnds> frontEndMaxWidth_;
	std::array<std::array<bool, MaxFrontEndBranchs>, MaxFrontEnds> frontEndDownscaler_;
	std::array<std::array<unsigned int, MaxFrontEndBranchs>, MaxFrontEnds> frontEndDownscalerMaxWidth_;
	unsigned int backEndMaxTileWidth_;
	std::array<unsigned int, MaxBackEnds> numBackEndBranches_;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> backEndIntegralImage_;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> backEndDownscaler_;

	unsigned int version() const
	{
		return version_;
	}

	unsigned int numFrontEnds() const
	{
		return numFrontEnds_;
	}

	unsigned int numBackEnds() const
	{
		return numBackEnds_;
	}

	unsigned int frontEndNumBranches(unsigned int id) const
	{
		return id < numFrontEnds_ ? numFrontEndBranches_[id] : 0;
	}

	unsigned int frontEndMaxWidth(unsigned int id) const
	{
		return (id < numFrontEnds_) ? frontEndMaxWidth_[id] : 0;
	}

	unsigned int frontEndDownscalerMaxWidth(unsigned int id, unsigned int branch) const
	{
		return (id < numFrontEnds_ && branch < numFrontEndBranches_[id]) ? frontEndDownscalerMaxWidth_[id][branch] : 0;
	}

	bool frontEndDownscalerAvailable(unsigned int id, unsigned int branch) const
	{
		return (id < numFrontEnds_ && branch < numFrontEndBranches_[id]) ? frontEndDownscaler_[id][branch] : 0;
	}

	unsigned int backEndNumBranches(unsigned int id) const
	{
		return id < numBackEnds_ ? numBackEndBranches_[id] : 0;
	}

	unsigned int backEndMaxTileWidth(unsigned int id) const
	{
		return (id < numBackEnds_) ? backEndMaxTileWidth_ : 0;
	}

	bool backEndIntegralImage(unsigned int id, unsigned int branch) const
	{
		return (id < numBackEnds_ && branch < numBackEndBranches_[id]) ? backEndIntegralImage_[id][branch] : 0;
	}

	bool backEndDownscalerAvailable(unsigned int id, unsigned int branch) const
	{
		return (id < numBackEnds_ && branch < numBackEndBranches_[id]) ? backEndDownscaler_[id][branch] : 0;
	}
};

extern const PiSPVariant BCM2712_C0;
extern const PiSPVariant BCM2712_D0;

} // namespace libpisp
