
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
		{ "YUV420SP_COL128", { V4L2_PIX_FMT_NV12MT_COL128, 2 } },
	};

	auto it = formats.find(format);
	if (it == formats.end())
		return { 0, {} };

	return it->second;
}

} // namespace

V4l2Device::Buffer::Buffer()
	: size(), mem(), fd({ -1, -1, -1 }), id(-1), queued(false)
{
}

V4l2Device::Buffer::Buffer(const std::array<int, 3> &fd, const std::array<size_t, 3> &size)
	: size(size), mem(), fd(fd), id(-1), queued(false)
{
}

V4l2Device::Buffer::Buffer(unsigned int id)
	: size(), mem(), fd({ -1, -1, -1 }), id(id), queued(false)
{
}

void V4l2Device::Buffer::RwSyncStart()
{
	struct dma_buf_sync dma_sync {};
	dma_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] > 0)
			ioctl(fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

void V4l2Device::Buffer::RwSyncEnd()
{
	struct dma_buf_sync dma_sync {};
	dma_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] > 0)
			ioctl(fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

void V4l2Device::Buffer::ReadSyncStart()
{
	struct dma_buf_sync dma_sync {};
	dma_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] > 0)
			ioctl(fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

void V4l2Device::Buffer::ReadSyncEnd()
{
	struct dma_buf_sync dma_sync {};
	dma_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;

	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] > 0)
			ioctl(fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

bool V4l2Device::Buffer::operator==(const Buffer &other) const
{
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] != other.fd[p] || size[p] != other.size[p])
			return false;
	}
	return true;
}

V4l2Device::V4l2Device(const std::string &device)
	: fd_(device, O_RDWR | O_NONBLOCK | O_CLOEXEC), num_memory_planes_(1), max_slots_(0)
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

	std::vector<Buffer> buffers;

	for (unsigned int i = 0; i < count; i++)
	{
		buffers.push_back({});

		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			const size_t size = isMeta() ? f.fmt.meta.buffersize : f.fmt.pix_mp.plane_fmt[p].sizeimage;
			int fd = dma_heap_.alloc("v4l2_device_buf", size);

			if (fd < 0)
				throw std::runtime_error("DMABUF allocation failed");

			buffers.back().fd[p] = fd;
			buffers.back().size[p] = size;
		}
	}

	return ImportBuffers(buffers);
}

int V4l2Device::ImportBuffers(const std::vector<Buffer> &buffers)
{
	struct v4l2_format f = {};
	int ret;

	f.type = buf_type_;
	ret = ioctl(fd_.Get(), VIDIOC_G_FMT, &f);
	if (ret)
		throw std::runtime_error("VIDIOC_G_FMT failed: " + std::to_string(ret));

	if (!max_slots_)
	{
		v4l2_requestbuffers req_bufs {};
		req_bufs.count = 64;
		req_bufs.type = buf_type_;
		req_bufs.memory = V4L2_MEMORY_DMABUF;

		ret = ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);
		if (ret < 0)
			throw std::runtime_error("VIDIOC_REQBUFS failed: " + std::to_string(ret));

		max_slots_ = req_bufs.count;
	}

	for (auto const &b : buffers)
	{
		// Just set the v4l2 buffer slot id to the index in the vector.
		unsigned int id = buffers_.size();

		// Check if buffer with matching fd and size already exists
		if (std::find(buffers_.begin(), buffers_.end(), b) != buffers_.end())
			continue;

		if (buffers_.size() == max_slots_)
		{
			// Find and remove the first buffer that is not queued.
			auto it = std::find_if(buffers_.begin(), buffers_.end(), [](const Buffer &buf) { return !buf.Queued(); });

			if (it != buffers_.end())
			{
				// Clean up memory mappings.
				for (unsigned int p = 0; p < num_memory_planes_; p++)
				{
					if (it->mem[p])
						munmap(it->mem[p], it->size[p]);
				}

				// Keep this id for reuse.
				id = it->id;
				buffers_.erase(it);
			}
			else
				throw std::runtime_error("Unable to import buffer, run out of slots.");
		}

		buffers_.push_back(id);

		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			const size_t size = isMeta() ? f.fmt.meta.buffersize : f.fmt.pix_mp.plane_fmt[p].sizeimage;

			if (b.fd[p] < 0 || b.size[p] < size)
				throw std::runtime_error("Plane " + std::to_string(p) + " buffer is invalid.");

			void *mem = mmap(0, b.size[p], PROT_READ | PROT_WRITE, MAP_SHARED, b.fd[p], 0);
			if (mem == MAP_FAILED)
				throw std::runtime_error("Unable to mmap buffer");

			buffers_.back().fd[p] = b.fd[p];
			buffers_.back().size[p] = b.size[p];
			buffers_.back().mem[p] = (uint8_t *)mem;
		}
	}

	return buffers_.size();
}

void V4l2Device::ReleaseBuffers()
{
	if (!buffers_.size())
		return;

	v4l2_requestbuffers req_bufs {};

	req_bufs.type = buf_type_;
	req_bufs.count = 0;
	req_bufs.memory = V4L2_MEMORY_DMABUF;
	ioctl(fd_.Get(), VIDIOC_REQBUFS, &req_bufs);

	for (auto const &b : buffers_)
	{
		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			if (b.mem[p])
				munmap(b.mem[p], b.size[p]);
		}
	}

	buffers_ = {};
	max_slots_ = 0;
}

int V4l2Device::QueueBuffer(const Buffer &buffer)
{
	v4l2_plane planes[VIDEO_MAX_PLANES] = {};
	v4l2_buffer buf {};

	// Check if buffer with matching fd and size already exists - if not, import it now.
	if (std::find(buffers_.begin(), buffers_.end(), buffer) == buffers_.end())
		ImportBuffers({ buffer });

	// Now find the buffer id that might be cached.
	auto const buf_it = std::find(buffers_.begin(), buffers_.end(), buffer);

	buf.index = buf_it->id;
	buf.type = buf_type_;
	buf.memory = V4L2_MEMORY_DMABUF;

	if (!isMeta())
	{
		buf.m.planes = planes;
		buf.length = num_memory_planes_;
		for (unsigned int p = 0; p < num_memory_planes_; p++)
		{
			buf.m.planes[p].bytesused = buffer.size[p];
			buf.m.planes[p].length = buffer.size[p];
			buf.m.planes[p].m.fd = buffer.fd[p];
		}
	}
	else
	{
		buf.bytesused = buffer.size[0];
		buf.m.fd = buffer.fd[0];
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
