/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * rescale_stage.cpp - Tiling library component for rescaling stages
 */
#include "rescale_stage.hpp"

#include "common/logging.hpp"

#include "pipeline.hpp"

using namespace tiling;

RescaleStage::RescaleStage(char const *name, Stage *upstream, Config const &config, int structOffset)
	: BasicStage(name, upstream->getPipeline(), upstream, structOffset), config_(config)
{
	roundUp = (1 << config.precision) - 1;
}

Length2 RescaleStage::getOutputImageSize() const
{
	return config_.outputImageSize;
}

// In what follows, the suffix _P indicates that a variable is a fixed point value,left-shifted by PRECISION.
// We also use _w_context and _no_context to denote input coordinates where, respectively, the context pixels
// required by the resample filter either are or are not included.

void RescaleStage::pushStartUp(int outputStart, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputStart " << outputStart);

	int input_start_P = outputStart * config_.scale[dir];
	int input_start = input_start_P >> config_.precision;
	int input_start_w_context = input_start - config_.startContext[dir];
	// input_start_w_context is allowed to go negative here if, for example, the branch starts producing output
	// on the second tile in a row/column and the resampler requires left context pixels. In such cases, the left/top
	// edge flag will not be set on the tile and the hardware cannot remove the left/top context pixels. So allow
	// negative values on all but the left/top edge tile.
	if (getPipeline()->firstTile() && input_start_w_context < 0)
		input_start_w_context = 0;
	outputInterval_.offset = outputStart;
	inputInterval_.offset = input_start_w_context;

	PISP_LOG(debug, "(" << name_ << ") Exit with input_start " << input_start_w_context);
	upstream_->pushStartUp(input_start_w_context, dir);
}

// Furthermore, when we're talking about end pixels we need to be careful whether we're talking inclusively
// or exclusively. For example, the End() pixel of an interval is an exclusive pixel number, so End()-1 is
// actually the last pixel still in the interval. We'll denote these with the suffices _exc or _inc.

int RescaleStage::pushEndDown(int inputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with inputEnd " << inputEnd);

	int inputImageSize = getInputImageSize()[dir];
	inputInterval_.setEnd(inputEnd);
	int outputEnd_exc;

	if (config_.rescalerType == RescalerType::Downscaler)
	{
		// Trapezoidal downscaler has a variable-sized kernel. Round down its end
		// position to get the number of complete output samples that can be generated
		// (provided scale was rounded down, there should be no shortage of input).
		outputEnd_exc = (inputEnd << config_.precision) / config_.scale[dir];
	}
	else
	{
		// Resampler: find the last ("inclusive") sample that can be generated.
		// Take off context plus an extra 2 pixels off to allow for an initial phase
		// (except that at the bottom of the image, no more context is available).
		int inputEnd_inc = inputEnd - 1;
		int inputEnd_inc_no_context = inputEnd_inc;
		if (inputEnd < inputImageSize)
		{
			inputEnd_inc_no_context = inputEnd_inc - config_.endContext[dir] - 2;
		}
		int inputEnd_inc_no_context_P = inputEnd_inc_no_context << config_.precision;
		int outputEnd_inc = (inputEnd_inc_no_context_P + roundUp) / config_.scale[dir];
		outputEnd_exc = outputEnd_inc + 1;
	}

	if (outputEnd_exc > config_.outputImageSize[dir])
		outputEnd_exc = config_.outputImageSize[dir];

	// Upscaling could generate larger output tiles than we can handle, so avoid doing that!
	if (outputEnd_exc > outputInterval_.offset + getPipeline()->getConfig().maxTileSize[dir])
		outputEnd_exc = getPipeline()->getConfig().maxTileSize[dir] + outputInterval_.offset;

	outputInterval_.setEnd(outputEnd_exc);

	PISP_LOG(debug, "(" << name_ << ") Exit with outputEnd " << outputEnd_exc);
	pushEndUp(downstream_->pushEndDown(outputEnd_exc, dir), dir);

	// If we didn't quite finish the output then we can't get too close to the end of the input because another tile will be
	// needed, and we can't let that become infeasibly small. So pull our inputEnd back and simply try again.
	if (outputInterval_.end() < config_.outputImageSize[dir] &&
		inputInterval_.end() > inputImageSize - getPipeline()->getConfig().minTileSize[dir])
	{
		PISP_LOG(debug, "(" << name_ << ") Too close to input image edge - try again");
		pushEndDown(inputImageSize - getPipeline()->getConfig().minTileSize[dir], dir);
	}

	return inputInterval_.end();
}

void RescaleStage::pushEndUp(int outputEnd, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with outputEnd " << outputEnd);

	int inputEnd_w_context_exc;
	if (config_.rescalerType == RescalerType::Downscaler)
	{
		// Trapezoidal downscaler has a variable-sized kernel. Round up its fractional end position.
		int inputEnd_exc_P = outputEnd * config_.scale[dir];
		inputEnd_w_context_exc = (inputEnd_exc_P + roundUp) >> config_.precision;
	}
	else
	{
		// Resampler has a fixed-sized context, so calculations are based on the start position
		// for its final output sample ("inclusive" dimensions). Need 2 more pixels to the end
		// to allow for an initial phase that pushes us to use up to 2 extra samples on the right.
		int outputEnd_inc = outputEnd - 1;
		int inputEnd_P = outputEnd_inc * config_.scale[dir];
		int inputEnd = inputEnd_P >> config_.precision;
		int inputEnd_w_context = inputEnd + config_.endContext[dir] + 2;
		inputEnd_w_context_exc = inputEnd_w_context + 1;
	}

	Length2 inputImageSize(getInputImageSize());
	if (inputEnd_w_context_exc > inputImageSize[dir])
		inputEnd_w_context_exc = inputImageSize[dir];

	outputInterval_.setEnd(outputEnd);
	inputInterval_.setEnd(inputEnd_w_context_exc);

	PISP_LOG(debug, "(" << name_ << ") Exit with inputEnd " << inputEnd_w_context_exc);
}

void RescaleStage::pushCropDown(Interval interval, Dir dir)
{
	PISP_LOG(debug, "(" << name_ << ") Enter with interval " << interval);

	PISP_ASSERT(interval > inputInterval_);
	crop_ = interval - inputInterval_;
	inputInterval_ = interval;

	PISP_LOG(debug, "(" << name_ << ") Exit with interval " << outputInterval_);
	downstream_->pushCropDown(outputInterval_, dir);
}
