/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * stages.cpp - Tiling library Stage class implementation
 */
#include "stages.hpp"

#include "common/logging.hpp"
#include "pipeline.hpp"

#include <cstdint>

using namespace tiling;

Stage::Stage(char const *name, Pipeline *pipeline, int structOffset)
	: name_(name), pipeline_(pipeline), structOffset_(structOffset)
{
	if (pipeline)
		pipeline->addStage(this);
}

Pipeline *Stage::getPipeline() const
{
	return pipeline_;
}

void Stage::mergeRegions(void *dest, void *xSrc, void *ySrc) const
{
	if (structOffset_ >= 0) // Hacky test to exclude stages that don't store a Region in each Tile
	{
		Region *destRegion = (Region *)((int8_t *)dest + structOffset_);
		Region *xSrc_region = (Region *)((int8_t *)xSrc + structOffset_);
		Region *ySrc_region = (Region *)((int8_t *)ySrc + structOffset_);
		destRegion->input[Dir::X] = xSrc_region->input[Dir::X];
		destRegion->crop[Dir::X] = xSrc_region->crop[Dir::X];
		destRegion->output[Dir::X] = xSrc_region->output[Dir::X];
		destRegion->input[Dir::Y] = ySrc_region->input[Dir::Y];
		destRegion->crop[Dir::Y] = ySrc_region->crop[Dir::Y];
		destRegion->output[Dir::Y] = ySrc_region->output[Dir::Y];
	}
}

BasicStage::BasicStage(char const *name, Pipeline *pipeline, Stage *upstream, int structOffset)
	: Stage(name, pipeline, structOffset), upstream_(upstream), downstream_(nullptr)
{
	if (upstream)
		upstream->setDownstream(this);
}

Length2 BasicStage::getInputImageSize() const
{
	return upstream_->getOutputImageSize();
}

Length2 BasicStage::getOutputImageSize() const
{
	return getInputImageSize();
}

void BasicStage::setDownstream(Stage *downstream)
{
	downstream_ = downstream;
}

void BasicStage::reset()
{
	inputInterval_ = Interval(0, 0);
	crop_ = Crop(0, 0);
	outputInterval_ = Interval(0, 0);
}

void BasicStage::copyOut(void *dest, Dir dir)
{
	if (structOffset_ >= 0)
	{
		Region *region = (Region *)((uint8_t *)dest + structOffset_);

		PISP_LOG(debug, "(" << name_ << ") complete: " << getBranchComplete() << " inactive: " << getBranchInactive());
		if (getBranchComplete() || getBranchInactive())
			BasicStage::reset();

		region->input[dir] = inputInterval_;
		region->crop[dir] = crop_;
		region->output[dir] = outputInterval_;
	}
}

bool BasicStage::getBranchComplete() const
{
	return downstream_->getBranchComplete();
}

bool BasicStage::getBranchInactive() const
{
	if (!upstream_)
		return false;
	return upstream_->getBranchInactive();
}
