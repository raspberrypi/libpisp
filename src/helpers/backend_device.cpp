/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.cpp - PiSP Backend device helper
 */

#include "backend_device.hpp"

#include <cstring>

using namespace libpisp::helpers;

namespace
{

const Buffer &AsBuffer(BufferRef ref)
{
	return ref.get();
}
const Buffer &AsBuffer(const Buffer &b)
{
	return b;
}

} // namespace

BackendDevice::BackendDevice(const std::string &device)
	: valid_(true)
{
	nodes_ = MediaDevice().OpenV4l2Nodes(device);
	if (nodes_.empty())
	{
		valid_ = false;
		return;
	}

	// Allocate a config buffer to persist.
	nodes_.at("pispbe-config").AllocateBuffers(1);
	nodes_.at("pispbe-config").StreamOn();
}

BackendDevice::~BackendDevice()
{
	nodes_.at("pispbe-config").StreamOff();

	for (auto const &n : nodes_enabled_)
		nodes_.at(n).StreamOff();
}

void BackendDevice::Setup(const pisp_be_tiles_config &config, unsigned int buffer_count, bool use_opaque_format)
{
	for (auto const &n : nodes_enabled_)
		nodes_.at(n).StreamOff();

	nodes_enabled_.clear();

	if ((config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_INPUT) ||
		(config.config.global.bayer_enables & PISP_BE_BAYER_ENABLE_INPUT))
	{
		nodes_.at("pispbe-input").SetFormat(config.config.input_format, use_opaque_format);
		nodes_.at("pispbe-input").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-input");
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT0)
	{
		nodes_.at("pispbe-output0").SetFormat(config.config.output_format[0].image, use_opaque_format);
		nodes_.at("pispbe-output0").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-output0");
	}

	if (config.config.global.rgb_enables & PISP_BE_RGB_ENABLE_OUTPUT1)
	{
		nodes_.at("pispbe-output1").SetFormat(config.config.output_format[1].image, use_opaque_format);
		nodes_.at("pispbe-output1").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-output1");
	}

	if (config.config.global.bayer_enables & PISP_BE_BAYER_ENABLE_TDN_INPUT)
	{
		nodes_.at("pispbe-tdn_input").SetFormat(config.config.tdn_input_format, use_opaque_format);
		nodes_.at("pispbe-tdn_input").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-tdn_input");
	}

	if (config.config.global.bayer_enables & PISP_BE_BAYER_ENABLE_TDN_OUTPUT)
	{
		nodes_.at("pispbe-tdn_output").SetFormat(config.config.tdn_output_format, use_opaque_format);
		nodes_.at("pispbe-tdn_output").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-tdn_output");
	}

	if (config.config.global.bayer_enables & PISP_BE_BAYER_ENABLE_STITCH_INPUT)
	{
		nodes_.at("pispbe-stitch_input").SetFormat(config.config.stitch_input_format, use_opaque_format);
		nodes_.at("pispbe-stitch_input").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-stitch_input");
	}

	if (config.config.global.bayer_enables & PISP_BE_BAYER_ENABLE_STITCH_OUTPUT)
	{
		nodes_.at("pispbe-stitch_output").SetFormat(config.config.stitch_output_format, use_opaque_format);
		nodes_.at("pispbe-stitch_output").AllocateBuffers(buffer_count);
		nodes_enabled_.emplace("pispbe-stitch_output");
	}

	for (auto const &n : nodes_enabled_)
		nodes_.at(n).StreamOn();

	auto config_buffer = nodes_.at("pispbe-config").Buffers()[0];
	Buffer::Sync s(config_buffer, Buffer::Sync::Access::ReadWrite);
	std::memcpy(reinterpret_cast<pisp_be_tiles_config *>(s.Get()[0]), &config, sizeof(config));
}

std::map<std::string, std::vector<BufferRef>> BackendDevice::GetBuffers()
{
	std::map<std::string, std::vector<BufferRef>> buffers;

	for (auto const &n : nodes_enabled_)
	{
		for (auto &ref : nodes_.at(n).Buffers())
			buffers[n].push_back(ref);
	}

	return buffers;
}

std::map<std::string, BufferRef> BackendDevice::GetBufferSlice() const
{
	std::map<std::string, BufferRef> buffers;

	for (auto const &n : nodes_enabled_)
		buffers.emplace(n, nodes_.at(n).Buffers()[0]);

	return buffers;
}

template <typename T>
int BackendDevice::Run(const T &buffers)
{
	int ret = 0;

	for (auto const &n : nodes_enabled_)
	{
		if (nodes_.at(n).QueueBuffer(AsBuffer(buffers.at(n))))
			ret = -1;
	}

	auto config_buffer = nodes_.at("pispbe-config").Buffers()[0];
	if (nodes_.at("pispbe-config").QueueBuffer(config_buffer))
		ret = -1;

	for (auto const &n : nodes_enabled_)
	{
		if (nodes_.at(n).DequeueBuffer(1000) < 0)
			ret = -1;
	}

	/* Must dequeue the config buffer in case it's used again. */
	if (nodes_.at("pispbe-config").DequeueBuffer(1000) < 0)
		ret = -1;

	return ret;
}

template int BackendDevice::Run(const std::map<std::string, BufferRef> &);
template int BackendDevice::Run(const std::map<std::string, Buffer> &);
