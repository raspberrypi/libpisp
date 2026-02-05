
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * buffer.cpp - PiSP V4L2 Buffer and Sync implementation
 */

#include "buffer.hpp"

#include <errno.h>
#include <stdexcept>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/dma-buf.h>

using namespace libpisp::helpers;

Buffer::Sync::Sync(BufferRef buffer, Access access)
	: buffer_(buffer), access_(access)
{
	struct dma_buf_sync dma_sync {};

	dma_sync.flags = DMA_BUF_SYNC_START;
	if (access_ == Access::Read || access_ == Access::ReadWrite)
		dma_sync.flags |= DMA_BUF_SYNC_READ;
	if (access_ == Access::Write || access_ == Access::ReadWrite)
		dma_sync.flags |= DMA_BUF_SYNC_WRITE;

	for (unsigned int p = 0; p < 3; p++)
	{
		if (buffer_.get().fd[p] >= 0)
			ioctl(buffer_.get().fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

Buffer::Sync::~Sync()
{
	struct dma_buf_sync dma_sync {};

	dma_sync.flags = DMA_BUF_SYNC_END;
	if (access_ == Access::Read || access_ == Access::ReadWrite)
		dma_sync.flags |= DMA_BUF_SYNC_READ;
	if (access_ == Access::Write || access_ == Access::ReadWrite)
		dma_sync.flags |= DMA_BUF_SYNC_WRITE;

	for (unsigned int p = 0; p < 3; p++)
	{
		if (buffer_.get().fd[p] >= 0)
			ioctl(buffer_.get().fd[p], DMA_BUF_IOCTL_SYNC, &dma_sync);
	}
}

const std::array<uint8_t *, 3> &Buffer::Sync::Get() const
{
	if (!buffer_.get().mem[0])
		buffer_.get().mmap();

	return buffer_.get().mem;
}

Buffer::Buffer()
	: size(), mem(), fd({ -1, -1, -1 })
{
}

Buffer::Buffer(const Buffer &other)
	: size(other.size), mem(), fd({ -1, -1, -1 })
{
	for (unsigned int p = 0; p < 3; p++)
	{
		if (other.fd[p] < 0)
			break;

		fd[p] = dup(other.fd[p]);
		if (fd[p] < 0)
		{
			// Close any fds we already dup'd before throwing.
			for (unsigned int q = 0; q < p; q++)
				close(fd[q]);

			throw std::runtime_error("Unable to dup fd: " + std::string(strerror(errno)));
		}
	}
}

Buffer::Buffer(Buffer &&other)
	: size(other.size), mem(other.mem), fd(other.fd)
{
	// Clear other without closing: we took ownership of its fds.
	other.size = {};
	other.fd = { -1, -1, -1 };
	other.mem = {};
}

Buffer::Buffer(const std::array<int, 3> &fd, const std::array<size_t, 3> &size)
	: size(size), mem(), fd(fd)
{
}

Buffer::~Buffer()
{
	release();
}

bool Buffer::operator==(const Buffer &other) const
{
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] != other.fd[p] || size[p] != other.size[p])
			return false;
	}
	return true;
}

Buffer &Buffer::operator=(const Buffer &other)
{
	if (this == &other)
		return *this;

	std::array<int, 3> new_fd = { -1, -1, -1 };
	for (unsigned int p = 0; p < 3; p++)
	{
		if (other.fd[p] < 0)
			break;

		new_fd[p] = dup(other.fd[p]);
		if (new_fd[p] < 0)
		{
			// Close any fds we already dup'd before throwing.
			for (unsigned int q = 0; q < p; q++)
				close(new_fd[q]);

			throw std::runtime_error("Unable to dup fd: " + std::string(strerror(errno)));
		}
	}

	release();
	fd = new_fd;
	size = other.size;
	return *this;
}

Buffer &Buffer::operator=(Buffer &&other)
{
	release();

	size = other.size;
	fd = other.fd;
	mem = other.mem;

	// Clear other without closing: we took ownership of its fds.
	other.size = {};
	other.fd = { -1, -1, -1 };
	other.mem = {};
	return *this;
}

void Buffer::release()
{
	for (unsigned int p = 0; p < 3; p++)
	{
		if (mem[p] && size[p])
			munmap(mem[p], size[p]);

		if (fd[p] >= 0)
			close(fd[p]);

		mem[p] = nullptr;
		fd[p] = -1;
		size[p] = 0;
	}
}

void Buffer::mmap() const
{
	for (unsigned int p = 0; p < 3; p++)
	{
		if (fd[p] < 0)
			break;

		void *m = ::mmap(0, size[p], PROT_READ | PROT_WRITE, MAP_SHARED, fd[p], 0);
		if (m == MAP_FAILED)
		{
			// Unmap any regions we already mapped before throwing.
			for (unsigned int q = 0; q < p; q++)
			{
				munmap(mem[q], size[q]);
				mem[q] = nullptr;
			}

			throw std::runtime_error("Unable to mmap buffer");
		}

		mem[p] = (uint8_t *)m;
	}
}
