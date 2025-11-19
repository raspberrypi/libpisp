/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * output_stage.cpp - Tiling library component for output stages
 */
#include "output_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

// There's a rather crucial convention here that when the output image is flipped, we use
// a coordinate system to describe it starting from the RH edge of the image travelling left.
// This means that tile coordinates don't change, it's the coordinate system that did. The
// upshot is that not very much changes when things are flipped, we just have to be careful
// that alignment applies not to the given tile offsets/lengths (which are now working from
// the right to left), but only when we subtract them from the image width (effectively now
// the offset/length from the LH edge).

OutputStage::OutputStage(char const *name, Stage *upstream, Config const &config, int structOffset)
	: BasicStage(name, upstream->getPipeline(), upstream, structOffset), config_(config), branchComplete_(false)
{
	pipeline_->addOutputStage(this);
}

Interval OutputStage::getOutputInterval() const
{
	return outputInterval_;
}

void OutputStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	outputInterval_.offset = inputInterval_.offset = outputStart;

	PISP_LOG(debug, "(" << name_ << ") Exit with input_start " << inputInterval_.offset);
	upstream_->pushStartUp(inputInterval_.offset, dir);
}

static int align_end(int inputEnd, int image_size, int align, bool mirrored)
{
	int outputEnd = inputEnd, aligned_outputEnd = outputEnd;
	if (mirrored)
	{
		// It's the end in the unflipped coordinate space that must align.
		int unflipped_end = image_size - inputEnd;
		unflipped_end = ((unflipped_end + align - 1) / align) * align;
		aligned_outputEnd = image_size - unflipped_end;
	}
	else
	{
		if (inputEnd < image_size)
			aligned_outputEnd = outputEnd - (inputEnd % align);
	}
	return aligned_outputEnd;
}

int OutputStage::pushEndDown(int inputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	int outputEnd = inputEnd;
	int image_size = getInputImageSize()[dir];
	bool mirrored = (dir == Dir::X && config_.xMirrored);
	int aligned_outputEnd = align_end(outputEnd, image_size, config_.maxAlignment[dir], mirrored);
	if (aligned_outputEnd >= outputInterval_.offset + config_.maxAlignment[dir])
		outputEnd = aligned_outputEnd;
	else
	{
		aligned_outputEnd = align_end(outputEnd, image_size, config_.minAlignment[dir], mirrored);
		if (aligned_outputEnd > outputInterval_.offset)
		{
			outputEnd = aligned_outputEnd;
			PISP_LOG(debug, "(" << name_ << ") Unable to achieve optimal alignment " << config_.maxAlignment[dir]);
		}
		else if (inputInterval_.offset < image_size)
		{ // test against size in case this branch already finished
			PISP_LOG(warning, "(" << name_ << ") Unable to achieve mandatory alignment " << config_.minAlignment[dir]);
			outputEnd = aligned_outputEnd;
			// Just because this output can't make progress, the other branch may - at which point this
			// branch may then succeed again. So this isn't necessarily fatal. Let the split stage decide
			// if there was no progress at all.
		}
	}
	inputInterval_.setEnd(inputEnd);
	outputInterval_.setEnd(outputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with outputEnd " << outputEnd);
	pushEndUp(outputEnd, dir);
	return inputInterval_.end();
}

void OutputStage::pushEndUp(int outputEnd, [[maybe_unused]] Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);

	// We should just get given back our own output value.
	PISP_ASSERT(outputEnd == outputInterval_.end());
	inputInterval_.setEnd(outputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << outputEnd);
}

void OutputStage::pushCropDown(Interval interval, [[maybe_unused]] Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// We can't push crop any further down, it has to be trimmed here.
	inputInterval_ = interval;
	crop_ = interval - outputInterval_;
	PISP_ASSERT(crop_.start >= 0 && crop_.end >= 0);

	// Note that we don't flip our output interval when horizontally mirrored; we assume our caller expects to do that.
	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << outputInterval_);
}

void OutputStage::reset()
{
	BasicStage::reset();
	branchComplete_ = false;
}

bool OutputStage::getBranchComplete() const
{
	return branchComplete_;
}

void OutputStage::setBranchComplete()
{
	branchComplete_ = true;
	PISP_LOG(debug, "(" << name_ << ") Setting branch complete");
}
