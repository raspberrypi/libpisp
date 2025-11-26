/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * split_stage.hpp - Tiling library component for splitter stages
 */
#pragma once

#include <vector>

#include "stages.hpp"

namespace tiling
{

class SplitStage : public Stage
{
public:
	SplitStage(char const *name, Stage *upstream);
	virtual Length2 get_input_image_size() const;
	virtual Length2 get_output_image_size() const;
	virtual void set_downstream(Stage *downstream);
	virtual void reset();
	virtual void push_start_up(int output_start, Dir dir);
	virtual int push_end_down(int input_end, Dir dir);
	virtual void push_end_up(int output_end, Dir dir);
	virtual void push_crop_down(Interval interval, Dir dir);
	virtual void copy_out(void *dest, Dir dir);
	virtual bool get_branch_complete() const;
	virtual bool get_branch_inactive() const;

private:
	Stage *upstream_;
	std::vector<Stage *> downstream_;
	Interval input_interval_;
	unsigned int count_;
};

} // namespace tiling