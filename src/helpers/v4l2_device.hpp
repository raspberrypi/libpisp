
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * v4l2_device.hpp - PiSP V4L2 device helper
 */
#pragma once

#include <array>
#include <vector>

#include <linux/videodev2.h>

#ifndef V4L2_PIX_FMT_NV12MT_COL128
#define V4L2_PIX_FMT_NV12MT_COL128 v4l2_fourcc('N', 'c', '1', '2') /* 12  Y/CbCr 4:2:0 128 pixel wide column */
#endif
#ifndef V4L2_PIX_FMT_NV12MT_10_COL128
#define V4L2_PIX_FMT_NV12MT_10_COL128 v4l2_fourcc('N', 'c', '3', '0')
#endif

#include "backend/pisp_be_config.h"

#include "buffer.hpp"
#include "device_fd.hpp"
#include "dma_heap.hpp"

namespace libpisp::helpers
{

class V4l2Device
{
public:
	V4l2Device(const std::string &device);
	~V4l2Device();

	V4l2Device(V4l2Device &&) = default;
	V4l2Device &operator=(V4l2Device &&) = default;

	V4l2Device(const V4l2Device &) = delete;
	V4l2Device &operator=(const V4l2Device &) = delete;

	unsigned int Fd()
	{
		return fd_.Get();
	}

	bool Valid()
	{
		return fd_.Valid();
	}

	void Close()
	{
		if (fd_.Valid())
			fd_.Close();
	}

	int AllocateBuffers(unsigned int count = 1);
	int ImportBuffer(BufferRef buffer);
	void ReleaseBuffers();
	std::vector<BufferRef> Buffers() const;
	int QueueBuffer(const Buffer &buffer);
	int DequeueBuffer(unsigned int timeout_ms = 500);

	void SetFormat(const pisp_image_format_config &format, bool use_opaque_format = false);

	void StreamOn();
	void StreamOff();

private:
	bool isMeta()
	{
		return buf_type_ == V4L2_BUF_TYPE_META_OUTPUT;
	}

	bool isCapture()
	{
		return buf_type_ == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	}

	bool isOutput()
	{
		return !isCapture();
	}

	struct BufferCache
	{
		BufferCache(const std::array<int, 3> &fd, const std::array<size_t, 3> &size, unsigned int id)
			: fd(fd), size(size), id(id), queued(false)
		{
		}

		std::array<int, 3> fd;
		std::array<size_t, 3> size;
		unsigned int id;
		bool queued;

		bool operator==(const Buffer &other) const
		{
			return fd == other.Fd() && size == other.Size();
		}
	};

	std::vector<BufferCache> buffer_cache_;
	std::vector<Buffer> buffer_allocs_;
	DeviceFd fd_;
	enum v4l2_buf_type buf_type_;
	unsigned int num_memory_planes_;
	DmaHeap dma_heap_;
	unsigned int max_slots_;
	v4l2_format v4l2_format_;
};

} // namespace libpisp
