
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

#include "libpisp/common/utils.hpp"

#include "v4l2_device.hpp"

using namespace libpisp::helpers;

namespace {

struct FormatInfo
{
	unsigned int v4l2_pixfmt;
	unsigned int num_memory_planes;
};

static FormatInfo getV4l2Format(const std::string &format)
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

} // namespace

V4l2Device::V4l2Device(const std::string &device)
	: fd_(device, O_RDWR | O_NONBLOCK | O_CLOEXEC), numMemoryPlanes_(1)
{
	struct v4l2_capability caps;

	int ret = ioctl(fd_.get(), VIDIOC_QUERYCAP, &caps);
	if (ret < 0)
		throw std::runtime_error("Cannot query device caps");

	if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		bufType_ = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else if (caps.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		bufType_ = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else if (caps.capabilities & V4L2_CAP_META_OUTPUT)
		bufType_ = V4L2_BUF_TYPE_META_OUTPUT;
	else
		throw std::runtime_error("Invalid buffer_type - caps: " + std::to_string(caps.capabilities));
}

V4l2Device::~V4l2Device()
{
	returnBuffers();
	close();
}

int V4l2Device::requestBuffers(unsigned int count)
{
	int ret;

	returnBuffers();

	v4l2_requestbuffers reqBufs {};

	reqBufs.count = count;
	reqBufs.type = bufType_;
	reqBufs.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(fd_.get(), VIDIOC_REQBUFS, &reqBufs);
	if (ret < 0)
		throw std::runtime_error("VIDIOC_REQBUFS failed: " + std::to_string(ret));

	for (unsigned int i = 0; i < reqBufs.count; i++)
	{
		v4l2_plane planes[VIDEO_MAX_PLANES] = {};
		v4l2_buffer buffer {};

		buffer.index = i;
		buffer.type = bufType_;
		buffer.memory = V4L2_MEMORY_MMAP;
		if (!isMeta())
		{
			buffer.m.planes = planes;
			buffer.length = numMemoryPlanes_;
		}

		ret = ioctl(fd_.get(), VIDIOC_QUERYBUF, &buffer);
		if (ret < 0)
			throw std::runtime_error("VIDIOC_QUERYBUF failed: " + std::to_string(ret));

		// Don't keep this pointer dangling when putting into v4l2Buffers_.
		buffer.m.planes = NULL;
		v4l2Buffers_.emplace_back(buffer);
		availableBuffers_.push(i);

		for (unsigned int p = 0; p < numMemoryPlanes_; p++)
		{
			size_t size = !isMeta() ? planes[p].length : buffer.length;
			unsigned int offset = !isMeta() ? planes[p].m.mem_offset : buffer.m.offset;

			void *mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.get(), offset);
			if (mem == MAP_FAILED)
				throw std::runtime_error("Unable to mmap buffer");

			v4l2Buffers_.back().size[p] = size;
			v4l2Buffers_.back().mem[p] = (uint8_t *)mem;
		}
	}

	return v4l2Buffers_.size();
}

void V4l2Device::returnBuffers()
{
	v4l2_requestbuffers reqBufs {};

	if (!v4l2Buffers_.size())
		return;

	for (auto const &b : v4l2Buffers_)
	{
		for (unsigned int p = 0; p < numMemoryPlanes_; p++)
			munmap(b.mem[p], b.size[p]);
	}

	reqBufs.type = bufType_;
	reqBufs.count = 0;
	reqBufs.memory = V4L2_MEMORY_MMAP;
	ioctl(fd_.get(), VIDIOC_REQBUFS, &reqBufs);

	v4l2Buffers_.clear();
}

std::optional<V4l2Device::Buffer> V4l2Device::acquireBuffer()
{
	if (availableBuffers_.empty())
		return {};

	unsigned int index = availableBuffers_.front();
	availableBuffers_.pop();
	return findBuffer(index);
}

void V4l2Device::releaseBuffer(const Buffer &buffer)
{
	availableBuffers_.push(buffer.buffer.index);
}

int V4l2Device::queueBuffer(unsigned int index)
{
	std::optional<Buffer> buf = findBuffer(index);
	if (!buf)
		return -1;

	v4l2_plane planes[VIDEO_MAX_PLANES] = {};
		if (!isMeta())
		{
			buf->buffer.m.planes = planes;
			buf->buffer.length = numMemoryPlanes_;
			for (unsigned int p = 0; p < numMemoryPlanes_; p++)
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

	int ret = ioctl(fd_.get(), VIDIOC_QBUF, &buf->buffer);
	if (ret < 0)
		throw std::runtime_error("Unable to queue buffer: " + std::string(strerror(errno)));

	return ret;
}

int V4l2Device::dequeueBuffer(unsigned int timeoutMs)
{
	short int pollEvent = isOutput() ? POLLOUT : POLLIN;
	pollfd p = { fd_.get(), pollEvent, 0 };

	int ret = poll(&p, 1, timeoutMs);
	if (ret <= 0)
		return -1;

	if (!(p.revents & pollEvent))
		return -1;

	v4l2_buffer buf = {};
	v4l2_plane planes[VIDEO_MAX_PLANES] = {};

	buf.type = bufType_;
	buf.memory = V4L2_MEMORY_MMAP;
	if (!isMeta())
	{
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	ret = ioctl(fd_.get(), VIDIOC_DQBUF, &buf);
	if (ret)
		return -1;

	return buf.index;
}

void V4l2Device::setFormat(const pisp_image_format_config &format, bool useOpaqueFormat)
{
	struct v4l2_format f = {};
	FormatInfo info = getV4l2Format(libpisp::getPispImageFormat(format.format));

	numMemoryPlanes_ = info.num_memory_planes;

	f.type = bufType_;
	f.fmt.pix_mp.width = format.width;
	f.fmt.pix_mp.height = format.height;
	f.fmt.pix_mp.pixelformat = info.v4l2_pixfmt;
	f.fmt.pix_mp.field = V4L2_FIELD_NONE;
	f.fmt.pix_mp.num_planes = numMemoryPlanes_;

	unsigned int numImagePlanes = libpisp::numPlanes((pisp_image_format)format.format);

	if (useOpaqueFormat || info.v4l2_pixfmt == 0)
	{
		// This format is not specified by V4L2, we use an opaque buffer buffer as a workaround.
		// Size the dimensions down so the kernel drive does not attempt to resize it.
		f.fmt.pix_mp.width = 16;
		f.fmt.pix_mp.height = 16;
		f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
		numMemoryPlanes_ = 3;
		f.fmt.pix_mp.plane_fmt[0].bytesperline = format.stride;

		f.fmt.pix_mp.plane_fmt[0].sizeimage = 0;
		for (unsigned int i = 0; i < 3; i++)
			f.fmt.pix_mp.plane_fmt[0].sizeimage += libpisp::getPlaneSize(format, i);

		f.fmt.pix_mp.plane_fmt[1].sizeimage = f.fmt.pix_mp.plane_fmt[2].sizeimage = f.fmt.pix_mp.plane_fmt[0].sizeimage;
		f.fmt.pix_mp.plane_fmt[1].bytesperline = f.fmt.pix_mp.plane_fmt[2].bytesperline = format.stride2;
	}
	else
	{
		unsigned int p = 0;
		for (; p < numMemoryPlanes_; p++)
		{
			const unsigned int stride = p == 0 ? format.stride : format.stride2;
			// Wallpaper stride is not something the V4L2 kernel knows about!
			f.fmt.pix_mp.plane_fmt[p].bytesperline = stride;
			f.fmt.pix_mp.plane_fmt[p].sizeimage = libpisp::getPlaneSize(format, p);
		}

		for (; p < numImagePlanes; p++)
			f.fmt.pix_mp.plane_fmt[numMemoryPlanes_ - 1].sizeimage += libpisp::getPlaneSize(format, p);
	}

	int ret = ioctl(fd_.get(), VIDIOC_S_FMT, &f);
	if (ret)
		throw std::runtime_error("Unable to set format: " + std::string(strerror(errno)));
}

void V4l2Device::streamOn()
{
	int ret = ioctl(fd_.get(), VIDIOC_STREAMON, &bufType_);
	if (ret < 0)
		throw std::runtime_error("Stream on failed: " + std::string(strerror(errno)));
}

void V4l2Device::streamOff()
{
	int ret = ioctl(fd_.get(), VIDIOC_STREAMOFF, &bufType_);
	if (ret < 0)
		throw std::runtime_error("Stream off failed: " + std::string(strerror(errno)));
}

std::optional<V4l2Device::Buffer> V4l2Device::findBuffer(unsigned int index) const
{
	auto it = std::find_if(v4l2Buffers_.begin(), v4l2Buffers_.end(),
						   [index](auto const &b) { return b.buffer.index == index; });
	if (it == v4l2Buffers_.end())
	{
		throw std::runtime_error("find buffers failed");
		return {};
	}

	return *it;
}
