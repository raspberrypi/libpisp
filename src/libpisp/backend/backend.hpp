
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend.hpp - PiSP backend implementation
 */
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "common/shm_mutex.hpp"
#include "tiling/pisp_tiling.hpp"
#include "variants/variant.hpp"

#include "backend_config_adapter.hpp"

#include "pisp_be_config.h"

// Definition of the PiSP Back End class.

namespace libpisp
{

class BackEnd final
{
public:
	struct SoftwareConfig
	{
		enum class Flags
		{
			None = 0,
			LowLatency = 1,	// Attempt to process image with lowest possible latency (no longer implemented)
			HighPriority = 2 	// Not currently implemented
		};

		SoftwareConfig(unsigned int max_stripe_height_ = 0, unsigned int max_tile_width_ = 0, unsigned int flags_ = 0,
			   std::string defaults_file_ = {})
			: max_stripe_height(max_stripe_height_), max_tile_width(max_tile_width_), flags(flags_),
			  defaults_file(defaults_file_)
		{
		}

		unsigned int max_stripe_height; // Use zero to get "default behaviour"
		unsigned int max_tile_width; // Can only go larger than h/w defined limit in simulations
		unsigned int flags; // An "or" of the Flags above
		std::string defaults_file; // json file for default IQ settings
	};

	struct SmartResize
	{
		uint16_t width;
		uint16_t height;
	};

	BackEnd(SoftwareConfig const &non_reg_config, PiSPVariant const &variant, std::span<uint8_t> config_storage = {});
	~BackEnd();

	void set_blc(BlaConfig const &blc);
	BlaConfig get_blc() const;
	void set_cac(BeCacConfig const &cac);
	BeCacConfig get_cac() const;
	void set_ccm(BeCcmConfig const &ccm);
	BeCcmConfig get_ccm() const;
	void set_cdn(BeCdnConfig const &cdn);
	BeCdnConfig get_cdn() const;
	void set_crop(BeCropConfig const &crop);
	BeCropConfig get_crop() const;
	void set_crop(unsigned int i, BeCropConfig const &crop);
	BeCropConfig get_crop(unsigned int i) const;
	void set_debin(BeDebinConfig const &debin);
	BeDebinConfig get_debin() const;
	void set_decompress(DecompressConfig const &decompress);
	DecompressConfig get_decompress() const;
	void set_dedistort(const BeDedistortionConfig &dedistort);
	BeDedistortionConfig get_dedistort() const;
	void set_demosaic(BeDemosaicConfig const &demosaic);
	BeDemosaicConfig get_demosaic() const;
	void set_downscale(unsigned int i, BeDownscaleConfig const &downscale);
	BeDownscaleConfig get_downscale(unsigned int i) const;
	void set_dpc(BeDpcConfig const &dpc);
	BeDpcConfig get_dpc() const;
	void set_false_colour(BeFalseColourConfig const &false_colour);
	BeFalseColourConfig get_false_colour() const;
	void set_gamma(BeGammaConfig const &gamma);
	void set_gamma(unsigned int i, BeGammaConfig const &gamma);
	BeGammaConfig get_gamma(unsigned int i = 0) const;
	void set_geq(BeGeqConfig const &geq);
	BeGeqConfig get_geq() const;
	void set_global(BeGlobal const &global);
	BeGlobal get_global() const;
	void set_input_format(ImageFormatConfig const &input_format);
	ImageFormatConfig get_input_format() const;
	void set_lsc(BeLscConfig const &lsc);
	BeLscConfig get_lsc() const;
	void set_output_compress(unsigned int i, const BeCompressConfig &compress);
	BeCompressConfig get_output_compress(unsigned int i) const;
	void set_output_format(unsigned int i, BeOutputFormatConfig const &output_format);
	BeOutputFormatConfig get_output_format(unsigned int i) const;
	void set_post_csc(unsigned int i, BeCcmConfig const &csc);
	BeCcmConfig get_post_csc(unsigned int i) const;
	void set_pre_csc(unsigned int i, BeCcmConfig const &csc);
	BeCcmConfig get_pre_csc(unsigned int i) const;
	void set_pref_colours(BePrefColoursConfig const &pref_colours);
	BePrefColoursConfig get_pref_colours() const;
	void set_resample(unsigned int i, BeResampleConfig const &resample);
	BeResampleConfig get_resample(unsigned int i) const;
	void set_sat_control(BeSatControlConfig const &sat_control);
	BeSatControlConfig get_sat_control() const;
	void set_sdn(BeSdnConfig const &sdn);
	BeSdnConfig get_sdn() const;
	void set_sh_fc_combine(BeShFcCombineConfig const &sh_fc_combine);
	BeShFcCombineConfig get_sh_fc_combine() const;
	void set_sharpen(BeSharpenConfig const &sharpen);
	BeSharpenConfig get_sharpen() const;
	void set_stitch(BeStitchConfig const &stitch);
	BeStitchConfig get_stitch() const;
	void set_stitch_compress(CompressConfig const &stitch_compress);
	CompressConfig get_stitch_compress() const;
	void set_stitch_decompress(DecompressConfig const &stitch_decompress);
	DecompressConfig get_stitch_decompress() const;
	void set_stitch_input_format(ImageFormatConfig const &stitch_input_format);
	ImageFormatConfig get_stitch_input_format() const;
	void set_stitch_output_format(ImageFormatConfig const &stitch_output_format);
	ImageFormatConfig get_stitch_output_format() const;
	void set_tdn(BeTdnConfig const &tdn);
	BeTdnConfig get_tdn() const;
	void set_tdn_compress(CompressConfig const &tdn_compress);
	CompressConfig get_tdn_compress() const;
	void set_tdn_decompress(DecompressConfig const &tdn_decompress);
	DecompressConfig get_tdn_decompress() const;
	void set_tdn_input_format(ImageFormatConfig const &tdn_input_format);
	ImageFormatConfig get_tdn_input_format() const;
	void set_tdn_output_format(ImageFormatConfig const &tdn_output_format);
	ImageFormatConfig get_tdn_output_format() const;
	void set_tonemap(BeTonemapConfig const &tonemap);
	BeTonemapConfig get_tonemap() const;
	void set_tonemap2(BeTonemap2Config const &tonemap);
	BeTonemap2Config get_tonemap2() const;
	void set_tonemap_format(ImageFormatConfig const &format);
	ImageFormatConfig get_tonemap_format() const;
	void set_wbg(WbgConfig const &wbg);
	WbgConfig get_wbg() const;
	void set_ycbcr(BeCcmConfig const &ycbcr);
	BeCcmConfig get_ycbcr() const;
	void set_ycbcr_inverse(BeCcmConfig const &ycbcr_inverse);
	BeCcmConfig get_ycbcr_inverse() const;

	void initialise_ycbcr(BeCcmConfig &ycbcr, const std::string &colour_space);
	void initialise_ycbcr_inverse(BeCcmConfig &ycbcr_inverse, const std::string &colour_space);
	// TODO Add H/V variants?
	void initialise_resample(BeResampleConfig &resample, const std::string &filter);
	void initialise_resample(BeResampleConfig &resample, double downscale);
	void initialise_resample_h(BeResampleConfig &resample, const std::string &filter);
	void initialise_resample_v(BeResampleConfig &resample, const std::string &filter);
	void initialise_resample_h(BeResampleConfig &resample, double downscale);
	void initialise_resample_v(BeResampleConfig &resample, double downscale);	
	void initialise_sharpen(BeSharpenConfig &sharpen, BeShFcCombineConfig &shfc);

	void prepare(pisp_be_tiles_config *config);

	bool compute_output_image_format(unsigned int i, ImageFormatConfig &output_format,
								  ImageFormatConfig const &input_format) const;

	void set_smart_resize(unsigned int i, SmartResize const &smart_resize);
	SmartResize get_smart_resize(unsigned int i) const;

	unsigned int get_max_downscale() const;

	void inject_config(const void *config_storage);

	std::string get_json_config(pisp_be_tiles_config *config);
	void set_json_config(const std::string &json_config);

	void set_max_tile_width(unsigned int width)
	{
		sw_.max_tile_width = std::min(width, variant_.back_end_max_tile_width(0));
	}

	void set_max_stripe_height(unsigned int height)
	{
		sw_.max_stripe_height = height;
	}

	const PiSPVariant &get_variant() const
	{
		return variant_;
	}

	static size_t get_storage_size([[maybe_unused]] const PiSPVariant &variant)
	{
		return sizeof(PipelineStorage);
	}

	const void *get_config_storage()
	{
		return storage_;
	}

	void lock()
	{
		pthread_mutex_lock(&storage_->mutex);
	}

	void unlock()
	{
		pthread_mutex_unlock(&storage_->mutex);
	}

	bool try_lock()
	{
		return pthread_mutex_trylock(&storage_->mutex) == 0;
	}

private:
	// All the pipeline state. It may live in storage shared between processes, and inject_config() copies it
	// between them, so it must be safe to copy as raw bytes.
	struct PipelineConfig
	{
		pisp_be_config hw_config {};
		pisp_be_config2 hw_config_v2 {};
		BeSwParams sw_params {};

		BayerEnables dirty_flags_bayer;
		RgbEnables dirty_flags_rgb;
		DirtyFlags dirty_flags_extra;

		bool retile = true;
		bool finalise_tiling = true;

		std::array<SmartResize, PISP_BACK_END_NUM_OUTPUTS> smart_resize {};
		uint32_t smart_resize_dirty = 0;

		std::array<pisp_tile, PISP_BACK_END_NUM_TILES> tiles {};
		std::array<pisp_tile2, PISP_BACK_END_NUM_TILES> tiles2 {};
		unsigned int num_tiles_x = 0;
		unsigned int num_tiles_y = 0;
	};

	static_assert(std::is_standard_layout_v<PipelineConfig> && std::is_trivially_copyable_v<PipelineConfig>,
				  "PipelineConfig must be safe to copy as raw bytes");

	// The layout of the config storage: the pipeline state and the process-shared mutex guarding it. The mutex
	// must never be copied, so it is kept outside of PipelineConfig and copying the storage as a whole is
	// disallowed.
	//
	// External storage may be shared by several BackEnds, e.g. in different processes. The first BackEnd
	// constructed on it initialises it and sets init_, and must finish doing so before any others are
	// constructed. Those then attach to the existing state, and must be destroyed before the first one.
	struct PipelineStorage
	{
		PipelineStorage() = default;
		PipelineStorage(const PipelineStorage &) = delete;
		PipelineStorage &operator=(const PipelineStorage &) = delete;

		PipelineConfig config;
		pthread_mutex_t mutex;
		bool init_ = false;
	};

	static_assert(std::is_standard_layout_v<PipelineStorage>, "PipelineStorage must be a standard layout type");

	void finalise_bayer_rgb_inputs() const;
	void finalise_inputs() const;
	void finalise_lsc();
	void finalise_cac();
	void finalise_dedistort();
	void finalise_tonemap2();
	void finalise_resample(unsigned int index, uint16_t width, uint16_t height);
	void finalise_downscale(unsigned int index, uint16_t width, uint16_t height);
	void finalise_decompression() const;
	void finalise_tdn();
	void finalise_stitch();
	void finalise_output(unsigned int index);
	void check_tiles(const TilingConfig &tiling_config);
	tiling::Length2 calculate_input_alignment() const;
	void finalise_config();
	void update_smart_resize();
	void update_tiles();
	void retile_pipeline(TilingConfig const &tiling_config);
	void finalise_tiling();
	void get_output_size(int output_num, uint16_t *width, uint16_t *height, ImageFormatConfig const &ifmt) const;
	uint16_t post_transpose_width();
	uint16_t post_transpose_height();

	void initialise_default_config(const std::string &filename, bool set_defaults);
	const std::string &select_resample_filter(double downscale) const;

	SoftwareConfig sw_;
	PipelineStorage *storage_;
	PipelineConfig *pipe_; // &storage_->config
	bool owns_storage_; // This BackEnd initialised storage_, so is responsible for tearing it down.
	const PiSPVariant variant_;

	// Internal storage use
	PipelineStorage internal_storage_;

	std::unique_ptr<BackendConfigAdapter> config_;

	// Default config
	std::map<std::string, BeCcmConfig> ycbcr_map_;
	std::map<std::string, BeCcmConfig> inverse_ycbcr_map_;
	std::map<std::string, std::vector<int16_t>> resample_filter_map_;
	std::vector<std::pair<double, std::string>> resample_select_list_;
	BeSharpenConfig default_sharpen_;
	BeShFcCombineConfig default_shfc_;
};

} // namespace libpisp
