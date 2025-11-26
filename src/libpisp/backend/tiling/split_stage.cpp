/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * split_stage.cpp - Tiling library component for splitter stages
 */
#include "split_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

SplitStage::SplitStage(char const *name, Stage *upstream)
	: Stage(name, upstream->get_pipeline(), -1), upstream_(upstream)
{
	upstream->set_downstream(this);
}

Length2 SplitStage::get_input_image_size() const
{
	return upstream_->get_output_image_size();
}

Length2 SplitStage::get_output_image_size() const
{
	return get_input_image_size();
}

void SplitStage::set_downstream(Stage *stage)
{
	downstream_.push_back(stage);
}

void SplitStage::reset()
{
	input_interval_ = Interval(0, 0);
	count_ = 0;
}

void SplitStage::push_start_up(int output_start, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_start " << output_start);

	// We must wait till all the downstream branches have given us their number, then we send the leftmost
	// one up the pipeline.
	if (count_++ == 0)
		input_interval_ = Interval(output_start);
	else
		input_interval_ |= output_start;

	unsigned int branch_incomplete_count = 0;
	for (auto const &d : downstream_)
		if (!d->get_branch_complete())
			branch_incomplete_count++;

	if (count_ == branch_incomplete_count)
	{
		count_ = 0;
		PISP_LOG(debug, "(" << name_ << ") Exit - call push_start_up with " << input_interval_.offset);
		upstream_->push_start_up(input_interval_.offset, dir);
	}
}

int SplitStage::push_end_down(int input_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with input_end " << input_end);

	// First tell all the branches what the maximum number of pixels is that they can have so that we find
	// out what they can do with it. Then remember the least far-on end position that they need. This avoid
	// potential over-read if one branch can only accept way fewer pixels than another.
	input_interval_.set_end(0);
	for (auto d : downstream_)
	{
		if (d->get_branch_complete())
			continue;
		int branch_end = d->push_end_down(input_end, dir);
		// (It is OK for a branch to make no progress at all - so long as another branch does!)
		if (branch_end > input_interval_.end())
			input_interval_.set_end(branch_end);
	}

	// Finally tell all the branches now what they will really get, which is that end point.
	PISP_LOG(debug, "(" << name_ << ") Split using input_end " << input_interval_.end());
	if (input_interval_.length == 0)
	{
		PISP_LOG(fatal, "(" << name_ << ") Neither branch can make progress");
		throw TilingException();
	}

	for (auto d : downstream_)
		if (!d->get_branch_complete())
			d->push_end_down(input_interval_.end(), dir);

	push_end_up(input_interval_.end(), dir);
	return input_interval_.end();
}

void SplitStage::push_end_up([[maybe_unused]] int output_end, [[maybe_unused]] Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_end " << output_end);
	// Genuinely nothing to do here, but we just like to log the usual trace information for consistency.
	PISP_LOG(debug, "(" << name_ << ") Exit with input_end " << output_end);
}

void SplitStage::push_crop_down(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// Whatever we get goes down all the branches. If there are any that don't like it then they'll have to
	// start by cropping off what they can't handle.
	PISP_ASSERT(interval > input_interval_);
	input_interval_ = interval;
	for (auto d : downstream_)
	{
		if (d->get_branch_complete())
			continue;
		PISP_LOG(debug, "(" << name_ << ") Exit with interval " << interval);
		d->push_crop_down(interval, dir);
	}
}

void SplitStage::copy_out([[maybe_unused]] void *dest, [[maybe_unused]] Dir dir)
{
}

bool SplitStage::get_branch_complete() const
{
	bool done = true;
	for (auto d : downstream_)
		done &= d->get_branch_complete();
	return done;
}

bool SplitStage::get_branch_inactive() const
{
	if (!upstream_)
		return false;
	return upstream_->get_branch_inactive();
}
