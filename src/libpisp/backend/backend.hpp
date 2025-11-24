
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
	BeGlobal getGlobal() const;
	void setInputFormat(ImageFormatConfig const &inputFormat);
	ImageFormatConfig getInputFormat() const;
	void setDecompress(DecompressConfig const &decompress);
	DecompressConfig getDecompress() const;
	void setDpc(BeDpcConfig const &dpc);
	BeDpcConfig getDpc() const;
	void setGeq(BeGeqConfig const &geq);
	BeGeqConfig getGeq() const;
	void setTdnInputFormat(ImageFormatConfig const &tdnInputFormat);
	ImageFormatConfig getTdnInputFormat() const;
	void setTdnDecompress(DecompressConfig const &tdnDecompress);
	DecompressConfig getTdnDecompress() const;
	void setTdn(BeTdnConfig const &tdn);
	BeTdnConfig getTdn() const;
	void setTdnCompress(CompressConfig const &tdnCompress);
	CompressConfig getTdnCompress() const;
	void setTdnOutputFormat(ImageFormatConfig const &tdnOutputFormat);
	ImageFormatConfig getTdnOutputFormat() const;
	void setSdn(BeSdnConfig const &sdn);
	BeSdnConfig getSdn() const;
	void setBlc(BlaConfig const &blc);
	BlaConfig getBlc() const;
	void setStitchInputFormat(ImageFormatConfig const &stitchInputFormat);
	ImageFormatConfig getStitchInputFormat() const;
	void setStitchDecompress(DecompressConfig const &stitchDecompress);
	DecompressConfig getStitchDecompress() const;
	void setStitch(BeStitchConfig const &stitch);
	BeStitchConfig getStitch() const;
	void setStitchCompress(CompressConfig const &stitchCompress);
	CompressConfig getStitchCompress() const;
	void setStitchOutputFormat(ImageFormatConfig const &stitchOutputFormat);
	ImageFormatConfig getStitchOutputFormat() const;
	void setWbg(WbgConfig const &wbg);
	WbgConfig getWbg() const;
	void setCdn(BeCdnConfig const &cdn);
	BeCdnConfig getCdn() const;
	void setLsc(BeLscConfig const &lsc);
	BeLscConfig getLsc() const;
	void setCac(BeCacConfig const &cac);
	BeCacConfig getCac() const;
	void setDebin(BeDebinConfig const &debin);
	BeDebinConfig getDebin() const;
	void setTonemap(BeTonemapConfig const &tonemap);
	BeTonemapConfig getTonemap() const;
	void setDemosaic(BeDemosaicConfig const &demosaic);
	BeDemosaicConfig getDemosaic() const;
	void setCcm(BeCcmConfig const &ccm);
	BeCcmConfig getCcm() const;
	void setSatControl(BeSatControlConfig const &satControl);
	BeSatControlConfig getSatControl() const;
	void setYcbcr(BeCcmConfig const &ycbcr);
	BeCcmConfig getYcbcr() const;
	void setFalseColour(BeFalseColourConfig const &falseColour);
	BeFalseColourConfig getFalseColour() const;
	void setSharpen(BeSharpenConfig const &sharpen);
	BeSharpenConfig getSharpen() const;
	void setShFcCombine(BeShFcCombineConfig const &shFcCombine);
	BeShFcCombineConfig getShFcCombine() const;
	void setYcbcrInverse(BeCcmConfig const &ycbcrInverse);
	BeCcmConfig getYcbcrInverse() const;
	void setGamma(BeGammaConfig const &gamma);
	BeGammaConfig getGamma() const;
	void setCrop(BeCropConfig const &crop);
	BeCropConfig getCrop() const;
	void setCrop(unsigned int i, BeCropConfig const &crop);
	BeCropConfig getCrop(unsigned int i) const;
	void setCsc(unsigned int i, BeCcmConfig const &csc);
	BeCcmConfig getCsc(unsigned int i) const;
	void setOutputFormat(unsigned int i, BeOutputFormatConfig const &outputFormat);
	BeOutputFormatConfig getOutputFormat(unsigned int i) const;
	void setResample(unsigned int i, BeResampleConfig const &resample);
	BeResampleConfig getResample(unsigned int i) const;
	void setDownscale(unsigned int i, BeDownscaleConfig const &downscale);
	BeDownscaleConfig getDownscale(unsigned int i) const;
	void setDedistort(const BeDedistortionConfig &dedistort);
	BeDedistortionConfig getDedistort() const;

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
	uint16_t postTransposeWidth();
	uint16_t postTransposeHeight();

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
