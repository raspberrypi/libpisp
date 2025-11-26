/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pipeline.cpp - Tiling library pipeline generator
 */
#include "pipeline.hpp"

#include "common/logging.hpp"

#include "input_stage.hpp"
#include "output_stage.hpp"
#include "stages.hpp"

#include <cstdint>

using namespace tiling;

Pipeline::Pipeline(char const *name, Config const &config) : name_(name), config_(config), first_tile_(false)
{
}

Pipeline::Config const &Pipeline::get_config() const
{
	return config_;
}

void Pipeline::add_stage(Stage *stage)
{
	stages_.push_back(stage);
}

void Pipeline::add_input_stage(InputStage *input_stage)
{
	inputs_.push_back(input_stage);
}

void Pipeline::add_output_stage(OutputStage *output_stage)
{
	outputs_.push_back(output_stage);
}

void Pipeline::tile(void *mem, size_t num_items, size_t item_size, Length2 *grid)
{
	// Tiling produces a rectangular X by Y grid. We create X direction tiles along the first row,
	// then Y direction tiles down the column. Finally we copy the X/Y information to all the other
	// tiles in the grid.
	grid->dx = tile_direction(Dir::X, mem, num_items, item_size);
	grid->dy = tile_direction(Dir::Y, mem, num_items / grid->dx, item_size * grid->dx);
	int i, j;
	for (j = 0; j < grid->dy; j++)
	{
		void *y_src = (uint8_t *)mem + item_size * grid->dx * j;
		for (i = 0; i < grid->dx; i++)
		{
			void *x_src = (uint8_t *)mem + item_size * i;
			void *dest = (uint8_t *)y_src + item_size * i;
			for (auto s : stages_)
				s->merge_regions(dest, x_src, y_src);
		}
	}
}

void Pipeline::reset()
{
	for (auto s : stages_)
		s->reset();
}

int Pipeline::tile_direction(Dir dir, void *mem, size_t num_items, size_t item_size)
{
	PISP_LOG(debug, "Tiling direction " << dir);

	reset();
	bool done = false;
	unsigned int num_tiles = 0;
	first_tile_ = true;
	for (; !done; num_tiles++)
	{
		PISP_LOG(debug, "----------------------------------------------------------------");
		if (num_tiles == num_items)
			throw std::runtime_error("Too many tiles!");
		for (auto s : outputs_)
		{
			if (!s->get_branch_complete())
				s->push_start_up(s->get_output_interval().end(), dir);
		}
		for (auto s : inputs_)
			s->push_end_down(s->get_input_interval().offset + config_.max_tile_size[dir], dir);
		for (auto s : inputs_)
			s->push_crop_down(s->get_input_interval(), dir);
		void *dest = (uint8_t *)mem + num_tiles * item_size;
		for (auto s : stages_)
			s->copy_out(dest, dir);
		done = true;
		for (auto s : outputs_)
		{
			if (s->get_branch_complete())
				continue;
			else if (s->get_output_interval().end() >= s->get_output_image_size()[dir])
				s->set_branch_complete();
			else
				done = false;
		}
		first_tile_ = false;
	}

	PISP_LOG(debug, "Made " << num_tiles << " tiles in direction " << dir);
	return num_tiles;
}
