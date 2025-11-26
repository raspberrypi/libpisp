/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * inputStage.cpp - Tiling library component for input stages
 */
#include "input_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

InputStage::InputStage(char const *name, Pipeline *pipeline, Config const &config, int struct_offset)
	: BasicStage(name, pipeline, nullptr, struct_offset), config_(config)
{
	pipeline->add_input_stage(this);
	// If the compression requires more alignment than the basic X alignment, just bump the X alignment up.
	PISP_ASSERT(config_.compression_alignment ==
					0 || // (assume one alignment is a multiple of the other to make life easy!)
				config_.alignment.dx % config_.compression_alignment == 0 ||
				config_.compression_alignment % config_.alignment.dx == 0);
	config_.alignment.dx = std::max(config_.alignment.dx, config_.compression_alignment);
}

Length2 InputStage::get_input_image_size() const
{
	return config_.input_image_size;
}

Interval InputStage::get_input_interval() const
{
	return input_interval_;
}

void InputStage::push_start_up(int output_start, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_start " << output_start);

	// We may have to read a more aligned value than we were given.
	output_interval_.offset = output_start;
	input_interval_.offset = output_start - output_start % config_.alignment[dir];

	PISP_LOG(debug, "(" << name_ << ") Exit with input_start " << input_interval_.offset);
}

int InputStage::push_end_down(int input_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with input_end " << input_end);

	if (input_end >= get_input_image_size()[dir])
		input_end = get_input_image_size()[dir];
	else
	{
		// We don't get given pixels which we crop off as we go through the block, there's no
		// sense in our input containing any unnecessary pixels so we adjust our input_interval
		// directly. Watch out for the end-of-image case when we must write whatever we have.
		input_end -= input_end % config_.alignment[dir];
	}

	input_interval_.set_end(input_end);
	output_interval_.set_end(input_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with output_end " << input_end);
	push_end_up(downstream_->push_end_down(input_end, dir), dir);
	return input_interval_.end();
}

void InputStage::push_end_up(int output_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_end " << output_end);

	int align = config_.alignment[dir];
	int input_end = ((output_end + align - 1) / align) * align;
	if (input_end > get_input_image_size()[dir])
	{
		input_end = get_input_image_size()[dir];
		// When compressed, we must always read a full compressed block, even if this extends beyond the nominal image width.
		if (dir == Dir::X && config_.compression_alignment)
			input_end = ((input_end + config_.compression_alignment - 1) / config_.compression_alignment) *
						config_.compression_alignment;
	}
	output_interval_.set_end(output_end);
	input_interval_.set_end(input_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with input_end " << input_interval_.end());
}

void InputStage::push_crop_down(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// As we're at the head of the pipeline, no one should be trying to give us extra pixels.
	PISP_ASSERT(interval == input_interval_);
	crop_ = Crop(0, 0);
	output_interval_ = interval;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << interval);
	downstream_->push_crop_down(interval, dir);
}
