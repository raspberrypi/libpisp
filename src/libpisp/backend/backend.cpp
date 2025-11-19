
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
		pipe_ = &internal_pipeline_config_;
	
	config_ = std::make_unique<BackendConfigAdapter>(&pipe_->hwConfig);

	const char *env = std::getenv("LIBPISP_BE_CONFIG_FILE");
	initialiseDefaultConfig(env ? std::string(env) : swConfig.defaultsFile);
}

BackEnd::~BackEnd()
{
}

void BackEnd::setGlobal(BackendConfigAdapter::Global const &global)
{
	RgbEnables changedRgbEnables(global.rgbEnables ^ pipe_->rgbEnables);

	if (changedRgbEnables & (RgbEnable::Downscale0 | RgbEnable::Downscale1 |
							 RgbEnable::Resample0 | RgbEnable::Resample1))
		pipe_->retile = true; // must retile when rescale change

	pipe_->dirtyFlagsBayer |= (global.bayerEnables & ~pipe_->bayerEnables); // label anything newly enabled as dirty
	pipe_->dirtyFlagsRgb |= (global.rgbEnables & ~pipe_->rgbEnables); // label anything newly enabled as dirty
	pipe_->dirtyFlagsExtra |= DirtyFlag::Global;
	config_->setGlobal(global);

	// For convenience:
	pipe_->bayerEnables = global.bayerEnables;
	pipe_->rgbEnables = global.rgbEnables;
}

void BackEnd::getGlobal(BackendConfigAdapter::Global &global) const
{
	global = config_->getGlobal();
}

void BackEnd::setInputFormat(pisp_image_format_config const &inputFormat)
{
	config_->setInputFormat(inputFormat);
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(inputFormat.format))
		pipe_->dirtyFlagsBayer |= BayerEnable::Input;
	else
		pipe_->dirtyFlagsRgb |= RgbEnable::Input;
	pipe_->retile = true;
}

void BackEnd::getInputFormat(pisp_image_format_config &inputFormat) const
{
	inputFormat = config_->getInputFormat();
}

void BackEnd::setDecompress(pisp_decompress_config const &decompress)
{
	config_->setDecompress(decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::Decompress;
}

void BackEnd::setDpc(pisp_be_dpc_config const &dpc)
{
	pisp_be_dpc_config dpc_copy = dpc;
	dpc_copy.pad = 0;
	config_->setDpc(dpc_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Dpc;
}

void BackEnd::setGeq(pisp_be_geq_config const &geq)
{
	pisp_be_geq_config geq_copy = geq;
	geq_copy.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	config_->setGeq(geq_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Geq;
}

void BackEnd::setTdnInputFormat(pisp_image_format_config const &tdnInputFormat)
{
	config_->setTdnInputFormat(tdnInputFormat);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnInput; // TDN input address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnInputFormat(pisp_image_format_config &tdnInputFormat) const
{
	tdnInputFormat = config_->getTdnInputFormat();
}

void BackEnd::setTdnDecompress(pisp_decompress_config const &tdn_decompress)
{
	config_->setTdnDecompress(tdn_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnDecompress;
}

void BackEnd::setTdn(pisp_be_tdn_config const &tdn)
{
	pisp_be_tdn_config tdn_copy = tdn;
	tdn_copy.pad = 0;
	config_->setTdn(tdn_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tdn;
}

void BackEnd::getTdn(pisp_be_tdn_config &tdn) const
{
	tdn = config_->getTdn();
}

void BackEnd::setTdnCompress(pisp_compress_config const &tdn_compress)
{
	config_->setTdnCompress(tdn_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnCompress;
}

void BackEnd::setTdnOutputFormat(pisp_image_format_config const &tdn_output_format)
{
	config_->setTdnOutputFormat(tdn_output_format);
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnOutput; // TDN output address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnOutputFormat(pisp_image_format_config &tdn_output_format) const
{
	tdn_output_format = config_->getTdnOutputFormat();
}

void BackEnd::setSdn(pisp_be_sdn_config const &sdn)
{
	pisp_be_sdn_config sdn_copy = sdn;
	sdn_copy.pad = 0;
	config_->setSdn(sdn_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Sdn;
}

void BackEnd::setBlc(pisp_bla_config const &blc)
{
	pisp_bla_config blc_copy = blc;
	blc_copy.pad[0] = blc_copy.pad[1] = 0;
	config_->setBlc(blc_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Blc;
}

void BackEnd::getBlc(pisp_bla_config &blc) const
{
	blc = config_->getBlc();
}

void BackEnd::setStitchInputFormat(pisp_image_format_config const &stitch_input_format)
{
	config_->setStitchInputFormat(stitch_input_format);
	// Note: stitch.pad clearing would need variant-specific handling
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchInput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchInputFormat(pisp_image_format_config &stitch_input_format) const
{
	stitch_input_format = config_->getStitchInputFormat();
}

void BackEnd::setStitchDecompress(pisp_decompress_config const &stitch_decompress)
{
	config_->setStitchDecompress(stitch_decompress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchDecompress;
}

void BackEnd::setStitch(pisp_be_stitch_config const &stitch)
{
	config_->setStitch(stitch);
	pipe_->dirtyFlagsBayer |= BayerEnable::Stitch;
}

void BackEnd::setStitchCompress(pisp_compress_config const &stitch_compress)
{
	config_->setStitchCompress(stitch_compress);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchCompress;
}

void BackEnd::setStitchOutputFormat(pisp_image_format_config const &stitch_output_format)
{
	config_->setStitchOutputFormat(stitch_output_format);
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchOutput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchOutputFormat(pisp_image_format_config &stitch_output_format) const
{
	stitch_output_format = config_->getStitchOutputFormat();
}

void BackEnd::setCdn(pisp_be_cdn_config const &cdn)
{
	config_->setCdn(cdn);
	pipe_->dirtyFlagsBayer |= BayerEnable::Cdn;
}

void BackEnd::setWbg(pisp_wbg_config const &wbg)
{
	pisp_wbg_config wbg_copy = wbg;
	wbg_copy.pad[0] = wbg_copy.pad[1] = 0;
	config_->setWbg(wbg_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Wbg;
}

void BackEnd::getWbg(pisp_wbg_config &wbg) const
{
	wbg = config_->getWbg();
}

void BackEnd::setLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lsc_extra)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finaliseTiling |= config_->getLsc().grid_step_x != lsc.grid_step_x ||
							config_->getLsc().grid_step_y != lsc.grid_step_y;
	config_->setLsc(lsc);
	pipe_->dirtyFlagsBayer |= BayerEnable::Lsc;
	pipe_->configExtra.lsc = lsc_extra;
}

void BackEnd::setCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cac_extra)
{
	pipe_->finaliseTiling |= config_->getCac().grid_step_x != cac.grid_step_x ||
							config_->getCac().grid_step_y != cac.grid_step_y;
	config_->setCac(cac);
	pipe_->configExtra.cac = cac_extra;
	pipe_->dirtyFlagsBayer |= BayerEnable::Cac;
}

void BackEnd::setDebin(pisp_be_debin_config const &debin)
{
	pisp_be_debin_config debin_copy = debin;
	debin_copy.pad[0] = debin_copy.pad[1] = 0;
	config_->setDebin(debin_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Debin;
}

void BackEnd::getDebin(pisp_be_debin_config &debin) const
{
	debin = config_->getDebin();
}

void BackEnd::setTonemap(pisp_be_tonemap_config const &tonemap)
{
	config_->setTonemap(tonemap);
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap;
}

void BackEnd::setDemosaic(pisp_be_demosaic_config const &demosaic)
{
	pisp_be_demosaic_config demosaic_copy = demosaic;
	demosaic_copy.pad[0] = demosaic_copy.pad[1] = 0;
	config_->setDemosaic(demosaic_copy);
	pipe_->dirtyFlagsBayer |= BayerEnable::Demosaic;
}

void BackEnd::getDemosaic(pisp_be_demosaic_config &demosaic) const
{
	demosaic = config_->getDemosaic();
}

void BackEnd::setCcm(pisp_be_ccm_config const &ccm)
{
	pisp_be_ccm_config ccm_copy = ccm;
	ccm_copy.pad[0] = ccm_copy.pad[1] = 0;
	config_->setCcm(ccm_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ccm;
}

void BackEnd::setSatControl(pisp_be_sat_control_config const &sat_control)
{
	pisp_be_sat_control_config sat_control_copy = sat_control;
	sat_control_copy.pad = 0;
	config_->setSatControl(sat_control_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::SatControl;
}

void BackEnd::setYcbcr(pisp_be_ccm_config const &ycbcr)
{
	pisp_be_ccm_config ycbcr_copy = ycbcr;
	ycbcr_copy.pad[0] = ycbcr_copy.pad[1] = 0;
	config_->setYcbcr(ycbcr_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::Ycbcr;
}

void BackEnd::getYcbcr(pisp_be_ccm_config &ycbcr) const
{
	ycbcr = config_->getYcbcr();
}

void BackEnd::setFalseColour(pisp_be_false_colour_config const &false_colour)
{
	pisp_be_false_colour_config false_colour_copy = false_colour;
	false_colour_copy.pad[0] = false_colour_copy.pad[1] = false_colour_copy.pad[2] = 0;
	config_->setFalseColour(false_colour_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::FalseColour;
}

void BackEnd::setSharpen(pisp_be_sharpen_config const &sharpen)
{
	pisp_be_sharpen_config sharpen_copy = sharpen;
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

void BackEnd::getSharpen(pisp_be_sharpen_config &sharpen) const
{
	sharpen = config_->getSharpen();
}

void BackEnd::setShFcCombine(pisp_be_sh_fc_combine_config const &sh_fc_combine)
{
	pisp_be_sh_fc_combine_config sh_fc_combine_copy = sh_fc_combine;
	sh_fc_combine_copy.pad = 0;
	config_->setShFcCombine(sh_fc_combine_copy);
	pipe_->dirtyFlagsExtra |= DirtyFlag::ShFcCombine;
}

void BackEnd::setYcbcrInverse(pisp_be_ccm_config const &ycbcr_inverse)
{
	pisp_be_ccm_config ycbcr_inverse_copy = ycbcr_inverse;
	ycbcr_inverse_copy.pad[0] = ycbcr_inverse_copy.pad[1] = 0;
	config_->setYcbcrInverse(ycbcr_inverse_copy);
	pipe_->dirtyFlagsRgb |= RgbEnable::YcbcrInverse;
}

void BackEnd::setGamma(pisp_be_gamma_config const &gamma)
{
	config_->setGamma(gamma);
	pipe_->dirtyFlagsRgb |= RgbEnable::Gamma0;
}

void BackEnd::getGamma(pisp_be_gamma_config &gamma) const
{
	gamma = config_->getGamma();
}

void BackEnd::setCrop(pisp_be_crop_config const &crop)
{
	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++)
		pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

void BackEnd::setCrop(unsigned int i, pisp_be_crop_config const &crop)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	pipe_->configExtra.crop[i] = crop;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

void BackEnd::setCsc(unsigned int i, pisp_be_ccm_config const &csc)
{
	config_->setCsc(i, csc);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::PostCsc0, i);
}

void BackEnd::getCsc(unsigned int i, pisp_be_ccm_config &csc) const
{
	csc = config_->getCsc(i);
}

void BackEnd::setDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
						   pisp_be_downscale_extra const &downscale_extra)
{
	config_->setDownscale(i, downscale);
	pipe_->configExtra.downscale[i] = downscale_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

void BackEnd::setDownscale(unsigned int i, pisp_be_downscale_extra const &downscale_extra)
{
	pipe_->configExtra.downscale[i] = downscale_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

void BackEnd::setResample(unsigned int i, pisp_be_resample_config const &resample,
						  pisp_be_resample_extra const &resample_extra)
{
	config_->setResample(i, resample);
	pipe_->configExtra.resample[i] = resample_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

void BackEnd::setResample(unsigned int i, pisp_be_resample_extra const &resample_extra)
{
	pipe_->configExtra.resample[i] = resample_extra;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

void BackEnd::setOutputFormat(unsigned int i, pisp_be_output_format_config const &output_format)
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	pisp_be_output_format_config output_format_copy = output_format;
	output_format_copy.rebayer = output_format_copy.bayer_order = output_format_copy.chroma_pos = 0;
	config_->setOutputFormat(i, output_format_copy);
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile = true;
}

void BackEnd::getOutputFormat(unsigned int i, pisp_be_output_format_config &output_format) const
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
