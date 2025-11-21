
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend_default_config.cpp - Default configuration setup for the PiSP Back End
 */
#include "backend.hpp"

#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <link.h>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "backend/pisp_build_config.h"
#include "common/pisp_pwl.hpp"

// musl doesn't declare _DYNAMIC in link.h, declare it manually, see:
// https://github.com/raspberrypi/libpisp/issues/25
extern ElfW(Dyn) _DYNAMIC[];

using json = nlohmann::json;

// Where it might be helpful we initialise some blocks with the "obvious" default parameters. This saves users the trouble,
// and they can just "enable" the blocks.

namespace
{

// Check if the RPATH or RUNPATH definitions exist in the ELF file. If they don't exist, it means that meson has
// stripped them out when doing the install step.
//
// Source: https://stackoverflow.com/questions/2836330/is-there-a-programmatic-way-to-inspect-the-current-rpath-on-linux
bool isInstalled()
{
	const ElfW(Dyn) *dyn = _DYNAMIC;
	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		if (dyn->d_tag == DT_RPATH || dyn->d_tag == DT_RUNPATH)
			return false;
	}
	return true;
}

std::string sourcePath()
{
	Dl_info dl_info;
	std::string path;

	if (dladdr(reinterpret_cast<void *>(sourcePath), &dl_info))
		path = dl_info.dli_fname;

	if (path.empty())
		return {};

	auto const pos = path.find_last_of('/');
	if (pos == std::string::npos)
		return {};

	path.erase(pos, path.length() - pos);
	return path;
}

void initialiseDebin(libpisp::BeDebinConfig &debin, const json &root)
{
	constexpr unsigned int num_coefs = sizeof(debin.coeffs) / sizeof(debin.coeffs[0]);
	auto coefs = root["debin"]["coefs"].get<std::vector<int8_t>>();

	if (coefs.size() != num_coefs)
		throw std::runtime_error("Debin filter size mismatch");

	memcpy(debin.coeffs, coefs.data(), sizeof(debin.coeffs));
	debin.h_enable = debin.v_enable = 1;
}

void initialiseDemosaic(libpisp::BeDemosaicConfig &demosaic, const json &root)
{
	auto &params = root["demosaic"];
	demosaic.sharper = params["sharper"].get<uint8_t>();
	demosaic.fc_mode = params["fc_mode"].get<uint8_t>();
}

void initialiseFalseColour(pisp_be_false_colour_config &fc, const json &root)
{
	auto &params = root["false_colour"];
	fc.distance = params["distance"].get<uint8_t>();
}

void initialiseGamma(libpisp::BeGammaConfig &gamma, const json &root)
{
	constexpr unsigned int num_points = sizeof(gamma.lut) / sizeof(gamma.lut[0]);

	libpisp::Pwl pwl;
	pwl.read(root["gamma"]["lut"]);

	static constexpr unsigned int SlopeBits = 14;
	static constexpr unsigned int PosBits = 16;

	int lastY = 0;
	for (unsigned int i = 0; i < num_points; i++)
	{
		int x, y;
		if (i < 32)
			x = i * 512;
		else if (i < 48)
			x = (i - 32) * 1024 + 16384;
		else
			x = std::min(65535u, (i - 48) * 2048 + 32768);

		y = pwl.eval(x);
		if (y < 0 || (i && y < lastY))
			throw std::runtime_error("initialiseGamma: Malformed LUT");

		if (i)
		{
			unsigned int slope = y - lastY;
			if (slope >= (1u << SlopeBits))
			{
				slope = (1u << SlopeBits) - 1;
				y = lastY + slope;
			}
			gamma.lut[i - 1] |= slope << PosBits;
		}

		gamma.lut[i] = y;
		lastY = y;
	}
}

void readResample(std::map<std::string, libpisp::BeResampleConfig> &resampleFilterMap,
				   std::vector<std::pair<double, std::string>> &resampleSelectList, const json &root)
{
	auto &filters = root["resample"]["filters"];

	for (auto const &[name, filter] : filters.items())
	{
		libpisp::BeResampleConfig r;
		constexpr unsigned int num_coefs = sizeof(r.coef) / sizeof(r.coef[0]);
		auto coefs = filter.get<std::vector<int16_t>>();

		if (coefs.size() != num_coefs)
			throw std::runtime_error("readResample: Incorrect number of filter coefficients");

		memcpy(r.coef, coefs.data(), sizeof(r.coef));
		resampleFilterMap.emplace(name, r);
	}

	auto &smart = root["resample"]["smart_selection"];
	for (auto &scale : smart["downscale"])
		resampleSelectList.emplace_back(scale.get<double>(), std::string {});

	unsigned int i = 0;
	for (auto &filter : smart["filter"])
	{
		resampleSelectList[i].second = filter.get<std::string>();
		if (++i == resampleSelectList.size())
			break;
	}
	if (i != resampleSelectList.size())
		throw std::runtime_error("readResample: Incorrect number of filters");
}

// Macros for the sharpening filters, to avoid repeating the same code 5 times
#define FILTER(i)                                                                                                      \
	{                                                                                                                  \
		auto filter = params["filter" #i];                                                                             \
		std::vector<int8_t> kernel_v;                                                                                  \
		for (auto &x : filter["kernel"])                                                                               \
			kernel_v.push_back(x.get<int8_t>());                                                                       \
		int8_t *kernel = &kernel_v[0];                                                                                 \
		uint16_t offset = filter["offset"].get<uint16_t>();                                                            \
		uint16_t threshold_slope = filter["threshold_slope"].get<uint16_t>();                                          \
		uint16_t scale = filter["scale"].get<uint16_t>();                                                              \
		memcpy(sharpen.kernel##i, kernel, sizeof(sharpen.kernel##i));                                                  \
		memcpy(&sharpen.threshold_offset##i, &offset, sizeof(sharpen.threshold_offset##i));                            \
		memcpy(&sharpen.threshold_slope##i, &threshold_slope, sizeof(sharpen.threshold_slope##i));                     \
		memcpy(&sharpen.scale##i, &scale, sizeof(sharpen.scale##i));                                                   \
	}

#define POS_NEG(i)                                                                                                     \
	{                                                                                                                  \
		auto tive = params[#i "tive"];                                                                                 \
		uint16_t strength = tive["strength"].get<uint16_t>();                                                          \
		uint16_t pre_limit = tive["pre_limit"].get<uint16_t>();                                                        \
		std::vector<uint16_t> function_v;                                                                              \
		for (auto &x : tive["function"])                                                                               \
			function_v.push_back(x.get<uint16_t>());                                                                   \
		uint16_t *function = &function_v[0];                                                                           \
		uint16_t limit = tive["limit"].get<uint16_t>();                                                                \
		memcpy(&sharpen.i##tive_strength, &strength, sizeof(sharpen.i##tive_strength));                                \
		memcpy(&sharpen.i##tive_pre_limit, &pre_limit, sizeof(sharpen.i##tive_pre_limit));                             \
		memcpy(sharpen.i##tive_func, function, sizeof(sharpen.i##tive_func));                                          \
		memcpy(&sharpen.i##tive_limit, &limit, sizeof(sharpen.i##tive_limit));                                         \
	}

void readSharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc, const json &root)
{
	auto params = root["sharpen"];

	FILTER(0);
	FILTER(1);
	FILTER(2);
	FILTER(3);
	FILTER(4);

	POS_NEG(posi);
	POS_NEG(nega);

	std::string enables_s = params["enables"].get<std::string>();
	sharpen.enables = std::stoul(enables_s, nullptr, 16);
	sharpen.white = params["white"].get<uint8_t>();
	sharpen.black = params["black"].get<uint8_t>();
	sharpen.grey = params["grey"].get<uint8_t>();

	memset(&shfc, 0, sizeof(shfc));
	shfc.y_factor = params["shfc_y_factor"].get<double>() * (1 << 8);
}

void readYCbCr(std::map<std::string, libpisp::BeCcmConfig> &ycbcr_map,
				std::map<std::string, libpisp::BeCcmConfig> &inverse_ycbcr_map, const json &root)
{
	auto encoding = root["colour_encoding"];

	for (auto const &[format, enc] : encoding.items())
	{
		static const std::string keys[2] { "ycbcr", "ycbcr_inverse" };

		for (auto &key : keys)
		{
			auto &matrix = enc[key];
			libpisp::BeCcmConfig ccm;

			auto coeffs = matrix["coeffs"].get<std::vector<int16_t>>();
			if (coeffs.size() != 9)
				throw std::runtime_error("readYCbCr: Incorrect number of matrix coefficients");
			memcpy(ccm.coeffs, coeffs.data(), sizeof(ccm.coeffs));

			auto offsets = matrix["offsets"].get<std::vector<int32_t>>();
			if (offsets.size() != 3)
				throw std::runtime_error("readYCbCr: Incorrect number of matrix offsets");
			memcpy(ccm.offsets, offsets.data(), sizeof(ccm.offsets));

			if (key == "ycbcr")
				ycbcr_map.emplace(format, ccm);
			else
				inverse_ycbcr_map.emplace(format, ccm);
		}
	}
}

void getMatrix(libpisp::BeCcmConfig &matrix, const std::map<std::string, libpisp::BeCcmConfig> &map,
			   const std::string &cs)
{
	memset(matrix.coeffs, 0, sizeof(matrix.coeffs));
	memset(matrix.offsets, 0, sizeof(matrix.offsets));

	auto it = map.find(cs);
	if (it != map.end())
	{
		memcpy(matrix.coeffs, it->second.coeffs, sizeof(matrix.coeffs));
		memcpy(matrix.offsets, it->second.offsets, sizeof(matrix.offsets));
	}
}

} // namespace

namespace libpisp
{

void BackEnd::initialiseYcbcr(libpisp::BeCcmConfig &ycbcr, const std::string &cs)
{
	getMatrix(ycbcr, ycbcrMap_, cs);
}

void BackEnd::initialiseYcbcrInverse(libpisp::BeCcmConfig &ycbcr_inverse, const std::string &cs)
{
	getMatrix(ycbcr_inverse, inverseYcbcrMap_, cs);
}

void BackEnd::initialiseResample(BeResampleConfig &resample, const std::string &filter)
{
	memset(resample.coef, 0, sizeof(resample.coef));

	auto it = resampleFilterMap_.find(filter);
	if (it != resampleFilterMap_.end())
		memcpy(resample.coef, it->second.coef, sizeof(resample.coef));
}

void BackEnd::initialiseResample(BeResampleConfig &resample, double downscale)
{
	auto it = std::find_if(resampleSelectList_.begin(), resampleSelectList_.end(),
						   [downscale](const auto &item) { return item.first >= downscale; });

	if (it != resampleSelectList_.end())
		initialiseResample(resample, it->second);
	else
		initialiseResample(resample, resampleSelectList_.back().second);
}

void BackEnd::initialiseSharpen(libpisp::BeSharpenConfig &sharpen, libpisp::BeShFcCombineConfig &shfc)
{
	sharpen = defaultSharpen_;
	shfc = defaultShfc_;
}

void BackEnd::initialiseDefaultConfig(const std::string &filename)
{
	std::string file(filename);

	if (file.empty())
	{
		if (!isInstalled())
		{
			std::string path = sourcePath();
			if (path.empty())
				throw std::runtime_error("BE: Could not determine the local source path");

			file = path + "/libpisp/backend/backend_default_config.json";
		}
		else
			file = std::string(PISP_BE_CONFIG_DIR) + "/" + "backend_default_config.json";
	}

	std::ifstream ifs(file);
	if (!ifs.good())
		throw std::runtime_error("BE: Could not find config json file: " + file);

	json root = json::parse(ifs);
	ifs.close();

	memset(&pipe_->hwConfig, 0, sizeof(pipe_->hwConfig));

	libpisp::BeDebinConfig debin;
	initialiseDebin(debin, root);
	setDebin(debin);

	libpisp::BeDemosaicConfig demosaic;
	initialiseDemosaic(demosaic, root);
	setDemosaic(demosaic);

	BeFalseColourConfig fc;
	initialiseFalseColour(fc, root);
	setFalseColour(fc);

	libpisp::BeGammaConfig gamma;
	initialiseGamma(gamma, root);
	setGamma(gamma);

	readYCbCr(ycbcrMap_, inverseYcbcrMap_, root);
	readResample(resampleFilterMap_, resampleSelectList_, root);
	readSharpen(defaultSharpen_, defaultShfc_, root);

	libpisp::BeSharpenConfig sharpen;
	libpisp::BeShFcCombineConfig shfc;
	initialiseSharpen(sharpen, shfc);
	setSharpen(sharpen);
	setShFcCombine(shfc);

	// Start with a sensible default YCbCr -- must be full-range on 2712C1
	libpisp::BeCcmConfig ycbcr, ycbcrInverse;
	initialiseYcbcr(ycbcr, "jpeg");
	setYcbcr(ycbcr);

	initialiseYcbcrInverse(ycbcrInverse, "jpeg");
	setYcbcrInverse(ycbcrInverse);	

	// Start with a sensible default resamplers.
	libpisp::BeResampleConfig resample;
	initialiseResample(resample, "lanczos3");
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		setResample(i, resample);
}

} // namespace libpisp
