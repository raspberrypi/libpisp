/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.hpp - PiSP Backend device helper
 */
#pragma once

#include <memory>
#include <optional>
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

	void setup(unsigned int bufferCount = 1, bool useOpaqueFormat = false);
	int run(const std::map<std::string, V4l2Device::Buffer> &buffers);

	bool valid() const
	{
		return valid_;
	}

	V4l2Device &node(const std::string &node)
	{
		return nodes_.at(node);
	}

	std::string mediaDev() const
	{
		return mediaDev_;
	}

	libpisp::BackEnd *get()
	{
		return be_.get();
	}

	std::map<std::string, V4l2Device::Buffer> acquireBuffers();
	void releaseBuffer(const std::map<std::string, V4l2Device::Buffer> &buffers);
	V4l2Device::Buffer &configBuffer()
	{
		return configBuffer_;
	}

private:
	bool valid_;
	V4l2DevMap nodes_;
	MediaDevice devices_;
	std::unordered_set<std::string> nodesEnabled_;
	V4l2Device::Buffer configBuffer_;
	std::string mediaDev_;
	std::unique_ptr<libpisp::BackEnd> be_;
	libpisp::PiSPVariant variant_;
};

} // namespace libpisp
