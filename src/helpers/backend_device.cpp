/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.cpp - PiSP Backend device helper
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>

#include "backend_device.hpp"
#include "libpisp/backend/backend_config_adapter.hpp"

using namespace libpisp::helpers;

BackendDevice::BackendDevice(const std::string &device)
	: valid_(true)
{
	media_dev_ = MediaDevice().acquire(device);
	if (media_dev_.empty())
		throw std::runtime_error("Unable to acquire any pisp_be device!");

	nodes_ = MediaDevice().open_v4l2_nodes(media_dev_);
	if (nodes_.empty())
		valid_ = false;

	// Allocate a config buffer to persist.
	nodes_.at("pispbe-config").request_buffers(1);
	nodes_.at("pispbe-config").stream_on();
	config_buffer_ = nodes_.at("pispbe-config").acquire_buffer().value();

	const std::vector<libpisp::PiSPVariant> &variants = libpisp::get_variants();
	const media_device_info info = MediaDevice().device_info(media_dev_);
	auto variant = std::find_if(variants.begin(), variants.end(),
								[&info](const auto &v) { return v.back_end_version() == info.hw_revision; });
	if (variant == variants.end())
		throw std::runtime_error("Backend hardware cound not be identified: " + std::to_string(info.hw_revision));
	be_ = std::make_unique<libpisp::BackEnd>(libpisp::BackEnd::SoftwareConfig({}), *variant);
}


BackendDevice::~BackendDevice()
{
	nodes_.at("pispbe-config").stream_off();
}

void BackendDevice::prepare(unsigned int buffer_count, bool use_opaque_format)
{
	struct NodeConfig
	{
		const char *name;
		std::function<bool(const BeGlobal &)> enabled;
		std::function<ImageFormatConfig(const BackEnd *)> get_format;
	};

	// clang-format off
	const NodeConfig node_configs[] =
	{
		{  "pispbe-input",
			[](const BeGlobal &g) { return (g.rgb_enables & RgbEnable::Input) || (g.bayer_enables & BayerEnable::Input); },
			[](const BackEnd *be) { return be->get_input_format(); } },

		{ "pispbe-output0",
			[](const BeGlobal &g) { return g.rgb_enables & RgbEnable::Output0; },
		  	[](const BackEnd *be) { return be->get_output_format(0).image; } },

		{ "pispbe-output1",
			[](const BeGlobal &g) { return g.rgb_enables & RgbEnable::Output1; },
		  	[](const BackEnd *be) { return be->get_output_format(1).image; } },

		{ "pispbe-tdn_input",
			[](const BeGlobal &g) { return g.bayer_enables & BayerEnable::TdnInput; },
			[](const BackEnd *be) { return be->get_tdn_input_format(); } },

		{ "pispbe-tdn_output",
			[](const BeGlobal &g) { return g.bayer_enables & BayerEnable::TdnOutput; },
			[](const BackEnd *be) { return be->get_tdn_output_format(); } },

		{ "pispbe-stitch_input",
			[](const BeGlobal &g) { return g.bayer_enables & BayerEnable::StitchInput; },
		  	[](const BackEnd *be) { return be->get_stitch_input_format(); } },

		{ "pispbe-stitch_output",
			[](const BeGlobal &g) { return g.bayer_enables & BayerEnable::StitchOutput; },
		  	[](const BackEnd *be) { return be->get_stitch_output_format(); } },

		{ "pispbe-tonemap_input",
			[](const BeGlobal &g) { return g.bayer_enables & BayerEnable::Tonemap2Input; },
		  	[](const BackEnd *be) { return be->get_tonemap2_format(); } },
	};
	// clang-format on

	pisp_be_tiles_config config = {};
	be_->prepare(&config);

	const BeGlobal &global = be_->get_global();
	nodes_enabled_.clear();

	for (const auto &node_config : node_configs)
	{
		if (nodes_.count(node_config.name) && node_config.enabled(global))
		{
			nodes_.at(node_config.name).set_format(node_config.get_format(be_.get()), use_opaque_format);
			// Release old/allocate a single buffer.
			nodes_.at(node_config.name).return_buffers();
			nodes_.at(node_config.name).request_buffers(buffer_count);
			nodes_enabled_.emplace(node_config.name);
		}
	}

	std::memcpy(reinterpret_cast<pisp_be_tiles_config *>(config_buffer_.mem[0]), &config, sizeof(config));
}

std::map<std::string, V4l2Device::Buffer> BackendDevice::acquire_buffers()
{
	std::map<std::string, V4l2Device::Buffer> buffers;

	for (auto const &n : nodes_enabled_)
		buffers[n] = nodes_.at(n).acquire_buffer().value();

	return buffers;
}

void BackendDevice::release_buffer(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	for (auto const &[n, b] : buffers)
		nodes_.at(n).release_buffer(b);
}

int BackendDevice::run(const std::map<std::string, V4l2Device::Buffer> &buffers)
{
	int ret = 0;

	for (auto const &n : nodes_enabled_)
	{
		nodes_.at(n).stream_on();
		if (nodes_.at(n).queue_buffer(buffers.at(n).buffer.index))
			ret = -1;
	}

	// Triggers the HW job.
	if (nodes_.at("pispbe-config").queue_buffer(config_buffer_.buffer.index))
		ret = -1;

	for (auto const &n : nodes_enabled_)
	{
		if (nodes_.at(n).dequeue_buffer(1000) < 0)
			ret = -1;
	}

	// Must dequeue the config buffer in case it's used again.
	if (nodes_.at("pispbe-config").dequeue_buffer(1000) < 0)
		ret = -1;

	for (auto const &n : nodes_enabled_)
		nodes_.at(n).stream_off();

	return ret;
}
