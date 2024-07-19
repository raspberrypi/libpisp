
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025 Raspberry Pi Ltd
 *
 * convert.cpp - libpisp simple image converter example
 */

#include <array>
#include <assert.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdint.h>
#include <string>

#include <linux/media.h>

#include <cxxopts.hpp>

#include "helpers/backend_device.hpp"
#include "helpers/media_device.hpp"

#include "libpisp/backend/backend.hpp"
#include "libpisp/common/logging.hpp"
#include "libpisp/common/utils.hpp"
#include "libpisp/variants/variant.hpp"

void readPlane(uint8_t *mem, std::ifstream &in, unsigned int width, unsigned int height, unsigned int fileStride,
				unsigned int bufferStride)
{
	width = std::min(width, fileStride);

	for (unsigned int y = 0; y < height; y++)
	{
		in.read((char *)mem + y * bufferStride, width);
		in.seekg(fileStride - width, std::ios_base::cur);
	}
}

void writePlane(std::ofstream &out, uint8_t *mem, unsigned int width, unsigned int height, unsigned int fileStride,
				 unsigned int bufferStride)
{
	width = std::min(width, fileStride);

	for (unsigned int y = 0; y < height; y++)
	{
		out.write((char *)mem + y * bufferStride, width);
		for (unsigned int i = 0; i < fileStride - width; i++)
			out.put(0);
	}
}

void readRgb888(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
				 unsigned int fileStride, unsigned int bufferStride)
{
	readPlane((uint8_t *)mem[0], in, width * 3, height, fileStride, bufferStride);
}

void writeRgb888(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
				  unsigned int fileStride, unsigned int bufferStride)
{
	writePlane(out, (uint8_t *)mem[0], width * 3, height, fileStride, bufferStride);
}

void read32(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
			 unsigned int fileStride, unsigned int bufferStride)
{
	readPlane((uint8_t *)mem[0], in, width * 4, height, fileStride, bufferStride);
}

void write32(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
			  unsigned int fileStride, unsigned int bufferStride)
{
	writePlane(out, (uint8_t *)mem[0], width * 4, height, fileStride, bufferStride);
}

void readYuv(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
			  unsigned int fileStride, unsigned int bufferStride, unsigned int ssX, unsigned int ssY)
{
	uint8_t *dst = mem[0];
	// Y
	readPlane(dst, in, width, height, fileStride, bufferStride);
	// U
	dst = mem[1] ? mem[1] : dst + bufferStride * height;
	readPlane(dst, in, width / ssX, height / ssY, fileStride / ssX, bufferStride / ssX);
	// V
	dst = mem[2] ? mem[2] : dst + bufferStride / ssX * height / ssY;
	readPlane(dst, in, width / ssX, height / ssY, fileStride / ssX, bufferStride / ssX);
}

void writeYuv(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
			   unsigned int fileStride, unsigned int bufferStride, unsigned int ssX, unsigned int ssY)
{
	uint8_t *src = mem[0];
	// Y
	writePlane(out, src, width, height, fileStride, bufferStride);
	// U
	src = mem[1] ? mem[1] : src + bufferStride * height;
	writePlane(out, src, width / ssX, height / ssY, fileStride / ssX, bufferStride / ssX);
	// V
	src = mem[2] ? mem[2] : src + bufferStride / ssX * height / ssY;
	writePlane(out, src, width / ssX, height / ssY, fileStride / ssX, bufferStride / ssX);
}

void readYuv420(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
				 unsigned int fileStride, unsigned int bufferStride)
{
	readYuv(mem, in, width, height, fileStride, bufferStride, 2, 2);
}

void readYuv422p(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
				  unsigned int fileStride, unsigned int bufferStride)
{
	readYuv(mem, in, width, height, fileStride, bufferStride, 2, 1);
}

void readYuv444p(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
				  unsigned int fileStride, unsigned int bufferStride)
{
	readYuv(mem, in, width, height, fileStride, bufferStride, 1, 1);
}

void readYuv422i(std::array<uint8_t *, 3> &mem, std::ifstream &in, unsigned int width, unsigned int height,
				  unsigned int fileStride, unsigned int bufferStride)
{
	readPlane(mem[0], in, width * 2, height, fileStride, bufferStride);
}

void writeYuv420(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
				  unsigned int fileStride, unsigned int bufferStride)
{
	writeYuv(out, mem, width, height, fileStride, bufferStride, 2, 2);
}

void writeYuv422p(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
				   unsigned int fileStride, unsigned int bufferStride)
{
	writeYuv(out, mem, width, height, fileStride, bufferStride, 2, 1);
}

void writeYuv444p(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
				   unsigned int fileStride, unsigned int bufferStride)
{
	writeYuv(out, mem, width, height, fileStride, bufferStride, 1, 1);
}

void writeYuv422i(std::ofstream &out, std::array<uint8_t *, 3> &mem, unsigned int width, unsigned int height,
				   unsigned int fileStride, unsigned int bufferStride)
{
	writePlane(out, mem[0], width * 2, height, fileStride, bufferStride);
}

struct FormatFuncs
{
	std::function<void(std::array<uint8_t *, 3> &, std::ifstream &, unsigned int, unsigned int, unsigned int,
					   unsigned int)> readFile;
	std::function<void(std::ofstream &, std::array<uint8_t *, 3> &, unsigned int, unsigned int, unsigned int,
					   unsigned int)> writeFile;
};

const std::map<std::string, FormatFuncs> Formats =
{
	{ "RGB888", { readRgb888, writeRgb888 } },
	{ "RGBX8888", { read32, write32 } },
	{ "YUV420P", { readYuv420, writeYuv420 } },
	{ "YUV422P", { readYuv422p, writeYuv422p } },
	{ "YUV444P", { readYuv444p, writeYuv444p } },
	{ "YUYV", { readYuv422i, writeYuv422i } },
	{ "UYVY", { readYuv422i, writeYuv422i } },
};

struct Format
{
	unsigned int width;
	unsigned int height;
	unsigned int stride;
	std::string format;
};

Format parseFormat(const std::string &fmt)
{
	Format format;
	size_t pos = 0, start = 0;

	pos = fmt.find(':', start);
	if (pos == std::string::npos)
		return {};
	format.width = std::stoi(fmt.substr(start, pos - start));
	start = pos + 1;

	pos = fmt.find(':', start);
	if (pos == std::string::npos)
		return {};
	format.height = std::stoi(fmt.substr(start, pos - start));
	start = pos + 1;

	pos = fmt.find(':', start);
	if (pos == std::string::npos)
		return {};
	format.stride = std::stoi(fmt.substr(start, pos - start));
	start = pos + 1;

	format.format = fmt.substr(start);

	return format;
}

int main(int argc, char *argv[])
{
	libpisp::helpers::MediaDevice devices;

	libpisp::loggingInit();

	cxxopts::Options options(argv[0], "PiSP Image Converter");

	options.add_options()
		("input", "Input file", cxxopts::value<std::string>())
		("output", "Output file", cxxopts::value<std::string>())
		("input-format", "Input format in the form width:height:stride:format\n"
						 "Bit-depth is assumed to be 8-bit.",cxxopts::value<std::string>()->default_value(""))
		("output-format", "Output format in the form width:height:stride:format\n"
						  "Bit-depth is assumed to be 8-bit.", cxxopts::value<std::string>()->default_value(""))
		("f,formats", "List available format strings that can be used")
		("l,list", "Enumerate the media device nodes")
		("h,help", "Print usage")
	;

	options.parse_positional({ "input", "output" });
	options.positional_help("<input file> <output file>");
	options.set_width(120);

	auto args = options.parse(argc, argv);

	if (args.count("help"))
	{
		std::cerr << options.help() << std::endl;
		exit(0);
	}
	else if (args.count("list"))
	{
		std::cerr << devices.list() << std::endl;
		exit(0);
	}
	else if (args.count("formats"))
	{
		for (const auto &f : Formats)
			std::cerr << f.first << " ";
		std::cerr << std::endl;
		exit(0);
	}

	libpisp::helpers::BackendDevice backendDevice { };
	std::cerr << "Acquired device " << backendDevice.mediaDev() << std::endl;

	auto inFile = parseFormat(args["input-format"].as<std::string>());
	if (!Formats.count(inFile.format))
	{
		std::cerr << "Invalid input-format specified" << std::endl;
		exit(-1);
	}

	auto outFile = parseFormat(args["output-format"].as<std::string>());
	if (!Formats.count(outFile.format))
	{
		std::cerr << "Invalid output-format specified" << std::endl;
		exit(-1);
	}

	libpisp::BackEnd *be = backendDevice.get();
	libpisp::PiSPVariant variant = be->getVariant();

	libpisp::BeGlobal global = be->getGlobal();
	global.bayerEnables = libpisp::BayerEnable::None;
	global.rgbEnables = libpisp::RgbEnable::Input | libpisp::RgbEnable::Output0;

	if (inFile.format == "RGBX8888" && !variant.backendRgb32Supported(0))
	{
		std::cerr << "Backend hardware does not support RGBX input" << std::endl;
		exit(-1);
	}
	libpisp::ImageFormatConfig i = {};
	i.width = inFile.width;
	i.height = inFile.height;
	i.format = libpisp::getPispImageFormat(inFile.format);
	assert(i.format);
	libpisp::computeOptimalStride(i);
	be->setInputFormat(i);

	libpisp::BeOutputFormatConfig o = {};
	if (outFile.format == "RGBX8888" && !variant.backendRgb32Supported(0))
	{
		// Hack to generate RGBX even when BE_MINOR_VERSION < 1 using Resample
		if (outFile.width < i.width)
			std::cerr << "Backend hardware has limited RGBX support; resize artifacts may be present" << std::endl;

		o.image.width = outFile.width * 2 - 1;
		o.image.height = outFile.height;
		o.image.format = libpisp::getPispImageFormat("UYVY");

		libpisp::BeCcmConfig csc = {}; // Define a matrix to swap components [0] and [1]
		csc.coeffs[1] = 1024;
		csc.coeffs[3] = 1024;
		csc.coeffs[8] = 1024;
		csc.offsets[0] = 131072; // round to nearest after Resample, for 8-bit output
		csc.offsets[1] = 131072;
		csc.offsets[2] = 131072;
		be->setPostCsc(0, csc);
		global.rgbEnables |= libpisp::RgbEnable::PostCsc0;
	}
	else
	{
		o.image.width = outFile.width;
		o.image.height = outFile.height;
		o.image.format = libpisp::getPispImageFormat(outFile.format);
	}
	assert(o.image.format);
	libpisp::computeOptimalStride(o.image, true);
	be->setOutputFormat(0, o);

	if (!outFile.stride)
		outFile.stride = o.image.stride;

	if (inFile.format >= "U")
	{
		libpisp::BeCcmConfig csc;
		be->initialiseYcbcrInverse(csc, "jpeg");
		be->setCcm(csc);
		global.rgbEnables |= libpisp::RgbEnable::Ccm;
	}

	if (outFile.format >= "U")
	{
		libpisp::BeCcmConfig csc;
		be->initialiseYcbcr(csc, "jpeg");
		be->setPostCsc(0, csc);
		global.rgbEnables |= libpisp::RgbEnable::PostCsc0;
	}

	be->setGlobal(global);
	be->setCrop(0, { 0, 0, i.width, i.height });
	be->setSmartResize(0, { o.image.width, o.image.height });

	backendDevice.setup();
	auto buffers = backendDevice.acquireBuffers();

	std::string inputFilename = args["input"].as<std::string>();
	std::ifstream in(inputFilename, std::ios::binary);
	if (!in.is_open())
	{
		std::cerr << "Unable to open " << inputFilename << std::endl;
		exit(-1);
	}

	std::cerr << "Reading " << inputFilename << " "
			  << inFile.width << ":" << inFile.height << ":" << inFile.stride << ":" << inFile.format << std::endl;

	Formats.at(inFile.format)
		.readFile(buffers["pispbe-input"].mem, in, inFile.width, inFile.height, inFile.stride,
				   i.stride);
	in.close();

	int ret = backendDevice.run(buffers);
	if (ret)
	{
		std::cerr << "Job run error!" << std::endl;
		exit(-1);
	}

	std::string outputFile = args["output"].as<std::string>();
	std::ofstream out(outputFile, std::ios::binary);
	if (!out.is_open())
	{
		std::cerr << "Unable to open " << outputFile << std::endl;
		exit(-1);
	}

	Formats.at(outFile.format)
		.writeFile(out, buffers["pispbe-output0"].mem, outFile.width, outFile.height, outFile.stride,
					o.image.stride);
	out.close();

	backendDevice.releaseBuffer(buffers);

	std::cerr << "Writing " << outputFile << " "
			  << outFile.width << ":" << outFile.height << ":" << outFile.stride << ":" << outFile.format << std::endl;

	return 0;
}
