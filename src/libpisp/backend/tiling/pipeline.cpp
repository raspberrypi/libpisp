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

Pipeline::Pipeline(char const *name, Config const &config) : name_(name), config_(config), firstTile_(false)
{
}

Pipeline::Config const &Pipeline::getConfig() const
{
	return config_;
}

void Pipeline::addStage(Stage *stage)
{
	stages_.push_back(stage);
}

void Pipeline::addInputStage(InputStage *inputStage)
{
	inputs_.push_back(inputStage);
}

void Pipeline::addOutputStage(OutputStage *output_stage)
{
	outputs_.push_back(output_stage);
}

void Pipeline::tile(void *mem, size_t num_items, size_t item_size, Length2 *grid)
{
	// Tiling produces a rectangular X by Y grid. We create X direction tiles along the first row,
	// then Y direction tiles down the column. Finally we copy the X/Y information to all the other
	// tiles in the grid.
	grid->dx = tileDirection(Dir::X, mem, num_items, item_size);
	grid->dy = tileDirection(Dir::Y, mem, num_items / grid->dx, item_size * grid->dx);
	int i, j;
	for (j = 0; j < grid->dy; j++)
	{
		void *ySrc = (uint8_t *)mem + item_size * grid->dx * j;
		for (i = 0; i < grid->dx; i++)
		{
			void *xSrc = (uint8_t *)mem + item_size * i;
			void *dest = (uint8_t *)ySrc + item_size * i;
			for (auto s : stages_)
				s->mergeRegions(dest, xSrc, ySrc);
		}
	}
}

void Pipeline::reset()
{
	for (auto s : stages_)
		s->reset();
}

int Pipeline::tileDirection(Dir dir, void *mem, size_t num_items, size_t item_size)
{
	PISP_LOG(debug, "Tiling direction " << dir);

	reset();
	bool done = false;
	unsigned int numTiles = 0;
	firstTile_ = true;
	for (; !done; numTiles++)
	{
		PISP_LOG(debug, "----------------------------------------------------------------");
		if (numTiles == num_items)
			throw std::runtime_error("Too many tiles!");
		for (auto s : outputs_)
		{
			if (!s->getBranchComplete())
				s->pushStartUp(s->getOutputInterval().end(), dir);
		}
		for (auto s : inputs_)
			s->pushEndDown(s->getInputInterval().offset + config_.maxTileSize[dir], dir);
		for (auto s : inputs_)
			s->pushCropDown(s->getInputInterval(), dir);
		void *dest = (uint8_t *)mem + numTiles * item_size;
		for (auto s : stages_)
			s->copyOut(dest, dir);
		done = true;
		for (auto s : outputs_)
		{
			if (s->getBranchComplete())
				continue;
			else if (s->getOutputInterval().end() >= s->getOutputImageSize()[dir])
				s->setBranchComplete();
			else
				done = false;
		}
		firstTile_ = false;
	}

	PISP_LOG(debug, "Made " << numTiles << " tiles in direction " << dir);
	return numTiles;
}
