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
	Stage(char const *name, Pipeline *pipeline, int structOffset);
	virtual ~Stage() {}
	Pipeline *getPipeline() const;
	virtual Length2 getInputImageSize() const = 0;
	virtual Length2 getOutputImageSize() const = 0;
	virtual void setDownstream(Stage *downstream) = 0;
	virtual void reset() = 0;
	virtual void pushStartUp(int outputStart, Dir dir) = 0;
	virtual int pushEndDown(int inputEnd, Dir dir) = 0;
	virtual void pushEndUp(int outputEnd, Dir dir) = 0;
	virtual void pushCropDown(Interval interval, Dir dir) = 0;
	virtual void copyOut(void *dest, Dir dir) = 0;
	virtual bool getBranchComplete() const = 0;
	virtual bool getBranchInactive() const = 0;
	void mergeRegions(void *dest, void *xSrc, void *ySrc) const;

protected:
	std::string name_;
	Pipeline *pipeline_;
	int structOffset_;
};

// BasicStage is a Stage that has a single upstream and a single downstream stage (though they can be NULL).

class BasicStage : public Stage
{
public:
	BasicStage(char const *name, Pipeline *pipeline, Stage *upstream, int structOffset);
	virtual Length2 getInputImageSize() const;
	virtual Length2 getOutputImageSize() const;
	virtual void setDownstream(Stage *downstream);
	virtual void reset();
	virtual void copyOut(void *dest, Dir dir);
	virtual bool getBranchComplete() const;
	virtual bool getBranchInactive() const;

protected:
	Stage *upstream_;
	Stage *downstream_;
	Interval inputInterval_;
	Crop crop_;
	Interval outputInterval_;
};

} // namespace tiling