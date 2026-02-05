/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * backend_device.hpp - PiSP Backend device helper
 */
#pragma once

#include <string>
#include <unordered_set>

#include "backend/pisp_be_config.h"
#include "buffer.hpp"
#include "media_device.hpp"
#include "v4l2_device.hpp"

namespace libpisp::helpers
{

class BackendDevice
{
public:
	BackendDevice(const std::string &device);
	~BackendDevice();

	void Setup(const pisp_be_tiles_config &config, unsigned int buffer_count = 1, bool use_opaque_format = false);

	template <typename T>
	int Run(const T &buffers);

	bool Valid() const
	{
		return valid_;
	}

	V4l2Device &Node(const std::string &node)
	{
		return nodes_.at(node);
	}

	std::map<std::string, std::vector<BufferRef>> GetBuffers();
	std::map<std::string, BufferRef> GetBufferSlice() const;
	BufferRef ConfigBuffer()
	{
		return nodes_.at("pispbe-config").Buffers()[0];
	}

private:
	bool valid_;
	V4l2DevMap nodes_;
	MediaDevice devices_;
	std::unordered_set<std::string> nodes_enabled_;
};

} // namespace libpisp
