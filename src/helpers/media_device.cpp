
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * media_device.cpp - PiSP media device helper
 */

#include "media_device.hpp"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <vector>

namespace fs = std::filesystem;
using namespace libpisp::helpers;

struct DeviceNode
{
	std::string name;
	std::string dev_node;
};

struct MediaDevMap
{
	MediaDevMap(const std::string &_media_node, const std::vector<DeviceNode> &&_device_nodes)
		: media_node(_media_node), device_nodes(_device_nodes)
	{
	}
	std::string media_node;
	std::vector<DeviceNode> device_nodes;
};

class MediaEnumerator
{
public:
	using MediaDevList = std::vector<MediaDevMap>;

	MediaEnumerator(MediaEnumerator &other) = delete;
	void operator=(const MediaEnumerator &other) = delete;

	static const MediaEnumerator *get()
	{
		static std::unique_ptr<MediaEnumerator> mdev(new MediaEnumerator);
		return mdev.get();
	}

	const MediaDevList &mediaDeviceList() const
	{
		return deviceList_;
	}

private:
	MediaEnumerator()
	{
		// Sysfs path for media devices - correct for Raspberry Pi, but not sure if this is globally consistent.
		const fs::path mediaSysfs("/sys/bus/media/devices");

		if (!fs::exists(mediaSysfs))
			return;

		for (auto const &mediaDevice : fs::directory_iterator { mediaSysfs })
		{
			if (!fs::is_symlink(mediaDevice))
				continue;

			// Find a pisp_be device link,
			const std::string link(fs::read_symlink(mediaDevice).string());
			if (link.find("pisp_be") == std::string::npos)
				continue;

			// Find the /dev/mediaX device node.
			const std::string::size_type pos = link.find_last_of("/");
			const std::string mediaNode("/dev/" + link.substr(pos + 1));

			DeviceFd fd(mediaNode.c_str(), O_RDWR | O_CLOEXEC);
			if (!fd.valid())
				continue;

			struct media_v2_topology topology;
			std::memset(&topology, 0, sizeof(topology));

			// First ioctl call to get the number of interfaces.
			int ret = ioctl(fd.get(), MEDIA_IOC_G_TOPOLOGY, &topology);
			if (ret < 0 || !topology.num_interfaces)
				continue;

			auto interfaces = std::make_unique<struct media_v2_interface[]>(topology.num_interfaces);
			topology.ptr_interfaces = reinterpret_cast<uintptr_t>(interfaces.get());

			// Second ioctl call with allocated space to populate the interface array.
			ret = ioctl(fd.get(), MEDIA_IOC_G_TOPOLOGY, &topology);
			if (ret < 0)
				continue;

			std::vector<DeviceNode> deviceNodes;
			for (unsigned int i = 0; i < topology.num_interfaces; i++)
			{
				const media_v2_interface &intf = interfaces[i];

				if (intf.intf_type != MEDIA_INTF_T_V4L_VIDEO)
					break;

				// Find the char dev with the major:minor advertised by the interface.
				const fs::path charDev("/sys/dev/char/" + std::to_string(intf.devnode.major) + ":" +
										std::to_string(intf.devnode.minor));
				if (!fs::is_symlink(charDev))
					break;

				const std::string charDevLink = fs::read_symlink(charDev).string();
				const std::string::size_type npos = charDevLink.find_last_of("/");
				const std::string devNode("/dev/" + charDevLink.substr(npos + 1));

				// Finally get the /dev/videoX node for the interface.
				std::ifstream devNameFile(charDev / "name");
				if (!devNameFile.is_open())
					break;

				std::string devName;
				std::getline(devNameFile, devName);
				deviceNodes.push_back({ devName, devNode });
			}

			// If we have some device nodes enumerated, add them to our map.
			if (!deviceNodes.empty())
				deviceList_.emplace_back(mediaNode, std::move(deviceNodes));
		}
	}

	MediaDevList deviceList_;
};

MediaDevice::MediaDevice()
	: mediaEnumerator_(MediaEnumerator::get())
{
}

MediaDevice::~MediaDevice()
{
	for (auto it = lockMap_.begin(); it != lockMap_.end();)
		it = unlock(it->first);
}

std::string MediaDevice::acquire(const std::string &device)
{
	for (const auto &m : mediaEnumerator_->mediaDeviceList())
	{
		if (!device.empty() && m.media_node != device)
			continue;

		// Check if this process has the lock.
		auto it = lockMap_.find(m.media_node.c_str());
		if (it != lockMap_.end())
			continue;

		// Check if another process has the lock.
		DeviceFd fd(m.media_node.c_str(), O_RDWR | O_CLOEXEC);
		if (!fd.valid())
			continue;

		if (lockf(fd.get(), F_TLOCK, 0))
			continue;

		lockMap_.emplace(std::piecewise_construct,
						  std::forward_as_tuple(m.media_node), std::forward_as_tuple(std::move(fd)));

		return m.media_node;
	}

	return {};
}

void MediaDevice::release(const std::string &device)
{
	unlock(device);
}

V4l2DevMap MediaDevice::openV4l2Nodes(const std::string &device) const
{
	V4l2DevMap devMap;

	for (const auto &m : mediaEnumerator_->mediaDeviceList())
	{
		if (m.media_node != device)
			continue;

		for (auto const &n : m.device_nodes)
		{
			V4l2Device dev(n.dev_node);
			if (!dev.valid())
				return {};

			devMap.emplace(std::piecewise_construct, std::forward_as_tuple(n.name),
							std::forward_as_tuple(std::move(dev)));
		}

		return devMap;
	}

	return {};
}

void MediaDevice::closeV4l2Nodes(V4l2DevMap &deviceMap)
{
	for (auto &d : deviceMap)
		d.second.close();
}

std::string MediaDevice::list() const
{
	std::stringstream ss;

	for (const auto &it : mediaEnumerator_->mediaDeviceList())
	{
		ss << std::endl << it.media_node << std::endl;
		for (const auto &it1 : it.device_nodes)
			ss << "    " << it1.dev_node << " " << it1.name << std::endl;
	}

	return ss.str();
}

media_device_info MediaDevice::deviceInfo(const std::string &device) const
{
	media_device_info info;

	DeviceFd fd(device.c_str(), O_RDONLY | O_CLOEXEC);
	if (!fd.valid())
		return {};

	int ret = ioctl(fd.get(), MEDIA_IOC_DEVICE_INFO, &info);
	if (ret)
		return {};

	return info;
}

std::map<std::string, DeviceFd>::iterator MediaDevice::unlock(const std::string &device)
{
	auto it = lockMap_.find(device);
	if (it == lockMap_.end())
		return lockMap_.end();

	std::ignore = lockf(it->second.get(), F_ULOCK, 0);
	return lockMap_.erase(it);
}
