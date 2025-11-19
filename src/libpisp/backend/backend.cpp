
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
	
	config_ = std::make_unique<BackendConfigAdapter>(&pipe_->hwConfig);

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

void BackEnd::getGlobal(BeGlobal &global) const
{
	global = config_->getGlobal();
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

void BackEnd::getInputFormat(ImageFormatConfig &inputFormat) const
{
	inputFormat = config_->getInputFormat();
}

void BackEnd::setDecompress(DecompressConfig const &decompress)
{
	config_->setDecompress(decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::Decompress;
}

void BackEnd::setDpc(BeDpcConfig const &dpc)
{
	BeDpcConfig dpc_copy = dpc;
	dpc_copy.pad = 0;
	config_->setDpc(dpc_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Dpc;
}

void BackEnd::setGeq(BeGeqConfig const &geq)
{
	BeGeqConfig geq_copy = geq;
	geq_copy.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	config_->setGeq(geq_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Geq;
}

void BackEnd::setTdnInputFormat(ImageFormatConfig const &tdnInputFormat)
{
	config_->setTdnInputFormat(tdnInputFormat);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnInput; // TDN input address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnInputFormat(ImageFormatConfig &tdnInputFormat) const
{
	tdnInputFormat = config_->getTdnInputFormat();
}

void BackEnd::setTdnDecompress(DecompressConfig const &tdn_decompress)
{
	config_->setTdnDecompress(tdn_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnDecompress;
}

void BackEnd::setTdn(BeTdnConfig const &tdn)
{
	BeTdnConfig tdn_copy = tdn;
	tdn_copy.pad = 0;
	config_->setTdn(tdn_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tdn;
}

void BackEnd::getTdn(BeTdnConfig &tdn) const
{
	tdn = config_->getTdn();
}

void BackEnd::setTdnCompress(CompressConfig const &tdn_compress)
{
	config_->setTdnCompress(tdn_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnCompress;
}

void BackEnd::setTdnOutputFormat(ImageFormatConfig const &tdn_output_format)
{
	config_->setTdnOutputFormat(tdn_output_format);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnOutput; // TDN output address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnOutputFormat(ImageFormatConfig &tdn_output_format) const
{
	tdn_output_format = config_->getTdnOutputFormat();
}

void BackEnd::setSdn(BeSdnConfig const &sdn)
{
	BeSdnConfig sdn_copy = sdn;
	sdn_copy.pad = 0;
	config_->setSdn(sdn_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Sdn;
}

void BackEnd::setBlc(BlaConfig const &blc)
{
	BlaConfig blc_copy = blc;
	blc_copy.pad[0] = blc_copy.pad[1] = 0;
	config_->setBlc(blc_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Blc;
}

void BackEnd::getBlc(BlaConfig &blc) const
{
	blc = config_->getBlc();
}

void BackEnd::setStitchInputFormat(ImageFormatConfig const &stitch_input_format)
{
	config_->setStitchInputFormat(stitch_input_format);
	// Note: stitch.pad clearing would need variant-specific handling
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchInput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchInputFormat(ImageFormatConfig &stitch_input_format) const
{
	stitch_input_format = config_->getStitchInputFormat();
}

void BackEnd::setStitchDecompress(DecompressConfig const &stitch_decompress)
{
	config_->setStitchDecompress(stitch_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchDecompress;
}

void BackEnd::setStitch(BeStitchConfig const &stitch)
{
	config_->setStitch(stitch);
	pipe_->dirtyFlagsBayer |= BayerEnable::Stitch;
}

void BackEnd::setStitchCompress(CompressConfig const &stitch_compress)
{
	config_->setStitchCompress(stitch_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchCompress;
}

void BackEnd::setStitchOutputFormat(ImageFormatConfig const &stitch_output_format)
{
	config_->setStitchOutputFormat(stitch_output_format);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchOutput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchOutputFormat(ImageFormatConfig &stitch_output_format) const
{
	stitch_output_format = config_->getStitchOutputFormat();
}

void BackEnd::setCdn(BeCdnConfig const &cdn)
{
	config_->setCdn(cdn);
	pipe_->dirtyFlagsBayer |= BayerEnable::Cdn;
}

void BackEnd::setWbg(WbgConfig const &wbg)
{
	WbgConfig wbg_copy = wbg;
	wbg_copy.pad[0] = wbg_copy.pad[1] = 0;
	config_->setWbg(wbg_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Wbg;
}

void BackEnd::getWbg(WbgConfig &wbg) const
{
	wbg = config_->getWbg();
}

void BackEnd::setLsc(BeLscConfig const &lsc, BeLscExtra lsc_extra)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finaliseTiling |= config_->getLsc().grid_step_x != lsc.grid_step_x ||
							config_->getLsc().grid_step_y != lsc.grid_step_y;
	config_->setLsc(lsc);
	pipe_->dirtyFlagsBayer |= BayerEnable::Lsc;
	pipe_->configExtra.lsc = lsc_extra;
}

void BackEnd::setCac(BeCacConfig const &cac, BeCacExtra cac_extra)
{
	pipe_->finaliseTiling |= config_->getCac().grid_step_x != cac.grid_step_x ||
							config_->getCac().grid_step_y != cac.grid_step_y;
	config_->setCac(cac);
	pipe_->configExtra.cac = cac_extra;
	pipe_->dirtyFlagsBayer |= BayerEnable::Cac;
}

void BackEnd::setDebin(BeDebinConfig const &debin)
{
	BeDebinConfig debin_copy = debin;
	debin_copy.pad[0] = debin_copy.pad[1] = 0;
	config_->setDebin(debin_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Debin;
}

void BackEnd::getDebin(BeDebinConfig &debin) const
{
	debin = config_->getDebin();
}

void BackEnd::setTonemap(BeTonemapConfig const &tonemap)
{
	config_->setTonemap(tonemap);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap;
}

void BackEnd::setDemosaic(BeDemosaicConfig const &demosaic)
{
	BeDemosaicConfig demosaic_copy = demosaic;
	demosaic_copy.pad[0] = demosaic_copy.pad[1] = 0;
	config_->setDemosaic(demosaic_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Demosaic;
}

void BackEnd::getDemosaic(BeDemosaicConfig &demosaic) const
{
	demosaic = config_->getDemosaic();
}

void BackEnd::setCcm(BeCcmConfig const &ccm)
{
	BeCcmConfig ccm_copy = ccm;
	ccm_copy.pad[0] = ccm_copy.pad[1] = 0;
	config_->setCcm(ccm_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ccm;
}

void BackEnd::setSatControl(BeSatControlConfig const &sat_control)
{
	BeSatControlConfig sat_control_copy = sat_control;
	sat_control_copy.pad = 0;
	config_->setSatControl(sat_control_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::SatControl;
}

void BackEnd::setYcbcr(BeCcmConfig const &ycbcr)
{
	BeCcmConfig ycbcr_copy = ycbcr;
	ycbcr_copy.pad[0] = ycbcr_copy.pad[1] = 0;
	config_->setYcbcr(ycbcr_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ycbcr;
}

void BackEnd::getYcbcr(BeCcmConfig &ycbcr) const
{
	ycbcr = config_->getYcbcr();
}

void BackEnd::setFalseColour(BeFalseColourConfig const &false_colour)
{
	BeFalseColourConfig false_colour_copy = false_colour;
	false_colour_copy.pad[0] = false_colour_copy.pad[1] = false_colour_copy.pad[2] = 0;
	config_->setFalseColour(false_colour_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::FalseColour;
}

void BackEnd::setSharpen(BeSharpenConfig const &sharpen)
{
	BeSharpenConfig sharpen_copy = sharpen;
	sharpen_copy.pad0[0] = sharpen_copy.pad0[1] = sharpen_copy.pad0[2] = 0;
	sharpen_copy.pad1[0] = sharpen_copy.pad1[1] = sharpen_copy.pad1[2] = 0;
	sharpen_copy.pad2[0] = sharpen_copy.pad2[1] = sharpen_copy.pad2[2] = 0;
	sharpen_copy.pad3[0] = sharpen_copy.pad3[1] = sharpen_copy.pad3[2] = 0;
	sharpen_copy.pad4[0] = sharpen_copy.pad4[1] = sharpen_copy.pad4[2] = 0;
	sharpen_copy.pad5 = sharpen_copy.pad6 = sharpen_copy.pad7 = sharpen_copy.pad8 =
		sharpen_copy.pad9 = 0;
	config_->setSharpen(sharpen_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::Sharpen;
}

void BackEnd::getSharpen(BeSharpenConfig &sharpen) const
{
	sharpen = config_->getSharpen();
}

void BackEnd::setShFcCombine(BeShFcCombineConfig const &sh_fc_combine)
{
	BeShFcCombineConfig sh_fc_combine_copy = sh_fc_combine;
	sh_fc_combine_copy.pad = 0;
	config_->setShFcCombine(sh_fc_combine_copy);
	pipe_->dirtyFlagsExtra |= DirtyFlag::ShFcCombine;
}

void BackEnd::setYcbcrInverse(BeCcmConfig const &ycbcr_inverse)
{
	BeCcmConfig ycbcr_inverse_copy = ycbcr_inverse;
	ycbcr_inverse_copy.pad[0] = ycbcr_inverse_copy.pad[1] = 0;
	config_->setYcbcrInverse(ycbcr_inverse_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::YcbcrInverse;
}

void BackEnd::setGamma(BeGammaConfig const &gamma)
{
	config_->setGamma(gamma);
	pipe_->dirtyFlagsRgb |= RgbEnable::Gamma0;
}

void BackEnd::getGamma(BeGammaConfig &gamma) const
{
	gamma = config_->getGamma();
}

void BackEnd::setCrop(BeCropConfig const &crop)
{
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

void BackEnd::setCrop(unsigned int i, BeCropConfig const &crop)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

void BackEnd::setCsc(unsigned int i, BeCcmConfig const &csc)
{
	config_->setCsc(i, csc);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::PostCsc0, i);
}

void BackEnd::getCsc(unsigned int i, BeCcmConfig &csc) const
{
	csc = config_->getCsc(i);
}

void BackEnd::setDownscale(unsigned int i, BeDownscaleConfig const &downscale,
						   BeDownscaleExtra const &downscale_extra)
{
	config_->setDownscale(i, downscale);
	pipe_->configExtra.downscale[i] = downscale_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

void BackEnd::setDownscale(unsigned int i, BeDownscaleExtra const &downscale_extra)
{
	pipe_->configExtra.downscale[i] = downscale_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

void BackEnd::setResample(unsigned int i, BeResampleConfig const &resample,
						  BeResampleExtra const &resample_extra)
{
	config_->setResample(i, resample);
	pipe_->configExtra.resample[i] = resample_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

void BackEnd::setResample(unsigned int i, BeResampleExtra const &resample_extra)
{
	pipe_->configExtra.resample[i] = resample_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

void BackEnd::setOutputFormat(unsigned int i, BeOutputFormatConfig const &output_format)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	BeOutputFormatConfig output_format_copy = output_format;
	output_format_copy.rebayer = output_format_copy.bayer_order = output_format_copy.chroma_pos = 0;
	config_->setOutputFormat(i, output_format_copy);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile = true;
}

void BackEnd::getOutputFormat(unsigned int i, BeOutputFormatConfig &output_format) const
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	output_format = config_->getOutputFormat(i);
}

void BackEnd::setSmartResize(unsigned int i, BackEnd::SmartResize const &smart_resize)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	// Non-zero width and height will be interpreted as "enabled".
	pipe_->smartResize[i] = smart_resize;
	pipe_->smartResizeDirty |= (1 << i);
}

unsigned int BackEnd::getMaxDownscale() const
{
	// Return an estimate of the largest horizontal downscale we can do.
	unsigned int max_tile_width = sw_.maxTileWidth;
	if (!max_tile_width)
		max_tile_width = variant_.backEndMaxTileWidth(0);

	// We reckon a 640 tile-width implementation can do 24x safely with formats that
	// want 1 byte per pixel. This should approximately scale with the max tile width.
	unsigned int downscale = 24;
	const unsigned int ref_tile_width = 640;
	return downscale * max_tile_width / ref_tile_width;
}
