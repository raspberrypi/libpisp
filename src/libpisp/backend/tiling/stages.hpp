/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * stage.cpp - Tiling library stage definition
 */
#pragma once

#include <string>

#include "types.hpp"

namespace tiling
{

class Pipeline;

class TilingException : public std::exception
{
public:
	char const *what() const noexcept { return "Tiling Failed"; }
};

class Stage
{
public:
	Stage(char const *name, Pipeline *pipeline, int struct_offset);
	virtual ~Stage() {}
	Pipeline *get_pipeline() const;
	virtual Length2 get_input_image_size() const = 0;
	virtual Length2 get_output_image_size() const = 0;
	virtual void set_downstream(Stage *downstream) = 0;
	virtual void reset() = 0;
	virtual void push_start_up(int output_start, Dir dir) = 0;
	virtual int push_end_down(int input_end, Dir dir) = 0;
	virtual void push_end_up(int output_end, Dir dir) = 0;
	virtual void push_crop_down(Interval interval, Dir dir) = 0;
	virtual void copy_out(void *dest, Dir dir) = 0;
	virtual bool get_branch_complete() const = 0;
	virtual bool get_branch_inactive() const = 0;
	void merge_regions(void *dest, void *x_src, void *y_src) const;

protected:
	std::string name_;
	Pipeline *pipeline_;
	int struct_offset_;
};

// BasicStage is a Stage that has a single upstream and a single downstream stage (though they can be NULL).

class BasicStage : public Stage
{
public:
	BasicStage(char const *name, Pipeline *pipeline, Stage *upstream, int struct_offset);
	virtual Length2 get_input_image_size() const;
	virtual Length2 get_output_image_size() const;
	virtual void set_downstream(Stage *downstream);
	virtual void reset();
	virtual void copy_out(void *dest, Dir dir);
	virtual bool get_branch_complete() const;
	virtual bool get_branch_inactive() const;

protected:
	Stage *upstream_;
	Stage *downstream_;
	Interval input_interval_;
	Crop crop_;
	Interval output_interval_;
};

} // namespace tiling