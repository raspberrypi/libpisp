
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2424 Raspberry Pi Ltd
 *
 * v4l2_device.cpp - PiSP V4L2 device helper
 */

#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <stdexcept>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

#include "v4l2_device.hpp"

using namespace libpisp::helpers;

struct FormatInfo
{
	unsigned int v4l2_pixfmt;
	unsigned int num_memory_planes;
};

static FormatInfo get_v4l2_format(const std::string &format)
{
	std::map<std::string, FormatInfo> formats {
		{ "RGB888", { V4L2_PIX_FMT_RGB24, 1 } },
		{ "RGBX8888", { V4L2_PIX_FMT_RGBX32, 1 } },
		{ "YUV420P", { V4L2_PIX_FMT_YUV420, 1 } },
		{ "YUV422P", { V4L2_PIX_FMT_YUV422P, 1 } },
		{ "YUV444P", { V4L2_PIX_FMT_YUV444M, 3 } },
		{ "YUYV", { V4L2_PIX_FMT_YUYV, 1 } },
		{ "UYVY", { V4L2_PIX_FMT_UYVY, 1 } },
	};

	auto it = formats.find(format);
	if (it == formats.end())
		return { 0, {} };

	return it->second;
}

V4l2Device::V4l2Device(const std::string &device)
	: fd_(device, O_RDWR | O_NONBLOCK | O_CLOEXEC), num_memory_planes_(1)
{
	struct v4l2_capability caps;

	int ret = ioctl(fd_.Get(), VIDIOC_QUERYCAP, &caps);
	if (ret < 0)
		throw std::runtime_error("Cannot query device caps");

	if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		buf_type_ = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else if (caps.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		buf_type_ = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else if (caps.capabilities & V4L2_CAP_META_OUTPUT)
		buf_type_ = V4L2_BUF_TYPE_META_OUTPUT;
	else
		throw std::runtime_error("Invalid buffer_type - caps: " + std::to_string(caps.capabilities));
}

V4l2Device::~V4l2Device()
{
	ReleaseBuffers();
	Close();
}

int V4l2Device::RequestBuffers(unsigned int count)
{
	int ret;

	ReleaseBuffers();

	v4l2_requestbuffers req_bufs {};

	req_bufs.count = count;
	req_bufs.type = buf_type_;
	req_bufs.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);
	if (ret < 0)
		throw std::runtime_error("VIDIOC_REQBUFS failed: " + std::to_string(ret));

	for (unsigned int i = 0; i < req_bufs.count; i++)
	{
		v4l2_plane planes[VIDEO_MAX_PLANES] = {};
		v4l2_buffer buffer {};

		buffer.index = i;
		buffer.type = buf_type_;
		buffer.memory = V4L2_MEMORY_MMAP;
		if (!isMeta())
		{
			buffer.m.planes = planes;
			buffer.length = num_memory_planes_;
		}

		ret = ioctl(fd_.Get(), VIDIOC_QUERYBUF, &buffer);
		if (ret < 0)
			throw std::runtime_error("VIDIOC_QUERYBUF failed: " + std::to_string(ret));

		// Don't keep this pointer dangling when putting into v4l2_buffers_.
		buffer.m.planes = NULL;
		v4l2_buffers_.emplace_back(buffer);
		available_buffers_.push(i);

		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			size_t size = !isMeta() ? planes[p].length : buffer.length;
			unsigned int offset = !isMeta() ? planes[p].m.mem_offset : buffer.m.offset;

			void *mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.Get(), offset);
			if (mem == MAP_FAILED)
				throw std::runtime_error("Unable to mmap buffer");

			v4l2_buffers_.back().size[p] = size;
			v4l2_buffers_.back().mem[p] = (uint8_t *)mem;
		}
	}

	return v4l2_buffers_.size();
}

void V4l2Device::ReleaseBuffers()
{
	v4l2_requestbuffers req_bufs {};

	if (!v4l2_buffers_.size())
		return;

	for (auto const &b : v4l2_buffers_)
	{
		for (unsigned int p = 0; p < num_memory_planes_; p++)
			munmap(b.mem[p], b.size[p]);
	}

	req_bufs.type = buf_type_;
	req_bufs.count = 0;
	req_bufs.memory = V4L2_MEMORY_MMAP;
	ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);

	v4l2_buffers_.clear();
}

std::optional<V4l2Device::Buffer> V4l2Device::GetBuffer()
{
	if (available_buffers_.empty())
		return {};

	unsigned int index = available_buffers_.front();
	available_buffers_.pop();
	return findBuffer(index);
}

int V4l2Device::QueueBuffer(unsigned int index)
{
	std::optional<Buffer> buf = findBuffer(index);
	if (!buf)
		return -1;

	v4l2_plane planes[VIDEO_MAX_PLANES];
	if (!isMeta())
	{
		buf->buffer.m.planes = planes;
		buf->buffer.length = num_memory_planes_;
		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			buf->buffer.m.planes[p].bytesused = buf->size[p];
			buf->buffer.m.planes[p].length = buf->size[p];
		}
	}
	else
		buf->buffer.bytesused = buf->size[0];

	buf->buffer.timestamp.tv_sec = time(NULL);
	buf->buffer.field = V4L2_FIELD_NONE;
	buf->buffer.flags = 0;

	int ret = ioctl(fd_.Get(), VIDIOC_QBUF, &buf->buffer);
	if (ret < 0)
		throw std::runtime_error("Unable to queue buffer: " + std::string(strerror(errno)));

	return ret;
}

int V4l2Device::DequeueBuffer(unsigned int timeout_ms)
{
	short int poll_event = isOutput() ? POLLOUT : POLLIN;
	pollfd p = { fd_.Get(), poll_event, 0 };

	int ret = poll(&p, 1, timeout_ms);
	if (ret <= 0)
		return -1;

	if (!(p.revents & poll_event))
		return -1;

	v4l2_buffer buf = {};
	v4l2_plane planes[VIDEO_MAX_PLANES] = {};

	buf.type = buf_type_;
	buf.memory = V4L2_MEMORY_MMAP;
	if (!isMeta())
	{
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	ret = ioctl(fd_.Get(), VIDIOC_DQBUF, &buf);
	if (ret)
		return -1;

	available_buffers_.push(buf.index);
	return buf.index;
}

void V4l2Device::SetFormat(unsigned int width, unsigned int height, unsigned int stride, unsigned int stride2,
						   const std::string &format)
{
	struct v4l2_format f = {};
	FormatInfo info = get_v4l2_format(format);

	assert(info.v4l2_pixfmt);

	num_memory_planes_ = info.num_memory_planes;

	f.type = buf_type_;
	f.fmt.pix_mp.width = width;
	f.fmt.pix_mp.height = height;
	f.fmt.pix_mp.pixelformat = info.v4l2_pixfmt;
	f.fmt.pix_mp.field = V4L2_FIELD_NONE;
	f.fmt.pix_mp.num_planes = num_memory_planes_;

	for (unsigned int p = 0; p < num_memory_planes_; p++)
	{
		f.fmt.pix_mp.plane_fmt[p].bytesperline = p == 0 ? stride : stride2;
		f.fmt.pix_mp.plane_fmt[p].sizeimage = 0;
	}

	int ret = ioctl(fd_.Get(), VIDIOC_S_FMT, &f);
	if (ret)
		throw std::runtime_error("Unable to set format: " + std::string(strerror(errno)));
}

void V4l2Device::StreamOn()
{
	int ret = ioctl(fd_.Get(), VIDIOC_STREAMON, &buf_type_);
	if (ret < 0)
		throw std::runtime_error("Stream on failed: " + std::string(strerror(errno)));
}

void V4l2Device::StreamOff()
{
	int ret = ioctl(fd_.Get(), VIDIOC_STREAMOFF, &buf_type_);
	if (ret < 0)
		throw std::runtime_error("Stream off failed: " + std::string(strerror(errno)));
}

std::optional<V4l2Device::Buffer> V4l2Device::findBuffer(unsigned int index) const
{
	auto it = std::find_if(v4l2_buffers_.begin(), v4l2_buffers_.end(),
						   [index](auto const &b) { return b.buffer.index == index; });
	if (it == v4l2_buffers_.end())
	{
		throw std::runtime_error("find buffers failed");
		return {};
	}

	return *it;
}
