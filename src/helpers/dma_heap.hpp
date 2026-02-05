/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026, Raspberry Pi Ltd
 *
 * dma_heap.hpp - Helper class for dma-heap allocations.
 */

#pragma once

#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <vector>

#include "common/logging.hpp"
#include "device_fd.hpp"

namespace libpisp::helpers
{

class DmaHeap
{
public:
	DmaHeap()
	{
		// /dev/dma-heap/vidbuf_cached sym links to either the system heap (Pi 5) or the
		// CMA allocator (Pi 4 and below). If missing, fallback to the system allocator.
		static const std::vector<const char *> heapNames {
			"/dev/dma_heap/vidbuf_cached",
			"/dev/dma_heap/system",
		};

		for (const char *name : heapNames)
		{
			dmaHeapHandle_ = DeviceFd(name, O_RDWR | O_CLOEXEC);
			if (!dmaHeapHandle_.Valid())
			{
				PISP_LOG(debug, "Failed to open " << name);
				continue;
			}
			break;
		}

		if (!dmaHeapHandle_.Valid())
			PISP_LOG(warning, "Could not open any dmaHeap device");
	}

	~DmaHeap()
	{
	}

	DmaHeap(DmaHeap &&other) = default;

	bool Valid() const
	{
		return dmaHeapHandle_.Valid();
	}

	int alloc(const char *name, std::size_t size) const
	{
		int ret;

		if (!name)
			return {};

		struct dma_heap_allocation_data alloc = {};

		alloc.len = size;
		alloc.fd_flags = O_CLOEXEC | O_RDWR;

		ret = ::ioctl(dmaHeapHandle_.Get(), DMA_HEAP_IOCTL_ALLOC, &alloc);
		if (ret < 0)
		{
			PISP_LOG(warning, "dmaHeap allocation failure for " << name);
			return -1;
		}

		ret = ::ioctl(alloc.fd, DMA_BUF_SET_NAME, name);
		if (ret < 0)
		{
			PISP_LOG(warning, "dmaHeap naming failure for " << name);
			return -1;
		}

		return alloc.fd;
	}

private:
	DeviceFd dmaHeapHandle_;
};

} //libpisp::helpers
