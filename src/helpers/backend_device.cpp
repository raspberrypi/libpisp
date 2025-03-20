/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.cpp - PiSP Backend device helper
 */

#include <cstdint>
#include <cstring>

#include "libpisp/common/utils.hpp"

#include "backend_device.hpp"

using namespace libpisp::helpers;

BackendDevice::BackendDevice(const std::string &device)
	: valid_(true)
{
	nodes_ = MediaDevice().OpenV4l2Nodes(device);
	if (nodes_.empty())
		valid_ = false;

	// Allocate a config buffer to persist.
	nodes_.at("pispbe-config").RequestBuffers(1);
	nodes_.at("pispbe-config").StreamOn();
	config_buffer_ = nodes_.at("pispbe-config").GetBuffer().value();
}

BackendDevice::~BackendDevice()
{
	nodes_.at("pispbe-config").StreamOff();
}

void BackendDevice::Setup(const pisp_be_tiles_config &config)
{
	nodes_enabled_.clear();

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_INPUT)
	{
		const pisp_image_format_config &f = config.config.input_format;
		nodes_.at("pispbe-input").SetFormat(f.width, f.height, f.stride, f.stride2,
											libpisp::get_pisp_image_format(f.format));
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-input").ReleaseBuffers();
		nodes_.at("pispbe-input").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-input");
		buffers_["pispbe-input"] = nodes_.at("pispbe-input").GetBuffer().value();
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT0)
	{
		const pisp_image_format_config &f = config.config.output_format[0].image;
		nodes_.at("pispbe-output0").SetFormat(f.width, f.height, f.stride, f.stride2,
											  libpisp::get_pisp_image_format(f.format));
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output0").ReleaseBuffers();
		nodes_.at("pispbe-output0").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-output0");
		buffers_["pispbe-output0"] = nodes_.at("pispbe-output0").GetBuffer().value();
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT1)
	{
		const pisp_image_format_config &f = config.config.output_format[1].image;
		nodes_.at("pispbe-output1").SetFormat(f.width, f.height, f.stride, f.stride2,
											  libpisp::get_pisp_image_format(f.format));
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output1").ReleaseBuffers();
		nodes_.at("pispbe-output1").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-output1");
		buffers_["pispbe-output1"] = nodes_.at("pispbe-output1").GetBuffer().value();
	}

	std::memcpy(reinterpret_cast<pisp_be_tiles_config *>(config_buffer_.mem[0]), &config, sizeof(config));
}

int BackendDevice::Run()
{
	int ret = 0;

	for (auto const &n : nodes_enabled_)
	{
		nodes_.at(n).StreamOn();
		if (nodes_.at(n).QueueBuffer(buffers_.at(n).buffer.index))
			ret = -1;
	}

	// Triggers the HW job.
	if (nodes_.at("pispbe-config").QueueBuffer(config_buffer_.buffer.index))
		ret = -1;

	for (auto const &n : nodes_enabled_)
	{
		if (nodes_.at(n).DequeueBuffer(1000) < 0)
			ret = -1;
	}

	for (auto const &n : nodes_enabled_)
		nodes_.at(n).StreamOff();

	return ret;
}
