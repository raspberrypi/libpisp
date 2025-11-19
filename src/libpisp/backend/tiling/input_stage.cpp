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

InputStage::InputStage(char const *name, Pipeline *pipeline, Config const &config, int structOffset)
	: BasicStage(name, pipeline, nullptr, structOffset), config_(config)
{
	pipeline->addInputStage(this);
	// If the compression requires more alignment than the basic X alignment, just bump the X alignment up.
	PISP_ASSERT(config_.compressionAlignment ==
					0 || // (assume one alignment is a multiple of the other to make life easy!)
				config_.alignment.dx % config_.compressionAlignment == 0 ||
				config_.compressionAlignment % config_.alignment.dx == 0);
	config_.alignment.dx = std::max(config_.alignment.dx, config_.compressionAlignment);
}

Length2 InputStage::getInputImageSize() const
{
	return config_.inputImageSize;
}

Interval InputStage::getInputInterval() const
{
	return inputInterval_;
}

void InputStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	// We may have to read a more aligned value than we were given.
	outputInterval_.offset = outputStart;
	inputInterval_.offset = outputStart - outputStart % config_.alignment[dir];

	PISP_LOG(debug, "(" << name_ << ") Exit with input_start " << inputInterval_.offset);
}

int InputStage::pushEndDown(int inputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	if (inputEnd >= getInputImageSize()[dir])
		inputEnd = getInputImageSize()[dir];
	else
	{
		// We don't get given pixels which we crop off as we go through the block, there's no
		// sense in our input containing any unnecessary pixels so we adjust our input_interval
		// directly. Watch out for the end-of-image case when we must write whatever we have.
		inputEnd -= inputEnd % config_.alignment[dir];
	}

	inputInterval_.setEnd(inputEnd);
	outputInterval_.setEnd(inputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with outputEnd " << inputEnd);
	pushEndUp(downstream_->pushEndDown(inputEnd, dir), dir);
	return inputInterval_.end();
}

void InputStage::pushEndUp(int outputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);

	int align = config_.alignment[dir];
	int inputEnd = ((outputEnd + align - 1) / align) * align;
	if (inputEnd > getInputImageSize()[dir])
	{
		inputEnd = getInputImageSize()[dir];
		// When compressed, we must always read a full compressed block, even if this extends beyond the nominal image width.
		if (dir == Dir::X && config_.compressionAlignment)
			inputEnd = ((inputEnd + config_.compressionAlignment - 1) / config_.compressionAlignment) *
						config_.compressionAlignment;
	}
	outputInterval_.setEnd(outputEnd);
	inputInterval_.setEnd(inputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << inputInterval_.end());
}

void InputStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// As we're at the head of the pipeline, no one should be trying to give us extra pixels.
	PISP_ASSERT(interval == inputInterval_);
	crop_ = Crop(0, 0);
	outputInterval_ = interval;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << interval);
	downstream_->pushCropDown(interval, dir);
}
