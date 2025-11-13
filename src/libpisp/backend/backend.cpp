
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
	return sizeof(HardwareConfig);
}

BackEnd::BackEnd(SoftwareConfig const &sw_config, PiSPVariant const &variant, std::span<uint8_t> config_storage)
	: sw_(sw_config), variant_(variant)
{
	unsigned int max_tile_width = variant_.BackEndMaxTileWidth(0);

	if (sw_.max_tile_width > max_tile_width)
		PISP_LOG(fatal, "Configured max tile width " << sw_.max_tile_width << " exceeds " << max_tile_width);

	if (config_storage.data() && config_storage.size() >= sizeof(HardwareConfig))
		hw_ = reinterpret_cast<HardwareConfig *>(config_storage.data());
	else
		hw_ = &internal_hw_config_;

	// Initialize hardware config
	hw_->smart_resize.fill({ 0, 0 });
	hw_->smart_resize_dirty = 0;
	hw_->retile_ = true;
	hw_->finalise_tiling = true;

	memset(&hw_->config, 0, sizeof(hw_->config));
	memset(&hw_->config_extra, 0, sizeof(hw_->config_extra));

	const char *env = std::getenv("LIBPISP_BE_CONFIG_FILE");
	initialiseDefaultConfig(env ? std::string(env) : sw_config.defaults_file);
}

BackEnd::~BackEnd()
{
}

void BackEnd::SetGlobal(pisp_be_global_config const &global)
{
	uint32_t changed_rgb_enables = (global.rgb_enables ^ hw_->config.global.rgb_enables);

	if (changed_rgb_enables & (PISP_BE_RGB_ENABLE_DOWNSCALE0 | PISP_BE_RGB_ENABLE_DOWNSCALE1 |
							   PISP_BE_RGB_ENABLE_RESAMPLE0 | PISP_BE_RGB_ENABLE_RESAMPLE1))
		hw_->retile_ = true; // must retile when rescale change

	if (global.rgb_enables & PISP_BE_RGB_ENABLE_HOG)
		throw std::runtime_error("HOG output is not supported.");

	hw_->config_extra.dirty_flags_bayer |=
		(global.bayer_enables & ~hw_->config.global.bayer_enables); // label anything newly enabled as dirty
	hw_->config_extra.dirty_flags_rgb |=
		(global.rgb_enables & ~hw_->config.global.rgb_enables); // label anything newly enabled as dirty
	hw_->config.global = global;
	hw_->config.global.pad[0] = hw_->config.global.pad[1] = hw_->config.global.pad[2] = 0;
	hw_->config_extra.dirty_flags_extra |= PISP_BE_DIRTY_GLOBAL;
}

void BackEnd::GetGlobal(pisp_be_global_config &global) const
{
	global = hw_->config.global;
}

void BackEnd::SetInputFormat(pisp_image_format_config const &input_format)
{
	hw_->config.input_format = input_format;
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(input_format.format))
		hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_INPUT;
	else
		hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_INPUT;
	hw_->retile_ = true;
}

void BackEnd::GetInputFormat(pisp_image_format_config &input_format) const
{
	input_format = hw_->config.input_format;
}

void BackEnd::SetDecompress(pisp_decompress_config const &decompress)
{
	hw_->config.decompress = decompress;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DECOMPRESS;
}

void BackEnd::SetDpc(pisp_be_dpc_config const &dpc)
{
	hw_->config.dpc = dpc;
	hw_->config.dpc.pad = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DPC;
}

void BackEnd::SetGeq(pisp_be_geq_config const &geq)
{
	hw_->config.geq = geq;
	hw_->config.geq.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_GEQ;
}

void BackEnd::SetTdnInputFormat(pisp_image_format_config const &tdn_input_format)
{
	hw_->config.tdn_input_format = tdn_input_format;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TDN_INPUT; // TDN input address will always be written
	hw_->finalise_tiling = true;
}

void BackEnd::GetTdnInputFormat(pisp_image_format_config &tdn_input_format) const
{
	tdn_input_format = hw_->config.tdn_input_format;
}

void BackEnd::SetTdnDecompress(pisp_decompress_config const &tdn_decompress)
{
	hw_->config.tdn_decompress = tdn_decompress;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS;
}

void BackEnd::SetTdn(pisp_be_tdn_config const &tdn)
{
	hw_->config.tdn = tdn;
	hw_->config.tdn.pad = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TDN;
}

void BackEnd::GetTdn(pisp_be_tdn_config &tdn) const
{
	tdn = hw_->config.tdn;
}

void BackEnd::SetTdnCompress(pisp_compress_config const &tdn_compress)
{
	hw_->config.tdn_compress = tdn_compress;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TDN_COMPRESS;
}

void BackEnd::SetTdnOutputFormat(pisp_image_format_config const &tdn_output_format)
{
	hw_->config.tdn_output_format = tdn_output_format;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TDN_OUTPUT; // TDN output address will always be written
	hw_->finalise_tiling = true;
}

void BackEnd::GetTdnOutputFormat(pisp_image_format_config &tdn_output_format) const
{
	tdn_output_format = hw_->config.tdn_output_format;
}

void BackEnd::SetSdn(pisp_be_sdn_config const &sdn)
{
	hw_->config.sdn = sdn;
	hw_->config.sdn.pad = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_SDN;
}

void BackEnd::SetBlc(pisp_bla_config const &blc)
{
	hw_->config.blc = blc;
	hw_->config.blc.pad[0] = hw_->config.blc.pad[1] = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_BLC;
}

void BackEnd::GetBlc(pisp_bla_config &blc) const
{
	blc = hw_->config.blc;
}

void BackEnd::SetStitchInputFormat(pisp_image_format_config const &stitch_input_format)
{
	hw_->config.stitch_input_format = stitch_input_format;
	hw_->config.stitch.pad = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_STITCH_INPUT;
	hw_->finalise_tiling = true;
}

void BackEnd::GetStitchInputFormat(pisp_image_format_config &stitch_input_format) const
{
	stitch_input_format = hw_->config.stitch_input_format;
}

void BackEnd::SetStitchDecompress(pisp_decompress_config const &stitch_decompress)
{
	hw_->config.stitch_decompress = stitch_decompress;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS;
}

void BackEnd::SetStitch(pisp_be_stitch_config const &stitch)
{
	hw_->config.stitch = stitch;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_STITCH;
}

void BackEnd::SetStitchCompress(pisp_compress_config const &stitch_compress)
{
	hw_->config.stitch_compress = stitch_compress;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_STITCH_COMPRESS;
}

void BackEnd::SetStitchOutputFormat(pisp_image_format_config const &stitch_output_format)
{
	hw_->config.stitch_output_format = stitch_output_format;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_STITCH_OUTPUT;
	hw_->finalise_tiling = true;
}

void BackEnd::GetStitchOutputFormat(pisp_image_format_config &stitch_output_format) const
{
	stitch_output_format = hw_->config.stitch_output_format;
}

void BackEnd::SetCdn(pisp_be_cdn_config const &cdn)
{
	hw_->config.cdn = cdn;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_CDN;
}

void BackEnd::SetWbg(pisp_wbg_config const &wbg)
{
	hw_->config.wbg = wbg;
	hw_->config.wbg.pad[0] = hw_->config.wbg.pad[1] = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_WBG;
}

void BackEnd::GetWbg(pisp_wbg_config &wbg) const
{
	wbg = hw_->config.wbg;
}

void BackEnd::SetLsc(pisp_be_lsc_config const &lsc, pisp_be_lsc_extra lsc_extra)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	hw_->finalise_tiling |= hw_->config.lsc.grid_step_x != lsc.grid_step_x ||
							hw_->config.lsc.grid_step_y != lsc.grid_step_y;
	hw_->config.lsc = lsc;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_LSC;
	hw_->config_extra.lsc = lsc_extra;
}

void BackEnd::SetCac(pisp_be_cac_config const &cac, pisp_be_cac_extra cac_extra)
{
	hw_->finalise_tiling |= hw_->config.cac.grid_step_x != cac.grid_step_x ||
							hw_->config.cac.grid_step_y != cac.grid_step_y;
	hw_->config.cac = cac;
	hw_->config_extra.cac = cac_extra;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_CAC;
}

void BackEnd::SetDebin(pisp_be_debin_config const &debin)
{
	hw_->config.debin = debin;
	hw_->config.debin.pad[0] = hw_->config.debin.pad[1] = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEBIN;
}

void BackEnd::GetDebin(pisp_be_debin_config &debin) const
{
	debin = hw_->config.debin;
}

void BackEnd::SetTonemap(pisp_be_tonemap_config const &tonemap)
{
	hw_->config.tonemap = tonemap;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_TONEMAP;
}

void BackEnd::SetDemosaic(pisp_be_demosaic_config const &demosaic)
{
	hw_->config.demosaic = demosaic;
	hw_->config.demosaic.pad[0] = hw_->config.demosaic.pad[1] = 0;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEMOSAIC;
}

void BackEnd::GetDemosaic(pisp_be_demosaic_config &demosaic) const
{
	demosaic = hw_->config.demosaic;
}

void BackEnd::SetCcm(pisp_be_ccm_config const &ccm)
{
	hw_->config.ccm = ccm;
	hw_->config.ccm.pad[0] = hw_->config.ccm.pad[1] = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_CCM;
}

void BackEnd::SetSatControl(pisp_be_sat_control_config const &sat_control)
{
	hw_->config.sat_control = sat_control;
	hw_->config.sat_control.pad = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_SAT_CONTROL;
}

void BackEnd::SetYcbcr(pisp_be_ccm_config const &ycbcr)
{
	hw_->config.ycbcr = ycbcr;
	hw_->config.ycbcr.pad[0] = hw_->config.ycbcr.pad[1] = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_YCBCR;
}

void BackEnd::GetYcbcr(pisp_be_ccm_config &ycbcr) const
{
	ycbcr = hw_->config.ycbcr;
}

void BackEnd::SetFalseColour(pisp_be_false_colour_config const &false_colour)
{
	hw_->config.false_colour = false_colour;
	hw_->config.false_colour.pad[0] = hw_->config.false_colour.pad[1] = hw_->config.false_colour.pad[2] = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_FALSE_COLOUR;
}

void BackEnd::SetSharpen(pisp_be_sharpen_config const &sharpen)
{
	hw_->config.sharpen = sharpen;
	hw_->config.sharpen.pad0[0] = hw_->config.sharpen.pad0[1] = hw_->config.sharpen.pad0[2] = 0;
	hw_->config.sharpen.pad1[0] = hw_->config.sharpen.pad1[1] = hw_->config.sharpen.pad1[2] = 0;
	hw_->config.sharpen.pad2[0] = hw_->config.sharpen.pad2[1] = hw_->config.sharpen.pad2[2] = 0;
	hw_->config.sharpen.pad3[0] = hw_->config.sharpen.pad3[1] = hw_->config.sharpen.pad3[2] = 0;
	hw_->config.sharpen.pad4[0] = hw_->config.sharpen.pad4[1] = hw_->config.sharpen.pad4[2] = 0;
	hw_->config.sharpen.pad5 = hw_->config.sharpen.pad6 = hw_->config.sharpen.pad7 = hw_->config.sharpen.pad8 =
		hw_->config.sharpen.pad9 = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_SHARPEN;
}

void BackEnd::GetSharpen(pisp_be_sharpen_config &sharpen) const
{
	sharpen = hw_->config.sharpen;
}

void BackEnd::SetShFcCombine(pisp_be_sh_fc_combine_config const &sh_fc_combine)
{
	hw_->config.sh_fc_combine = sh_fc_combine;
	hw_->config.sh_fc_combine.pad = 0;
	hw_->config_extra.dirty_flags_extra |= PISP_BE_DIRTY_SH_FC_COMBINE;
}

void BackEnd::SetYcbcrInverse(pisp_be_ccm_config const &ycbcr_inverse)
{
	hw_->config.ycbcr_inverse = ycbcr_inverse;
	hw_->config.ycbcr_inverse.pad[0] = hw_->config.ycbcr_inverse.pad[1] = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_YCBCR_INVERSE;
}

void BackEnd::SetGamma(pisp_be_gamma_config const &gamma)
{
	hw_->config.gamma = gamma;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_GAMMA;
}

void BackEnd::GetGamma(pisp_be_gamma_config &gamma) const
{
	gamma = hw_->config.gamma;
}

void BackEnd::SetCrop(pisp_be_crop_config const &crop)
{
	for (unsigned int i = 0; i < variant_.BackEndNumBranches(0); i++)
		hw_->config_extra.crop[i] = crop;
	hw_->config_extra.dirty_flags_extra |= PISP_BE_DIRTY_CROP;
	hw_->retile_ = true;
}

void BackEnd::SetCrop(unsigned int i, pisp_be_crop_config const &crop)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	hw_->config_extra.crop[i] = crop;
	hw_->config_extra.dirty_flags_extra |= PISP_BE_DIRTY_CROP;
	hw_->retile_ = true;
}

void BackEnd::SetCsc(unsigned int i, pisp_be_ccm_config const &csc)
{
	hw_->config.csc[i] = csc;
	hw_->config_extra.dirty_flags_bayer |= PISP_BE_RGB_ENABLE_CSC(i);
}

void BackEnd::GetCsc(unsigned int i, pisp_be_ccm_config &csc) const
{
	csc = hw_->config.csc[i];
}

void BackEnd::SetDownscale(unsigned int i, pisp_be_downscale_config const &downscale,
						   pisp_be_downscale_extra const &downscale_extra)
{
	hw_->config.downscale[i] = downscale;
	hw_->config_extra.downscale[i] = downscale_extra;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_DOWNSCALE(i);
	hw_->retile_ = true;
}

void BackEnd::SetDownscale(unsigned int i, pisp_be_downscale_extra const &downscale_extra)
{
	hw_->config_extra.downscale[i] = downscale_extra;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_DOWNSCALE(i);
	hw_->retile_ = true;
}

void BackEnd::SetResample(unsigned int i, pisp_be_resample_config const &resample,
						  pisp_be_resample_extra const &resample_extra)
{
	hw_->config.resample[i] = resample;
	hw_->config_extra.resample[i] = resample_extra;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_RESAMPLE(i);
	hw_->retile_ = true;
}

void BackEnd::SetResample(unsigned int i, pisp_be_resample_extra const &resample_extra)
{
	hw_->config_extra.resample[i] = resample_extra;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_RESAMPLE(i);
	hw_->retile_ = true;
}

void BackEnd::SetOutputFormat(unsigned int i, pisp_be_output_format_config const &output_format)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	hw_->config.output_format[i] = output_format;
	hw_->config.output_format[i].rebayer = hw_->config.output_format[i].bayer_order = hw_->config.output_format[i].chroma_pos = 0;
	hw_->config_extra.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_OUTPUT(i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	hw_->retile_ = true;
}

void BackEnd::GetOutputFormat(unsigned int i, pisp_be_output_format_config &output_format) const
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	output_format = hw_->config.output_format[i];
}

void BackEnd::SetSmartResize(unsigned int i, BackEnd::SmartResize const &smart_resize)
{
	PISP_ASSERT(i < variant_.BackEndNumBranches(0));
	// Non-zero width and height will be interpreted as "enabled".
	hw_->smart_resize[i] = smart_resize;
	hw_->smart_resize_dirty |= (1 << i);
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
