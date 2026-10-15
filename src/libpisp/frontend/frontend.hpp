
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * frontend.hpp - PiSP Front End implementation
 */
#pragma once

#include <cstring>
#include <type_traits>

#include "common/pisp_common.h"
#include "common/shm_mutex.hpp"
#include "variants/variant.hpp"

#include "pisp_fe_config.h"
#include "pisp_statistics.h"

namespace libpisp
{

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
using ImageFormatConfig = pisp_image_format_config;
using DecompressConfig = pisp_decompress_config;
using CompressConfig = pisp_compress_config;
using BlaConfig = pisp_bla_config;

class FrontEnd
{
public:
	static constexpr uint32_t ScalePrecision = 10;
	static constexpr uint32_t InterpPrecision = 6;

	FrontEnd(bool streaming, PiSPVariant const &variant, int align = 64);
	~FrontEnd();

	void set_global(FeGlobalConfig const &global);
	FeGlobalConfig get_global() const;
	void set_input(FeInputConfig const &input);
	FeInputConfig get_input() const;
	void set_input_buffer(FeInputBufferConfig const &input_buffer);
	FeInputBufferConfig get_input_buffer() const;
	void set_decompress(DecompressConfig const &decompress);
	DecompressConfig get_decompress() const;
	void set_decompand(FeDecompandConfig const &decompand);
	FeDecompandConfig get_decompand() const;
	void set_dpc(FeDpcConfig const &dpc);
	FeDpcConfig get_dpc() const;
	void set_bla(BlaConfig const &bla);
	BlaConfig get_bla() const;
	void set_stats_crop(FeCropConfig const &stats_crop);
	FeCropConfig get_stats_crop() const;
	void set_stats_decimate(FeStatsDecimateConfig const &stats_decimate);
	void set_blc(BlaConfig const &blc);
	BlaConfig get_blc() const;
	void set_rgby(FeRgbyConfig const &rgby);
	FeRgbyConfig get_rgby() const;
	void set_lsc(FeLscConfig const &lsc);
	FeLscConfig get_lsc() const;
	void set_agc_stats(FeAgcStatsConfig const &agc_stats);
	FeAgcStatsConfig get_agc_stats() const;
	void set_awb_stats(FeAwbStatsConfig const &awb_stats);
	FeAwbStatsConfig get_awb_stats() const;
	void set_floating_stats(FeFloatingStatsConfig const &floating_stats);
	FeFloatingStatsConfig get_floating_stats() const;
	void set_cdaf_stats(FeCdafStatsConfig const &cdaf_stats);
	FeCdafStatsConfig get_cdaf_stats() const;
	void set_crop(unsigned int output_num, FeCropConfig const &crop);
	FeCropConfig get_crop(unsigned int output_num) const;
	void set_downscale(unsigned int output_num, FeDownscaleConfig const &downscale);
	FeDownscaleConfig get_downscale(unsigned int output_num) const;
	void set_compress(unsigned int output_num, CompressConfig const &compress);
	CompressConfig get_compress(unsigned int output_num) const;
	void set_output_format(unsigned int output_num, ImageFormatConfig const &output_format);
	ImageFormatConfig get_output_format(unsigned int output_num) const;
	void set_output_buffer(unsigned int output_num, FeOutputBufferConfig const &output_buffer);
	FeOutputBufferConfig get_output_buffer(unsigned int output_num) const;
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

	const PiSPVariant variant_;
	pisp_fe_config fe_config_;
	int align_;
	mutable ShmMutex mutex_;
};

// This is required to ensure we can safely share a FrontEnd object across multiple processes.
static_assert(std::is_standard_layout_v<FrontEnd>, "FrontEnd must be a standard layout type");

} // namespace libpisp
