
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

	void setGlobal(BeGlobal const &global);
	void getGlobal(BeGlobal &global) const;
	void setInputFormat(ImageFormatConfig const &inputFormat);
	void getInputFormat(ImageFormatConfig &inputFormat) const;
	void setDecompress(DecompressConfig const &decompress);
	void getDecompress(DecompressConfig &decompress) const;
	void setDpc(BeDpcConfig const &dpc);
	void getDpc(BeDpcConfig &dpc) const;
	void setGeq(BeGeqConfig const &geq);
	void getGeq(BeGeqConfig &geq) const;
	void setTdnInputFormat(ImageFormatConfig const &tdnInputFormat);
	void getTdnInputFormat(ImageFormatConfig &tdnInputFormat) const;
	void setTdnDecompress(DecompressConfig const &tdnDecompress);
	void getTdnDecompress(DecompressConfig &tdnDecompress) const;
	void setTdn(BeTdnConfig const &tdn);
	void getTdn(BeTdnConfig &tdn) const;
	void setTdnCompress(CompressConfig const &tdnCompress);
	void getTdnCompress(CompressConfig &tdnCompress) const;
	void setTdnOutputFormat(ImageFormatConfig const &tdnOutputFormat);
	void getTdnOutputFormat(ImageFormatConfig &tdnOutputFormat) const;
	void setSdn(BeSdnConfig const &sdn);
	void getSdn(BeSdnConfig &sdn) const;
	void setBlc(BlaConfig const &blc);
	void getBlc(BlaConfig &blc) const;
	void setStitchInputFormat(ImageFormatConfig const &stitchInputFormat);
	void getStitchInputFormat(ImageFormatConfig &stitchInputFormat) const;
	void setStitchDecompress(DecompressConfig const &stitchDecompress);
	void getStitchDecompress(DecompressConfig &stitchDecompress) const;
	void setStitch(BeStitchConfig const &stitch);
	void getStitch(BeStitchConfig &stitch) const;
	void setStitchCompress(CompressConfig const &stitchCompress);
	void getStitchCompress(CompressConfig &stitchCompress) const;
	void setStitchOutputFormat(ImageFormatConfig const &stitchOutputFormat);
	void getStitchOutputFormat(ImageFormatConfig &stitchOutputFormat) const;
	void setWbg(WbgConfig const &wbg);
	void getWbg(WbgConfig &wbg) const;
	void setCdn(BeCdnConfig const &cdn);
	void getCdn(BeCdnConfig &cdn) const;
	void setLsc(BeLscConfig const &lsc);
	void getLsc(BeLscConfig &lsc) const;
	void setCac(BeCacConfig const &cac);
	void getCac(BeCacConfig &cac) const;
	void setDebin(BeDebinConfig const &debin);
	void getDebin(BeDebinConfig &debin) const;
	void setTonemap(BeTonemapConfig const &tonemap);
	void getTonemap(BeTonemapConfig &tonemap) const;
	void setDemosaic(BeDemosaicConfig const &demosaic);
	void getDemosaic(BeDemosaicConfig &demosaic) const;
	void setCcm(BeCcmConfig const &ccm);
	void getCcm(BeCcmConfig &ccm) const;
	void setSatControl(BeSatControlConfig const &satControl);
	void getSatControl(BeSatControlConfig &satControl) const;
	void setYcbcr(BeCcmConfig const &ycbcr);
	void getYcbcr(BeCcmConfig &ccm) const;
	void setFalseColour(BeFalseColourConfig const &falseColour);
	void getFalseColour(BeFalseColourConfig &falseColour) const;
	void setSharpen(BeSharpenConfig const &sharpen);
	void getSharpen(BeSharpenConfig &sharpen) const;
	void setShFcCombine(BeShFcCombineConfig const &shFcCombine);
	void getShFcCombine(BeShFcCombineConfig &shFcCombine) const;
	void setYcbcrInverse(BeCcmConfig const &ycbcrInverse);
	void getYcbcrInverse(BeCcmConfig &ycbcrInverse) const;
	void setGamma(BeGammaConfig const &gamma);
	void getGamma(BeGammaConfig &gamma) const;
	void setCrop(BeCropConfig const &crop);
	void getCrop(BeCropConfig &crop) const;
	void setCrop(unsigned int i, BeCropConfig const &crop);
	void getCrop(unsigned int i, BeCropConfig &crop) const;
	void setCsc(unsigned int i, BeCcmConfig const &csc);
	void getCsc(unsigned int i, BeCcmConfig &csc) const;
	void setOutputFormat(unsigned int i, BeOutputFormatConfig const &outputFormat);
	void getOutputFormat(unsigned int i, BeOutputFormatConfig &outputFormat) const;
	void setResample(unsigned int i, BeResampleConfig const &resample);
	void getResample(unsigned int i, BeResampleConfig &resample) const;
	void setDownscale(unsigned int i, BeDownscaleConfig const &downscale);
	void getDownscale(unsigned int i, BeDownscaleConfig &downscale) const;

	void initialiseYcbcr(BeCcmConfig &ycbcr, const std::string &colourSpace);
	void initialiseYcbcrInverse(BeCcmConfig &ycbcrInverse, const std::string &colourSpace);
	void initialiseResample(BeResampleConfig &resample, const std::string &filter);
	void initialiseResample(BeResampleConfig &resample, double downscale);
	void initialiseSharpen(BeSharpenConfig &sharpen, BeShFcCombineConfig &shfc);

	void prepare(pisp_be_tiles_config *config);

	bool computeOutputImageFormat(unsigned int i, ImageFormatConfig &outputFormat,
								  ImageFormatConfig const &inputFormat) const;

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
	void getOutputSize(int outputNum, uint16_t *width, uint16_t *height, ImageFormatConfig const &ifmt) const;

	void initialiseDefaultConfig(const std::string &filename);

	SoftwareConfig sw_;
	PipelineConfig *pipe_;
	const PiSPVariant variant_;

	// Internal storage use
	PipelineConfig internalPipelineConfig_;

	std::unique_ptr<BackendConfigAdapter> config_;

	// Default config
	std::map<std::string, BeCcmConfig> ycbcrMap_;
	std::map<std::string, BeCcmConfig> inverseYcbcrMap_;
	std::map<std::string, BeResampleConfig> resampleFilterMap_;
	std::vector<std::pair<double, std::string>> resampleSelectList_;
	BeSharpenConfig defaultSharpen_;
	BeShFcCombineConfig defaultShfc_;
};

} // namespace libpisp
