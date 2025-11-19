/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * contextStage.hpp - Tiling library component for context stages
 */
#pragma once

#include "stages.hpp"

namespace tiling
{

class ContextStage : public BasicStage
{
public:
	struct Config
	{
		Config(Crop2 const &_context, Length2 const &_alignment)
			: context(_context), alignment(_alignment) {}
		Crop2 context;
		Length2 alignment;
	};
	ContextStage(char const *name, Stage *upstream, Config const &config, int structOffset);
	virtual void pushStartUp(int outputStart, Dir dir);
	virtual int pushEndDown(int inputEnd, Dir dir);
	virtual void pushEndUp(int outputEnd, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling