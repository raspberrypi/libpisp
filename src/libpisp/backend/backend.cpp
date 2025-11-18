
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
	: sw_(swConfig), variant_(variant), adaptor_(this)
{
	unsigned int maxTileWidth = variant_.backEndMaxTileWidth(0);

	if (sw_.maxTileWidth > maxTileWidth)
		PISP_LOG(fatal, "Configured max tile width " << sw_.maxTileWidth << " exceeds " << maxTileWidth);

	if (configStorage.data() && configStorage.size() >= sizeof(PipelineConfig))
		pipe_ = reinterpret_cast<PipelineConfig *>(configStorage.data());
	else
		pipe_ = &internal_pipeline_config_;

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
	pipe_->hwConfig.global = adaptor_.globalToHw(global);
	pipe_->hwConfig.global.pad[0] = pipe_->hwConfig.global.pad[1] = pipe_->hwConfig.global.pad[2] = 0;
	pipe_->dirtyFlagsExtra |= DirtyFlag::Global;

	// For convenience:
	pipe_->bayerEnables = global.bayerEnables;
	pipe_->rgbEnables = global.rgbEnables;
}

void BackEnd::getGlobal(BackendConfigAdapter::Global &global) const
{
	global = adaptor_.hwToGlobal(pipe_->hwConfig.global);
}

void BackEnd::setInputFormat(pisp_image_format_config const &inputFormat)
{
	pipe_->hwConfig.input_format = inputFormat;
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(inputFormat.format))
		pipe_->dirtyFlagsBayer |= BayerEnable::Input;
	else
		pipe_->dirtyFlagsRgb |= RgbEnable::Input;
	pipe_->retile = true;
}

void BackEnd::getInputFormat(pisp_image_format_config &inputFormat) const
{
	inputFormat = pipe_->hwConfig.input_format;
}

void BackEnd::setDecompress(pisp_decompress_config const &decompress)
{
	pipe_->hwConfig.decompress = decompress;
	pipe_->dirtyFlagsBayer |= BayerEnable::Decompress;
}

void BackEnd::setDpc(pisp_be_dpc_config const &dpc)
{
	pipe_->hwConfig.dpc = dpc;
	pipe_->hwConfig.dpc.pad = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Dpc;
}

void BackEnd::setGeq(pisp_be_geq_config const &geq)
{
	pipe_->hwConfig.geq = geq;
	pipe_->hwConfig.geq.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	pipe_->dirtyFlagsBayer |= BayerEnable::Geq;
}

void BackEnd::setTdnInputFormat(pisp_image_format_config const &tdnInputFormat)
{
	pipe_->hwConfig.tdn_input_format = tdnInputFormat;
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnInput; // TDN input address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnInputFormat(pisp_image_format_config &tdnInputFormat) const
{
	tdnInputFormat = pipe_->hwConfig.tdn_input_format;
}

void BackEnd::setTdnDecompress(pisp_decompress_config const &tdn_decompress)
{
	pipe_->hwConfig.tdn_decompress = tdn_decompress;
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnDecompress;
}

void BackEnd::setTdn(pisp_be_tdn_config const &tdn)
{
	pipe_->hwConfig.tdn = tdn;
	pipe_->hwConfig.tdn.pad = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Tdn;
}

void BackEnd::getTdn(pisp_be_tdn_config &tdn) const
{
	tdn = pipe_->hwConfig.tdn;
}

void BackEnd::setTdnCompress(pisp_compress_config const &tdn_compress)
{
	pipe_->hwConfig.tdn_compress = tdn_compress;
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnCompress;
}

void BackEnd::setTdnOutputFormat(pisp_image_format_config const &tdn_output_format)
{
	pipe_->hwConfig.tdn_output_format = tdn_output_format;
	pipe_->dirtyFlagsBayer |= BayerEnable::TdnOutput; // TDN output address will always be written
	pipe_->finaliseTiling = true;
}

void BackEnd::getTdnOutputFormat(pisp_image_format_config &tdn_output_format) const
{
	tdn_output_format = pipe_->hwConfig.tdn_output_format;
}

void BackEnd::setSdn(pisp_be_sdn_config const &sdn)
{
	pipe_->hwConfig.sdn = sdn;
	pipe_->hwConfig.sdn.pad = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Sdn;
}

void BackEnd::setBlc(pisp_bla_config const &blc)
{
	pipe_->hwConfig.blc = blc;
	pipe_->hwConfig.blc.pad[0] = pipe_->hwConfig.blc.pad[1] = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Blc;
}

void BackEnd::getBlc(pisp_bla_config &blc) const
{
	blc = pipe_->hwConfig.blc;
}

void BackEnd::setStitchInputFormat(pisp_image_format_config const &stitch_input_format)
{
	pipe_->hwConfig.stitch_input_format = stitch_input_format;
	pipe_->hwConfig.stitch.pad = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchInput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchInputFormat(pisp_image_format_config &stitch_input_format) const
{
	stitch_input_format = pipe_->hwConfig.stitch_input_format;
}

void BackEnd::setStitchDecompress(pisp_decompress_config const &stitch_decompress)
{
	pipe_->hwConfig.stitch_decompress = stitch_decompress;
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchDecompress;
}

void BackEnd::setStitch(pisp_be_stitch_config const &stitch)
{
	pipe_->hwConfig.stitch = stitch;
	pipe_->dirtyFlagsBayer |= BayerEnable::Stitch;
}

void BackEnd::setStitchCompress(pisp_compress_config const &stitch_compress)
{
	pipe_->hwConfig.stitch_compress = stitch_compress;
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchCompress;
}

void BackEnd::setStitchOutputFormat(pisp_image_format_config const &stitch_output_format)
{
	pipe_->hwConfig.stitch_output_format = stitch_output_format;
	pipe_->dirtyFlagsBayer |= BayerEnable::StitchOutput;
	pipe_->finaliseTiling = true;
}

void BackEnd::getStitchOutputFormat(pisp_image_format_config &stitch_output_format) const
{
	stitch_output_format = pipe_->hwConfig.stitch_output_format;
}

void BackEnd::setCdn(pisp_be_cdn_config const &cdn)
{
	pipe_->hwConfig.cdn = cdn;
	pipe_->dirtyFlagsBayer |= BayerEnable::Cdn;
}

void BackEnd::setWbg(pisp_wbg_config const &wbg)
{
	pipe_->hwConfig.wbg = wbg;
	pipe_->hwConfig.wbg.pad[0] = pipe_->hwConfig.wbg.pad[1] = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Wbg;
}

void BackEnd::getWbg(pisp_wbg_config &wbg) const
{
	wbg = pipe_->hwConfig.wbg;
}

void BackEnd::setLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lsc_extra)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finaliseTiling |= pipe_->hwConfig.lsc.grid_step_x != lsc.grid_step_x ||
							pipe_->hwConfig.lsc.grid_step_y != lsc.grid_step_y;
	pipe_->hwConfig.lsc = lsc;
	pipe_->dirtyFlagsBayer |= BayerEnable::Lsc;
	pipe_->configExtra.lsc = lsc_extra;
}

void BackEnd::setCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cac_extra)
{
	pipe_->finaliseTiling |= pipe_->hwConfig.cac.grid_step_x != cac.grid_step_x ||
							pipe_->hwConfig.cac.grid_step_y != cac.grid_step_y;
	pipe_->hwConfig.cac = cac;
	pipe_->configExtra.cac = cac_extra;
	pipe_->dirtyFlagsBayer |= BayerEnable::Cac;
}

void BackEnd::setDebin(pisp_be_debin_config const &debin)
{
	pipe_->hwConfig.debin = debin;
	pipe_->hwConfig.debin.pad[0] = pipe_->hwConfig.debin.pad[1] = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Debin;
}

void BackEnd::getDebin(pisp_be_debin_config &debin) const
{
	debin = pipe_->hwConfig.debin;
}

void BackEnd::setTonemap(pisp_be_tonemap_config const &tonemap)
{
	pipe_->hwConfig.tonemap = tonemap;
	pipe_->dirtyFlagsBayer |= BayerEnable::Tonemap;
}

void BackEnd::setDemosaic(pisp_be_demosaic_config const &demosaic)
{
	pipe_->hwConfig.demosaic = demosaic;
	pipe_->hwConfig.demosaic.pad[0] = pipe_->hwConfig.demosaic.pad[1] = 0;
	pipe_->dirtyFlagsBayer |= BayerEnable::Demosaic;
}

void BackEnd::getDemosaic(pisp_be_demosaic_config &demosaic) const
{
	demosaic = pipe_->hwConfig.demosaic;
}

void BackEnd::setCcm(pisp_be_ccm_config const &ccm)
{
	pipe_->hwConfig.ccm = ccm;
	pipe_->hwConfig.ccm.pad[0] = pipe_->hwConfig.ccm.pad[1] = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::Ccm;
}

void BackEnd::setSatControl(pisp_be_sat_control_config const &sat_control)
{
	pipe_->hwConfig.sat_control = sat_control;
	pipe_->hwConfig.sat_control.pad = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::SatControl;
}

void BackEnd::setYcbcr(pisp_be_ccm_config const &ycbcr)
{
	pipe_->hwConfig.ycbcr = ycbcr;
	pipe_->hwConfig.ycbcr.pad[0] = pipe_->hwConfig.ycbcr.pad[1] = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::Ycbcr;
}

void BackEnd::getYcbcr(pisp_be_ccm_config &ycbcr) const
{
	ycbcr = pipe_->hwConfig.ycbcr;
}

void BackEnd::setFalseColour(pisp_be_false_colour_config const &false_colour)
{
	pipe_->hwConfig.false_colour = false_colour;
	pipe_->hwConfig.false_colour.pad[0] = pipe_->hwConfig.false_colour.pad[1] = pipe_->hwConfig.false_colour.pad[2] = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::FalseColour;
}

void BackEnd::setSharpen(pisp_be_sharpen_config const &sharpen)
{
	pipe_->hwConfig.sharpen = sharpen;
	pipe_->hwConfig.sharpen.pad0[0] = pipe_->hwConfig.sharpen.pad0[1] = pipe_->hwConfig.sharpen.pad0[2] = 0;
	pipe_->hwConfig.sharpen.pad1[0] = pipe_->hwConfig.sharpen.pad1[1] = pipe_->hwConfig.sharpen.pad1[2] = 0;
	pipe_->hwConfig.sharpen.pad2[0] = pipe_->hwConfig.sharpen.pad2[1] = pipe_->hwConfig.sharpen.pad2[2] = 0;
	pipe_->hwConfig.sharpen.pad3[0] = pipe_->hwConfig.sharpen.pad3[1] = pipe_->hwConfig.sharpen.pad3[2] = 0;
	pipe_->hwConfig.sharpen.pad4[0] = pipe_->hwConfig.sharpen.pad4[1] = pipe_->hwConfig.sharpen.pad4[2] = 0;
	pipe_->hwConfig.sharpen.pad5 = pipe_->hwConfig.sharpen.pad6 = pipe_->hwConfig.sharpen.pad7 = pipe_->hwConfig.sharpen.pad8 =
		pipe_->hwConfig.sharpen.pad9 = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::Sharpen;
}

void BackEnd::getSharpen(pisp_be_sharpen_config &sharpen) const
{
	sharpen = pipe_->hwConfig.sharpen;
}

void BackEnd::setShFcCombine(pisp_be_sh_fc_combine_config const &sh_fc_combine)
{
	pipe_->hwConfig.sh_fc_combine = sh_fc_combine;
	pipe_->hwConfig.sh_fc_combine.pad = 0;
	pipe_->dirtyFlagsExtra |= DirtyFlag::ShFcCombine;
}

void BackEnd::setYcbcrInverse(pisp_be_ccm_config const &ycbcr_inverse)
{
	pipe_->hwConfig.ycbcr_inverse = ycbcr_inverse;
	pipe_->hwConfig.ycbcr_inverse.pad[0] = pipe_->hwConfig.ycbcr_inverse.pad[1] = 0;
	pipe_->dirtyFlagsRgb |= RgbEnable::YcbcrInverse;
}

void BackEnd::setGamma(pisp_be_gamma_config const &gamma)
{
	pipe_->hwConfig.gamma = gamma;
	pipe_->dirtyFlagsRgb |= RgbEnable::Gamma0;
}

void BackEnd::getGamma(pisp_be_gamma_config &gamma) const
{
	gamma = pipe_->hwConfig.gamma;
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
	pipe_->hwConfig.csc[i] = csc;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::PostCsc0, i);
}

void BackEnd::getCsc(unsigned int i, pisp_be_ccm_config &csc) const
{
	csc = pipe_->hwConfig.csc[i];
}

void BackEnd::setDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
						   pisp_be_downscale_extra const &downscale_extra)
{
	pipe_->hwConfig.downscale[i] = downscale;
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
	pipe_->hwConfig.resample[i] = resample;
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
	pipe_->hwConfig.output_format[i] = output_format;
	pipe_->hwConfig.output_format[i].rebayer = pipe_->hwConfig.output_format[i].bayer_order = pipe_->hwConfig.output_format[i].chroma_pos = 0;
	pipe_->dirtyFlagsRgb |= getFlagForBranch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile = true;
}

void BackEnd::getOutputFormat(unsigned int i, pisp_be_output_format_config &output_format) const
{
	PISP_ASSERT(i < variant_.backEndNumBranches(0));
	output_format = pipe_->hwConfig.output_format[i];
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
