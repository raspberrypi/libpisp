/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * buffer.hpp - PiSP V4L2 Buffer and Sync (included by v4l2_device.hpp)
 * No includes - must be included after <array>, <functional>, <stdint.h>.
 */
#pragma once

#include <array>
#include <functional>
#include <stdint.h>

namespace libpisp::helpers
{

struct Buffer
{
public:
	Buffer();
	Buffer(const Buffer &other);
	Buffer(Buffer &&other);
	Buffer(const std::array<int, 3> &fd, const std::array<size_t, 3> &size);
	~Buffer();

	bool operator==(const Buffer &other) const;
	Buffer &operator=(const Buffer &other);
	Buffer &operator=(Buffer &&);

	const std::array<size_t, 3> &Size() const { return size; }
	const std::array<int, 3> &Fd() const { return fd; }

	struct Sync;

private:
	void release();
	void mmap() const;

	std::array<size_t, 3> size;
	mutable std::array<uint8_t *, 3> mem;
	std::array<int, 3> fd;
};

using BufferRef = std::reference_wrapper<const Buffer>;

struct Buffer::Sync
{
	enum class Access
	{
		Read,
		Write,
		ReadWrite
	};

	Sync(BufferRef buffer, Access access);
	~Sync();

	const std::array<uint8_t *, 3> &Get() const;

private:
	BufferRef buffer_;
	Access access_;
};

} // namespace libpisp::helpers
