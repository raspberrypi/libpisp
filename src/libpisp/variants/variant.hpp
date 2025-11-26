/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_variant.hpp - PiSP variant configuration definitions
 */
#pragma once

#include <array>
#include <compare>
#include <optional>
#include <string>
#include <vector>

// This header files defines functions that can be called to discover the details about the specific
// implementation of the PiSP that is being used. Whatever the build system being used is, it needs
// to select the correct C file in this directory that implements these functions.

namespace libpisp
{

class PiSPVariant
{
public:
	static constexpr unsigned int MaxFrontEnds = 4;
	static constexpr unsigned int MaxBackEnds = 4;
	static constexpr unsigned int MaxFrontEndBranches = 4;
	static constexpr unsigned int MaxBackEndBranches = 4;

	std::string name;
	unsigned int id;
	unsigned int feVersion;
	unsigned int beVersion;
	unsigned int numFe;
	unsigned int numBe;

	std::array<unsigned int, MaxFrontEnds> numFeBranches;
	std::array<unsigned int, MaxFrontEnds> feStatsMaxWidth;
	std::array<std::array<bool, MaxFrontEndBranches>, MaxFrontEnds> feDownscaler;
	std::array<std::array<unsigned int, MaxFrontEndBranches>, MaxFrontEnds> feDownscalerMaxWidth;

	unsigned int beMaxTileWidth;
	std::array<unsigned int, MaxBackEnds> numBeBranches;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> beIntegralImages;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> beDownscaler;
	bool beRgb32Support;

	const std::string &getName() const
	{
		return name;
	}

	unsigned int backEndVersion() const
	{
		return beVersion;
	}

	unsigned int frontEndVersion() const
	{
		return feVersion;
	}

	unsigned int numFrontEnds() const
	{
		return numFe;
	}

	unsigned int numBackEnds() const
	{
		return numBe;
	}

	unsigned int frontEndNumBranches(unsigned int id) const
	{
		return id < numFe ? numFeBranches[id] : 0;
	}

	unsigned int frontEndStatsMaxWidth(unsigned int id) const
	{
		return (id < numFe) ? feStatsMaxWidth[id] : 0;
	}

	unsigned int frontEndDownscalerMaxWidth(unsigned int id, unsigned int branch) const
	{
		return (id < numFe && branch < numFeBranches[id]) ? feDownscalerMaxWidth[id][branch] : 0;
	}

	bool frontEndDownscalerAvailable(unsigned int id, unsigned int branch) const
	{
		return (id < numFe && branch < numFeBranches[id]) ? feDownscaler[id][branch] : 0;
	}

	unsigned int backEndNumBranches(unsigned int id) const
	{
		return id < numBe ? numBeBranches[id] : 0;
	}

	unsigned int backEndMaxTileWidth(unsigned int id) const
	{
		return (id < numBe) ? beMaxTileWidth : 0;
	}

	bool backEndIntegralImage(unsigned int id, unsigned int branch) const
	{
		return (id < numBe && branch < numBeBranches[id]) ? beIntegralImages[id][branch] : 0;
	}

	bool backEndDownscalerAvailable(unsigned int id, unsigned int branch) const
	{
		return (id < numBe && branch < numBeBranches[id]) ? beDownscaler[id][branch] : 0;
	}

	bool backendRgb32Supported(unsigned int id) const
	{
		return id < numBe ? beRgb32Support : false;
	}

	auto operator<=>(const PiSPVariant &other) const
	{
		return id <=> other.id;
	}
};

extern const PiSPVariant BCM2712_C0, BCM2712_D0, BERYL;

const std::vector<PiSPVariant> &getVariants();
std::optional<PiSPVariant> getVariant(unsigned int feVersion, unsigned int beVersion);

} // namespace libpisp
