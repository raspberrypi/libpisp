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
	: Stage(name, upstream->getPipeline(), -1), upstream_(upstream)
{
	upstream->setDownstream(this);
}

Length2 SplitStage::getInputImageSize() const
{
	return upstream_->getOutputImageSize();
}

Length2 SplitStage::getOutputImageSize() const
{
	return getInputImageSize();
}

void SplitStage::setDownstream(Stage *stage)
{
	downstream_.push_back(stage);
}

void SplitStage::reset()
{
	inputInterval_ = Interval(0, 0);
	count_ = 0;
}

void SplitStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	// We must wait till all the downstream branches have given us their number, then we send the leftmost
	// one up the pipeline.
	if (count_++ == 0)
		inputInterval_ = Interval(outputStart);
	else
		inputInterval_ |= outputStart;

	unsigned int branch_incomplete_count = 0;
	for (auto const &d : downstream_)
		if (!d->getBranchComplete())
			branch_incomplete_count++;

	if (count_ == branch_incomplete_count)
	{
		count_ = 0;
		PISP_LOG(debug, "(" << name_ << ") Exit - call pushStartUp with " << inputInterval_.offset);
		upstream_->pushStartUp(inputInterval_.offset, dir);
	}
}

int SplitStage::pushEndDown(int inputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	// First tell all the branches what the maximum number of pixels is that they can have so that we find
	// out what they can do with it. Then remember the least far-on end position that they need. This avoid
	// potential over-read if one branch can only accept way fewer pixels than another.
	inputInterval_.setEnd(0);
	for (auto d : downstream_)
	{
		if (d->getBranchComplete())
			continue;
		int branch_end = d->pushEndDown(inputEnd, dir);
		// (It is OK for a branch to make no progress at all - so long as another branch does!)
		if (branch_end > inputInterval_.end())
			inputInterval_.setEnd(branch_end);
	}

	// Finally tell all the branches now what they will really get, which is that end point.
	PISP_LOG(debug, "(" << name_ << ") Split using inputEnd " << inputInterval_.end());
	if (inputInterval_.length == 0)
	{
		PISP_LOG(fatal, "(" << name_ << ") Neither branch can make progress");
		throw TilingException();
	}

	for (auto d : downstream_)
		if (!d->getBranchComplete())
			d->pushEndDown(inputInterval_.end(), dir);

	pushEndUp(inputInterval_.end(), dir);
	return inputInterval_.end();
}

void SplitStage::pushEndUp([[maybe_unused]] int outputEnd, [[maybe_unused]] Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);
	// Genuinely nothing to do here, but we just like to log the usual trace information for consistency.
	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << outputEnd);
}

void SplitStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// Whatever we get goes down all the branches. If there are any that don't like it then they'll have to
	// start by cropping off what they can't handle.
	PISP_ASSERT(interval > inputInterval_);
	inputInterval_ = interval;
	for (auto d : downstream_)
	{
		if (d->getBranchComplete())
			continue;
		PISP_LOG(debug, "(" << name_ << ") Exit with interval " << interval);
		d->pushCropDown(interval, dir);
	}
}

void SplitStage::copyOut([[maybe_unused]] void *dest, [[maybe_unused]] Dir dir)
{
}

bool SplitStage::getBranchComplete() const
{
	bool done = true;
	for (auto d : downstream_)
		done &= d->getBranchComplete();
	return done;
}

bool SplitStage::getBranchInactive() const
{
	if (!upstream_)
		return false;
	return upstream_->getBranchInactive();
}
