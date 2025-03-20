/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.hpp - PiSP Backend device helper
 */
#pragma once

#include <string>
#include <unordered_set>

#include "libpisp/backend/pisp_be_config.h"
#include "media_device.hpp"
#include "v4l2_device.hpp"

namespace libpisp::helpers
{

class BackendDevice
{
public:
	BackendDevice(const std::string &device);
	~BackendDevice();

	void Setup(const pisp_be_tiles_config &config);
	int Run();

	bool Valid() const
	{
		return valid_;
	}

	const std::map<std::string, V4l2Device::Buffer> &GetBuffers() const
	{
		return buffers_;
	}

private:
	bool valid_;
	V4l2DevMap nodes_;
	MediaDevice devices_;
	std::unordered_set<std::string> nodes_enabled_;
	V4l2Device::Buffer config_buffer_;
	std::map<std::string, V4l2Device::Buffer> buffers_;
};

} // namespace libpisp
