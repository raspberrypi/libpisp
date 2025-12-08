
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * frontend.hpp - PiSP Front End implementation
 */
#pragma once

#include <cstring>

#include "common/config_types.hpp"
#include "common/shm_mutex.hpp"
#include "variants/variant.hpp"

#include "pisp_fe_config.h"
#include "pisp_statistics.h"

namespace libpisp
{

// Frontend-specific type aliases
using FeGlobalConfig = pisp_fe_global_config;
using FeInputConfig = pisp_fe_input_config;
using FeInputBufferConfig = pisp_fe_input_buffer_config;
using FeDecompandConfig = pisp_fe_decompand_config;
using FeDpcConfig = pisp_fe_dpc_config;
using FeCropConfig = pisp_fe_crop_config;
using FeStatsDecimateConfig = pisp_fe_stats_decimate_config;
using FeRgbyConfig = pisp_fe_rgby_config;
using FeLscConfig = pisp_fe_lsc_config;
using FeAgcStatsConfig = pisp_fe_agc_stats_config;
using FeAwbStatsConfig = pisp_fe_awb_stats_config;
using FeFloatingStatsConfig = pisp_fe_floating_stats_config;
using FeCdafStatsConfig = pisp_fe_cdaf_stats_config;
using FeDownscaleConfig = pisp_fe_downscale_config;
using FeOutputBufferConfig = pisp_fe_output_buffer_config;

class FrontEnd
{
public:
	static constexpr uint32_t ScalePrecision = 10;
	static constexpr uint32_t InterpPrecision = 6;

	FrontEnd(bool streaming, PiSPVariant const &variant, int align = 64);
	~FrontEnd();

	void set_global(FeGlobalConfig const &global);
	void get_global(FeGlobalConfig &global) const;
	void set_input(FeInputConfig const &input);
	void get_input(FeInputConfig &input) const;
	void set_input_buffer(FeInputBufferConfig const &input_buffer);
	void get_input_buffer(FeInputBufferConfig &input_buffer) const;
	void set_decompress(DecompressConfig const &decompress);
	void get_decompress(DecompressConfig &decompress) const;
	void set_decompand(FeDecompandConfig const &decompand);
	void get_decompand(FeDecompandConfig &decompand) const;
	void set_dpc(FeDpcConfig const &dpc);
	void get_dpc(FeDpcConfig &dpc) const;
	void set_bla(BlaConfig const &bla);
	void get_bla(BlaConfig &bla) const;
	void set_stats_crop(FeCropConfig const &stats_crop);
	void get_stats_crop(FeCropConfig &stats_crop) const;
	void set_stats_decimate(FeStatsDecimateConfig const &stats_decimate);
	void set_blc(BlaConfig const &blc);
	void get_blc(BlaConfig &blc) const;
	void set_rgby(FeRgbyConfig const &rgby);
	void get_rgby(FeRgbyConfig &rgby) const;
	void set_lsc(FeLscConfig const &lsc);
	void get_lsc(FeLscConfig &lsc) const;
	void set_agc_stats(FeAgcStatsConfig const &agc_stats);
	void get_agc_stats(FeAgcStatsConfig &agc_stats) const;
	void set_awb_stats(FeAwbStatsConfig const &awb_stats);
	void get_awb_stats(FeAwbStatsConfig &awb_stats) const;
	void set_floating_stats(FeFloatingStatsConfig const &floating_stats);
	void get_floating_stats(FeFloatingStatsConfig &floating_stats) const;
	void set_cdaf_stats(FeCdafStatsConfig const &cdaf_stats);
	void get_cdaf_stats(FeCdafStatsConfig &cdaf_stats) const;
	void set_crop(unsigned int output_num, FeCropConfig const &crop);
	void get_crop(unsigned int output_num, FeCropConfig &crop) const;
	void set_downscale(unsigned int output_num, FeDownscaleConfig const &downscale);
	void get_downscale(unsigned int output_num, FeDownscaleConfig &downscale) const;
	void set_compress(unsigned int output_num, CompressConfig const &compress);
	void get_compress(unsigned int output_num, CompressConfig &compress) const;
	void set_output_format(unsigned int output_num, ImageFormatConfig const &output_format);
	void get_output_format(unsigned int output_num, ImageFormatConfig &output_format) const;
	void set_output_buffer(unsigned int output_num, FeOutputBufferConfig const &output_buffer);
	void get_output_buffer(unsigned int output_num, FeOutputBufferConfig &output_buffer) const;
	void set_output_intr_lines(unsigned int output_num, int lines);
	int get_output_intr_lines(unsigned int output_num) const;
	void prepare(pisp_fe_config *config);

	void lock()
	{
		mutex_.lock();
	}

	void unlock()
	{
		mutex_.unlock();
	}

	bool try_lock()
	{
		return mutex_.try_lock();
	}

private:
	void fix_output_size(unsigned int output_num);

	const PiSPVariant &variant_;
	pisp_fe_config fe_config_;
	int align_;
	mutable ShmMutex mutex_;
};

} // namespace libpisp
