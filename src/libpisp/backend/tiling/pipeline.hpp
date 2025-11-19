/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pipeline.hpp - Tiling library pipeline generator
 */
#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace tiling
{

class Stage;
class InputStage;
class OutputStage;

class Pipeline
{
public:
	struct Config
	{
		Config(Length2 const &maxTileSize, Length2 const &minTileSize)
			: maxTileSize(maxTileSize), minTileSize(minTileSize)
		{
		}
		Length2 maxTileSize;
		Length2 minTileSize;
	};

	Pipeline(char const *name, Config const &config);
	Config const &getConfig() const;
	void addStage(Stage *stage);
	void addInputStage(InputStage *inputStage);
	void addOutputStage(OutputStage *outputStage);
	void tile(void *mem, size_t numItems, size_t itemSize, Length2 *grid);
	bool firstTile() const { return firstTile_; }

private:
	int tileDirection(Dir dir, void *mem, size_t numItems, size_t itemSize);
	void reset();
	std::string name_;
	Config config_;
	std::vector<Stage *> stages_;
	std::vector<InputStage *> inputs_;
	std::vector<OutputStage *> outputs_;
	bool firstTile_;
};

} // namespace tiling