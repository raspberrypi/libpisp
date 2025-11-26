/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * output_stage.hpp - Tiling library component for output stages
 */
#pragma once

#include "stages.hpp"

namespace tiling
{

class OutputStage : public BasicStage
{
public:
	struct Config
	{
		Config(Length2 const &max_alignment, Length2 const &min_alignment, bool x_mirrored)
			: max_alignment(max_alignment), min_alignment(min_alignment), x_mirrored(x_mirrored)
		{
		}
		Length2 max_alignment;
		Length2 min_alignment;
		bool x_mirrored;
	};
	OutputStage(char const *name, Stage *upstream, Config const &config, int struct_offset);
	virtual Interval get_output_interval() const;
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);
	virtual void reset();
	bool get_branch_complete() const;
	void set_branch_complete();

private:
	Config config_;
	bool branch_complete_;
};

} // namespace tiling