/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_tiling.hpp - Tiling library top level
 */
#pragma once

// Some data structures and an API function to make it easier to tile up and image for a PiSP-like pipeline.

#include <iostream>

#include "common/pisp_pwl.hpp"
#include "types.hpp"

namespace libpisp
{

constexpr int NumOutputBranches = 2;

struct Tile
{
	tiling::Region input;
	tiling::Region dedistort;
	tiling::Region context;
	tiling::Region crop[NumOutputBranches];
	tiling::Region downscale[NumOutputBranches];
	tiling::Region resample[NumOutputBranches];
	tiling::Region output[NumOutputBranches];
};

struct TilingConfig
{
	tiling::Length2 inputImageSize;
	tiling::Interval2 crop[NumOutputBranches];
	tiling::Length2 downscaleImageSize[NumOutputBranches];
	tiling::Length2 outputImageSize[NumOutputBranches];
	tiling::Length2 maxTileSize;
	tiling::Length2 minTileSize;
	tiling::Length2 downscaleFactor[NumOutputBranches];
	tiling::Length2 resampleFactor[NumOutputBranches];
	bool outputHMirror[NumOutputBranches];
	int resampleEnables;
	int downscaleEnables;
	bool compressedInput;
	tiling::Length2 inputAlignment;
	tiling::Length2 outputMaxAlignment[NumOutputBranches]; // "preferred" alignment
	tiling::Length2 outputMinAlignment[NumOutputBranches]; // "required" minimum alignment
	Pwl dedistortPwls[3];
};

inline std::ostream &operator<<(std::ostream &os, TilingConfig const &tc)
{
	os << "TilingConfig:" << std::endl;
	os << "\tinputImageSize " << tc.inputImageSize << " align " << tc.inputAlignment << std::endl;
	for (int i = 0; i < NumOutputBranches; i++)
	{
		os << "\tcrop[" << i << "] " << tc.crop[i] << std::endl;
		os << "\toutputImageSize[" << i << "] " << tc.outputImageSize[i] << " align max "
		   << tc.outputMaxAlignment[i] << " min " << tc.outputMinAlignment[i] << std::endl;
		os << "\tdownscaleImageSize " << tc.downscaleImageSize[i] << " downscaleFactor " << tc.downscaleFactor[i]
		   << " resampleFactor " << tc.resampleFactor[i] << std::endl;
	}
	os << "\tenables resample " << tc.resampleEnables << " downscale " << tc.downscaleEnables << std::endl;
	return os << "\tmaxTileSize " << tc.maxTileSize;
}

void tilePipeline(TilingConfig const &config, Tile *tile, int numTile, tiling::Length2 *grid);

} // namespace libpisp
