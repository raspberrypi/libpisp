
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

BackEnd::BackEnd(SoftwareConfig const &sw_config, PiSPVariant const &variant, std::span<uint8_t> config_storage)
	: sw_(sw_config), variant_(variant)
{
	unsigned int max_tile_width = variant_.back_end_max_tile_width(0);

	if (sw_.max_tile_width > max_tile_width)
		PISP_LOG(fatal, "Configured max tile width " << sw_.max_tile_width << " exceeds " << max_tile_width);

	if (config_storage.data() && config_storage.size() >= sizeof(PipelineConfig))
		pipe_ = reinterpret_cast<PipelineConfig *>(config_storage.data());
	else
		pipe_ = &internal_pipeline_config_;

	if (variant < BERYL)
		config_ = std::make_unique<BackendConfigAdapter>(&pipe_->hw_config, pipe_->tiles.data(), &pipe_->config_extra);
	else
		config_ =
			std::make_unique<BackendConfigAdapter>(&pipe_->hw_config_v2, pipe_->tiles2.data(), &pipe_->config_extra);

	const char *env = std::getenv("LIBPISP_BE_CONFIG_FILE");
	initialise_default_config(env ? std::string(env) : sw_config.defaults_file);
}

BackEnd::~BackEnd()
{
}

void BackEnd::set_global(BeGlobal const &global)
{
	RgbEnables current_rgb_enables = config_->get_global().rgb_enables;
	BayerEnables current_bayer_enables = config_->get_global().bayer_enables;

	RgbEnables changed_rgb_enables = global.rgb_enables ^ current_rgb_enables;

	if (changed_rgb_enables & (RgbEnable::Downscale0 | RgbEnable::Downscale1 |
							 RgbEnable::Resample0 | RgbEnable::Resample1))
		pipe_->retile = true; // must retile when rescale change

	pipe_->dirty_flags_bayer |= (global.bayer_enables & ~current_bayer_enables); // label anything newly enabled as dirty
	pipe_->dirty_flags_rgb |= (global.rgb_enables & ~current_rgb_enables); // label anything newly enabled as dirty
	pipe_->dirty_flags_extra |= DirtyFlag::Global;
	config_->set_global(global);
}

BeGlobal BackEnd::get_global() const
{
	return config_->get_global();
}

void BackEnd::set_input_format(ImageFormatConfig const &input_format)
{
	config_->set_input_format(input_format);
	if (PISP_IMAGE_FORMAT_THREE_CHANNEL(input_format.format))
		pipe_->dirty_flags_bayer |= BayerEnable::Input;
	else
		pipe_->dirty_flags_rgb |= RgbEnable::Input;
	pipe_->retile = true;
}

ImageFormatConfig BackEnd::get_input_format() const
{
	return config_->get_input_format();
}

void BackEnd::set_decompress(DecompressConfig const &decompress)
{
	config_->set_decompress(decompress);
	pipe_->dirty_flags_bayer |= BayerEnable::Decompress;
}

DecompressConfig BackEnd::get_decompress() const
{
	return config_->get_decompress();
}

void BackEnd::set_dpc(BeDpcConfig const &dpc)
{
	config_->set_dpc(dpc);
	pipe_->dirty_flags_bayer |= BayerEnable::Dpc;
}

BeDpcConfig BackEnd::get_dpc() const
{
	return config_->get_dpc();
}

void BackEnd::set_geq(BeGeqConfig const &geq)
{
	config_->set_geq(geq);
	pipe_->dirty_flags_bayer |= BayerEnable::Geq;
}

BeGeqConfig BackEnd::get_geq() const
{
	return config_->get_geq();
}

void BackEnd::set_tdn_input_format(ImageFormatConfig const &tdn_input_format)
{
	config_->set_tdn_input_format(tdn_input_format);
	pipe_->dirty_flags_bayer |= BayerEnable::TdnInput; // TDN input address will always be written
	pipe_->finalise_tiling = true;
}

ImageFormatConfig BackEnd::get_tdn_input_format() const
{
	return config_->get_tdn_input_format();
}

void BackEnd::set_tdn_decompress(DecompressConfig const &tdn_decompress)
{
	config_->set_tdn_decompress(tdn_decompress);
	pipe_->dirty_flags_bayer |= BayerEnable::TdnDecompress;
}

DecompressConfig BackEnd::get_tdn_decompress() const
{
	return config_->get_tdn_decompress();
}

void BackEnd::set_tdn(BeTdnConfig const &tdn)
{
	config_->set_tdn(tdn);
	pipe_->dirty_flags_bayer |= BayerEnable::Tdn;
}

BeTdnConfig BackEnd::get_tdn() const
{
	return config_->get_tdn();
}

void BackEnd::set_tdn_compress(CompressConfig const &tdn_compress)
{
	config_->set_tdn_compress(tdn_compress);
	pipe_->dirty_flags_bayer |= BayerEnable::TdnCompress;
}

CompressConfig BackEnd::get_tdn_compress() const
{
	return config_->get_tdn_compress();
}

void BackEnd::set_tdn_output_format(ImageFormatConfig const &tdn_outputFormat)
{
	config_->set_tdn_output_format(tdn_outputFormat);
	pipe_->dirty_flags_bayer |= BayerEnable::TdnOutput; // TDN output address will always be written
	pipe_->finalise_tiling = true;
}

ImageFormatConfig BackEnd::get_tdn_output_format() const
{
	return config_->get_tdn_output_format();
}

void BackEnd::set_sdn(BeSdnConfig const &sdn)
{
	config_->set_sdn(sdn);
	pipe_->dirty_flags_bayer |= BayerEnable::Sdn;
}

BeSdnConfig BackEnd::get_sdn() const
{
	return config_->get_sdn();
}

void BackEnd::set_blc(BlaConfig const &blc)
{
	config_->set_blc(blc);
	pipe_->dirty_flags_bayer |= BayerEnable::Blc;
}

BlaConfig BackEnd::get_blc() const
{
	return config_->get_blc();
}

void BackEnd::set_stitch_input_format(ImageFormatConfig const &stitch_input_format)
{
	config_->set_stitch_input_format(stitch_input_format);
	// Note: stitch.pad clearing would need variant-specific handling
	pipe_->dirty_flags_bayer |= BayerEnable::StitchInput;
	pipe_->finalise_tiling = true;
}

ImageFormatConfig BackEnd::get_stitch_input_format() const
{
	return config_->get_stitch_input_format();
}

void BackEnd::set_stitch_decompress(DecompressConfig const &stitch_decompress)
{
	config_->set_stitch_decompress(stitch_decompress);
	pipe_->dirty_flags_bayer |= BayerEnable::StitchDecompress;
}

DecompressConfig BackEnd::get_stitch_decompress() const
{
	return config_->get_stitch_decompress();
}

void BackEnd::set_stitch(BeStitchConfig const &stitch)
{
	config_->set_stitch(stitch);
	pipe_->dirty_flags_bayer |= BayerEnable::Stitch;
}

BeStitchConfig BackEnd::get_stitch() const
{
	return config_->get_stitch();
}

void BackEnd::set_stitch_compress(CompressConfig const &stitch_compress)
{
	config_->set_stitch_compress(stitch_compress);
	pipe_->dirty_flags_bayer |= BayerEnable::StitchCompress;
}

CompressConfig BackEnd::get_stitch_compress() const
{
	return config_->get_stitch_compress();
}

void BackEnd::set_stitch_output_format(ImageFormatConfig const &stitch_outputFormat)
{
	config_->set_stitch_output_format(stitch_outputFormat);
	pipe_->dirty_flags_bayer |= BayerEnable::StitchOutput;
	pipe_->finalise_tiling = true;
}

ImageFormatConfig BackEnd::get_stitch_output_format() const
{
	return config_->get_stitch_output_format();
}

void BackEnd::set_cdn(BeCdnConfig const &cdn)
{
	config_->set_cdn(cdn);
	pipe_->dirty_flags_bayer |= BayerEnable::Cdn;
}

BeCdnConfig BackEnd::get_cdn() const
{
	return config_->get_cdn();
}

void BackEnd::set_wbg(WbgConfig const &wbg)
{
	config_->set_wbg(wbg);
	pipe_->dirty_flags_bayer |= BayerEnable::Wbg;
}

WbgConfig BackEnd::get_wbg() const
{
	return config_->get_wbg();
}

void BackEnd::set_lsc(BeLscConfig const &lsc)
{
	// Should not need a finalise_tile if only the cell coefficients have changed.
	pipe_->finalise_tiling |= config_->get_lsc().grid_step_x != lsc.grid_step_x ||
							config_->get_lsc().grid_step_y != lsc.grid_step_y;
	config_->set_lsc(lsc);
	pipe_->dirty_flags_bayer |= BayerEnable::Lsc;
}

BeLscConfig BackEnd::get_lsc() const
{
	return config_->get_lsc();
}

void BackEnd::set_cac(BeCacConfig const &cac)
{
	pipe_->finalise_tiling |= config_->get_cac().grid_step_x != cac.grid_step_x ||
							config_->get_cac().grid_step_y != cac.grid_step_y;
	config_->set_cac(cac);
	pipe_->dirty_flags_bayer |= BayerEnable::Cac;
}

BeCacConfig BackEnd::get_cac() const
{
	return config_->get_cac();
}

void BackEnd::set_debin(BeDebinConfig const &debin)
{
	config_->set_debin(debin);
	pipe_->dirty_flags_bayer |= BayerEnable::Debin;
}

BeDebinConfig BackEnd::get_debin() const
{
	return config_->get_debin();
}

void BackEnd::set_tonemap(BeTonemapConfig const &tonemap)
{
	config_->set_tonemap(tonemap);
	pipe_->dirty_flags_bayer |= BayerEnable::Tonemap;
}

BeTonemapConfig BackEnd::get_tonemap() const
{
	return config_->get_tonemap();
}

void BackEnd::set_tonemap2(BeTonemap2Config const &tonemap)
{
	config_->set_tonemap2(tonemap);
	pipe_->dirty_flags_bayer |= BayerEnable::Tonemap2;
}

BeTonemap2Config BackEnd::get_tonemap2() const
{
	return config_->get_tonemap2();
}

void BackEnd::set_tonemap2_format(ImageFormatConfig const &format)
{
	config_->set_tonemap2_format(format);
	pipe_->dirty_flags_bayer |= BayerEnable::Tonemap2Input;
}

ImageFormatConfig BackEnd::get_tonemap2_format() const
{
	return config_->get_tonemap2_format();
}

void BackEnd::set_demosaic(BeDemosaicConfig const &demosaic)
{
	config_->set_demosaic(demosaic);
	pipe_->dirty_flags_bayer |= BayerEnable::Demosaic;
}

BeDemosaicConfig BackEnd::get_demosaic() const
{
	return config_->get_demosaic();
}

void BackEnd::set_ccm(BeCcmConfig const &ccm)
{
	config_->set_ccm(ccm);
	pipe_->dirty_flags_rgb |= RgbEnable::Ccm;
}

BeCcmConfig BackEnd::get_ccm() const
{
	return config_->get_ccm();
}

void BackEnd::set_sat_control(BeSatControlConfig const &sat_control)
{
	config_->set_sat_control(sat_control);
	pipe_->dirty_flags_rgb |= RgbEnable::SatControl;
}

BeSatControlConfig BackEnd::get_sat_control() const
{
	return config_->get_sat_control();
}

void BackEnd::set_ycbcr(BeCcmConfig const &ycbcr)
{
	config_->set_ycbcr(ycbcr);
	pipe_->dirty_flags_rgb |= RgbEnable::Ycbcr;
}

BeCcmConfig BackEnd::get_ycbcr() const
{
	return config_->get_ycbcr();
}

void BackEnd::set_false_colour(BeFalseColourConfig const &false_colour)
{
	config_->set_false_colour(false_colour);
	pipe_->dirty_flags_rgb |= RgbEnable::FalseColour;
}

BeFalseColourConfig BackEnd::get_false_colour() const
{
	return config_->get_false_colour();
}

void BackEnd::set_sharpen(BeSharpenConfig const &sharpen)
{
	config_->set_sharpen(sharpen);
	pipe_->dirty_flags_rgb |= RgbEnable::Sharpen;
}

BeSharpenConfig BackEnd::get_sharpen() const
{
	return config_->get_sharpen();
}

void BackEnd::set_sh_fc_combine(BeShFcCombineConfig const &sh_fc_combine)
{
	config_->set_sh_fc_combine(sh_fc_combine);
	pipe_->dirty_flags_extra |= DirtyFlag::ShFcCombine;
}

BeShFcCombineConfig BackEnd::get_sh_fc_combine() const
{
	return config_->get_sh_fc_combine();
}

void BackEnd::set_ycbcr_inverse(BeCcmConfig const &ycbcr_inverse)
{
	config_->set_ycbcr_inverse(ycbcr_inverse);
	pipe_->dirty_flags_rgb |= RgbEnable::YcbcrInverse;
}

BeCcmConfig BackEnd::get_ycbcr_inverse() const
{
	return config_->get_ycbcr_inverse();
}

void BackEnd::set_gamma(BeGammaConfig const &gamma)
{
	set_gamma(0, gamma);
}

void BackEnd::set_gamma(unsigned int i, BeGammaConfig const &gamma)
{
	config_->set_gamma(i, gamma);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Gamma0, i);
}

BeGammaConfig BackEnd::get_gamma(unsigned int i) const
{
	return config_->get_gamma(i);
}

void BackEnd::set_crop(BeCropConfig const &crop)
{
	for (unsigned int i = 0; i < variant_.back_end_num_branches(0); i++)
		pipe_->config_extra.crop[i] = crop;
	pipe_->dirty_flags_extra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

BeCropConfig BackEnd::get_crop() const
{
	return pipe_->config_extra.crop[0];
}

void BackEnd::set_crop(unsigned int i, BeCropConfig const &crop)
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	pipe_->config_extra.crop[i] = crop;
	pipe_->dirty_flags_extra |= DirtyFlag::Crop;
	pipe_->retile = true;
}

BeCropConfig BackEnd::get_crop(unsigned int i) const
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	return pipe_->config_extra.crop[i];
}

void BackEnd::set_pre_csc(unsigned int i, BeCcmConfig const &csc)
{
	config_->set_pre_csc(i, csc);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::PostCsc0, i);
}

BeCcmConfig BackEnd::get_pre_csc(unsigned int i) const
{
	return config_->get_pre_csc(i);
}

void BackEnd::set_post_csc(unsigned int i, BeCcmConfig const &csc)
{
	config_->set_post_csc(i, csc);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::PostCsc0, i);
}

BeCcmConfig BackEnd::get_post_csc(unsigned int i) const
{
	return config_->get_post_csc(i);
}

void BackEnd::set_downscale(unsigned int i, BeDownscaleConfig const &downscale)
{
	config_->set_downscale(i, downscale);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Downscale0, i);
	pipe_->retile = true;
}

BeDownscaleConfig BackEnd::get_downscale(unsigned int i) const
{
	return config_->get_downscale(i);
}

void BackEnd::set_resample(unsigned int i, BeResampleConfig const &resample)
{
	config_->set_resample(i, resample);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Resample0, i);
	pipe_->retile = true;
}

BeResampleConfig BackEnd::get_resample(unsigned int i) const
{
	return config_->get_resample(i);
}

void BackEnd::set_output_format(unsigned int i, BeOutputFormatConfig const &output_format)
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	config_->set_output_format(i, output_format);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Output0, i);
	// Should only need a retile if the transform has changed, othwise a finalise_tile will do.
	pipe_->retile = true;
}

BeOutputFormatConfig BackEnd::get_output_format(unsigned int i) const
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	return config_->get_output_format(i);
}

void BackEnd::set_smart_resize(unsigned int i, BackEnd::SmartResize const &smart_resize)
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	// Non-zero width and height will be interpreted as "enabled".
	pipe_->smart_resize[i] = smart_resize;
	pipe_->smart_resize_dirty |= (1 << i);
}

void BackEnd::set_dedistort(const BeDedistortionConfig &dedistort)
{
	config_->set_dedistort(dedistort);
	pipe_->dirty_flags_rgb |= RgbEnable::Dedistort;
	pipe_->retile = true;
}

void BackEnd::set_output_compress(unsigned int i, const BeCompressConfig &compress)
{
	PISP_ASSERT(i < variant_.back_end_num_branches(0));
	config_->set_output_compress(i, compress);
	pipe_->dirty_flags_rgb |= get_flag_for_branch(RgbEnable::Compress0, i);
	// No need to retile, since outputs must in all cases meet min alignment of 16.
}

BeCompressConfig BackEnd::get_output_compress(unsigned int i) const
{
	return config_->get_output_compress(i);
}

unsigned int BackEnd::get_max_downscale() const
{
	// Return an estimate of the largest horizontal downscale we can do.
	unsigned int max_tile_width = sw_.max_tile_width;
	if (!max_tile_width)
		max_tile_width = variant_.back_end_max_tile_width(0);

	// We reckon a 640 tile-width implementation can do 24x safely with formats that
	// want 1 byte per pixel. This should approximately scale with the max tile width.
	unsigned int downscale = 24;
	const unsigned int ref_tile_width = 640;
	return downscale * max_tile_width / ref_tile_width;
}
