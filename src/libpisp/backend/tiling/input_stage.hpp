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
		Config(Length2 const &input_image_size, Length2 const &alignment, int compression_alignment = 0)
			: input_image_size(input_image_size), alignment(alignment), compression_alignment(compression_alignment)
		{
		}
		Length2 input_image_size;
		Length2 alignment;
		int compression_alignment;
	};
	InputStage(char const *name, Pipeline *pipeline, Config const &config, int struct_offset);
	virtual Length2 get_input_image_size() const;
	virtual Interval get_input_interval() const;
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling