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
	virtual Length2 getInputImageSize() const;
	virtual Length2 getOutputImageSize() const;
	virtual void setDownstream(Stage *downstream);
	virtual void reset();
	virtual void pushStartUp(int outputStart, Dir dir);
	virtual int pushEndDown(int inputEnd, Dir dir);
	virtual void pushEndUp(int outputEnd, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);
	virtual void copyOut(void *dest, Dir dir);
	virtual bool getBranchComplete() const;
	virtual bool getBranchInactive() const;

private:
	Stage *upstream_;
	std::vector<Stage *> downstream_;
	Interval inputInterval_;
	unsigned int count_;
};

} // namespace tiling