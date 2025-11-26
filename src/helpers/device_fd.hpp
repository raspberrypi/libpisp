
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
		: device_fd_(-1)
	{
		int device_fd = ::open(file.c_str(), mode);
		if (device_fd >= 0)
			device_fd_ = device_fd;
	}

	~DeviceFd()
	{
		close();
	}

	DeviceFd(DeviceFd &&other)
	{
		this->device_fd_ = other.device_fd_;
		other.device_fd_ = -1;
	}

	DeviceFd &operator=(DeviceFd &&other)
	{
		if (this != &other)
		{
			this->device_fd_ = other.device_fd_;
			other.device_fd_ = -1;
		}

		return *this;
	}

	int get()
	{
		return device_fd_;
	}

	void close()
	{
		if (valid())
			::close(device_fd_);

		device_fd_ = -1;
	}

	bool valid()
	{
		return device_fd_ >= 0;
	}

private:
	int device_fd_;
};

} // namespace libpisp
