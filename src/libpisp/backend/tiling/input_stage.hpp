/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * inputStage.hpp - Tiling library component for input stages
 */
#pragma once

#include "stages.hpp"

namespace tiling
{

class InputStage : public BasicStage
{
public:
	struct Config
	{
		Config(Length2 const &inputImageSize, Length2 const &alignment, int compressionAlignment = 0)
			: inputImageSize(inputImageSize), alignment(alignment), compressionAlignment(compressionAlignment)
		{
		}
		Length2 inputImageSize;
		Length2 alignment;
		int compressionAlignment;
	};
	InputStage(char const *name, Pipeline *pipeline, Config const &config, int structOffset);
	virtual Length2 getInputImageSize() const;
	virtual Interval getInputInterval() const;
	virtual void pushStartUp(int outputStart, Dir dir);
	virtual int pushEndDown(int inputEnd, Dir dir);
	virtual void pushEndUp(int outputEnd, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling