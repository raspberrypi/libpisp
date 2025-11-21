/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.cpp - PiSP Backend device helper
 */

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "backend_device.hpp"
#include "libpisp/backend/backend_config_adapter.hpp"

using namespace libpisp::helpers;

BackendDevice::BackendDevice(const std::string &device)
	: valid_(true)
{
	mediaDev_ = MediaDevice().acquire(device);
	if (mediaDev_.empty())
		throw std::runtime_error("Unable to acquire any pisp_be device!");

	nodes_ = MediaDevice().openV4l2Nodes(mediaDev_);
	if (nodes_.empty())
		valid_ = false;

	// Allocate a config buffer to persist.
	nodes_.at("pispbe-config").requestBuffers(1);
	nodes_.at("pispbe-config").streamOn();
	configBuffer_ = nodes_.at("pispbe-config").acquireBuffer().value();

		const std::vector<libpisp::PiSPVariant> &variants = libpisp::getVariants();
	const media_device_info info = MediaDevice().deviceInfo(mediaDev_);
	auto variant = std::find_if(variants.begin(), variants.end(),
								[&info](const auto &v) { return v.backEndVersion() == info.hw_revision; });
	if (variant == variants.end())
		throw std::runtime_error("Backend hardware cound not be identified: " + std::to_string(info.hw_revision));

	be_ = std::make_unique<libpisp::BackEnd>(libpisp::BackEnd::SoftwareConfig({}), *variant);
}

BackendDevice::~BackendDevice()
{
	nodes_.at("pispbe-config").streamOff();
}

void BackendDevice::setup(unsigned int bufferCount, bool useOpaqueFormat)
{
	BeGlobal global = be_->getGlobal();

	pisp_be_tiles_config config = {};
	be_->prepare(&config);

	nodesEnabled_.clear();

	if ((global.rgbEnables & RgbEnable::Input) || (global.bayerEnables & BayerEnable::Input))
	{
		nodes_.at("pispbe-input").setFormat(config.config.input_format, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-input").returnBuffers();
		nodes_.at("pispbe-input").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-input");
	}

	if (global.rgbEnables & RgbEnable::Output0)
	{
		nodes_.at("pispbe-output0").setFormat(config.config.output_format[0].image, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output0").returnBuffers();
		nodes_.at("pispbe-output0").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-output0");
	}

	if (global.rgbEnables & RgbEnable::Output1)
	{
		nodes_.at("pispbe-output1").setFormat(config.config.output_format[1].image, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-output1").returnBuffers();
		nodes_.at("pispbe-output1").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-output1");
	}

	if (global.bayerEnables & BayerEnable::TdnInput)
	{
		nodes_.at("pispbe-tdn_input").setFormat(config.config.tdn_input_format, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-tdn_input").returnBuffers();
		nodes_.at("pispbe-tdn_input").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-tdn_input");
	}

	if (global.bayerEnables & BayerEnable::TdnOutput)
	{
		nodes_.at("pispbe-tdn_output").setFormat(config.config.tdn_output_format, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-tdn_output").returnBuffers();
		nodes_.at("pispbe-tdn_output").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-tdn_output");
	}

	if (global.bayerEnables & BayerEnable::StitchInput)
	{
		nodes_.at("pispbe-stitch_input").setFormat(config.config.stitch_input_format, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-stitch_input").returnBuffers();
		nodes_.at("pispbe-stitch_input").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-stitch_input");
	}

	if (global.bayerEnables & BayerEnable::StitchOutput)
	{
		nodes_.at("pispbe-stitch_output").setFormat(config.config.stitch_output_format, useOpaqueFormat);
		// Release old/allocate a single buffer.
		nodes_.at("pispbe-stitch_output").returnBuffers();
		nodes_.at("pispbe-stitch_output").requestBuffers(bufferCount);
		nodesEnabled_.emplace("pispbe-stitch_output");
	}

	std::memcpy(reinterpret_cast<pisp_be_tiles_config *>(configBuffer_.mem[0]), &config, sizeof(config));
}

std::map<std::string, V4l2Device::Buffer> BackendDevice::acquireBuffers()
{
	std::map<std::string, V4l2Device::Buffer> buffers;

	for (auto const &n : nodesEnabled_)
		buffers[n] = nodes_.at(n).acquireBuffer().value();

	return buffers;
}

void BackendDevice::releaseBuffer(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	for (auto const &[n, b] : buffers)
		nodes_.at(n).releaseBuffer(b);
}

int BackendDevice::run(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	int ret = 0;

	for (auto const &n : nodesEnabled_)
	{
		nodes_.at(n).streamOn();
		if (nodes_.at(n).queueBuffer(buffers.at(n).buffer.index))
			ret = -1;
	}

	// Triggers the HW job.
	if (nodes_.at("pispbe-config").queueBuffer(configBuffer_.buffer.index))
		ret = -1;

	for (auto const &n : nodesEnabled_)
	{
		if (nodes_.at(n).dequeueBuffer(1000) < 0)
			ret = -1;
	}

	// Must dequeue the config buffer in case it's used again.
	if (nodes_.at("pispbe-config").dequeueBuffer(1000) < 0)
		ret = -1;

	for (auto const &n : nodesEnabled_)
		nodes_.at(n).streamOff();

	return ret;
}
