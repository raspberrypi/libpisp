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
		Config(Length2 const &maxAlignment, Length2 const &minAlignment, bool xMirrored)
			: maxAlignment(maxAlignment), minAlignment(minAlignment), xMirrored(xMirrored)
		{
		}
		Length2 maxAlignment;
		Length2 minAlignment;
		bool xMirrored;
	};
	OutputStage(char const *name, Stage *upstream, Config const &config, int structOffset);
	virtual Interval getOutputInterval() const;
	virtual void pushStartUp(int outputStart, Dir dir);
	virtual int pushEndDown(int inputEnd, Dir dir);
	virtual void pushEndUp(int outputEnd, Dir dir);
	virtual void pushCropDown(Interval interval, Dir dir);
	virtual void reset();
	bool getBranchComplete() const;
	void setBranchComplete();

private:
	Config config_;
	bool branchComplete_;
};

} // namespace tiling