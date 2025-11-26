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
		Config(Length2 const &output_image_size, Length2 const &scale, Length2 const &start_context,
			   Length2 const &end_context, uint8_t precision, RescalerType rescaler_type)
			: output_image_size(output_image_size), scale(scale), start_context(start_context),
			  end_context(end_context), precision(precision), rescaler_type(rescaler_type)
		{
		}
		Length2 output_image_size;
		Length2 scale;
		Length2 start_context;
		Length2 end_context;
		uint8_t precision;
		RescalerType rescaler_type;
	};

	RescaleStage(char const *name, Stage *upstream, Config const &config, int struct_offset);
	virtual Length2 get_output_image_size() const;
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);

private:
	Config config_;
	int round_up;
};

} // namespace tiling
