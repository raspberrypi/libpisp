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

DedistortStage::DedistortStage(char const *name, Stage *upstream, Config const &config, int struct_offset)
	: BasicStage(name, upstream->getPipeline(), upstream, struct_offset), config_(config)
{
}

void DedistortStage::pushStartUp(int output_start, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_start " << output_start);

	int input_start = output_start;
	if (dir == Dir::X && output_start > 0 && !config_.pwls[0].empty())
	{
		input_start = config_.pwls[0].eval(output_start);
		if (input_start < 0)
			input_start = 0;
	}
	outputInterval_.offset = output_start;
	inputInterval_.offset = input_start;

	PISP_LOG(debug, "(" << name_ << ") Exit - call pushStartUp with " << inputInterval_.offset);

	upstream_->pushStartUp(input_start, dir);
}

int DedistortStage::pushEndDown(int input_end, Dir dir)
{
	// incomprehensible comment removed
	PISP_LOG(debug, "(" << name_ << ") Enter with input_end " << input_end);

	int output_end = input_end;
	if (dir == Dir::X && input_end < getInputImageSize()[dir] && !config_.pwls[2].empty())
	{
		output_end = config_.pwls[2].eval(input_end);
		if (output_end > input_end)
			output_end = input_end;
	}
	inputInterval_.setEnd(input_end);
	outputInterval_.setEnd(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with output_end " << output_end);

	pushEndUp(downstream_->pushEndDown(output_end, dir), dir);
	return inputInterval_.end();
}

void DedistortStage::pushEndUp(int output_end, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with output_end " << output_end);
	PISP_ASSERT(output_end <= outputInterval_.end());

	int input_end = output_end;
	if (dir == Dir::X && output_end < getOutputImageSize()[dir] && !config_.pwls[1].empty())
	{
		input_end = config_.pwls[1].eval(output_end);
		if (input_end > getInputImageSize()[dir])
			input_end = getInputImageSize()[dir];
	}
	inputInterval_.setEnd(input_end);
	outputInterval_.setEnd(output_end);

	PISP_LOG(debug, "(" << name_ << ") Exit with input_end " << input_end);
}

void DedistortStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);
	PISP_ASSERT(inputInterval_ < interval);

	// I have no idea what this method is supposed to do
	outputInterval_ = interval;
	inputInterval_ = interval;
	crop_ = inputInterval_ - outputInterval_;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << outputInterval_);
	downstream_->pushCropDown(outputInterval_, dir);
}
