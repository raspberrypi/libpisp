
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend_default_config.cpp - Default configuration setup for the PiSP Back End
 */
#include "backend.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <string>
#include <vector>

#include "pisp_be_config.h"

#define DEFAULT_CONFIG_FILE "/usr/local/share/libpisp/backend_default_config.json"

// Where it might be helpful we initialise some blocks with the "obvious" default parameters. This saves users the trouble,
// and they can just "enable" the blocks.

namespace
{

std::map<std::string, pisp_be_ccm_config> ycbcr_map;
std::map<std::string, pisp_be_ccm_config> inverse_ycbcr_map;
std::map<std::string, pisp_be_resample_config> resample_map;

void initialise_debin(pisp_be_debin_config &debin)
{
	static const int8_t default_coeffs[] = { -7, 105, 35, -5 };
	static_assert(sizeof(default_coeffs) == sizeof(debin.coeffs), "Debin filter size mismatch");

	debin.h_enable = debin.v_enable = 1;
	memcpy(debin.coeffs, default_coeffs, sizeof(debin.coeffs));
}

void initialise_gamma(pisp_be_gamma_config &gamma)
{
	boost::property_tree::ptree root;
	boost::property_tree::read_json(DEFAULT_CONFIG_FILE, root);
	auto params = root.get_child("gamma_lut");

	std::vector<std::string> gamma_lut_v;
	for (auto &x : params)
		gamma_lut_v.push_back(x.second.data());
	uint32_t gamma_lut[PISP_BE_GAMMA_LUT_SIZE];
	for (int i = 0; i < PISP_BE_GAMMA_LUT_SIZE; i++)
		gamma_lut[i] = std::stoul(gamma_lut_v[i], nullptr, 16);

	memcpy(gamma.lut, gamma_lut, sizeof(gamma.lut));
}

void read_resample(boost::property_tree::ptree &root)
{
	auto &filters = root.get_child("resample_filters");

	for (auto const &filter : filters)
	{
		pisp_be_resample_config r;
		constexpr unsigned int num_coefs = sizeof(r.coef) / sizeof(r.coef[0]);
		unsigned int i = 0;

		for (auto &c : filter.second)
		{
			r.coef[i] = c.second.get_value<int16_t>();
			if (++i == num_coefs)
				break;
		}
		if (i != num_coefs)
			throw std::runtime_error("read_resample: Incorrect number of filter coefficients");

		resample_map.emplace(filter.first, r);
	}
}

// Macros for the sharpening filters, to avoid repeating the same code 5 times
#define FILTER(i) {																						\
		auto filter = params.get_child("filter" #i);													\
		std::vector<int8_t> kernel_v;																	\
		for (auto &x : filter.get_child("kernel"))														\
			kernel_v.push_back(x.second.get_value<int8_t>());											\
		int8_t* kernel = &kernel_v[0];																	\
		uint16_t offset = filter.get_child("offset").get_value<uint16_t>();								\
		uint16_t threshold_slope = filter.get_child("threshold_slope").get_value<uint16_t>();			\
		uint16_t scale = filter.get_child("scale").get_value<uint16_t>();								\
		memcpy(sharpen.kernel##i, kernel, sizeof(sharpen.kernel##i));									\
		memcpy(&sharpen.threshold_offset##i, &offset, sizeof(sharpen.threshold_offset##i));				\
		memcpy(&sharpen.threshold_slope##i, &threshold_slope, sizeof(sharpen.threshold_slope##i));		\
		memcpy(&sharpen.scale##i, &scale, sizeof(sharpen.scale##i));									\
	}

#define POS_NEG(i) {																					\
		auto tive = params.get_child(#i "tive");														\
		uint16_t strength = tive.get_child("strength").get_value<uint16_t>();							\
		uint16_t pre_limit = tive.get_child("pre_limit").get_value<uint16_t>();							\
		std::vector<uint16_t> function_v;																\
		for (auto &x : tive.get_child("function"))														\
			function_v.push_back(x.second.get_value<uint16_t>());										\
		uint16_t* function = &function_v[0];															\
		uint16_t limit = tive.get_child("limit").get_value<uint16_t>();									\
		memcpy(&sharpen.i##tive_strength, &strength, sizeof(sharpen.i##tive_strength));					\
		memcpy(&sharpen.i##tive_pre_limit, &pre_limit, sizeof(sharpen.i##tive_pre_limit));				\
		memcpy(sharpen.i##tive_func, function, sizeof(sharpen.i##tive_func));							\
		memcpy(&sharpen.i##tive_limit, &limit, sizeof(sharpen.i##tive_limit));				\
	}

void initialise_sharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc)
{
	boost::property_tree::ptree root;
	boost::property_tree::read_json(DEFAULT_CONFIG_FILE, root);
	auto params = root.get_child("sharpen");

	FILTER(0);
	FILTER(1);
	FILTER(2);
	FILTER(3);
	FILTER(4);

	POS_NEG(posi);
	POS_NEG(nega);

	std::string enables_s = params.get_child("enables").data();
	uint8_t enables = std::stoul(enables_s, nullptr, 16);
	uint8_t white = params.get_child("white").get_value<uint8_t>();
	uint8_t black = params.get_child("black").get_value<uint8_t>();
	uint8_t grey = params.get_child("grey").get_value<uint8_t>();
	memcpy(&sharpen.enables, &enables, sizeof(sharpen.enables));
	memcpy(&sharpen.white, &white, sizeof(sharpen.white));
	memcpy(&sharpen.black, &black, sizeof(sharpen.black));
	memcpy(&sharpen.grey, &grey, sizeof(sharpen.grey));

	memset(&shfc, 0, sizeof(shfc));
	shfc.y_factor = 0.75 * (1 << 8);
}

void read_ycbcr(boost::property_tree::ptree &root)
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

namespace PiSP
{

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

	auto it = resample_map.find(filter);
	if (it != resample_map.end())
		memcpy(resample.coef, it->second.coef, sizeof(resample.coef));
}

void BackEnd::InitialiseConfig()
{
	boost::property_tree::ptree root;
	boost::property_tree::read_json(DEFAULT_CONFIG_FILE, root);

	memset(&be_config_, 0, sizeof(be_config_));

	initialise_debin(be_config_.debin);
	be_config_.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEBIN;

	read_ycbcr(root);
	// Start with a sensible default
	initialise_ycbcr(be_config_.ycbcr, "jpeg");
	initialise_ycbcr_inverse(be_config_.ycbcr_inverse, "jpeg");
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_YCBCR + PISP_BE_RGB_ENABLE_YCBCR_INVERSE;

	initialise_gamma(be_config_.gamma);
	initialise_sharpen(be_config_.sharpen, be_config_.sh_fc_combine);
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_GAMMA + PISP_BE_RGB_ENABLE_SHARPEN;

	read_resample(root);
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
	{
		// Start with a sensible default
		initialise_resample(be_config_.resample[i], "lanczos3");
		be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_RESAMPLE(i);
	}
}

} // namespace PiSP
