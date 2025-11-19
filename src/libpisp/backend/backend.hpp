
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
#include <variant>
#include <vector>

#include "common/shm_mutex.hpp"
#include "tiling/pisp_tiling.hpp"
#include "variants/variant.hpp"

#include "pisp_be_config.h"

#include "backend_config_adapter.hpp"

// Definition of the PiSP Back End class.

namespace libpisp
{

class BackEnd final
{
public:
	struct SoftwareConfig
	{
		enum Flags
		{
			NONE = 0,
			LOW_LATENCY = 1,	// Attempt to process image with lowest possible latency (no longer implemented)
			HIGH_PRIORITY = 2 	// Not currently implemented
		};

		SoftwareConfig(unsigned int maxStripeHeight = 0, unsigned int maxTileWidth = 0, unsigned int flags = 0,
			   std::string defaultsFile = {})
			: maxStripeHeight(maxStripeHeight), maxTileWidth(maxTileWidth), flags(flags),
			  defaultsFile(defaultsFile)
		{
		}

		unsigned int maxStripeHeight; // Use zero to get "default behaviour"
		unsigned int maxTileWidth; // Can only go larger than h/w defined limit in simulations
		unsigned int flags; // An "or" of the Flags above
		std::string defaultsFile; // json file for default IQ settings
	};

	struct SmartResize
	{
		uint16_t width;
		uint16_t height;
	};

	BackEnd(SoftwareConfig const &swConfig, PiSPVariant const &variant, std::span<uint8_t> configStorage = {});
	~BackEnd();

	void setGlobal(BackendConfigAdapter::Global const &global);
	void getGlobal(BackendConfigAdapter::Global &global) const;
	void setInputFormat(pisp_image_format_config const &inputFormat);
	void getInputFormat(pisp_image_format_config &inputFormat) const;
	void setDecompress(pisp_decompress_config const &decompress);
	void getDecompress(pisp_decompress_config &decompress) const;
	void setDpc(pisp_be_dpc_config const &dpc);
	void getDpc(pisp_be_dpc_config &dpc) const;
	void setGeq(pisp_be_geq_config const &geq);
	void getGeq(pisp_be_geq_config &geq) const;
	void setTdnInputFormat(pisp_image_format_config const &tdnInputFormat);
	void getTdnInputFormat(pisp_image_format_config &tdnInputFormat) const;
	void setTdnDecompress(pisp_decompress_config const &tdnDecompress);
	void getTdnDecompress(pisp_decompress_config &tdnDecompress) const;
	void setTdn(pisp_be_tdn_config const &tdn);
	void getTdn(pisp_be_tdn_config &tdn) const;
	void setTdnCompress(pisp_compress_config const &tdnCompress);
	void getTdnCompress(pisp_compress_config &tdnCompress) const;
	void setTdnOutputFormat(pisp_image_format_config const &tdnOutputFormat);
	void getTdnOutputFormat(pisp_image_format_config &tdnOutputFormat) const;
	void setSdn(pisp_be_sdn_config const &sdn);
	void getSdn(pisp_be_sdn_config &sdn) const;
	void setBlc(pisp_bla_config const &blc);
	void getBlc(pisp_bla_config &blc) const;
	void setStitchInputFormat(pisp_image_format_config const &stitchInputFormat);
	void getStitchInputFormat(pisp_image_format_config &stitchInputFormat) const;
	void setStitchDecompress(pisp_decompress_config const &stitchDecompress);
	void getStitchDecompress(pisp_decompress_config &stitchDecompress) const;
	void setStitch(pisp_be_stitch_config const &stitch);
	void getStitch(pisp_be_stitch_config &stitch) const;
	void setStitchCompress(pisp_compress_config const &stitchCompress);
	void getStitchCompress(pisp_compress_config &stitchCompress) const;
	void setStitchOutputFormat(pisp_image_format_config const &stitchOutputFormat);
	void getStitchOutputFormat(pisp_image_format_config &stitchOutputFormat) const;
	void setWbg(pisp_wbg_config const &wbg);
	void getWbg(pisp_wbg_config &wbg) const;
	void setCdn(pisp_be_cdn_config const &cdn);
	void getCdn(pisp_be_cdn_config &cdn) const;
	void setLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lscExtra = { 0, 0 });
	void getLsc(pisp_be_lsc_config &lsc, pisp_be_lsc_extra &lscExtra) const;
	void setCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cacExtra = { 0, 0 });
	void getCac(pisp_be_cac_config &cac, pisp_be_cac_extra &cacExtra) const;
	void setDebin(pisp_be_debin_config const &debin);
	void getDebin(pisp_be_debin_config &debin) const;
	void setTonemap(pisp_be_tonemap_config const &tonemap);
	void getTonemap(pisp_be_tonemap_config &tonemap) const;
	void setDemosaic(pisp_be_demosaic_config const &demosaic);
	void getDemosaic(pisp_be_demosaic_config &demosaic) const;
	void setCcm(pisp_be_ccm_config const &ccm);
	void getCcm(pisp_be_ccm_config &ccm) const;
	void setSatControl(pisp_be_sat_control_config const &satControl);
	void getSatControl(pisp_be_sat_control_config &satControl) const;
	void setYcbcr(pisp_be_ccm_config const &ycbcr);
	void getYcbcr(pisp_be_ccm_config &ccm) const;
	void setFalseColour(pisp_be_false_colour_config const &falseColour);
	void getFalseColour(pisp_be_false_colour_config &falseColour) const;
	void setSharpen(pisp_be_sharpen_config const &sharpen);
	void getSharpen(pisp_be_sharpen_config &sharpen) const;
	void setShFcCombine(pisp_be_sh_fc_combine_config const &shFcCombine);
	void getShFcCombine(pisp_be_sh_fc_combine_config &shFcCombine) const;
	void setYcbcrInverse(pisp_be_ccm_config const &ycbcrInverse);
	void getYcbcrInverse(pisp_be_ccm_config &ycbcrInverse) const;
	void setGamma(pisp_be_gamma_config const &gamma);
	void getGamma(pisp_be_gamma_config &gamma) const;
	void setCrop(pisp_be_crop_config const &crop);
	void getCrop(pisp_be_crop_config &crop) const;
	void setCrop(unsigned int i, pisp_be_crop_config const &crop);
	void getCrop(unsigned int i, pisp_be_crop_config &crop) const;
	void setCsc(unsigned int i, pisp_be_ccm_config const &csc);
	void getCsc(unsigned int i, pisp_be_ccm_config &csc) const;
	void setOutputFormat(unsigned int i, pisp_be_output_format_config const &outputFormat);
	void getOutputFormat(unsigned int i, pisp_be_output_format_config &outputFormat) const;
	void setResample(unsigned int i, pisp_be_resample_config const &resample,
					 pisp_be_resample_extra const &resampleExtra);
	void getResample(unsigned int i, pisp_be_resample_config &resample,
					 pisp_be_resample_extra &resampleExtra) const;
	void setResample(unsigned int i, pisp_be_resample_extra const &resampleExtra);
	void getResample(unsigned int i, pisp_be_resample_extra &resampleExtra) const;
	void setDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
					  pisp_be_downscale_extra const &downscaleExtra);
	void getDownscale(unsigned int i, pisp_be_downscale_config &downscale,
					  pisp_be_downscale_extra &downscaleExtra) const;
	void setDownscale(unsigned int i, pisp_be_downscale_extra const &downscaleExtra);
	void getDownscale(unsigned int i, pisp_be_downscale_extra &downscaleExtra) const;

	void initialiseYcbcr(pisp_be_ccm_config &ycbcr, const std::string &colourSpace);
	void initialiseYcbcrInverse(pisp_be_ccm_config &ycbcrInverse, const std::string &colourSpace);
	void initialiseResample(pisp_be_resample_config &resample, const std::string &filter);
	void initialiseResample(pisp_be_resample_config &resample, double downscale);
	void initialiseSharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc);

	void prepare(pisp_be_tiles_config *config);

	bool computeOutputImageFormat(unsigned int i, pisp_image_format_config &outputFormat,
								  pisp_image_format_config const &inputFormat) const;

	void setSmartResize(unsigned int i, SmartResize const &smartResize);

	unsigned int getMaxDownscale() const;

	std::string getJsonConfig(pisp_be_tiles_config *config);
	void setJsonConfig(const std::string &jsonConfig);

	void setMaxTileWidth(unsigned int width)
	{
		sw_.maxTileWidth = width;
	}

	void setMaxStripeHeight(unsigned int height)
	{
		sw_.maxStripeHeight = height;
	}

	const PiSPVariant &getVariant() const
	{
		return variant_;
	}

	static size_t getStorageSize([[maybe_unused]] const PiSPVariant &variant)
	{
		return sizeof(PipelineConfig);
	}

private:
	struct BeConfigExtra
	{
		// Non-register fields:
		pisp_be_lsc_extra lsc {};
		pisp_be_cac_extra cac {};
		pisp_be_downscale_extra downscale[PISP_BACK_END_NUM_OUTPUTS] {};
		pisp_be_resample_extra resample[PISP_BACK_END_NUM_OUTPUTS] {};
		pisp_be_crop_config crop[PISP_BACK_END_NUM_OUTPUTS] {};
	};

	struct PipelineConfig
	{
		pisp_be_config hwConfig {};
		BeConfigExtra configExtra {};

		BayerEnables dirtyFlagsBayer;
		RgbEnables dirtyFlagsRgb;
		DirtyFlags dirtyFlagsExtra;

		bool retile = true;
		bool finaliseTiling = true;

		std::array<SmartResize, PISP_BACK_END_NUM_OUTPUTS> smartResize {};
		uint32_t smartResizeDirty = 0;

		std::array<pisp_tile, PISP_BACK_END_NUM_TILES> tiles {};
		unsigned int numTilesX = 0;
		unsigned int numTilesY = 0;
	};

	static_assert(std::is_standard_layout<PipelineConfig>::value, "PipelineConfig must be a standard layout type");

	void finaliseBayerRgbInputs() const;
	void finaliseInputs() const;
	void finaliseLsc();
	void finaliseCac();
	void finaliseResample(unsigned int index, uint16_t width, uint16_t height);
	void finaliseDownscale(unsigned int index, uint16_t width, uint16_t height);
	void finaliseDecompression() const;
	void finaliseTdn();
	void finaliseStitch();
	void finaliseOutput(unsigned int index);
	void checkTiles(const TilingConfig &tilingConfig);
	tiling::Length2 calculateInputAlignment() const;
	void finaliseConfig();
	void updateSmartResize();
	void updateTiles();
	void retilePipeline(TilingConfig const &tilingConfig);
	void finaliseTiling();
	void getOutputSize(int outputNum, uint16_t *width, uint16_t *height, pisp_image_format_config const &ifmt) const;

	void initialiseDefaultConfig(const std::string &filename);

	SoftwareConfig sw_;
	PipelineConfig *pipe_;
	const PiSPVariant variant_;

	// Internal storage use
	PipelineConfig internalPipelineConfig_;

	std::unique_ptr<BackendConfigAdapter> config_;

	// Default config
	std::map<std::string, pisp_be_ccm_config> ycbcrMap_;
	std::map<std::string, pisp_be_ccm_config> inverseYcbcrMap_;
	std::map<std::string, pisp_be_resample_config> resampleFilterMap_;
	std::vector<std::pair<double, std::string>> resampleSelectList_;
	pisp_be_sharpen_config defaultSharpen_;
	pisp_be_sh_fc_combine_config defaultShfc_;
};

} // namespace libpisp
