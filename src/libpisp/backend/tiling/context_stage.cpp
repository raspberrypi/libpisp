/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * contextStage.cpp - Tiling library component for context stages
 */
#include "context_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

ContextStage::ContextStage(char const *name, Stage *upstream, Config const &config, int struct_offset)
	: BasicStage(name, upstream->get_pipeline(), upstream, struct_offset), config_(config)
{
}

void ContextStage::push_start_up(int output_start, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_start " << output_start);

	int input_start = output_start - config_.context[dir].start;
	if (input_start < 0)
		input_start = 0;
	input_start -= input_start % config_.alignment[dir];
	output_interval_.offset = output_start;
	input_interval_.offset = input_start;

	PISP_LOG(debug, "(" << name_ << ") Exit - call push_start_up with " << input_interval_.offset);

	upstream_->push_start_up(input_start, dir);
}

int ContextStage::push_end_down(int input_end, Dir dir)
{
	// If we're given an end point that doesn't align then what are we supposed to do about it? Well,
	// we have to rely on the subsequent push_end_up to correct the upstream stage, but we must ensure
	// we send a value downstream that, when it comes back (possibly modified) in push_end_up, won't
	// cause us to demand a input larger than was given to us here. Simple, no?
	PISP_LOG(debug, "(" << name_ << ") Enter with input_end " << input_end);

	int output_end = input_end;
	if (input_end < get_input_image_size()[dir])
	{
		output_end -= output_end % config_.alignment[dir];
		output_end -= config_.context[dir].end;
	}
	input_interval_.set_end(input_end);
	output_interval_.set_end(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with output_end " << output_end);

	push_end_up(downstream_->push_end_down(output_end, dir), dir);
	return input_interval_.end();
}

void ContextStage::push_end_up(int output_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_end " << output_end);
	PISP_ASSERT(output_end <= output_interval_.end());

	int input_end = output_end;
	input_end += config_.context[dir].end;
	int align = config_.alignment[dir];
	input_end = ((input_end + align - 1) / align) * align;
	if (input_end > get_input_image_size()[dir])
		input_end = get_input_image_size()[dir];
	input_interval_.set_end(input_end);
	output_interval_.set_end(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with input_end " << input_end);
}

void ContextStage::push_crop_down(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);
	PISP_ASSERT(input_interval_ < interval);

	int align = config_.alignment[dir];
	if (interval.offset % align || (interval.end() % align && interval.end() != get_input_image_size()[dir]))
	{
		// Interval doesn't align properly. Some cropping will be required. Rather than calculating
		// the necessary crop it's safe just to send out former input tile downstream. This could
		// genuinely happen if people put weird alignments throughout their pipeline, but in practice
		// Bayer stages should all be 2-pixel aligned so there's no reason this should pop out.
		PISP_LOG(warning, "Stage receiving misaligned input - cropping will be required");
		output_interval_ = input_interval_;
	}
	else
		output_interval_ = interval;

	input_interval_ = interval;
	crop_ = input_interval_ - output_interval_;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << output_interval_);
	downstream_->push_crop_down(output_interval_, dir);
}
