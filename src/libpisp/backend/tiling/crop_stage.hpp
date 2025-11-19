/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * crop_stage.hpp - Tiling library component for crop stages
 */
#pragma once

#include "stages.hpp"

namespace tiling
{

class CropStage : public BasicStage
{
public:
	struct Config
	{
		Config(Interval2 const &_crop) : crop(_crop) {}
		Interval2 crop;
	};
	CropStage(char const *name, Stage *upstream, Config const &config, int structOffset);
	virtual Length2 getOutputImageSize() const override;
	virtual void pushStartUp(int outputStart, Dir dir) override;
	virtual int pushEndDown(int inputEnd, Dir dir) override;
	virtual void pushEndUp(int outputEnd, Dir dir) override;
	virtual void pushCropDown(Interval interval, Dir dir) override;
	bool getBranchInactive() const override;

private:
	Config config_;
};

} // namespace tiling