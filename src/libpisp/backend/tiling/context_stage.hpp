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
	ContextStage(char const *name, Stage *upstream, Config const &config, int struct_offset);
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling