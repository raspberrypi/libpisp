/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.hpp - PiSP Backend device helper
 */
#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "libpisp/backend/pisp_be_config.h"
#include "media_device.hpp"
#include "v4l2_device.hpp"

#include "backend/backend.hpp"

namespace libpisp::helpers
{

class BackendDevice
{
public:
	BackendDevice(const std::string &device = "");
	~BackendDevice();

	void prepare(const pisp_be_tiles_config &config, unsigned int buffer_count, bool use_opaque_format);
	void prepare(unsigned int buffer_count = 1, bool use_opaque_format = false);
	int run(const std::map<std::string, V4l2Device::Buffer> &buffers);

	bool valid() const
	{
		return valid_;
	}

	V4l2Device &node(const std::string &node)
	{
		return nodes_.at(node);
	}

	std::string media_dev() const
	{
		return media_dev_;
	}

	libpisp::BackEnd *get()
	{
		return be_.get();
	}

	std::map<std::string, V4l2Device::Buffer> acquire_buffers();
	void release_buffer(const std::map<std::string, V4l2Device::Buffer> &buffers);

private:
	bool valid_;
	V4l2DevMap nodes_;
	MediaDevice devices_;
	std::unordered_set<std::string> nodes_enabled_;
	V4l2Device::Buffer config_buffer_;
	std::string media_dev_;
	std::unique_ptr<libpisp::BackEnd> be_;
};

} // namespace libpisp
