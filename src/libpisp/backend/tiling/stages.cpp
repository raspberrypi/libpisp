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

Stage::Stage(char const *name, Pipeline *pipeline, int struct_offset)
	: name_(name), pipeline_(pipeline), struct_offset_(struct_offset)
{
	if (pipeline)
		pipeline->add_stage(this);
}

Pipeline *Stage::get_pipeline() const
{
	return pipeline_;
}

void Stage::merge_regions(void *dest, void *x_src, void *y_src) const
{
	if (struct_offset_ >= 0) // Hacky test to exclude stages that don't store a Region in each Tile
	{
		Region *dest_region = (Region *)((int8_t *)dest + struct_offset_);
		Region *x_src_region = (Region *)((int8_t *)x_src + struct_offset_);
		Region *y_src_region = (Region *)((int8_t *)y_src + struct_offset_);
		dest_region->input[Dir::X] = x_src_region->input[Dir::X];
		dest_region->crop[Dir::X] = x_src_region->crop[Dir::X];
		dest_region->output[Dir::X] = x_src_region->output[Dir::X];
		dest_region->input[Dir::Y] = y_src_region->input[Dir::Y];
		dest_region->crop[Dir::Y] = y_src_region->crop[Dir::Y];
		dest_region->output[Dir::Y] = y_src_region->output[Dir::Y];
	}
}

BasicStage::BasicStage(char const *name, Pipeline *pipeline, Stage *upstream, int struct_offset)
	: Stage(name, pipeline, struct_offset), upstream_(upstream), downstream_(nullptr)
{
	if (upstream)
		upstream->set_downstream(this);
}

Length2 BasicStage::get_input_image_size() const
{
	return upstream_->get_output_image_size();
}

Length2 BasicStage::get_output_image_size() const
{
	return get_input_image_size();
}

void BasicStage::set_downstream(Stage *downstream)
{
	downstream_ = downstream;
}

void BasicStage::reset()
{
	input_interval_ = Interval(0, 0);
	crop_ = Crop(0, 0);
	output_interval_ = Interval(0, 0);
}

void BasicStage::copy_out(void *dest, Dir dir)
{
	if (struct_offset_ >= 0)
	{
		Region *region = (Region *)((uint8_t *)dest + struct_offset_);

		PISP_LOG(debug, "(" << name_ << ") complete: " << get_branch_complete() << " inactive: " << get_branch_inactive());
		if (get_branch_complete() || get_branch_inactive())
			BasicStage::reset();

		region->input[dir] = input_interval_;
		region->crop[dir] = crop_;
		region->output[dir] = output_interval_;
	}
}

bool BasicStage::get_branch_complete() const
{
	return downstream_->get_branch_complete();
}

bool BasicStage::get_branch_inactive() const
{
	if (!upstream_)
		return false;
	return upstream_->get_branch_inactive();
}
