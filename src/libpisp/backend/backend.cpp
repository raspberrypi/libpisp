
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

size_t BackEnd::GetStorageSize([[maybe_unused]] const PiSPVariant &variant)
{
	return sizeof(PipelineConfig);
}

BackEnd::BackEnd(SoftwareConfig const &sw_config, PiSPVariant const &variant, std::span<uint8_t> config_storage)
	: sw_(sw_config), variant_(variant), adaptor_(this)
{
	unsigned int max_tile_width = variant_.BackEndMaxTileWidth(0);

	if (sw_.max_tile_width > max_tile_width)
		PISP_LOG(fatal, "Configured max tile width " << sw_.max_tile_width << " exceeds " << max_tile_width);

	if (config_storage.data() && config_storage.size() >= sizeof(PipelineConfig))
		pipe_ = reinterpret_cast<PipelineConfig *>(config_storage.data());
	else
		pipe_ = &internal_hw_config_;

	const char *env = std::getenv("LIBPISP_BE_CONFIG_FILE");
	initialiseDefaultConfig(env ? std::string(env) : sw_config.defaults_file);
}

BackEnd::~BackEnd()
{
}

void BackEnd::SetGlobal(BackendConfigAdapter::Global const &global)
{
	RgbEnables changed_rgb_enables(global.rgb_enables ^ pipe_->rgb_enables);

	if (changed_rgb_enables & (RgbEnable::Downscale0 | RgbEnable::Downscale1 |
							   RgbEnable::Resample0 | RgbEnable::Resample1))
		pipe_->retile_ = true; // must retile when rescale change

	pipe_->dirty_flags_bayer |= (global.bayer_enables & ~pipe_->bayer_enables); // label anything newly enabled as dirty
	pipe_->dirty_flags_rgb |= (global.rgb_enables & ~pipe_->rgb_enables); // label anything newly enabled as dirty
	pipe_->hw_config.global = adaptor_.GlobaltoHw(global);
	pipe_->hw_config.global.pad[0] = pipe_->hw_config.global.pad[1] = pipe_->hw_config.global.pad[2] = 0;
	pipe_->dirty_flags_extra |= DirtyFlag::Global;

	// For convenience:
	pipe_->bayer_enables = global.bayer_enables;
	pipe_->rgb_enables = global.rgb_enables;
}

void BackEnd::GetGlobal(BackendConfigAdapter::Global &global) const
{
	global = adaptor_.HwtoGlobal(pipe_->hw_config.global);
}

void BackEnd::SetInputFormat(pisp_image_format_config const &input_format)
{
	pipe_->hw_config.input_format = input_format;
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(input_format.format))
		pipe_->dirty_flags_bayer |= BayerEnable::Input;
	else
		pipe_->dirty_flags_rgb |= RgbEnable::Input;
	pipe_->retile_ = true;
}

void BackEnd::GetInputFormat(pisp_image_format_config &input_format) const
{
	input_format = pipe_->hw_config.input_format;
}

void BackEnd::SetDecompress(pisp_decompress_config const &decompress)
{
	pipe_->hw_config.decompress = decompress;
	pipe_->dirty_flags_bayer |= BayerEnable::Decompress;
}

void BackEnd::SetDpc(pisp_be_dpc_config const &dpc)
{
	pipe_->hw_config.dpc = dpc;
	pipe_->hw_config.dpc.pad = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Dpc;
}

void BackEnd::SetGeq(pisp_be_geq_config const &geq)
{
	pipe_->hw_config.geq = geq;
	pipe_->hw_config.geq.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	pipe_->dirty_flags_bayer |= BayerEnable::Geq;
}

void BackEnd::SetTdnInputFormat(pisp_image_format_config const &tdn_input_format)
{
	pipe_->hw_config.tdn_input_format = tdn_input_format;
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn_input; // TDN input address will always be written
	pipe_->finalise_tiling = true;
}

void BackEnd::GetTdnInputFormat(pisp_image_format_config &tdn_input_format) const
{
	tdn_input_format = pipe_->hw_config.tdn_input_format;
}

void BackEnd::SetTdnDecompress(pisp_decompress_config const &tdn_decompress)
{
	pipe_->hw_config.tdn_decompress = tdn_decompress;
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn_decompress;
}

void BackEnd::SetTdn(pisp_be_tdn_config const &tdn)
{
	pipe_->hw_config.tdn = tdn;
	pipe_->hw_config.tdn.pad = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn;
}

void BackEnd::GetTdn(pisp_be_tdn_config &tdn) const
{
	tdn = pipe_->hw_config.tdn;
}

void BackEnd::SetTdnCompress(pisp_compress_config const &tdn_compress)
{
	pipe_->hw_config.tdn_compress = tdn_compress;
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn_compress;
}

void BackEnd::SetTdnOutputFormat(pisp_image_format_config const &tdn_output_format)
{
	pipe_->hw_config.tdn_output_format = tdn_output_format;
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn_output; // TDN output address will always be written
	pipe_->finalise_tiling = true;
}

void BackEnd::GetTdnOutputFormat(pisp_image_format_config &tdn_output_format) const
{
	tdn_output_format = pipe_->hw_config.tdn_output_format;
}

void BackEnd::SetSdn(pisp_be_sdn_config const &sdn)
{
	pipe_->hw_config.sdn = sdn;
	pipe_->hw_config.sdn.pad = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Sdn;
}

void BackEnd::SetBlc(pisp_bla_config const &blc)
{
	pipe_->hw_config.blc = blc;
	pipe_->hw_config.blc.pad[0] = pipe_->hw_config.blc.pad[1] = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Blc;
}

void BackEnd::GetBlc(pisp_bla_config &blc) const
{
	blc = pipe_->hw_config.blc;
}

void BackEnd::SetStitchInputFormat(pisp_image_format_config const &stitch_input_format)
{
	pipe_->hw_config.stitch_input_format = stitch_input_format;
	pipe_->hw_config.stitch.pad = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch_input;
	pipe_->finalise_tiling = true;
}

void BackEnd::GetStitchInputFormat(pisp_image_format_config &stitch_input_format) const
{
	stitch_input_format = pipe_->hw_config.stitch_input_format;
}

void BackEnd::SetStitchDecompress(pisp_decompress_config const &stitch_decompress)
{
	pipe_->hw_config.stitch_decompress = stitch_decompress;
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch_decompress;
}

void BackEnd::SetStitch(pisp_be_stitch_config const &stitch)
{
	pipe_->hw_config.stitch = stitch;
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch;
}

void BackEnd::SetStitchCompress(pisp_compress_config const &stitch_compress)
{
	pipe_->hw_config.stitch_compress = stitch_compress;
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch_compress;
}

void BackEnd::SetStitchOutputFormat(pisp_image_format_config const &stitch_output_format)
{
	pipe_->hw_config.stitch_output_format = stitch_output_format;
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch_output;
	pipe_->finalise_tiling = true;
}

void BackEnd::GetStitchOutputFormat(pisp_image_format_config &stitch_output_format) const
{
	stitch_output_format = pipe_->hw_config.stitch_output_format;
}

void BackEnd::SetCdn(pisp_be_cdn_config const &cdn)
{
	pipe_->hw_config.cdn = cdn;
	pipe_->dirty_flags_bayer |= BayerEnable::Cdn;
}

void BackEnd::SetWbg(pisp_wbg_config const &wbg)
{
	pipe_->hw_config.wbg = wbg;
	pipe_->hw_config.wbg.pad[0] = pipe_->hw_config.wbg.pad[1] = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Wbg;
}

void BackEnd::GetWbg(pisp_wbg_config &wbg) const
{
	wbg = pipe_->hw_config.wbg;
}

void BackEnd::SetLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lsc_extra)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finalise_tiling |= pipe_->hw_config.lsc.grid_step_x != lsc.grid_step_x ||
							pipe_->hw_config.lsc.grid_step_y != lsc.grid_step_y;
	pipe_->hw_config.lsc = lsc;
	pipe_->dirty_flags_bayer |= BayerEnable::Lsc;
	pipe_->config_extra.lsc = lsc_extra;
}

void BackEnd::SetCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cac_extra)
{
	pipe_->finalise_tiling |= pipe_->hw_config.cac.grid_step_x != cac.grid_step_x ||
							pipe_->hw_config.cac.grid_step_y != cac.grid_step_y;
	pipe_->hw_config.cac = cac;
	pipe_->config_extra.cac = cac_extra;
	pipe_->dirty_flags_bayer |= BayerEnable::Cac;
}

void BackEnd::SetDebin(pisp_be_debin_config const &debin)
{
	pipe_->hw_config.debin = debin;
	pipe_->hw_config.debin.pad[0] = pipe_->hw_config.debin.pad[1] = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Debin;
}

void BackEnd::GetDebin(pisp_be_debin_config &debin) const
{
	debin = pipe_->hw_config.debin;
}

void BackEnd::SetTonemap(pisp_be_tonemap_config const &tonemap)
{
	pipe_->hw_config.tonemap = tonemap;
	pipe_->dirty_flags_bayer |= BayerEnable::Tonemap;
}

void BackEnd::SetDemosaic(pisp_be_demosaic_config const &demosaic)
{
	pipe_->hw_config.demosaic = demosaic;
	pipe_->hw_config.demosaic.pad[0] = pipe_->hw_config.demosaic.pad[1] = 0;
	pipe_->dirty_flags_bayer |= BayerEnable::Demosaic;
}

void BackEnd::GetDemosaic(pisp_be_demosaic_config &demosaic) const
{
	demosaic = pipe_->hw_config.demosaic;
}

void BackEnd::SetCcm(pisp_be_ccm_config const &ccm)
{
	pipe_->hw_config.ccm = ccm;
	pipe_->hw_config.ccm.pad[0] = pipe_->hw_config.ccm.pad[1] = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::Ccm;
}

void BackEnd::SetSatControl(pisp_be_sat_control_config const &sat_control)
{
	pipe_->hw_config.sat_control = sat_control;
	pipe_->hw_config.sat_control.pad = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::Sat_control;
}

void BackEnd::SetYcbcr(pisp_be_ccm_config const &ycbcr)
{
	pipe_->hw_config.ycbcr = ycbcr;
	pipe_->hw_config.ycbcr.pad[0] = pipe_->hw_config.ycbcr.pad[1] = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::Ycbcr;
}

void BackEnd::GetYcbcr(pisp_be_ccm_config &ycbcr) const
{
	ycbcr = pipe_->hw_config.ycbcr;
}

void BackEnd::SetFalseColour(pisp_be_false_colour_config const &false_colour)
{
	pipe_->hw_config.false_colour = false_colour;
	pipe_->hw_config.false_colour.pad[0] = pipe_->hw_config.false_colour.pad[1] = pipe_->hw_config.false_colour.pad[2] = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::False_colour;
}

void BackEnd::SetSharpen(pisp_be_sharpen_config const &sharpen)
{
	pipe_->hw_config.sharpen = sharpen;
	pipe_->hw_config.sharpen.pad0[0] = pipe_->hw_config.sharpen.pad0[1] = pipe_->hw_config.sharpen.pad0[2] = 0;
	pipe_->hw_config.sharpen.pad1[0] = pipe_->hw_config.sharpen.pad1[1] = pipe_->hw_config.sharpen.pad1[2] = 0;
	pipe_->hw_config.sharpen.pad2[0] = pipe_->hw_config.sharpen.pad2[1] = pipe_->hw_config.sharpen.pad2[2] = 0;
	pipe_->hw_config.sharpen.pad3[0] = pipe_->hw_config.sharpen.pad3[1] = pipe_->hw_config.sharpen.pad3[2] = 0;
	pipe_->hw_config.sharpen.pad4[0] = pipe_->hw_config.sharpen.pad4[1] = pipe_->hw_config.sharpen.pad4[2] = 0;
	pipe_->hw_config.sharpen.pad5 = pipe_->hw_config.sharpen.pad6 = pipe_->hw_config.sharpen.pad7 = pipe_->hw_config.sharpen.pad8 =
		pipe_->hw_config.sharpen.pad9 = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::Sharpen;
}

void BackEnd::GetSharpen(pisp_be_sharpen_config &sharpen) const
{
	sharpen = pipe_->hw_config.sharpen;
}

void BackEnd::SetShFcCombine(pisp_be_sh_fc_combine_config const &sh_fc_combine)
{
	pipe_->hw_config.sh_fc_combine = sh_fc_combine;
	pipe_->hw_config.sh_fc_combine.pad = 0;
	pipe_->dirty_flags_extra |= DirtyFlag::SH_FC_Combine;
}

void BackEnd::SetYcbcrInverse(pisp_be_ccm_config const &ycbcr_inverse)
{
	pipe_->hw_config.ycbcr_inverse = ycbcr_inverse;
	pipe_->hw_config.ycbcr_inverse.pad[0] = pipe_->hw_config.ycbcr_inverse.pad[1] = 0;
	pipe_->dirty_flags_rgb |= RgbEnable::Ycbcr_inverse;
}

void BackEnd::SetGamma(pisp_be_gamma_config const &gamma)
{
	pipe_->hw_config.gamma = gamma;
	pipe_->dirty_flags_rgb |= RgbEnable::Gamma0;
}

void BackEnd::GetGamma(pisp_be_gamma_config &gamma) const
{
	gamma = pipe_->hw_config.gamma;
}

void BackEnd::SetCrop(pisp_be_crop_config const &crop)
{
	for (unsigned int i = 0; i < variant_.BackEndNumBranches(0); i++)
		pipe_->config_extra.crop[i] = crop;
	pipe_->dirty_flags_extra |= DirtyFlag::Crop;
	pipe_->retile_ = true;
}

void BackEnd::SetCrop(unsigned int i, pisp_be_crop_config const &crop)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	pipe_->config_extra.crop[i] = crop;
	pipe_->dirty_flags_extra |= DirtyFlag::Crop;
	pipe_->retile_ = true;
}

void BackEnd::SetCsc(unsigned int i, pisp_be_ccm_config const &csc)
{
	pipe_->hw_config.csc[i] = csc;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Post_csc0, i);
}

void BackEnd::GetCsc(unsigned int i, pisp_be_ccm_config &csc) const
{
	csc = pipe_->hw_config.csc[i];
}

void BackEnd::SetDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
						   pisp_be_downscale_extra const &downscale_extra)
{
	pipe_->hw_config.downscale[i] = downscale;
	pipe_->config_extra.downscale[i] = downscale_extra;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Downscale0, i);
	pipe_->retile_ = true;
}

void BackEnd::SetDownscale(unsigned int i, pisp_be_downscale_extra const &downscale_extra)
{
	pipe_->config_extra.downscale[i] = downscale_extra;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Downscale0, i);
	pipe_->retile_ = true;
}

void BackEnd::SetResample(unsigned int i, pisp_be_resample_config const &resample,
						  pisp_be_resample_extra const &resample_extra)
{
	pipe_->hw_config.resample[i] = resample;
	pipe_->config_extra.resample[i] = resample_extra;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Resample0, i);
	pipe_->retile_ = true;
}

void BackEnd::SetResample(unsigned int i, pisp_be_resample_extra const &resample_extra)
{
	pipe_->config_extra.resample[i] = resample_extra;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Resample0, i);
	pipe_->retile_ = true;
}

void BackEnd::SetOutputFormat(unsigned int i, pisp_be_output_format_config const &output_format)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	pipe_->hw_config.output_format[i] = output_format;
	pipe_->hw_config.output_format[i].rebayer = pipe_->hw_config.output_format[i].bayer_order = pipe_->hw_config.output_format[i].chroma_pos = 0;
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile_ = true;
}

void BackEnd::GetOutputFormat(unsigned int i, pisp_be_output_format_config &output_format) const
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	output_format = pipe_->hw_config.output_format[i];
}

void BackEnd::SetSmartResize(unsigned int i, BackEnd::SmartResize const &smart_resize)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	// Non-zero width and height will be interpreted as "enabled".
	pipe_->smart_resize[i] = smart_resize;
	pipe_->smart_resize_dirty |= (1 << i);
}

unsigned int BackEnd::GetMaxDownscale() const
{
	// Return an estimate of the largest horizontal downscale we can do.
	unsigned int max_tile_width = sw_.max_tile_width;
	if (!max_tile_width)
		max_tile_width = variant_.BackEndMaxTileWidth(0);

	// We reckon a 640 tile-width implementation can do 24x safely with formats that
	// want 1 byte per pixel. This should approximately scale with the max tile width.
	unsigned int downscale = 24;
	const unsigned int ref_tile_width = 640;
	return downscale * max_tile_width / ref_tile_width;
}
