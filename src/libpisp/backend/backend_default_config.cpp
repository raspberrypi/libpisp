
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend_default_config.cpp - Default configuration setup for the PiSP Back End
 */
#include "backend.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "backend/pisp_build_config.h"
#include "common/pisp_pwl.hpp"
#include "pisp_be_config.h"

// Where it might be helpful we initialise some blocks with the "obvious" default parameters. This saves users the trouble,
// and they can just "enable" the blocks.

namespace
{

std::mutex mutex;
bool init = false;

std::map<std::string, pisp_be_ccm_config> ycbcr_map;
std::map<std::string, pisp_be_ccm_config> inverse_ycbcr_map;
std::map<std::string, pisp_be_resample_config> resample_filter_map;
std::vector<std::pair<double, std::string>> resample_select_list;
pisp_be_debin_config default_debin;
pisp_be_demosaic_config default_demosaic;
pisp_be_sharpen_config default_sharpen;
pisp_be_gamma_config default_gamma;
pisp_be_false_colour_config default_fc;
pisp_be_sh_fc_combine_config default_shfc;

void read_debin(const boost::property_tree::ptree &root)
{
	constexpr unsigned int num_coefs = sizeof(default_debin.coeffs) / sizeof(default_debin.coeffs[0]);
	auto &coefs = root.get_child("debin.coefs");
	unsigned int i = 0;

	for (auto &c : coefs)
	{
		default_debin.coeffs[i] = c.second.get_value<int8_t>();
		if (++i == num_coefs)
			break;
	}
	if (i != num_coefs)
		throw std::runtime_error("Debin filter size mismatch");

	default_debin.h_enable = default_debin.v_enable = 1;
}

void read_demosaic(const boost::property_tree::ptree &root)
{
	auto &params = root.get_child("demosaic");
	default_demosaic.sharper = params.get_child("sharper").get_value<uint8_t>();
	default_demosaic.fc_mode = params.get_child("fc_mode").get_value<uint8_t>();
}

void read_false_colour(const boost::property_tree::ptree &root)
{
	auto &params = root.get_child("false_colour");
	default_fc.distance = params.get_child("distance").get_value<uint8_t>();
}

void read_gamma(const boost::property_tree::ptree &root)
{
	constexpr unsigned int num_points = sizeof(default_gamma.lut) / sizeof(default_gamma.lut[0]);

	libpisp::Pwl pwl;
	pwl.Read(root.get_child("gamma.lut"));

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

		y = pwl.Eval(x);
		if (y < 0 || (i && y < lastY))
			throw std::runtime_error("initialise_gamma: Malformed LUT");

		if (i)
		{
			unsigned int slope = y - lastY;
			if (slope >= (1u << SlopeBits))
			{
				slope = (1u << SlopeBits) - 1;
				y = lastY + slope;
			}
			default_gamma.lut[i - 1] |= slope << PosBits;
		}

		default_gamma.lut[i] = y;
		lastY = y;
	}
}

void read_resample(const boost::property_tree::ptree &root)
{
	auto &filters = root.get_child("resample.filters");
	unsigned int i;

	for (auto const &filter : filters)
	{
		pisp_be_resample_config r;
		constexpr unsigned int num_coefs = sizeof(r.coef) / sizeof(r.coef[0]);

		i = 0;
		for (auto &c : filter.second)
		{
			r.coef[i] = c.second.get_value<int16_t>();
			if (++i == num_coefs)
				break;
		}
		if (i != num_coefs)
			throw std::runtime_error("read_resample: Incorrect number of filter coefficients");

		resample_filter_map.emplace(filter.first, r);
	}

	auto &smart = root.get_child("resample.smart_selection");
	for (auto &scale : smart.get_child("downscale"))
		resample_select_list.emplace_back(scale.second.get_value<double>(), std::string {});

	i = 0;
	for (auto &filter : smart.get_child("filter"))
	{
		resample_select_list[i].second = filter.second.get_value<std::string>();
		if (++i == resample_select_list.size())
			break;
	}
	if (i != resample_select_list.size())
		throw std::runtime_error("read_resample: Incorrect number of filters");
}

// Macros for the sharpening filters, to avoid repeating the same code 5 times
#define FILTER(i)                                                                                                      \
	{                                                                                                                  \
		auto filter = params.get_child("filter" #i);                                                                   \
		std::vector<int8_t> kernel_v;                                                                                  \
		for (auto &x : filter.get_child("kernel"))                                                                     \
			kernel_v.push_back(x.second.get_value<int8_t>());                                                          \
		int8_t *kernel = &kernel_v[0];                                                                                 \
		uint16_t offset = filter.get_child("offset").get_value<uint16_t>();                                            \
		uint16_t threshold_slope = filter.get_child("threshold_slope").get_value<uint16_t>();                          \
		uint16_t scale = filter.get_child("scale").get_value<uint16_t>();                                              \
		memcpy(default_sharpen.kernel##i, kernel, sizeof(default_sharpen.kernel##i));                                  \
		memcpy(&default_sharpen.threshold_offset##i, &offset, sizeof(default_sharpen.threshold_offset##i));            \
		memcpy(&default_sharpen.threshold_slope##i, &threshold_slope, sizeof(default_sharpen.threshold_slope##i));     \
		memcpy(&default_sharpen.scale##i, &scale, sizeof(default_sharpen.scale##i));                                   \
	}

#define POS_NEG(i)                                                                                                     \
	{                                                                                                                  \
		auto tive = params.get_child(#i "tive");                                                                       \
		uint16_t strength = tive.get_child("strength").get_value<uint16_t>();                                          \
		uint16_t pre_limit = tive.get_child("pre_limit").get_value<uint16_t>();                                        \
		std::vector<uint16_t> function_v;                                                                              \
		for (auto &x : tive.get_child("function"))                                                                     \
			function_v.push_back(x.second.get_value<uint16_t>());                                                      \
		uint16_t *function = &function_v[0];                                                                           \
		uint16_t limit = tive.get_child("limit").get_value<uint16_t>();                                                \
		memcpy(&default_sharpen.i##tive_strength, &strength, sizeof(default_sharpen.i##tive_strength));                \
		memcpy(&default_sharpen.i##tive_pre_limit, &pre_limit, sizeof(default_sharpen.i##tive_pre_limit));             \
		memcpy(default_sharpen.i##tive_func, function, sizeof(default_sharpen.i##tive_func));                          \
		memcpy(&default_sharpen.i##tive_limit, &limit, sizeof(default_sharpen.i##tive_limit));                         \
	}

void read_sharpen(const boost::property_tree::ptree &root)
{
	auto params = root.get_child("sharpen");

	FILTER(0);
	FILTER(1);
	FILTER(2);
	FILTER(3);
	FILTER(4);

	POS_NEG(posi);
	POS_NEG(nega);

	std::string enables_s = params.get_child("enables").data();
	default_sharpen.enables = std::stoul(enables_s, nullptr, 16);
	default_sharpen.white = params.get_child("white").get_value<uint8_t>();
	default_sharpen.black = params.get_child("black").get_value<uint8_t>();
	default_sharpen.grey = params.get_child("grey").get_value<uint8_t>();

	memset(&default_shfc, 0, sizeof(default_shfc));
	default_shfc.y_factor = 0.75 * (1 << 8);
}

void read_ycbcr(const boost::property_tree::ptree &root)
{
	auto encoding = root.get_child("colour_encoding");

	for (auto const &enc : encoding)
	{
		static const std::string keys[2] { "ycbcr", "ycbcr_inverse" };
		unsigned int i = 0;

		for (auto &key : keys)
		{
			auto matrix = enc.second.get_child(key);
			pisp_be_ccm_config ccm;

			i = 0;
			for (auto &x : matrix.get_child("coeffs"))
			{
				ccm.coeffs[i] = x.second.get_value<int16_t>();
				if (++i == 9)
					break;
			}
			if (i != 9)
				throw std::runtime_error("read_ycbcr: Incorrect number of matrix coefficients");

			i = 0;
			for (auto &x : matrix.get_child("offsets"))
			{
				ccm.offsets[i] = x.second.get_value<int32_t>();
				if (++i == 3)
					break;
			}
			if (i != 3)
				throw std::runtime_error("read_ycbcr: Incorrect number of matrix offsets");

			if (key == "ycbcr")
				ycbcr_map.emplace(enc.first, ccm);
			else
				inverse_ycbcr_map.emplace(enc.first, ccm);
		}
	}
}

void get_matrix(const std::map<std::string, pisp_be_ccm_config> &map, pisp_be_ccm_config &matrix,
				const std::string &colour_space)
{
	memset(matrix.coeffs, 0, sizeof(matrix.coeffs));
	memset(matrix.offsets, 0, sizeof(matrix.offsets));

	auto it = map.find(colour_space);
	if (it != map.end())
	{
		memcpy(matrix.coeffs, it->second.coeffs, sizeof(matrix.coeffs));
		memcpy(matrix.offsets, it->second.offsets, sizeof(matrix.offsets));
	}
}

} // namespace

namespace libpisp
{

void initialise_debin(pisp_be_debin_config &debin)
{
	debin = default_debin;
}

void initialise_demosaic(pisp_be_demosaic_config &demosaic)
{
	demosaic = default_demosaic;
}

void initialise_false_colour(pisp_be_false_colour_config &fc)
{
	fc = default_fc;
}

void initialise_gamma(pisp_be_gamma_config &gamma)
{
	gamma = default_gamma;
}

void initialise_ycbcr(pisp_be_ccm_config &ycbcr, const std::string &colour_space)
{
	get_matrix(ycbcr_map, ycbcr, colour_space);
}

void initialise_ycbcr_inverse(pisp_be_ccm_config &ycbcr_inverse, const std::string &colour_space)
{
	get_matrix(inverse_ycbcr_map, ycbcr_inverse, colour_space);
}

void initialise_resample(pisp_be_resample_config &resample, const std::string &filter)
{
	memset(resample.coef, 0, sizeof(resample.coef));

	auto it = resample_filter_map.find(filter);
	if (it != resample_filter_map.end())
		memcpy(resample.coef, it->second.coef, sizeof(resample.coef));
}

void initialise_resample(pisp_be_resample_config &resample, double downscale)
{
	auto it = std::find_if(resample_select_list.begin(), resample_select_list.end(),
						   [downscale](const auto &item) { return item.first >= downscale; });

	if (it != resample_select_list.end())
		initialise_resample(resample, it->second);
	else
		initialise_resample(resample, resample_select_list.back().second);
}

void initialise_sharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc)
{
	sharpen = default_sharpen;
	shfc = default_shfc;
}

void BackEnd::InitialiseConfig(const std::string filename)
{
	std::scoped_lock<std::mutex> l(mutex);

	if (!init)
	{
		std::string file = filename.empty() ? std::string(PISP_BE_CONFIG_DIR) + "/" + "backend_default_config.json"
											: filename;
		std::ifstream ifs(file);
		if (!ifs.good())
			throw std::runtime_error("BE: Could not find config json file: " + file);

		ifs.close();

		boost::property_tree::ptree root;
		boost::property_tree::read_json(file, root);
		read_debin(root);
		read_demosaic(root);
		read_false_colour(root);
		read_ycbcr(root);
		read_gamma(root);
		read_sharpen(root);
		read_resample(root);
		init = true;
	}

	memset(&be_config_, 0, sizeof(be_config_));

	initialise_debin(be_config_.debin);
	be_config_.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEBIN;

	initialise_demosaic(be_config_.demosaic);
	be_config_.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEMOSAIC;

	initialise_false_colour(be_config_.false_colour);
	be_config_.dirty_flags_bayer |= PISP_BE_RGB_ENABLE_FALSE_COLOUR;

	// Start with a sensible default YCbCr -- must be full-range on 2712C1
	initialise_ycbcr(be_config_.ycbcr, "jpeg");
	initialise_ycbcr_inverse(be_config_.ycbcr_inverse, "jpeg");
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_YCBCR + PISP_BE_RGB_ENABLE_YCBCR_INVERSE;

	initialise_gamma(be_config_.gamma);
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_GAMMA;

	initialise_sharpen(be_config_.sharpen, be_config_.sh_fc_combine);
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_SHARPEN;

	for (unsigned int i = 0; i < variant_.BackEndNumBranches(0); i++)
	{
		// Start with a sensible default
		initialise_resample(be_config_.resample[i], "lanczos3");
		be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_RESAMPLE(i);
	}
}

} // namespace libpisp
