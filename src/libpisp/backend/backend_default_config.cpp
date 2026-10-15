
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
bool is_installed()
{
	const ElfW(Dyn) *dyn = _DYNAMIC;
	for (; dyn->d_tag != DT_NULL; dyn++)
	{
		if (dyn->d_tag == DT_RPATH || dyn->d_tag == DT_RUNPATH)
			return false;
	}
	return true;
}

std::string source_path()
{
	Dl_info dl_info;
	std::string path;

	if (dladdr(reinterpret_cast<void *>(source_path), &dl_info))
		path = dl_info.dli_fname;

	if (path.empty())
		return {};

	auto const pos = path.find_last_of('/');
	if (pos == std::string::npos)
		return {};

	path.erase(pos, path.length() - pos);
	return path;
}

void initialise_debin(libpisp::BeDebinConfig &debin, const json &root)
{
	constexpr unsigned int num_coefs = sizeof(debin.coeffs) / sizeof(debin.coeffs[0]);
	auto coefs = root["debin"]["coefs"].get<std::vector<int8_t>>();

	if (coefs.size() != num_coefs)
		throw std::runtime_error("Debin filter size mismatch");

	memcpy(debin.coeffs, coefs.data(), sizeof(debin.coeffs));
	debin.h_enable = debin.v_enable = 1;
}

void initialise_demosaic(libpisp::BeDemosaicConfig &demosaic, const json &root)
{
	auto &params = root["demosaic"];
	demosaic.sharper = params["sharper"].get<uint8_t>();
	demosaic.fc_mode = params["fc_mode"].get<uint8_t>();
}

void initialise_false_colour(pisp_be_false_colour_config &fc, const json &root)
{
	auto &params = root["false_colour"];
	fc.distance = params["distance"].get<uint8_t>();
}

void initialise_gamma(libpisp::BeGammaConfig &gamma, const json &root, bool apply_slope)
{
	constexpr unsigned int num_points = sizeof(gamma.lut) / sizeof(gamma.lut[0]);

	libpisp::Pwl pwl;

	// First read the config as a generic PWL
	json const &params = root["gamma"]["lut"];
	double last_x = 0.0;
	for (auto it = params.begin(); it != params.end(); it++)
	{
		double x = it->get<double>();
		assert(it == params.begin() || x > last_x);
		it++;
		double y = it->get<double>();
		pwl.append(x, y);
		last_x = x;
	}

	// Now resample the PWL at the hardware's X points
	static constexpr unsigned int SlopeBits = 14;
	static constexpr unsigned int PosBits = 16;
	int last_y = 0;
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
		if (y < 0 || (i && y < last_y))
			throw std::runtime_error("initialise_gamma: Malformed LUT");

		if (apply_slope && i)
		{
			unsigned int slope = y - last_y;
			if (slope >= (1u << SlopeBits))
			{
				slope = (1u << SlopeBits) - 1;
				y = last_y + slope;
			}
			gamma.lut[i - 1] |= slope << PosBits;
		}

		gamma.lut[i] = y;
		last_y = y;
	}
}

void read_resample(std::map<std::string, std::vector<int16_t>> &resample_filter_map,
				   std::vector<std::pair<double, std::string>> &resample_select_list, const json &root)
{
	auto &filters = root["resample"]["filters"];

	for (auto const &[name, filter] : filters.items())
	{
		auto coefs = filter.get<std::vector<int16_t>>();

		if (coefs.size() != PISP_BE_RESAMPLE_FILTER_SIZE)
			throw std::runtime_error("read_resample: Incorrect number of filter coefficients");

		resample_filter_map.emplace(name, coefs);
	}

	auto &smart = root["resample"]["smart_selection"];
	for (auto &scale : smart["downscale"])
		resample_select_list.emplace_back(scale.get<double>(), std::string {});

	unsigned int i = 0;
	for (auto &filter : smart["filter"])
	{
		resample_select_list[i].second = filter.get<std::string>();
		if (++i == resample_select_list.size())
			break;
	}
	if (i != resample_select_list.size())
		throw std::runtime_error("read_resample: Incorrect number of filters");
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

void read_sharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc, const json &root)
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

void read_ycbcr(std::map<std::string, libpisp::BeCcmConfig> &ycbcr_map,
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
				throw std::runtime_error("read_ycbcr: Incorrect number of matrix coefficients");
			memcpy(ccm.coeffs, coeffs.data(), sizeof(ccm.coeffs));

			auto offsets = matrix["offsets"].get<std::vector<int32_t>>();
			if (offsets.size() != 3)
				throw std::runtime_error("read_ycbcr: Incorrect number of matrix offsets");
			memcpy(ccm.offsets, offsets.data(), sizeof(ccm.offsets));

			if (key == "ycbcr")
				ycbcr_map.emplace(format, ccm);
			else
				inverse_ycbcr_map.emplace(format, ccm);
		}
	}
}

void get_matrix(libpisp::BeCcmConfig &matrix, const std::map<std::string, libpisp::BeCcmConfig> &map,
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

void BackEnd::initialise_ycbcr(libpisp::BeCcmConfig &ycbcr, const std::string &cs)
{
	get_matrix(ycbcr, ycbcr_map_, cs);
}

void BackEnd::initialise_ycbcr_inverse(libpisp::BeCcmConfig &ycbcr_inverse, const std::string &cs)
{
	get_matrix(ycbcr_inverse, inverse_ycbcr_map_, cs);
}

void BackEnd::initialise_resample_h(BeResampleConfig &resample, const std::string &filter)
{
	resample.coef_h = {};

	auto it = resample_filter_map_.find(filter);
	if (it != resample_filter_map_.end())
		std::copy(it->second.begin(), it->second.end(), resample.coef_h.begin());
}

void BackEnd::initialise_resample_v(BeResampleConfig &resample, const std::string &filter)
{
	resample.coef_v = {};

	auto it = resample_filter_map_.find(filter);
	if (it != resample_filter_map_.end())
		std::copy(it->second.begin(), it->second.end(), resample.coef_v.begin());
}

void BackEnd::initialise_resample(BeResampleConfig &resample, const std::string &filter)
{
	initialise_resample_h(resample, filter);
	initialise_resample_v(resample, filter);
}

const std::string &BackEnd::select_resample_filter(double downscale) const
{
	auto it = std::find_if(resample_select_list_.begin(), resample_select_list_.end(),
						   [downscale](const auto &item) { return item.first >= downscale; });

	return it != resample_select_list_.end() ? it->second : resample_select_list_.back().second;
}

void BackEnd::initialise_resample_h(BeResampleConfig &resample, double downscale)
{
	initialise_resample_h(resample, select_resample_filter(downscale));
}

void BackEnd::initialise_resample_v(BeResampleConfig &resample, double downscale)
{
	initialise_resample_v(resample, select_resample_filter(downscale));
}

void BackEnd::initialise_resample(BeResampleConfig &resample, double downscale)
{
	initialise_resample(resample, select_resample_filter(downscale));
}

void BackEnd::initialise_sharpen(libpisp::BeSharpenConfig &sharpen, libpisp::BeShFcCombineConfig &shfc)
{
	sharpen = default_sharpen_;
	shfc = default_shfc_;
}

void BackEnd::initialise_default_config(const std::string &filename)
{
	std::string file(filename);

	if (file.empty())
	{
		if (!is_installed())
		{
			std::string path = source_path();
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

	memset(&pipe_->hw_config, 0, sizeof(pipe_->hw_config));

	libpisp::BeDebinConfig debin;
	initialise_debin(debin, root);
	set_debin(debin);

	libpisp::BeDemosaicConfig demosaic{};
	initialise_demosaic(demosaic, root);
	set_demosaic(demosaic);

	BeFalseColourConfig fc;
	initialise_false_colour(fc, root);
	set_false_colour(fc);

	libpisp::BeGammaConfig gamma;
	initialise_gamma(gamma, root, variant_ <= BCM2712_D0 ? true : false);
	set_gamma(0, gamma);
	set_gamma(1, gamma);

	read_ycbcr(ycbcr_map_, inverse_ycbcr_map_, root);
	read_resample(resample_filter_map_, resample_select_list_, root);
	read_sharpen(default_sharpen_, default_shfc_, root);

	libpisp::BeSharpenConfig sharpen;
	libpisp::BeShFcCombineConfig shfc;
	initialise_sharpen(sharpen, shfc);
	set_sharpen(sharpen);
	set_sh_fc_combine(shfc);

	// Start with a sensible default YCbCr -- must be full-range on 2712C1
	libpisp::BeCcmConfig ycbcr, ycbcr_inverse;
	initialise_ycbcr(ycbcr, "jpeg");
	set_ycbcr(ycbcr);

	initialise_ycbcr_inverse(ycbcr_inverse, "jpeg");
	set_ycbcr_inverse(ycbcr_inverse);	

	// Start with a sensible default resamplers.
	libpisp::BeResampleConfig resample;
	initialise_resample(resample, "lanczos3");
	for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
		set_resample(i, resample);
}

} // namespace libpisp
