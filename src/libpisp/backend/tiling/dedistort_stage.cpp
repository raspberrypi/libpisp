/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * dedistort_stage.cpp - Tiling library component for dedistort stages
 */
#include "dedistort_stage.hpp"

#include "common/logging.hpp"
#include "common/pisp_pwl.hpp"
#include "pipeline.hpp"

using namespace tiling;

// Actual context requirement is +/- 1 after floor or ceil (for cubic spline)
// but we'll assume +/- 2 to allow for rounding errors during grid calculations.
// Dedistort requires 2-pixel horizontal alignment each side (which "context"
// stages normally provide, but we enforce it here as well).
static constexpr int DedistortContext = 2;
static constexpr int DedistortAlign = 2;

DedistortStage::DedistortStage(char const *name, Stage *upstream, Config const &config, int struct_offset)
	: BasicStage(name, upstream->get_pipeline(), upstream, struct_offset), config_(config)
{
}

void DedistortStage::push_start_up(int output_start, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_start " << output_start);

	int input_start = output_start;
	if (dir == Dir::X && output_start > 0 && !config_.pwls[0].empty())
	{
		// pwls[0] gives the leftmost input needed in any row, to yield the output_start column
		input_start = config_.pwls[0].eval(output_start) - DedistortContext;
		input_start &= -DedistortAlign;
		if (input_start < 0)
			input_start = 0;
	}
	output_interval_.offset = output_start;
	input_interval_.offset = input_start;

	PISP_LOG(debug, "(" << name_ << ") Exit - call push_start_up with " << input_interval_.offset);

	upstream_->push_start_up(input_start, dir);
}

int DedistortStage::push_end_down(int input_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with input_end " << input_end);

	int output_end = input_end;
	if (dir == Dir::X && input_end < get_input_image_size()[dir] && !config_.pwls[2].empty())
	{
		int maxw = get_pipeline()->get_config().max_tile_size[dir];

		// pwls[2] gives the rightmost output column that can be generated for all rows
		output_end =
			config_.pwls[2].eval((input_end < get_input_image_size()[dir]) ? input_end - DedistortContext : input_end);
		if (output_end + 1 >= get_output_image_size()[dir])
			output_end = get_output_image_size()[dir];
		if (output_end > output_interval_.offset + maxw)
			output_end = output_interval_.offset + maxw;
		output_end &= -DedistortAlign;
	}
	input_interval_.set_end(input_end);
	output_interval_.set_end(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with output_end " << output_end);

	push_end_up(downstream_->push_end_down(output_end, dir), dir);
	return input_interval_.end();
}

void DedistortStage::push_end_up(int output_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_end " << output_end);
	PISP_ASSERT(output_end <= output_interval_.end());

	int input_end = output_end;
	if (dir == Dir::X && output_end < get_output_image_size()[dir] && !config_.pwls[1].empty())
	{
		// pwls[1] gives the rightmost input needed in any row, to yield the output_end column
		input_end = (int)ceil(config_.pwls[1].eval(output_end) + DedistortContext);
		input_end = (input_end + (DedistortAlign - 1)) & -DedistortAlign;
		if (input_end > get_input_image_size()[dir])
			input_end = get_input_image_size()[dir];
	}
	input_interval_.set_end(input_end);
	output_interval_.set_end(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with input_end " << input_end);
}

void DedistortStage::push_crop_down(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);
	PISP_ASSERT(input_interval_ < interval);

	// When dedistort is disabled, it acts as a pass-through
	if (dir != Dir::X || config_.pwls[0].empty())
		output_interval_ = interval;

	crop_ = interval - input_interval_;
	input_interval_ = interval;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << output_interval_);
	downstream_->push_crop_down(output_interval_, dir);
}
