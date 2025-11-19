/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * rescale_stage.hpp - Tiling library component for rescaling stages
 */
#pragma once

#include "stages.hpp"

#include <cstdint>

namespace tiling
{

class RescaleStage : public BasicStage
{
public:
	enum class RescalerType
	{
		Downscaler,
		Resampler
	};
	struct Config
	{
		Config(Length2 const &outputImageSize, Length2 const &scale, Length2 const &startContext,
			   Length2 const &endContext, uint8_t precision, RescalerType rescalerType)
			: outputImageSize(outputImageSize), scale(scale), startContext(startContext),
			  endContext(endContext), precision(precision), rescalerType(rescalerType)
		{
		}
		Length2 outputImageSize;
		Length2 scale;
		Length2 startContext;
		Length2 endContext;
		uint8_t precision;
		RescalerType rescalerType;
	};

	RescaleStage(char const *name, Stage *upstream, Config const &config, int structOffset);
	virtual Length2 getOutputImageSize() const;
	virtual void pushStartUp(int outputStart, Dir dir);
	virtual int pushEndDown(int inputEnd, Dir dir);
	virtual void pushEndUp(int outputEnd, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);

private:
	Config config_;
	int roundUp;
};

} // namespace tiling
