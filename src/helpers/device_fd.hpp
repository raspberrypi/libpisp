
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * device_fd.hpp - PiSP device file descriptor helper
 */
#pragma once

#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace libpisp::helpers
{

class DeviceFd
{
public:
	DeviceFd(DeviceFd const &) = delete;
	void operator=(DeviceFd const &) = delete;

	DeviceFd(const std::string &file, mode_t mode)
		: deviceFd_(-1)
	{
		int deviceFd = ::open(file.c_str(), mode);
		if (deviceFd >= 0)
			deviceFd_ = deviceFd;
	}

	~DeviceFd()
	{
		close();
	}

	DeviceFd(DeviceFd &&other)
	{
		this->deviceFd_ = other.deviceFd_;
		other.deviceFd_ = -1;
	}

	DeviceFd &operator=(DeviceFd &&other)
	{
		if (this != &other)
		{
			this->deviceFd_ = other.deviceFd_;
			other.deviceFd_ = -1;
		}

		return *this;
	}

	int get()
	{
		return deviceFd_;
	}

	void close()
	{
		if (valid())
			::close(deviceFd_);

		deviceFd_ = -1;
	}

	bool valid()
	{
		return deviceFd_ >= 0;
	}

private:
	int deviceFd_;
};

} // namespace libpisp
