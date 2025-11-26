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
	unsigned int fe_version;
	unsigned int be_version;
	unsigned int num_fe;
	unsigned int num_be;

	std::array<unsigned int, MaxFrontEnds> num_fe_branches;
	std::array<unsigned int, MaxFrontEnds> fe_stats_max_width;
	std::array<std::array<bool, MaxFrontEndBranches>, MaxFrontEnds> fe_downscaler;
	std::array<std::array<unsigned int, MaxFrontEndBranches>, MaxFrontEnds> fe_downscaler_max_width;

	unsigned int be_max_tile_width;
	std::array<unsigned int, MaxBackEnds> num_be_branches;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> be_integral_images;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> be_downscaler;
	bool be_rgb32_support;

	const std::string &get_name() const
	{
		return name;
	}

	unsigned int back_end_version() const
	{
		return be_version;
	}

	unsigned int front_end_version() const
	{
		return fe_version;
	}

	unsigned int num_front_ends() const
	{
		return num_fe;
	}

	unsigned int num_back_ends() const
	{
		return num_be;
	}

	unsigned int front_end_num_branches(unsigned int id) const
	{
		return id < num_fe ? num_fe_branches[id] : 0;
	}

	unsigned int front_end_stats_max_width(unsigned int id) const
	{
		return (id < num_fe) ? fe_stats_max_width[id] : 0;
	}

	unsigned int front_end_downscaler_max_width(unsigned int id, unsigned int branch) const
	{
		return (id < num_fe && branch < num_fe_branches[id]) ? fe_downscaler_max_width[id][branch] : 0;
	}

	bool front_end_downscaler_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_fe && branch < num_fe_branches[id]) ? fe_downscaler[id][branch] : 0;
	}

	unsigned int back_end_num_branches(unsigned int id) const
	{
		return id < num_be ? num_be_branches[id] : 0;
	}

	unsigned int back_end_max_tile_width(unsigned int id) const
	{
		return (id < num_be) ? be_max_tile_width : 0;
	}

	bool back_end_integral_images_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_be && branch < num_be_branches[id]) ? be_integral_images[id][branch] : 0;
	}

	bool back_end_downscaler_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_be && branch < num_be_branches[id]) ? be_downscaler[id][branch] : 0;
	}

	bool backend_rgb32_supported(unsigned int id) const
	{
		return id < num_be ? be_rgb32_support : false;
	}

	auto operator<=>(const PiSPVariant &other) const
	{
		return id <=> other.id;
	}
};

extern const PiSPVariant BCM2712_C0, BCM2712_D0, BERYL;

const std::vector<PiSPVariant> &get_variants();
std::optional<PiSPVariant> get_variant(unsigned int fe_version, unsigned int be_version);

} // namespace libpisp
