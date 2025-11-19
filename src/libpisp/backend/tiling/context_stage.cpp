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

ContextStage::ContextStage(char const *name, Stage *upstream, Config const &config, int structOffset)
	: BasicStage(name, upstream->getPipeline(), upstream, structOffset), config_(config)
{
}

void ContextStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	int input_start = outputStart - config_.context[dir].start;
	if (input_start < 0)
		input_start = 0;
	input_start -= input_start % config_.alignment[dir];
	outputInterval_.offset = outputStart;
	inputInterval_.offset = input_start;

	PISP_LOG(debug, "(" << name_ << ") Exit - call pushStartUp with " << inputInterval_.offset);

	upstream_->pushStartUp(input_start, dir);
}

int ContextStage::pushEndDown(int inputEnd, Dir dir)
{
	// If we're given an end point that doesn't align then what are we supposed to do about it? Well,
	// we have to rely on the subsequent pushEndUp to correct the upstream stage, but we must ensure
	// we send a value downstream that, when it comes back (possibly modified) in pushEndUp, won't
	// cause us to demand a input larger than was given to us here. Simple, no?
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	int outputEnd = inputEnd;
	if (inputEnd < getInputImageSize()[dir])
	{
		outputEnd -= outputEnd % config_.alignment[dir];
		outputEnd -= config_.context[dir].end;
	}
	inputInterval_.setEnd(inputEnd);
	outputInterval_.setEnd(outputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with outputEnd " << outputEnd);

	pushEndUp(downstream_->pushEndDown(outputEnd, dir), dir);
	return inputInterval_.end();
}

void ContextStage::pushEndUp(int outputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);
	PISP_ASSERT(outputEnd <= outputInterval_.end());

	int inputEnd = outputEnd;
	inputEnd += config_.context[dir].end;
	int align = config_.alignment[dir];
	inputEnd = ((inputEnd + align - 1) / align) * align;
	if (inputEnd > getInputImageSize()[dir])
		inputEnd = getInputImageSize()[dir];
	inputInterval_.setEnd(inputEnd);
	outputInterval_.setEnd(outputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << inputEnd);
}

void ContextStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);
	PISP_ASSERT(inputInterval_ < interval);

	int align = config_.alignment[dir];
	if (interval.offset % align || (interval.end() % align && interval.end() != getInputImageSize()[dir]))
	{
		// Interval doesn't align properly. Some cropping will be required. Rather than calculating
		// the necessary crop it's safe just to send out former input tile downstream. This could
		// genuinely happen if people put weird alignments throughout their pipeline, but in practice
		// Bayer stages should all be 2-pixel aligned so there's no reason this should pop out.
		PISP_LOG(warning, "Stage receiving misaligned input - cropping will be required");
		outputInterval_ = inputInterval_;
	}
	else
		outputInterval_ = interval;

	inputInterval_ = interval;
	crop_ = inputInterval_ - outputInterval_;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << outputInterval_);
	downstream_->pushCropDown(outputInterval_, dir);
}
