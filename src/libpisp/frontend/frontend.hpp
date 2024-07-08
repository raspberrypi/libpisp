
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

class FrontEnd final
{
public:
	static constexpr uint32_t ScalePrecision = 10;
	static constexpr uint32_t InterpPrecision = 6;

	FrontEnd(bool streaming, PiSPVariant const &variant, int align = 64);
	~FrontEnd();

	void SetGlobal(pisp_fe_global_config const &global);
	void GetGlobal(pisp_fe_global_config &global) const;
	void SetInput(pisp_fe_input_config const &input);
	void SetInputBuffer(pisp_fe_input_buffer_config const &input_buffer);
	void SetDecompress(pisp_decompress_config const &decompress);
	void SetDecompand(pisp_fe_decompand_config const &decompand);
	void SetDpc(pisp_fe_dpc_config const &dpc);
	void SetBla(pisp_bla_config const &bla);
	void SetStatsCrop(pisp_fe_crop_config const &stats_crop);
	void SetBlc(pisp_bla_config const &blc);
	void SetRGBY(pisp_fe_rgby_config const &rgby);
	void SetLsc(pisp_fe_lsc_config const &lsc);
	void SetAgcStats(pisp_fe_agc_stats_config const &agc_stats);
	void GetAgcStats(pisp_fe_agc_stats_config &agc_stats);
	void SetAwbStats(pisp_fe_awb_stats_config const &awb_stats);
	void GetAwbStats(pisp_fe_awb_stats_config &awb_stats);
	void SetFloatingStats(pisp_fe_floating_stats_config const &floating_stats);
	void SetCdafStats(pisp_fe_cdaf_stats_config const &cdaf_stats);
	void GetCdafStats(pisp_fe_cdaf_stats_config &cdaf_stats);
	void SetCrop(unsigned int output_num, pisp_fe_crop_config const &crop);
	void SetDownscale(unsigned int output_num, pisp_fe_downscale_config const &downscale);
	void SetCompress(unsigned int output_num, pisp_compress_config const &compress);
	void SetOutputFormat(unsigned int output_num, pisp_image_format_config const &output_format);
	void SetOutputBuffer(unsigned int output_num, pisp_fe_output_buffer_config const &output_buffer);
	void SetOutputIntrLines(unsigned int output_num, int lines);
	void Prepare(pisp_fe_config *config);

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
	void fixOutputSize(unsigned int output_num);

	const PiSPVariant variant_;
	pisp_fe_config fe_config_;
	int align_;
	mutable ShmMutex mutex_;
};

// This is required to ensure we can safely share a FrontEnd object across multiple processes.
static_assert(std::is_standard_layout<FrontEnd>::value, "FrontEnd must be a standard layout type");

} // namespace libpisp
