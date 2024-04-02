
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021 - 2023, Raspberry Pi Ltd
 *
 * backend.cpp - PiSP Back End implementation
 */
#include "backend.hpp"

#include <cstdlib>
#include <cstring>

#include "common/logging.hpp"
#include "common/pisp_common.h"

using namespace libpisp;

BackEnd::BackEnd(SoftwareConfig const &swConfig, PiSPVariant const &variant, std::span<uint8_t> configStorage)
	: sw_(swConfig), variant_(variant)
{
	unsigned int maxTileWidth = variant_.backEndMaxTileWidth(0);

	if (sw_.maxTileWidth > maxTileWidth)
		PISP_LOG(fatal, "Configured max tile width " << sw_.maxTileWidth << " exceeds " << maxTileWidth);

	if (configStorage.data() && configStorage.size() >= sizeof(PipelineConfig))
		pipe_ = reinterpret_cast<PipelineConfig *>(configStorage.data());
	else
		pipe_ = &internalPipelineConfig_;
	
	config_ = std::make_unique<BackendConfigAdapter>(&pipe_->hwConfig, pipe_->tiles.data(), &pipe_->configExtra);

	const char *env = std::getenv("LIBPISP_BE_CONFIG_FILE");
	initialiseDefaultConfig(env ? std::string(env) : swConfig.defaultsFile);
}

BackEnd::~BackEnd()
{
}

void BackEnd::setGlobal(BeGlobal const &global)
{
	RgbEnables currentRgbEnables = config_->getGlobal().rgbEnables;
	BayerEnables currentBayerEnables = config_->getGlobal().bayerEnables;

	RgbEnables changedRgbEnables(global.rgbEnables ^ currentRgbEnables);

	if (changedRgbEnables & (RgbEnable::Downscale0 | RgbEnable::Downscale1 |
							 RgbEnable::Resample0 | RgbEnable::Resample1))
		pipe_->retile = true; // must retile when rescale change

	pipe_->dirtyFlagsBayer |= (global.bayerEnables & ~currentBayerEnables); // label anything newly enabled as dirty
	pipe_->dirtyFlagsRgb |= (global.rgbEnables & ~currentRgbEnables); // label anything newly enabled as dirty
	pipe_->dirtyFlagsExtra |= DirtyFlag::Global;
	config_->setGlobal(global);
}

BeGlobal BackEnd::getGlobal() const
{
	return config_->getGlobal();
}

void BackEnd::setInputFormat(ImageFormatConfig const &inputFormat)
{
	config_->setInputFormat(inputFormat);
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(inputFormat.format))
		pipe_->dirtyFlagsBayer |= BayerEnable::Input;
	else
		pipe_->dirtyFlagsRgb |= RgbEnable::Input;
	pipe_->retile = true;
}

ImageFormatConfig BackEnd::getInputFormat() const
{
	return config_->getInputFormat();
}

void BackEnd::setDecompress(DecompressConfig const &decompress)
{
	config_->setDecompress(decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::Decompress;
}

DecompressConfig BackEnd::getDecompress() const
{
	return config_->getDecompress();
}

void BackEnd::setDpc(BeDpcConfig const &dpc)
{
	config_->setDpc(dpc);
	pipe_->dirtyFlagsBayer |= BayerEnable::Dpc;
}

BeDpcConfig BackEnd::getDpc() const
{
	return config_->getDpc();
}

void BackEnd::setGeq(BeGeqConfig const &geq)
{
	BeGeqConfig geq_copy = geq;
	geq_copy.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	config_->setGeq(geq_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Geq;
}

BeGeqConfig BackEnd::getGeq() const
{
	return config_->getGeq();
}

void BackEnd::setTdnInputFormat(ImageFormatConfig const &tdnInputFormat)
{
	config_->setTdnInputFormat(tdnInputFormat);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnInput; // TDN input address will always be written
	pipe_->finaliseTiling = true;
}

ImageFormatConfig BackEnd::getTdnInputFormat() const
{
	return config_->getTdnInputFormat();
}

void BackEnd::setTdnDecompress(DecompressConfig const &tdn_decompress)
{
	config_->setTdnDecompress(tdn_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnDecompress;
}

DecompressConfig BackEnd::getTdnDecompress() const
{
	return config_->getTdnDecompress();
}

void BackEnd::setTdn(BeTdnConfig const &tdn)
{
	config_->setTdn(tdn);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tdn;
}

BeTdnConfig BackEnd::getTdn() const
{
	return config_->getTdn();
}

void BackEnd::setTdnCompress(CompressConfig const &tdn_compress)
{
	config_->setTdnCompress(tdn_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnCompress;
}

CompressConfig BackEnd::getTdnCompress() const
{
	return config_->getTdnCompress();
}

void BackEnd::setTdnOutputFormat(ImageFormatConfig const &tdn_outputFormat)
{
	config_->setTdnOutputFormat(tdn_outputFormat);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnOutput; // TDN output address will always be written
	pipe_->finaliseTiling = true;
}

ImageFormatConfig BackEnd::getTdnOutputFormat() const
{
	return config_->getTdnOutputFormat();
}

void BackEnd::setSdn(BeSdnConfig const &sdn)
{
	config_->setSdn(sdn);
	pipe_->dirtyFlagsBayer |= BayerEnable::Sdn;
}

BeSdnConfig BackEnd::getSdn() const
{
	return config_->getSdn();
}

void BackEnd::setBlc(BlaConfig const &blc)
{
	config_->setBlc(blc);
	pipe_->dirtyFlagsBayer |= BayerEnable::Blc;
}

BlaConfig BackEnd::getBlc() const
{
	return config_->getBlc();
}

void BackEnd::setStitchInputFormat(ImageFormatConfig const &stitch_input_format)
{
	config_->setStitchInputFormat(stitch_input_format);
	// Note: stitch.pad clearing would need variant-specific handling
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchInput;
	pipe_->finaliseTiling = true;
}

ImageFormatConfig BackEnd::getStitchInputFormat() const
{
	return config_->getStitchInputFormat();
}

void BackEnd::setStitchDecompress(DecompressConfig const &stitch_decompress)
{
	config_->setStitchDecompress(stitch_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchDecompress;
}

DecompressConfig BackEnd::getStitchDecompress() const
{
	return config_->getStitchDecompress();
}

void BackEnd::setStitch(BeStitchConfig const &stitch)
{
	config_->setStitch(stitch);
	pipe_->dirtyFlagsBayer |= BayerEnable::Stitch;
}

BeStitchConfig BackEnd::getStitch() const
{
	return config_->getStitch();
}

void BackEnd::setStitchCompress(CompressConfig const &stitch_compress)
{
	config_->setStitchCompress(stitch_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchCompress;
}

CompressConfig BackEnd::getStitchCompress() const
{
	return config_->getStitchCompress();
}

void BackEnd::setStitchOutputFormat(ImageFormatConfig const &stitch_outputFormat)
{
	config_->setStitchOutputFormat(stitch_outputFormat);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchOutput;
	pipe_->finaliseTiling = true;
}

ImageFormatConfig BackEnd::getStitchOutputFormat() const
{
	return config_->getStitchOutputFormat();
}

void BackEnd::setCdn(BeCdnConfig const &cdn)
{
	config_->setCdn(cdn);
	pipe_->dirtyFlagsBayer |= BayerEnable::Cdn;
}

BeCdnConfig BackEnd::getCdn() const
{
	return config_->getCdn();
}

void BackEnd::setWbg(WbgConfig const &wbg)
{
	config_->setWbg(wbg);
	pipe_->dirtyFlagsBayer |= BayerEnable::Wbg;
}

WbgConfig BackEnd::getWbg() const
{
	return config_->getWbg();
}

void BackEnd::setLsc(BeLscConfig const &lsc)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finaliseTiling |= config_->getLsc().grid_step_x != lsc.grid_step_x ||
							config_->getLsc().grid_step_y != lsc.grid_step_y;
	config_->setLsc(lsc);
	pipe_->dirtyFlagsBayer |= BayerEnable::Lsc;
}

BeLscConfig BackEnd::getLsc() const
{
	return config_->getLsc();
}

void BackEnd::setCac(BeCacConfig const &cac)
{
	pipe_->finaliseTiling |= config_->getCac().grid_step_x != cac.grid_step_x ||
							config_->getCac().grid_step_y != cac.grid_step_y;
	config_->setCac(cac);
	pipe_->dirtyFlagsBayer |= BayerEnable::Cac;
}

BeCacConfig BackEnd::getCac() const
{
	return config_->getCac();
}

void BackEnd::setDebin(BeDebinConfig const &debin)
{
	config_->setDebin(debin);
	pipe_->dirtyFlagsBayer |= BayerEnable::Debin;
}

BeDebinConfig BackEnd::getDebin() const
{
	return config_->getDebin();
}

void BackEnd::setTonemap(BeTonemapConfig const &tonemap)
{
	config_->setTonemap(tonemap);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap;
}

BeTonemapConfig BackEnd::getTonemap() const
{
	return config_->getTonemap();
}

void BackEnd::setTonemap2(BeTonemap2Config const &tonemap)
{
	config_->setTonemap2(tonemap);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap2;
}

BeTonemap2Config BackEnd::getTonemap2() const
{
	return config_->getTonemap2();
}

void BackEnd::setTonemap2Format(ImageFormatConfig const &format)
{
	config_->setTonemap2Format(format);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap2Input;
}

ImageFormatConfig BackEnd::getTonemap2Format() const
{
	return config_->getTonemap2Format();
}

void BackEnd::setDemosaic(BeDemosaicConfig const &demosaic)
{
	config_->setDemosaic(demosaic);
	pipe_->dirtyFlagsBayer |= BayerEnable::Demosaic;
}

BeDemosaicConfig BackEnd::getDemosaic() const
{
	return config_->getDemosaic();
}

void BackEnd::setCcm(BeCcmConfig const &ccm)
{
	config_->setCcm(ccm);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ccm;
}

BeCcmConfig BackEnd::getCcm() const
{
	return config_->getCcm();
}

void BackEnd::setSatControl(BeSatControlConfig const &sat_control)
{
	config_->setSatControl(sat_control);
	pipe_->dirtyFlagsRgb |= RgbEnable::SatControl;
}

BeSatControlConfig BackEnd::getSatControl() const
{
	return config_->getSatControl();
}

void BackEnd::setYcbcr(BeCcmConfig const &ycbcr)
{
	config_->setYcbcr(ycbcr);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ycbcr;
}

BeCcmConfig BackEnd::getYcbcr() const
{
	return config_->getYcbcr();
}

void BackEnd::setFalseColour(BeFalseColourConfig const &false_colour)
{
	config_->setFalseColour(false_colour);
	pipe_->dirtyFlagsRgb |= RgbEnable::FalseColour;
}

BeFalseColourConfig BackEnd::getFalseColour() const
{
	return config_->getFalseColour();
}

void BackEnd::setSharpen(BeSharpenConfig const &sharpen)
{
	config_->setSharpen(sharpen);
	pipe_->dirtyFlagsRgb |= RgbEnable::Sharpen;
}

BeSharpenConfig BackEnd::getSharpen() const
{
	return config_->getSharpen();
}

void BackEnd::setShFcCombine(BeShFcCombineConfig const &sh_fc_combine)
{
	config_->setShFcCombine(sh_fc_combine);
	pipe_->dirtyFlagsExtra |= DirtyFlag::ShFcCombine;
}

BeShFcCombineConfig BackEnd::getShFcCombine() const
{
	return config_->getShFcCombine();
}

void BackEnd::setYcbcrInverse(BeCcmConfig const &ycbcr_inverse)
{
	config_->setYcbcrInverse(ycbcr_inverse);
	pipe_->dirtyFlagsRgb |= RgbEnable::YcbcrInverse;
}

BeCcmConfig BackEnd::getYcbcrInverse() const
{
	return config_->getYcbcrInverse();
}

void BackEnd::setGamma(BeGammaConfig const &gamma)
{
	config_->setGamma(gamma);
	pipe_->dirtyFlagsRgb |= RgbEnable::Gamma0;
}

BeGammaConfig BackEnd::getGamma() const
{
	return config_->getGamma();
}

void BackEnd::setCrop(BeCropConfig const &crop)
{
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

BeCropConfig BackEnd::getCrop() const
{
	return pipe_->configExtra.crop[0];
}

void BackEnd::setCrop(unsigned int i, BeCropConfig const &crop)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

BeCropConfig BackEnd::getCrop(unsigned int i) const
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	return pipe_->configExtra.crop[i];
}

void BackEnd::setCsc(unsigned int i, BeCcmConfig const &csc)
{
	config_->setCsc(i, csc);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::PostCsc0, i);
}

BeCcmConfig BackEnd::getCsc(unsigned int i) const
{
	return config_->getCsc(i);
}

void BackEnd::setDownscale(unsigned int i, BeDownscaleConfig const &downscale)
{
	BeDownscaleConfig config = downscale;
	config_->setDownscale(i, config);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

BeDownscaleConfig BackEnd::getDownscale(unsigned int i) const
{
	return config_->getDownscale(i);
}

void BackEnd::setResample(unsigned int i, BeResampleConfig const &resample)
{
	BeResampleConfig config = resample;
	config_->setResample(i, config);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

BeResampleConfig BackEnd::getResample(unsigned int i) const
{
	return config_->getResample(i);
}

void BackEnd::setOutputFormat(unsigned int i, BeOutputFormatConfig const &outputFormat)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	config_->setOutputFormat(i, outputFormat);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile = true;
}

BeOutputFormatConfig BackEnd::getOutputFormat(unsigned int i) const
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	return config_->getOutputFormat(i);
}

void BackEnd::setSmartResize(unsigned int i, BackEnd::SmartResize const &smartResize)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	// Non-zero width and height will be interpreted as "enabled".
	pipe_->smartResize[i] = smartResize;
	pipe_->smartResizeDirty |= (1 << i);
}

void BackEnd::setDedistort(const BeDedistortionConfig &dedistort)
{
	config_->setDedistort(dedistort);
	pipe_->dirtyFlagsRgb |= RgbEnable::Dedistort;
	pipe_->retile = true;
}

void BackEnd::setOutputCompress(unsigned int i, const BeCompressConfig &compress)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	config_->setOutputCompress(i, compress);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Compress0, i);
	// No need to retile, since outputs must in all cases meet min alignment of 16.
}

BeCompressConfig BackEnd::getOutputCompress(unsigned int i) const
{
	return config_->getOutputCompress(i);
}

unsigned int BackEnd::getMaxDownscale() const
{
	// Return an estimate of the largest horizontal downscale we can do.
	unsigned int maxTileWidth = sw_.maxTileWidth;
	if (!maxTileWidth)
		maxTileWidth = variant_.backEndMaxTileWidth(0);

	// We reckon a 640 tile-width implementation can do 24x safely with formats that
	// want 1 byte per pixel. This should approximately scale with the max tile width.
	unsigned int downscale = 24;
	const unsigned int refTileWidth = 640;
	return downscale * maxTileWidth / refTileWidth;
}
