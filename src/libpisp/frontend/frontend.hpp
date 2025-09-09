
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

class FrontEnd final
{
public:
	static constexpr uint32_t ScalePrecision = 10;
	static constexpr uint32_t InterpPrecision = 6;

	FrontEnd(bool streaming, PiSPVariant const &variant, int align = 64);
	~FrontEnd();

	void setGlobal(pisp_fe_global_config const &global);
	void getGlobal(pisp_fe_global_config &global) const;
	void setInput(pisp_fe_input_config const &input);
	void getInput(pisp_fe_input_config &input) const;
	void setInputBuffer(pisp_fe_input_buffer_config const &inputBuffer);
	void getInputBuffer(pisp_fe_input_buffer_config &inputBuffer) const;
	void setDecompress(pisp_decompress_config const &decompress);
	void getDecompress(pisp_decompress_config &decompress) const;
	void setDecompand(pisp_fe_decompand_config const &decompand);
	void getDecompand(pisp_fe_decompand_config &decompand) const;
	void setDpc(pisp_fe_dpc_config const &dpc);
	void getDpc(pisp_fe_dpc_config &dpc) const;
	void setBla(pisp_bla_config const &bla);
	void getBla(pisp_bla_config &bla) const;
	void setStatsCrop(pisp_fe_crop_config const &statsCrop);
	void getStatsCrop(pisp_fe_crop_config &statsCrop) const;
	void setStatsDecimate(pisp_fe_stats_decimate_config const &stats_decimate);
	void setBlc(pisp_bla_config const &blc);
	void getBlc(pisp_bla_config &blc) const;
	void setRgby(pisp_fe_rgby_config const &rgby);
	void getRgby(pisp_fe_rgby_config &rgby) const;
	void setLsc(pisp_fe_lsc_config const &lsc);
	void getLsc(pisp_fe_lsc_config &lsc) const;
	void setAgcStats(pisp_fe_agc_stats_config const &agcStats);
	void getAgcStats(pisp_fe_agc_stats_config &agcStats) const;
	void setAwbStats(pisp_fe_awb_stats_config const &awbStats);
	void getAwbStats(pisp_fe_awb_stats_config &awbStats) const;
	void setFloatingStats(pisp_fe_floating_stats_config const &floatingStats);
	void getFloatingStats(pisp_fe_floating_stats_config &floatingStats) const;
	void setCdafStats(pisp_fe_cdaf_stats_config const &cdafStats);
	void getCdafStats(pisp_fe_cdaf_stats_config &cdafStats) const;
	void setCrop(unsigned int outputNum, pisp_fe_crop_config const &crop);
	void getCrop(unsigned int outputNum, pisp_fe_crop_config &crop) const;
	void setDownscale(unsigned int outputNum, pisp_fe_downscale_config const &downscale);
	void getDownscale(unsigned int outputNum, pisp_fe_downscale_config &downscale) const;
	void setCompress(unsigned int outputNum, pisp_compress_config const &compress);
	void getCompress(unsigned int outputNum, pisp_compress_config &compress) const;
	void setOutputFormat(unsigned int outputNum, pisp_image_format_config const &outputFormat);
	void getOutputFormat(unsigned int outputNum, pisp_image_format_config &outputFormat) const;
	void setOutputBuffer(unsigned int outputNum, pisp_fe_output_buffer_config const &outputBuffer);
	void getOutputBuffer(unsigned int outputNum, pisp_fe_output_buffer_config &outputBuffer) const;
	void setOutputIntrLines(unsigned int outputNum, int lines);
	int getOutputIntrLines(unsigned int outputNum) const;
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
	void fixOutputSize(unsigned int outputNum);

	const PiSPVariant &variant_;
	pisp_fe_config feConfig_;
	int align_;
	mutable ShmMutex mutex_;
};

} // namespace libpisp
