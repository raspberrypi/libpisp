
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * media_device.hpp - PiSP media device helper
 */
#pragma once

#include <map>
#include <string>
#include <vector>

#include <linux/media.h>

#include "device_fd.hpp"
#include "v4l2_device.hpp"

class MediaEnumerator;

namespace libpisp::helpers
{

using V4l2DevMap = std::map<std::string, V4l2Device>;

class MediaDevice
{
public:
	MediaDevice();
	~MediaDevice();

	std::string acquire(const std::string &device = {});
	void release(const std::string &device);

	V4l2DevMap openV4l2Nodes(const std::string &device) const;
	void closeV4l2Nodes(V4l2DevMap &deviceMap);

	std::string list() const;
	struct media_device_info deviceInfo(const std::string &device) const;

private:
	std::map<std::string, DeviceFd>::iterator unlock(const std::string &device);
	std::map<std::string, DeviceFd> lockMap_;
	const MediaEnumerator *mediaEnumerator_;
};

} // namespace libpisp
