/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * pisp_tiling.cpp - Tiling library top level
 */
#include "pisp_tiling.hpp"

#include <memory>

#include "common/logging.hpp"

#include "context_stage.hpp"
#include "crop_stage.hpp"
#include "dedistort_stage.hpp"
#include "input_stage.hpp"
#include "output_stage.hpp"
#include "pipeline.hpp"
#include "rescale_stage.hpp"
#include "split_stage.hpp"

using namespace tiling;

namespace
{

constexpr unsigned int PipelineContextX = 16;
constexpr unsigned int PipelineContextY = 16;
constexpr unsigned int PipelineAlignX = 2;
constexpr unsigned int PipelineAlignY = 2;
constexpr unsigned int CompressionAlign = 8;

// Resampling parameters
constexpr unsigned int StartContext = 2;
constexpr unsigned int EndContext = 3;
constexpr unsigned int ScalePrecision = 12;
constexpr unsigned int RoundUp = (1 << ScalePrecision) - 1;

} // namespace

namespace libpisp
{

void tilePipeline(TilingConfig const &config, Tile *tiles, int numTiles, Length2 *grid)
{
	PISP_LOG(info, config);

	// First set up the pipeline.
	Pipeline::Config pipelineConfig(config.maxTileSize, config.minTileSize);
	Pipeline pipeline("PiSP", pipelineConfig);

	InputStage::Config inputConfig(config.inputImageSize, config.inputAlignment,
									config.compressedInput ? CompressionAlign : 0);
	InputStage inputStage("input", &pipeline, inputConfig, offsetof(Tile, input));

	DedistortStage::Config dedistortConfig(config.dedistortPwls);
	DedistortStage dedistortStage("dedistort", &inputStage, dedistortConfig, offsetof(Tile, dedistort));
	
	ContextStage::Config contextConfig(Crop2(Crop(PipelineContextX), Crop(PipelineContextY)),
										Length2(PipelineAlignX, PipelineAlignY));
	ContextStage contextStage("context", &dedistortStage, contextConfig, offsetof(Tile, context));

	SplitStage splitStage("split", &contextStage);

	std::unique_ptr<Stage> cropStages[NumOutputBranches];
	std::unique_ptr<Stage> downscaleStages[NumOutputBranches];
	std::unique_ptr<Stage> resampleStages[NumOutputBranches];
	std::unique_ptr<Stage> outputStages[NumOutputBranches];

	for (int i = 0; i < NumOutputBranches; i++)
	{
		Length2 const &outputImageSize = config.outputImageSize[i];
		// A zero-dimension output disables that branch.
		if (outputImageSize.dx == 0 || outputImageSize.dy == 0)
			continue;
		char name[32];
		Stage *prevStage = &splitStage;

		sprintf(name, "crop%d", i);
		cropStages[i] = std::unique_ptr<Stage>(
			new CropStage(name, prevStage, config.crop[i], offsetof(Tile, crop) + i * sizeof(Region)));
		prevStage = cropStages[i].get();

		// There's a little awkwardness if the resize blocks (downscale and resample) are not enabled. Resize *does* change the output tile
		// size, even if it's doing a 1-to-1 scaling (it loses context), so we must leave it out of the tiling
		// calculation.
		if (config.downscaleEnables & (1 << i))
		{
			sprintf(name, "downscale%d", i);
			Length2 const &downscaleImageSize = config.downscaleImageSize[i];
			// There is only right context here - and it is the scale factor rounded up.
			Length2 contextRight = Length2(((config.downscaleFactor[i][Dir::X] + RoundUp) >> ScalePrecision) - 1,
											((config.downscaleFactor[i][Dir::Y] + RoundUp) >> ScalePrecision) - 1);
			RescaleStage::Config downscaleConfig(downscaleImageSize, config.downscaleFactor[i], Length2(0, 0),
												  contextRight, ScalePrecision,
												  RescaleStage::RescalerType::Downscaler);
			downscaleStages[i] = std::unique_ptr<Stage>(
				new RescaleStage(name, prevStage, downscaleConfig, offsetof(Tile, downscale) + i * sizeof(Region)));
			prevStage = downscaleStages[i].get();
		}
		if (config.resampleEnables & (1 << i))
		{
			sprintf(name, "resample%d", i);
			RescaleStage::Config resampleConfig(outputImageSize, config.resampleFactor[i],
												 Length2(StartContext, StartContext), Length2(EndContext, EndContext),
												 ScalePrecision, RescaleStage::RescalerType::Resampler);
			resampleStages[i] = std::unique_ptr<Stage>(
				new RescaleStage(name, prevStage, resampleConfig, offsetof(Tile, resample) + i * sizeof(Region)));
			prevStage = resampleStages[i].get();
		}

		sprintf(name, "output%d", i);
		OutputStage::Config outputConfig(config.outputMaxAlignment[i], config.outputMinAlignment[i],
										  config.outputHMirror[i]);
		outputStages[i] = std::unique_ptr<Stage>(
			new OutputStage(name, prevStage, outputConfig, offsetof(Tile, output) + i * sizeof(Region)));
	}

	// Now tile it.
	pipeline.tile(tiles, numTiles, sizeof(Tile), grid);
	PISP_LOG(info, "Made " << grid->dx << "x" << grid->dy << " tiles");
}

} // namespace libpisp
