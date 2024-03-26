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
	virtual void pushStartUp(int output_start, Dir dir);
	virtual int pushEndDown(int input_end, Dir dir);
	virtual void pushEndUp(int output_end, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);

private:
	Config config_;
};

} // namespace tiling
