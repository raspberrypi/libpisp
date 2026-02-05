
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2424 Raspberry Pi Ltd
 *
 * v4l2_device.cpp - PiSP V4L2 device helper
 */

#include "v4l2_device.hpp"

#include <algorithm>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <stdexcept>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>

#include "libpisp/common/utils.hpp"

using namespace libpisp::helpers;

namespace {

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
		{ "NV12", { V4L2_PIX_FMT_NV12M, 2 } },
		{ "YUV420SP_COL128", { V4L2_PIX_FMT_NV12MT_COL128, 2 } },
	};

	auto it = formats.find(format);
	if (it == formats.end())
		return { 0, {} };

	return it->second;
}

} // namespace

V4l2Device::V4l2Device(const std::string &device)
	: fd_(device, O_RDWR | O_NONBLOCK | O_CLOEXEC), num_memory_planes_(1), max_slots_(0), v4l2_format_()
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

int V4l2Device::AllocateBuffers(unsigned int count)
{
	struct v4l2_format f = {};
	int ret;

	f.type = buf_type_;
	ret = ioctl(fd_.Get(), VIDIOC_G_FMT, &f);
	if (ret)
		throw std::runtime_error("VIDIOC_G_FMT failed: " + std::to_string(ret));

	for (unsigned int i = 0; i < count; i++)
	{
		std::array<int, 3> fds = { -1, -1, -1 };
		std::array<size_t, 3> sizes = { 0, 0, 0 };

		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			const size_t size = isMeta() ? f.fmt.meta.buffersize : f.fmt.pix_mp.plane_fmt[p].sizeimage;
			int fd = dma_heap_.alloc("v4l2_device_buf", size);

			if (fd < 0)
				throw std::runtime_error("DMABUF allocation failed");

			fds[p] = fd;
			sizes[p] = size;
		}

		const Buffer &b = buffer_allocs_.emplace_back(fds, sizes);

		// May as well, and this also calls REQBUFS.
		ImportBuffer(b);
	}

	return buffer_allocs_.size();
}

int V4l2Device::ImportBuffer(BufferRef buffer)
{
	std::vector<BufferCache>::iterator cache_it;

	if (!max_slots_)
	{
		v4l2_requestbuffers req_bufs {};
		req_bufs.count = 64;
		req_bufs.type = buf_type_;
		req_bufs.memory = V4L2_MEMORY_DMABUF;

		int ret = ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);
		if (ret < 0)
			throw std::runtime_error("VIDIOC_REQBUFS failed: " + std::to_string(ret));

		max_slots_ = req_bufs.count;
		buffer_cache_.reserve(max_slots_);
	}

	// Check if buffer with matching fd and size already exists and is not queued - reuse it.
	cache_it = std::find_if(buffer_cache_.begin(), buffer_cache_.end(),
							[&buffer](const auto &b) { return b == buffer && !b.queued; });

	if (cache_it != buffer_cache_.end())
		return cache_it - buffer_cache_.begin();

	for (unsigned int p = 0; p < num_memory_planes_; p++)
	{
		const size_t size = isMeta() ? v4l2_format_.fmt.meta.buffersize
									 : v4l2_format_.fmt.pix_mp.plane_fmt[p].sizeimage;

		if (buffer.get().Fd()[p] < 0 || buffer.get().Size()[p] < size)
			throw std::runtime_error("Plane " + std::to_string(p) + " buffer is invalid.");
	}

	if (buffer_cache_.size() == max_slots_)
	{
		// Find and replace the first buffer that is not queued.
		cache_it = std::find_if(buffer_cache_.begin(), buffer_cache_.end(),
								[](const auto &buf) { return !buf.queued; });
		if (cache_it == buffer_cache_.end())
			throw std::runtime_error("Unable to import buffer, run out of slots.");

		*cache_it = BufferCache(buffer.get().Fd(), buffer.get().Size(), cache_it->id);
	}
	else
	{
		cache_it = buffer_cache_.emplace(buffer_cache_.end(),
										 buffer.get().Fd(), buffer.get().Size(), buffer_cache_.size());
	}

	return cache_it - buffer_cache_.begin();
}

void V4l2Device::ReleaseBuffers()
{
	if (!buffer_cache_.size())
		return;

	v4l2_requestbuffers req_bufs {};

	req_bufs.type = buf_type_;
	req_bufs.count = 0;
	req_bufs.memory = V4L2_MEMORY_DMABUF;
	ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);

	buffer_allocs_ = {};
	buffer_cache_ = {};
	max_slots_ = 0;
}

std::vector<BufferRef> V4l2Device::Buffers() const
{
	std::vector<BufferRef> refs;

	for (const Buffer &b : buffer_allocs_)
		refs.push_back(b);

	return refs;
}

int V4l2Device::QueueBuffer(const Buffer &buffer)
{
	v4l2_plane planes[VIDEO_MAX_PLANES] = {};
	v4l2_buffer buf {};

	int idx = ImportBuffer(buffer);
	buffer_cache_[idx].queued = true;

	buf.index = buffer_cache_[idx].id;
	buf.type = buf_type_;
	buf.memory = V4L2_MEMORY_DMABUF;

	if (!isMeta())
	{
		buf.m.planes = planes;
		buf.length = num_memory_planes_;
		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			buf.m.planes[p].bytesused = buffer.Size()[p];
			buf.m.planes[p].length = buffer.Size()[p];
			buf.m.planes[p].m.fd = buffer.Fd()[p];
		}
	}
	else
	{
		buf.bytesused = buffer.Size()[0];
		buf.m.fd = buffer.Fd()[0];
	}

	buf.timestamp.tv_sec = time(NULL);
	buf.field = V4L2_FIELD_NONE;
	buf.flags = 0;

	int ret = ioctl(fd_.Get(), VIDIOC_QBUF, &buf);
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
	buf.memory = V4L2_MEMORY_DMABUF;
	if (!isMeta())
	{
		buf.m.planes = planes;
		buf.length = VIDEO_MAX_PLANES;
	}

	ret = ioctl(fd_.Get(), VIDIOC_DQBUF, &buf);
	if (ret)
		return -1;

	// Find the buffer in cache with matching id and set queued to false
	auto cache_it = std::find_if(buffer_cache_.begin(), buffer_cache_.end(),
								 [&buf](const auto &b) { return b.id == buf.index; });
	if (cache_it != buffer_cache_.end())
		cache_it->queued = false;

	return 0;
}

void V4l2Device::SetFormat(const pisp_image_format_config &format, bool use_opaque_format)
{
	// Release old buffers before setting the new format.
	ReleaseBuffers();

	struct v4l2_format f = {};
	FormatInfo info = get_v4l2_format(libpisp::get_pisp_image_format(format.format));

	num_memory_planes_ = info.num_memory_planes;

	f.type = buf_type_;
	f.fmt.pix_mp.width = format.width;
	f.fmt.pix_mp.height = format.height;
	f.fmt.pix_mp.pixelformat = info.v4l2_pixfmt;
	f.fmt.pix_mp.field = V4L2_FIELD_NONE;
	f.fmt.pix_mp.num_planes = num_memory_planes_;

	unsigned int num_image_planes = libpisp::num_planes((pisp_image_format)format.format);

	if (use_opaque_format || info.v4l2_pixfmt == 0)
	{
		// This format is not specified by V4L2, we use an opaque buffer buffer as a workaround.
		// Size the dimensions down so the kernel drive does not attempt to resize it.
		f.fmt.pix_mp.width = 16;
		f.fmt.pix_mp.height = 16;
		f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
		num_memory_planes_ = 3;
		f.fmt.pix_mp.plane_fmt[0].bytesperline = format.stride;

		f.fmt.pix_mp.plane_fmt[0].sizeimage = 0;
		for (unsigned int i = 0; i < 3; i++)
			f.fmt.pix_mp.plane_fmt[0].sizeimage += libpisp::get_plane_size(format, i);

		f.fmt.pix_mp.plane_fmt[1].sizeimage = f.fmt.pix_mp.plane_fmt[2].sizeimage = f.fmt.pix_mp.plane_fmt[0].sizeimage;
		f.fmt.pix_mp.plane_fmt[1].bytesperline = f.fmt.pix_mp.plane_fmt[2].bytesperline = format.stride2;
	}
	else
	{
		unsigned int p = 0;
		for (; p < num_memory_planes_; p++)
		{
			const unsigned int stride = p == 0 ? format.stride : format.stride2;
			// Wallpaper stride is not something the V4L2 kernel knows about!
			// Do NOT use the column strides that have been computed in compute_stride_align
			if (PISP_IMAGE_FORMAT_WALLPAPER(format.format))
				f.fmt.pix_mp.plane_fmt[p].bytesperline = (format.width + 127) & ~127;
			else
				f.fmt.pix_mp.plane_fmt[p].bytesperline = stride;
			f.fmt.pix_mp.plane_fmt[p].sizeimage = libpisp::get_plane_size(format, p);
		}

		for (; p < num_image_planes; p++)
			f.fmt.pix_mp.plane_fmt[num_memory_planes_ - 1].sizeimage += libpisp::get_plane_size(format, p);
	}

	int ret = ioctl(fd_.Get(), VIDIOC_S_FMT, &f);
	if (ret)
		throw std::runtime_error("Unable to set format: " + std::string(strerror(errno)));

	v4l2_format_ = f;
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
