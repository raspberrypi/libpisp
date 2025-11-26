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
private:
	static constexpr unsigned int MaxFrontEnds = 4;
	static constexpr unsigned int MaxBackEnds = 4;
	static constexpr unsigned int MaxFrontEndBranches = 4;
	static constexpr unsigned int MaxBackEndBranches = 4;

	std::string name_;
	unsigned int id_;
	unsigned int fe_version_;
	unsigned int be_version_;
	unsigned int num_fe_;
	unsigned int num_be_;

	std::array<unsigned int, MaxFrontEnds> num_fe_branches_;
	std::array<unsigned int, MaxFrontEnds> fe_stats_max_width_;
	std::array<std::array<bool, MaxFrontEndBranches>, MaxFrontEnds> fe_downscaler_;
	std::array<std::array<unsigned int, MaxFrontEndBranches>, MaxFrontEnds> fe_downscaler_max_width_;

	unsigned int be_max_tile_width_;
	std::array<unsigned int, MaxBackEnds> num_be_branches_;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> be_integral_images_;
	std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> be_downscaler_;
	bool be_rgb32_support_;

public:
	PiSPVariant(
		const std::string &name,
		unsigned int id,
		unsigned int fe_version,
		unsigned int be_version,
		unsigned int num_fe,
		unsigned int num_be,
		const std::array<unsigned int, MaxFrontEnds> &num_fe_branches,
		const std::array<unsigned int, MaxFrontEnds> &fe_stats_max_width,
		const std::array<std::array<bool, MaxFrontEndBranches>, MaxFrontEnds> &fe_downscaler,
		const std::array<std::array<unsigned int, MaxFrontEndBranches>, MaxFrontEnds> &fe_downscaler_max_width,
		unsigned int be_max_tile_width,
		const std::array<unsigned int, MaxBackEnds> &num_be_branches,
		const std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> &be_integral_images,
		const std::array<std::array<bool, MaxBackEndBranches>, MaxBackEnds> &be_downscaler,
		bool be_rgb32_support)
		: name_(name)
		, id_(id)
		, fe_version_(fe_version)
		, be_version_(be_version)
		, num_fe_(num_fe)
		, num_be_(num_be)
		, num_fe_branches_(num_fe_branches)
		, fe_stats_max_width_(fe_stats_max_width)
		, fe_downscaler_(fe_downscaler)
		, fe_downscaler_max_width_(fe_downscaler_max_width)
		, be_max_tile_width_(be_max_tile_width)
		, num_be_branches_(num_be_branches)
		, be_integral_images_(be_integral_images)
		, be_downscaler_(be_downscaler)
		, be_rgb32_support_(be_rgb32_support)
	{
	}

	const std::string &name() const
	{
		return name_;
	}

	unsigned int id() const
	{
		return id_;
	}

	unsigned int back_end_version() const
	{
		return be_version_;
	}

	unsigned int front_end_version() const
	{
		return fe_version_;
	}

	unsigned int num_front_ends() const
	{
		return num_fe_;
	}

	unsigned int num_back_ends() const
	{
		return num_be_;
	}

	unsigned int front_end_num_branches(unsigned int id) const
	{
		return id < num_fe_ ? num_fe_branches_[id] : 0;
	}

	unsigned int front_end_stats_max_width(unsigned int id) const
	{
		return (id < num_fe_) ? fe_stats_max_width_[id] : 0;
	}

	unsigned int front_end_downscaler_max_width(unsigned int id, unsigned int branch) const
	{
		return (id < num_fe_ && branch < num_fe_branches_[id]) ? fe_downscaler_max_width_[id][branch] : 0;
	}

	bool front_end_downscaler_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_fe_ && branch < num_fe_branches_[id]) ? fe_downscaler_[id][branch] : 0;
	}

	unsigned int back_end_num_branches(unsigned int id) const
	{
		return id < num_be_ ? num_be_branches_[id] : 0;
	}

	unsigned int back_end_max_tile_width(unsigned int id) const
	{
		return (id < num_be_) ? be_max_tile_width_ : 0;
	}

	bool back_end_integral_images_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_be_ && branch < num_be_branches_[id]) ? be_integral_images_[id][branch] : 0;
	}

	bool back_end_downscaler_available(unsigned int id, unsigned int branch) const
	{
		return (id < num_be_ && branch < num_be_branches_[id]) ? be_downscaler_[id][branch] : 0;
	}

	bool backend_rgb32_supported(unsigned int id) const
	{
		return id < num_be_ ? be_rgb32_support_ : false;
	}

	auto operator<=>(const PiSPVariant &other) const
	{
		return id_ <=> other.id_;
	}
};

extern const PiSPVariant BCM2712_C0, BCM2712_D0, BERYL;

const std::vector<PiSPVariant> &get_variants();
std::optional<PiSPVariant> get_variant(unsigned int fe_version, unsigned int be_version);

} // namespace libpisp
