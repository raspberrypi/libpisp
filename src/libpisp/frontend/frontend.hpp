
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * frontend.hpp - PiSP Front End implementation
 */
#pragma once

#include <cstring>

#include "common/pisp_common.h"
#include "common/shm_mutex.hpp"
#include "variants/variant.hpp"

#include "pisp_fe_config.h"
#include "pisp_statistics.h"

namespace libpisp
{

class FrontEnd
{
public:
	static constexpr uint32_t ScalePrecision = 10;
	static constexpr uint32_t InterpPrecision = 6;

	FrontEnd(bool streaming, PiSPVariant const &variant, int align = 64);
	~FrontEnd();

	void set_global(pisp_fe_global_config const &global);
	void get_global(pisp_fe_global_config &global) const;
	void set_input(pisp_fe_input_config const &input);
	void get_input(pisp_fe_input_config &input) const;
	void set_input_buffer(pisp_fe_input_buffer_config const &input_buffer);
	void get_input_buffer(pisp_fe_input_buffer_config &input_buffer) const;
	void set_decompress(pisp_decompress_config const &decompress);
	void get_decompress(pisp_decompress_config &decompress) const;
	void set_decompand(pisp_fe_decompand_config const &decompand);
	void get_decompand(pisp_fe_decompand_config &decompand) const;
	void set_dpc(pisp_fe_dpc_config const &dpc);
	void get_dpc(pisp_fe_dpc_config &dpc) const;
	void set_bla(pisp_bla_config const &bla);
	void get_bla(pisp_bla_config &bla) const;
	void set_stats_crop(pisp_fe_crop_config const &stats_crop);
	void get_stats_crop(pisp_fe_crop_config &stats_crop) const;
	void set_stats_decimate(pisp_fe_stats_decimate_config const &stats_decimate);
	void set_blc(pisp_bla_config const &blc);
	void get_blc(pisp_bla_config &blc) const;
	void set_rgby(pisp_fe_rgby_config const &rgby);
	void get_rgby(pisp_fe_rgby_config &rgby) const;
	void set_lsc(pisp_fe_lsc_config const &lsc);
	void get_lsc(pisp_fe_lsc_config &lsc) const;
	void set_agc_stats(pisp_fe_agc_stats_config const &agc_stats);
	void get_agc_stats(pisp_fe_agc_stats_config &agc_stats) const;
	void set_awb_stats(pisp_fe_awb_stats_config const &awb_stats);
	void get_awb_stats(pisp_fe_awb_stats_config &awb_stats) const;
	void set_floating_stats(pisp_fe_floating_stats_config const &floating_stats);
	void get_floating_stats(pisp_fe_floating_stats_config &floating_stats) const;
	void set_cdaf_stats(pisp_fe_cdaf_stats_config const &cdaf_stats);
	void get_cdaf_stats(pisp_fe_cdaf_stats_config &cdaf_stats) const;
	void set_crop(unsigned int output_num, pisp_fe_crop_config const &crop);
	void get_crop(unsigned int output_num, pisp_fe_crop_config &crop) const;
	void set_downscale(unsigned int output_num, pisp_fe_downscale_config const &downscale);
	void get_downscale(unsigned int output_num, pisp_fe_downscale_config &downscale) const;
	void set_compress(unsigned int output_num, pisp_compress_config const &compress);
	void get_compress(unsigned int output_num, pisp_compress_config &compress) const;
	void set_output_format(unsigned int output_num, pisp_image_format_config const &output_format);
	void get_output_format(unsigned int output_num, pisp_image_format_config &output_format) const;
	void set_output_buffer(unsigned int output_num, pisp_fe_output_buffer_config const &output_buffer);
	void get_output_buffer(unsigned int output_num, pisp_fe_output_buffer_config &output_buffer) const;
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
