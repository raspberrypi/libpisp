
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend.hpp - PiSP backend implementation
 */
#pragma once

#include <vector>

#include <boost/interprocess/sync/interprocess_mutex.hpp>

#include "backend/config.h"
#include "variants/pisp_variant.hpp"
#include "pisp_be_config.h"
#include "tiling/pisp_tiling.hpp"

// Definition of the PiSP Back End class.

namespace PiSP
{

class BackEnd
{
public:
	struct Config
	{
		enum Flags
		{
			NONE = 0,
			LOW_LATENCY = 1,	// Attempt to process image with lowest possible latency (no longer implemented)
			HIGH_PRIORITY = 2 	// Not currently implemented
		};

		Config(unsigned int _max_stripe_height = 0, unsigned int _max_tile_width = 0, unsigned int _flags = 0)
			: max_stripe_height(_max_stripe_height), max_tile_width(_max_tile_width), flags(_flags)
		{
		}

		unsigned int max_stripe_height; // Use zero to get "default behaviour"
		unsigned int max_tile_width; // Can only go larger than h/w defined limit in simulations
		unsigned int flags; // An "or" of the Flags above
	};

	struct SmartResize
	{
		uint16_t width;
		uint16_t height;
	};

	BackEnd(Config const &user_config, PiSPVariant const &variant);
	~BackEnd();

	void SetGlobal(pisp_be_global_config const &global);
	void GetGlobal(pisp_be_global_config &global) const;
	void SetInputFormat(pisp_image_format_config const &input_format);
	void SetInputBuffer(pisp_be_input_buffer_config const &input_buffer);
	void SetDecompress(pisp_decompress_config const &decompress);
	void SetDpc(pisp_be_dpc_config const &dpc);
	void SetGeq(pisp_be_geq_config const &geq);
	void SetTdnInputFormat(pisp_image_format_config const &tdn_input_format);
	void SetTdnDecompress(pisp_decompress_config const &tdn_decompress);
	void SetTdn(pisp_be_tdn_config const &tdn);
	void GetTdn(pisp_be_tdn_config &tdn) const;
	void SetTdnCompress(pisp_compress_config const &tdn_compress);
	void SetTdnOutputFormat(pisp_image_format_config const &tdn_output_format);
	void GetTdnOutputFormat(pisp_image_format_config &tdn_output_format) const;
	void SetSdn(pisp_be_sdn_config const &sdn);
	void SetBlc(pisp_bla_config const &blc);
	void SetStitchInputFormat(pisp_image_format_config const &stitch_input_format);
	void GetStitchInputFormat(pisp_image_format_config &stitch_input_format) const;
	void SetStitchDecompress(pisp_decompress_config const &stitch_decompress);
	void SetStitch(pisp_be_stitch_config const &stitch);
	void SetStitchCompress(pisp_compress_config const &stitch_compress);
	void SetStitchOutputFormat(pisp_image_format_config const &stitch_output_format);
	void GetStitchOutputFormat(pisp_image_format_config &stitch_output_format) const;
	void SetWbg(pisp_wbg_config const &wbg);
	void GetWbg(pisp_wbg_config &wbg);
	void SetCdn(pisp_be_cdn_config const &cdn);
	void SetLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lsc_extra = { 0, 0 });
	void SetCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cac_extra = { 0, 0 });
	void SetDebin(pisp_be_debin_config const &debin);
	void GetDebin(pisp_be_debin_config &debin);
	void SetTonemap(pisp_be_tonemap_config const &tonemap);
	void SetDemosaic(pisp_be_demosaic_config const &demosaic);
	void GetDemosaic(pisp_be_demosaic_config &demosaic) const;
	void SetCcm(pisp_be_ccm_config const &ccm);
	void SetSatControl(pisp_be_sat_control_config const &sat_control);
	void SetYcbcr(pisp_be_ccm_config const &ycbcr);
	void GetYcbcr(pisp_be_ccm_config &ccm);
	void SetFalseColour(pisp_be_false_colour_config const &false_colour);
	void SetSharpen(pisp_be_sharpen_config const &sharpen);
	void GetSharpen(pisp_be_sharpen_config &sharpen);
	void SetShFcCombine(pisp_be_sh_fc_combine_config const &sh_fc_combine);
	void SetYcbcrInverse(pisp_be_ccm_config const &ycbcr_inverse);
	void SetGamma(pisp_be_gamma_config const &gamma);
	void GetGamma(pisp_be_gamma_config &gamma);
	void SetCrop(pisp_be_crop_config const &crop);
	void SetCsc(unsigned int i, pisp_be_ccm_config const &csc);
	void GetCsc(unsigned int i, pisp_be_ccm_config &csc);
	void SetOutputFormat(unsigned int i, pisp_be_output_format_config const &output_format);
	void GetOutputFormat(unsigned int i, pisp_be_output_format_config &output_format) const;
	void SetResample(unsigned int i, pisp_be_resample_config const &resample,
					 pisp_be_resample_extra const &resample_extra);
	void SetResample(unsigned int i, pisp_be_resample_extra const &resample_extra);
	void SetDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
					  pisp_be_downscale_extra const &downscale_extra);
	void SetDownscale(unsigned int i, pisp_be_downscale_extra const &downscale_extra);
	void SetHog(pisp_be_hog_config const &hog);

	void InitialiseConfig(const std::string filename =
							std::string(PISP_BE_CONFIG_DIR) + "/" + "backend_default_config.json");
	void Prepare(pisp_be_tiles_config *config);
	void MergeConfig(const pisp_be_config &config);

	bool ComputeOutputImageFormat(unsigned int i, pisp_image_format_config &output_format,
								  pisp_image_format_config const &input_format) const;
	bool ComputeHogOutputImageFormat(pisp_image_format_config &output_format,
									 pisp_image_format_config const &input_format) const;

	void SetSmartResize(unsigned int i, SmartResize const &smart_resize);

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

protected:
	void finaliseConfig();
	void updateSmartResize();
	void updateTiles();
	std::vector<pisp_tile> retilePipeline(TilingConfig const &tiling_config);
	void finaliseTiling();
	void getOutputSize(int output_num, uint16_t *width, uint16_t *height, pisp_image_format_config const &ifmt) const;

	Config config_;
	const PiSPVariant variant_;
	pisp_be_config be_config_;
	pisp_image_format_config max_input_;
	bool retile_;
	bool finalise_tiling_;
	std::vector<pisp_tile> tiles_;
	int num_tiles_x_, num_tiles_y_;
	mutable boost::interprocess::interprocess_mutex mutex_;
	std::vector<SmartResize> smart_resize_;
	uint32_t smart_resize_dirty_;
};


// Fill in the matrix from a string naming the colour space ("jpeg", "smpte170m, "rec709", "bt2020").
void initialise_ycbcr(pisp_be_ccm_config &ycbcr, const std::string &colour_space);
void initialise_ycbcr_inverse(pisp_be_ccm_config &ycbcr_inverse, const std::string &colour_space);
// Fill in the coefficients from a filter string name ("lanczos3", "lanczos2", "bicubic-spline", "michel-netravali").
void initialise_resample(pisp_be_resample_config &resample, const std::string &filter);
void initialise_resample(pisp_be_resample_config &resample, double downscale);
void initialise_sharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc);

} // namespace PiSP
