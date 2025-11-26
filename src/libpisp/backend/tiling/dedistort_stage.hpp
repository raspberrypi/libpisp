/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * dedistort_stage.hpp - Tiling library component for dedistort stages
 */
#pragma once

#include "stages.hpp"

namespace libpisp
{
class Pwl;
}

namespace tiling
{

class DedistortStage : public BasicStage
{
public:
	struct Config
	{
		Config(libpisp::Pwl const pwl_array[3]) : pwls(pwl_array) {}
		libpisp::Pwl const *pwls;
	};
	DedistortStage(char const *name, Stage *upstream, Config const &config, int struct_offset);
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling
