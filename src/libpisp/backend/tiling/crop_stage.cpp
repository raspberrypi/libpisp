/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * crop_stage.cpp - Tiling library component for crop stages
 */
#include "crop_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

namespace
{

inline bool interval_valid(const Interval &interval, const int minTileSize)
{
	return interval.end() >= minTileSize && interval.length >= minTileSize;
}

} // namespace

CropStage::CropStage(char const *name, Stage *upstream, Config const &config, int structOffset)
	: BasicStage(name, upstream->getPipeline(), upstream, structOffset), config_(config)
{
}

Length2 CropStage::getOutputImageSize() const
{
	return Length2(config_.crop.x.length, config_.crop.y.length);
}

void CropStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	int input_start = outputStart + config_.crop[dir].offset;
	// input_start can never be negative here, but it is possible to have outputStart
	// negative if, for example, a branch starts producing output on the second
	// tile in a row (or column) and the resampler requires left (or top) context pixels.
	if (input_start < 0)
		throw std::runtime_error("input start is negative: " + std::to_string(input_start));
	outputInterval_.offset = outputStart;
	inputInterval_.offset = input_start;

	PISP_LOG(debug, "(" << name_ << ") Exit with input_start " << input_start);
	upstream_->pushStartUp(input_start, dir);
}

int CropStage::pushEndDown(int inputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	int outputEnd = inputEnd - config_.crop[dir].offset;

	if (outputEnd > config_.crop[dir].length)
		outputEnd = config_.crop[dir].length;

	outputInterval_.setEnd(outputEnd);

	// If this is the first tile to generate output, ensure we can make at least
	// minTileSize of output pixels. If not, terminate iteration here and don't
	// go futher downstream. Defer the output for the next tile.
	//
	// outputEnd may also be negative if no output will be generated for this tile.
	if (!interval_valid(outputInterval_, getPipeline()->getConfig().minTileSize[dir]))
	{
		PISP_LOG(debug, "(" << name_ << ") Output branch not started or output too small, terminating");
		BasicStage::reset();
		return 0;
	}

	inputInterval_.setEnd(inputEnd);

	PISP_LOG(debug, "(" << name_ << ") Exit with outputEnd " << outputEnd);
	pushEndUp(downstream_->pushEndDown(outputEnd, dir), dir);
	return inputInterval_.end();
}

void CropStage::pushEndUp(int outputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);

	int inputEnd = outputEnd + config_.crop[dir].offset;
	inputInterval_.setEnd(inputEnd);
	outputInterval_.setEnd(outputEnd);

	// Same check as we do in pushEndDown().
	if (!interval_valid(outputInterval_, getPipeline()->getConfig().minTileSize[dir]))
	{
		PISP_LOG(debug, "(" << name_ << ") Output branch not started or output too small, terminating");
		BasicStage::reset();
		return;
	}

	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << inputEnd);
}

void CropStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	// Branch has not started producing output.  Terminate the iteration here
	// and don't go futher downstream.
	if (!interval_valid(outputInterval_, getPipeline()->getConfig().minTileSize[dir]))
	{
		PISP_LOG(debug, "(" << name_ << ") Output branch not started or output too small, terminating");
		BasicStage::reset();
		return;
	}

	PISP_ASSERT(interval > inputInterval_);

	inputInterval_ = interval;
	interval.offset -= config_.crop[dir].offset;
	crop_ = interval - outputInterval_;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << outputInterval_);
	downstream_->pushCropDown(outputInterval_, dir);
}

bool CropStage::getBranchInactive() const
{
	return !outputInterval_.length;
}
