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
	config_buffer_ = nodes_.at("pispbe-config").AcquireBuffer().value();
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
		nodes_.at("pispbe-input").ReturnBuffers();
		nodes_.at("pispbe-input").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-input");
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT0)
	{
		const pisp_image_format_config &f = config.config.output_format[0].image;
		nodes_.at("pispbe-output0").SetFormat(f.width, f.height, f.stride, f.stride2,
											  libpisp::get_pisp_image_format(f.format));
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output0").ReturnBuffers();
		nodes_.at("pispbe-output0").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-output0");
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT1)
	{
		const pisp_image_format_config &f = config.config.output_format[1].image;
		nodes_.at("pispbe-output1").SetFormat(f.width, f.height, f.stride, f.stride2,
											  libpisp::get_pisp_image_format(f.format));
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output1").ReturnBuffers();
		nodes_.at("pispbe-output1").RequestBuffers(1);
		nodes_enabled_.emplace("pispbe-output1");
	}

	std::memcpy(reinterpret_cast<pisp_be_tiles_config *>(config_buffer_.mem[0]), &config, sizeof(config));
}

std::map<std::string, V4l2Device::Buffer> BackendDevice::AcquireBuffers()
{
	std::map<std::string, V4l2Device::Buffer> buffers;

	for (auto const &n : nodes_enabled_)
		buffers[n] = nodes_.at(n).AcquireBuffer().value();

	return buffers;
}

void BackendDevice::ReleaseBuffer(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	for (auto const &[n, b] : buffers)
		nodes_.at(n).ReleaseBuffer(b);
}

int BackendDevice::Run(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	int ret = 0;

	for (auto const &n : nodes_enabled_)
	{
		nodes_.at(n).StreamOn();
		if (nodes_.at(n).QueueBuffer(buffers.at(n).buffer.index))
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
