/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */

#include "backend_config_adapter.hpp"

#include <array>
#include <cstring>

using namespace libpisp;

namespace
{

template <EnumType E>
struct EnableMapping
{
	E enable;
	uint32_t v1;
	uint32_t v2;
};

constexpr auto rgb_enable_map = std::to_array<EnableMapping<RgbEnable>>(
{
	{ RgbEnable::Input,			PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT },
	{ RgbEnable::Dedistort,		0,									PISP_BE_RGB_ENABLE2_DEDISTORT },
	{ RgbEnable::Ccm,			PISP_BE_RGB_ENABLE_CCM,				PISP_BE_RGB_ENABLE2_CCM },
	{ RgbEnable::SatControl,	PISP_BE_RGB_ENABLE_SAT_CONTROL,		PISP_BE_RGB_ENABLE2_SAT_CONTROL },
	{ RgbEnable::Ycbcr,			PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR },
	{ RgbEnable::FalseColour,	PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR },
	{ RgbEnable::Sharpen,		PISP_BE_RGB_ENABLE_SHARPEN,			PISP_BE_RGB_ENABLE2_SHARPEN },
	{ RgbEnable::PrefColours,	0,									PISP_BE_RGB_ENABLE2_PREF_COLOURS },
	{ RgbEnable::YcbcrInverse,	PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE },
	{ RgbEnable::PreCsc0,		0,									PISP_BE_RGB_ENABLE2_PRE_CSC0 },
	{ RgbEnable::PreCsc1,		0,									PISP_BE_RGB_ENABLE2_PRE_CSC1 },
	{ RgbEnable::Gamma0,		PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 },
	{ RgbEnable::Gamma1,		0,									PISP_BE_RGB_ENABLE2_GAMMA1 },
	{ RgbEnable::PostCsc0,		PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_POST_CSC0 },
	{ RgbEnable::PostCsc1,		PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_POST_CSC1 },
	{ RgbEnable::Downscale0,	PISP_BE_RGB_ENABLE_DOWNSCALE0,		PISP_BE_RGB_ENABLE2_DOWNSCALE0 },
	{ RgbEnable::Downscale1,	PISP_BE_RGB_ENABLE_DOWNSCALE1,		PISP_BE_RGB_ENABLE2_DOWNSCALE1 },
	{ RgbEnable::Resample0,		PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 },
	{ RgbEnable::Resample1,		PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 },
	{ RgbEnable::Output0,		PISP_BE_RGB_ENABLE_OUTPUT0,			PISP_BE_RGB_ENABLE2_OUTPUT0 },
	{ RgbEnable::Output1,		PISP_BE_RGB_ENABLE_OUTPUT1,			PISP_BE_RGB_ENABLE2_OUTPUT1 },
	{ RgbEnable::Compress0,		0,									PISP_BE_RGB_ENABLE2_COMPRESS0 },
	{ RgbEnable::Compress1,		0,									PISP_BE_RGB_ENABLE2_COMPRESS1 },
});

constexpr auto bayer_enable_map = std::to_array<EnableMapping<BayerEnable>>(
{
	{ BayerEnable::Input,				PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT },
	{ BayerEnable::Decompress,			PISP_BE_BAYER_ENABLE_DECOMPRESS,		PISP_BE_BAYER_ENABLE_DECOMPRESS },
	{ BayerEnable::Dpc,					PISP_BE_BAYER_ENABLE_DPC,				PISP_BE_BAYER_ENABLE_DPC },
	{ BayerEnable::Geq,					PISP_BE_BAYER_ENABLE_GEQ,				PISP_BE_BAYER_ENABLE_GEQ },
	{ BayerEnable::TdnInput,			PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT },
	{ BayerEnable::TdnDecompress,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,	PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS },
	{ BayerEnable::Tdn,					PISP_BE_BAYER_ENABLE_TDN,				PISP_BE_BAYER_ENABLE_TDN },
	{ BayerEnable::TdnCompress,			PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS },
	{ BayerEnable::TdnOutput,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT,		PISP_BE_BAYER_ENABLE_TDN_OUTPUT },
	{ BayerEnable::Sdn,					PISP_BE_BAYER_ENABLE_SDN,				PISP_BE_BAYER_ENABLE_SDN },
	{ BayerEnable::Blc,					PISP_BE_BAYER_ENABLE_BLC,				PISP_BE_BAYER_ENABLE_BLC },
	{ BayerEnable::StitchInput,			PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT },
	{ BayerEnable::StitchDecompress,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS },
	{ BayerEnable::Stitch,				PISP_BE_BAYER_ENABLE_STITCH,			PISP_BE_BAYER_ENABLE_STITCH },
	{ BayerEnable::StitchCompress,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_COMPRESS },
	{ BayerEnable::StitchOutput,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT },
	{ BayerEnable::Wbg,					PISP_BE_BAYER_ENABLE_WBG,				PISP_BE_BAYER_ENABLE_WBG },
	{ BayerEnable::Cdn,					PISP_BE_BAYER_ENABLE_CDN,				PISP_BE_BAYER_ENABLE_CDN },
	{ BayerEnable::Lsc,					PISP_BE_BAYER_ENABLE_LSC,				PISP_BE_BAYER_ENABLE_LSC },
	{ BayerEnable::Tonemap,				PISP_BE_BAYER_ENABLE_TONEMAP,			0 },
	{ BayerEnable::Tonemap2,			0,										PISP_BE_BAYER_ENABLE_TONEMAP },
	{ BayerEnable::Cac,					PISP_BE_BAYER_ENABLE_CAC,				PISP_BE_BAYER_ENABLE_CAC },
	{ BayerEnable::Debin,				PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN },
	{ BayerEnable::Demosaic,			PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC },
	{ BayerEnable::TonemapInput,		0,										PISP_BE_BAYER_ENABLE_TONEMAP_INPUT },
	{ BayerEnable::Transpose,			PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE },
});

// A zero HW bit in the selected column means the block does not exist on that version.
template <bool V1, EnumType E, std::size_t N>
uint32_t enables_to_hw(Bitfield<E> enables, const std::array<EnableMapping<E>, N> &map)
{
	uint32_t hw_enables = 0;

	for (auto const &m : map)
		if (enables & m.enable)
			hw_enables |= V1 ? m.v1 : m.v2;

	return hw_enables;
}

template <bool V1, EnumType E, std::size_t N>
Bitfield<E> hw_to_enables(uint32_t hw_enables, const std::array<EnableMapping<E>, N> &map)
{
	Bitfield<E> enables = E::None;

	for (auto const &m : map)
		if (hw_enables & (V1 ? m.v1 : m.v2))
			enables |= m.enable;

	return enables;
}

// Untyped pointers to array fields, so that BeTile (std::array) and the packed HW tiles (C arrays) can be
// passed to memcpy interchangeably without taking typed pointers to packed members.
template <typename T, std::size_t N>
void *raw(std::array<T, N> &a)
{
	return a.data();
}

template <typename T, std::size_t N>
const void *raw(const std::array<T, N> &a)
{
	return a.data();
}

void *raw(void *p)
{
	return p;
}

const void *raw(const void *p)
{
	return p;
}

// Copy tile fields between a BeTile and a HW tile, in either direction.
template <typename Dst, typename Src>
void copy_tile(Dst &d, const Src &s)
{
	using HwTile = std::conditional_t<std::is_same_v<Dst, BeTile>, Src, Dst>;
	static_assert(std::is_same_v<HwTile, pisp_tile> || std::is_same_v<HwTile, pisp_tile2>);

	// Common fields
	d.input_addr_offset = s.input_addr_offset;
	d.input_addr_offset2 = s.input_addr_offset2;
	d.input_offset_x = s.input_offset_x;
	d.input_offset_y = s.input_offset_y;
	d.input_width = s.input_width;
	d.input_height = s.input_height;
	d.tdn_input_addr_offset = s.tdn_input_addr_offset;
	d.tdn_output_addr_offset = s.tdn_output_addr_offset;
	d.stitch_input_addr_offset = s.stitch_input_addr_offset;
	d.stitch_output_addr_offset = s.stitch_output_addr_offset;
	d.lsc_grid_offset_x = s.lsc_grid_offset_x;
	d.lsc_grid_offset_y = s.lsc_grid_offset_y;
	d.cac_grid_offset_x = s.cac_grid_offset_x;
	d.cac_grid_offset_y = s.cac_grid_offset_y;
	std::memcpy(raw(d.crop_x_start), raw(s.crop_x_start), sizeof(BeTile::crop_x_start));
	std::memcpy(raw(d.crop_y_start), raw(s.crop_y_start), sizeof(BeTile::crop_y_start));
	std::memcpy(raw(d.resample_in_width), raw(s.resample_in_width), sizeof(BeTile::resample_in_width));
	std::memcpy(raw(d.resample_in_height), raw(s.resample_in_height), sizeof(BeTile::resample_in_height));
	std::memcpy(raw(d.output_offset_x), raw(s.output_offset_x), sizeof(BeTile::output_offset_x));
	std::memcpy(raw(d.output_offset_y), raw(s.output_offset_y), sizeof(BeTile::output_offset_y));
	std::memcpy(raw(d.output_width), raw(s.output_width), sizeof(BeTile::output_width));
	std::memcpy(raw(d.output_height), raw(s.output_height), sizeof(BeTile::output_height));
	std::memcpy(raw(d.output_addr_offset), raw(s.output_addr_offset), sizeof(BeTile::output_addr_offset));
	std::memcpy(raw(d.output_addr_offset2), raw(s.output_addr_offset2), sizeof(BeTile::output_addr_offset2));

	if constexpr (std::is_same_v<HwTile, pisp_tile>)
	{
		// v1 specific fields
		d.edge = s.edge;
		std::memcpy(raw(d.crop_x_end), raw(s.crop_x_end), sizeof(BeTile::crop_x_end));
		std::memcpy(raw(d.crop_y_end), raw(s.crop_y_end), sizeof(BeTile::crop_y_end));
		std::memcpy(raw(d.downscale_phase_x), raw(s.downscale_phase_x), sizeof(BeTile::downscale_phase_x));
		std::memcpy(raw(d.downscale_phase_y), raw(s.downscale_phase_y), sizeof(BeTile::downscale_phase_y));
		std::memcpy(raw(d.resample_phase_x), raw(s.resample_phase_x), sizeof(BeTile::resample_phase_x));
		std::memcpy(raw(d.resample_phase_y), raw(s.resample_phase_y), sizeof(BeTile::resample_phase_y));
	}
	else
	{
		// v2 specific fields
		d.tonemap_grid_offset_x = s.tonemap_grid_offset_x;
		d.tonemap_grid_offset_y = s.tonemap_grid_offset_y;
		d.tonemap_lores_offset_x = s.tonemap_lores_offset_x;
		d.tonemap_lores_offset_y = s.tonemap_lores_offset_y;
		d.dedistort_grid_offset_x = s.dedistort_grid_offset_x;
		d.dedistort_grid_offset_y = s.dedistort_grid_offset_y;
		d.dedistorted_offset_x = s.dedistorted_offset_x;
		d.dedistorted_width = s.dedistorted_width;
		std::memcpy(raw(d.crop_width), raw(s.crop_width), sizeof(BeTile::crop_width));
		std::memcpy(raw(d.crop_height), raw(s.crop_height), sizeof(BeTile::crop_height));
		std::memcpy(raw(d.resample_start_context_x), raw(s.resample_start_context_x),
					sizeof(BeTile::resample_start_context_x));
		std::memcpy(raw(d.resample_start_context_y), raw(s.resample_start_context_y),
					sizeof(BeTile::resample_start_context_y));

		// v2 has per-output downscale/resample phases, BeTile has per-component, so only the first
		// PISP_BACK_END_NUM_OUTPUTS entries are used.
		for (unsigned int j = 0; j < PISP_BACK_END_NUM_OUTPUTS; j++)
		{
			d.downscale_phase_x[j] = s.downscale_phase_x[j];
			d.downscale_phase_y[j] = s.downscale_phase_y[j];
			d.resample_phase_x[j] = s.resample_phase_x[j];
			d.resample_phase_y[j] = s.resample_phase_y[j];
		}
	}
}

} // namespace

void BackendConfigAdapter::set_tile(unsigned int i, const BeTile &tile)
{
	visit_tiles([&tile, i](auto *tiles) { copy_tile(tiles[i], tile); });
}

BeTile BackendConfigAdapter::get_tile(unsigned int i) const
{
	BeTile tile {};

	visit_tiles([&tile, i](auto *tiles) { copy_tile(tile, tiles[i]); });

	return tile;
}

void BackendConfigAdapter::set_global(const BeGlobal &global)
{
	visit([&global](auto *cfg) {
		constexpr bool v1 = is_v1<decltype(cfg)>;
		cfg->global.bayer_enables = enables_to_hw<v1>(global.bayer_enables, bayer_enable_map);
		cfg->global.rgb_enables = enables_to_hw<v1>(global.rgb_enables, rgb_enable_map);
		cfg->global.bayer_order = global.bayer_order;
		cfg->global.quad_bayer = !v1 && global.quad_bayer;
		cfg->global.pad[0] = cfg->global.pad[1] = 0;
	});
}

BeGlobal BackendConfigAdapter::get_global() const
{
	BeGlobal global;

	visit([&global](auto *cfg) {
		constexpr bool v1 = is_v1<decltype(cfg)>;
		global.bayer_enables = hw_to_enables<v1>(cfg->global.bayer_enables, bayer_enable_map);
		global.rgb_enables = hw_to_enables<v1>(cfg->global.rgb_enables, rgb_enable_map);
		global.bayer_order = cfg->global.bayer_order;
		global.quad_bayer = !v1 && cfg->global.quad_bayer;
	});

	return global;
}

// Common field implementations (same type in both variants)
void BackendConfigAdapter::set_input_format(const ImageFormatConfig &input_format)
{
	visit([&input_format](auto *cfg) { cfg->input_format = input_format; });
}

ImageFormatConfig BackendConfigAdapter::get_input_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->input_format; });
}

void BackendConfigAdapter::set_decompress(const DecompressConfig &decompress)
{
	visit([&decompress](auto *cfg) { cfg->decompress = decompress; });
}

DecompressConfig BackendConfigAdapter::get_decompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->decompress; });
}

void BackendConfigAdapter::set_dpc(const BeDpcConfig &dpc)
{
	visit([&dpc](auto *cfg) {
		cfg->dpc = dpc;
		cfg->dpc.pad = 0;
	});
}

BeDpcConfig BackendConfigAdapter::get_dpc() const
{
	return visit([](auto *cfg) -> BeDpcConfig { return cfg->dpc; });
}

void BackendConfigAdapter::set_geq(const BeGeqConfig &geq)
{
	visit([&geq](auto *cfg)
		  {
			cfg->geq = geq;
			cfg->geq.slope_sharper &= (PISP_BE_GEQ_SLOPE | PISP_BE_GEQ_SHARPER);
		  });
}

BeGeqConfig BackendConfigAdapter::get_geq() const
{
	return visit([](auto *cfg) -> BeGeqConfig { return cfg->geq; });
}

void BackendConfigAdapter::set_tdn_input_format(const ImageFormatConfig &tdn_input_format)
{
	visit([&tdn_input_format](auto *cfg) { cfg->tdn_input_format = tdn_input_format; });
}

ImageFormatConfig BackendConfigAdapter::get_tdn_input_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_input_format; });
}

void BackendConfigAdapter::set_tdn_decompress(const DecompressConfig &tdn_decompress)
{
	visit([&tdn_decompress](auto *cfg) { cfg->tdn_decompress = tdn_decompress; });
}

DecompressConfig BackendConfigAdapter::get_tdn_decompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->tdn_decompress; });
}

void BackendConfigAdapter::set_tdn_compress(const CompressConfig &tdn_compress)
{
	visit([&tdn_compress](auto *cfg) { cfg->tdn_compress = tdn_compress; });
}

CompressConfig BackendConfigAdapter::get_tdn_compress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->tdn_compress; });
}

void BackendConfigAdapter::set_tdn_output_format(const ImageFormatConfig &tdn_output_format)
{
	visit([&tdn_output_format](auto *cfg) { cfg->tdn_output_format = tdn_output_format; });
}

ImageFormatConfig BackendConfigAdapter::get_tdn_output_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_output_format; });
}

void BackendConfigAdapter::set_sdn(const BeSdnConfig &sdn)
{
	visit([&sdn](auto *cfg) {
		cfg->sdn = sdn;
		cfg->sdn.pad = 0;
	});
}

BeSdnConfig BackendConfigAdapter::get_sdn() const
{
	return visit([](auto *cfg) -> BeSdnConfig { return cfg->sdn; });
}

void BackendConfigAdapter::set_blc(const BlaConfig &blc)
{
	visit([&blc](auto *cfg) {
		cfg->blc = blc;
		cfg->blc.pad[0] = cfg->blc.pad[1] = 0;
	});
}

BlaConfig BackendConfigAdapter::get_blc() const
{
	return visit([](auto *cfg) -> BlaConfig { return cfg->blc; });
}

void BackendConfigAdapter::set_stitch_input_format(const ImageFormatConfig &stitch_input_format)
{
	visit([&stitch_input_format](auto *cfg) { cfg->stitch_input_format = stitch_input_format; });
}

ImageFormatConfig BackendConfigAdapter::get_stitch_input_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_input_format; });
}

void BackendConfigAdapter::set_stitch_decompress(const DecompressConfig &stitch_decompress)
{
	visit([&stitch_decompress](auto *cfg) { cfg->stitch_decompress = stitch_decompress; });
}

DecompressConfig BackendConfigAdapter::get_stitch_decompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->stitch_decompress; });
}

void BackendConfigAdapter::set_stitch_compress(const CompressConfig &stitch_compress)
{
	visit([&stitch_compress](auto *cfg) { cfg->stitch_compress = stitch_compress; });
}

CompressConfig BackendConfigAdapter::get_stitch_compress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->stitch_compress; });
}

void BackendConfigAdapter::set_stitch_output_format(const ImageFormatConfig &stitch_output_format)
{
	visit([&stitch_output_format](auto *cfg) { cfg->stitch_output_format = stitch_output_format; });
}

ImageFormatConfig BackendConfigAdapter::get_stitch_output_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_output_format; });
}

void BackendConfigAdapter::set_lsc(const BeLscConfig &lsc)
{
	visit([&lsc](auto *cfg) { 
		cfg->lsc.grid_step_x = lsc.grid_step_x;
		cfg->lsc.grid_step_y = lsc.grid_step_y;
		std::memcpy(cfg->lsc.lut_packed, lsc.lut_packed.data(), sizeof(cfg->lsc.lut_packed));
	});

	// Store the non-register config as well!
	non_reg_->lsc = lsc;
}

BeLscConfig BackendConfigAdapter::get_lsc() const
{
	return non_reg_->lsc;
}

void BackendConfigAdapter::set_wbg(const WbgConfig &wbg)
{
	visit([&wbg](auto *cfg) {
		cfg->wbg = wbg;
		cfg->wbg.pad[0] = cfg->wbg.pad[1] = 0;
	});
}

WbgConfig BackendConfigAdapter::get_wbg() const
{
	return visit([](auto *cfg) -> WbgConfig { return cfg->wbg; });
}

void BackendConfigAdapter::set_cdn(const BeCdnConfig &cdn)
{
	visit([&cdn](auto *cfg) { cfg->cdn = cdn; });
}

BeCdnConfig BackendConfigAdapter::get_cdn() const
{
	return visit([](auto *cfg) -> BeCdnConfig { return cfg->cdn; });
}

void BackendConfigAdapter::set_cac(const BeCacConfig &cac)
{
	visit([&cac](auto *cfg) { 
		cfg->cac.grid_step_x = cac.grid_step_x;
		cfg->cac.grid_step_y = cac.grid_step_y;
		std::memcpy(cfg->cac.lut, cac.lut.data(), sizeof(cfg->cac.lut));
	});

	// Store the non-register config as well!
	non_reg_->cac = cac;
}

BeCacConfig BackendConfigAdapter::get_cac() const
{
	return non_reg_->cac;
}

void BackendConfigAdapter::set_debin(const BeDebinConfig &debin)
{
	visit([&debin](auto *cfg) {
		cfg->debin = debin;
		cfg->debin.pad[0] = cfg->debin.pad[1] = 0;
	});
}

BeDebinConfig BackendConfigAdapter::get_debin() const
{
	return visit([](auto *cfg) -> BeDebinConfig { return cfg->debin; });
}

void BackendConfigAdapter::set_ccm(const BeCcmConfig &ccm)
{
	visit([&ccm](auto *cfg) {
		cfg->ccm = ccm;
		cfg->ccm.pad[0] = cfg->ccm.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::get_ccm() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ccm; });
}

void BackendConfigAdapter::set_sat_control(const BeSatControlConfig &sat_control)
{
	visit([&sat_control](auto *cfg) {
		cfg->sat_control = sat_control;
		cfg->sat_control.pad = 0;
	});
}

BeSatControlConfig BackendConfigAdapter::get_sat_control() const
{
	return visit([](auto *cfg) -> BeSatControlConfig { return cfg->sat_control; });
}

void BackendConfigAdapter::set_ycbcr(const BeCcmConfig &ycbcr)
{
	visit([&ycbcr](auto *cfg) {
		cfg->ycbcr = ycbcr;
		cfg->ycbcr.pad[0] = cfg->ycbcr.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::get_ycbcr() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr; });
}

void BackendConfigAdapter::set_false_colour(const BeFalseColourConfig &false_colour)
{
	visit([&false_colour](auto *cfg) {
		cfg->false_colour = false_colour;
		cfg->false_colour.pad[0] = cfg->false_colour.pad[1] = cfg->false_colour.pad[2] = 0;
	});
}

BeFalseColourConfig BackendConfigAdapter::get_false_colour() const
{
	return visit([](auto *cfg) -> BeFalseColourConfig { return cfg->false_colour; });
}

void BackendConfigAdapter::set_sharpen(const BeSharpenConfig &sharpen)
{
	visit([&sharpen](auto *cfg) {
		cfg->sharpen = sharpen;
		cfg->sharpen.pad0[0] = cfg->sharpen.pad0[1] = cfg->sharpen.pad0[2] = 0;
		cfg->sharpen.pad1[0] = cfg->sharpen.pad1[1] = cfg->sharpen.pad1[2] = 0;
		cfg->sharpen.pad2[0] = cfg->sharpen.pad2[1] = cfg->sharpen.pad2[2] = 0;
		cfg->sharpen.pad3[0] = cfg->sharpen.pad3[1] = cfg->sharpen.pad3[2] = 0;
		cfg->sharpen.pad4[0] = cfg->sharpen.pad4[1] = cfg->sharpen.pad4[2] = 0;
		cfg->sharpen.pad5 = cfg->sharpen.pad6 = cfg->sharpen.pad7 = cfg->sharpen.pad8 = cfg->sharpen.pad9 = 0;
	});
}

BeSharpenConfig BackendConfigAdapter::get_sharpen() const
{
	return visit([](auto *cfg) -> BeSharpenConfig { return cfg->sharpen; });
}

void BackendConfigAdapter::set_sh_fc_combine(const BeShFcCombineConfig &sh_fc_combine)
{
	visit([&sh_fc_combine](auto *cfg) {
		cfg->sh_fc_combine = sh_fc_combine;
		cfg->sh_fc_combine.pad = 0;
	});
}

BeShFcCombineConfig BackendConfigAdapter::get_sh_fc_combine() const
{
	return visit([](auto *cfg) -> BeShFcCombineConfig { return cfg->sh_fc_combine; });
}

// Blocks that only exist on one HW version are detected by the presence of the corresponding config field.
// Setting an absent block is a no-op, and getting one returns a default constructed config.

void BackendConfigAdapter::set_pref_colours(const BePrefColoursConfig &pref_colours)
{
	visit([&pref_colours](auto *cfg) {
		if constexpr (requires { cfg->pref_colours; })
			cfg->pref_colours = pref_colours;
	});
}

BePrefColoursConfig BackendConfigAdapter::get_pref_colours() const
{
	return visit([](auto *cfg) -> BePrefColoursConfig {
		if constexpr (requires { cfg->pref_colours; })
			return cfg->pref_colours;
		else
			return {};
	});
}

void BackendConfigAdapter::set_ycbcr_inverse(const BeCcmConfig &ycbcr_inverse)
{
	visit([&ycbcr_inverse](auto *cfg) {
		cfg->ycbcr_inverse = ycbcr_inverse;
		cfg->ycbcr_inverse.pad[0] = cfg->ycbcr_inverse.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::get_ycbcr_inverse() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr_inverse; });
}

void BackendConfigAdapter::set_pre_csc(unsigned int i, const BeCcmConfig &csc)
{
	visit([&csc, i](auto *cfg) {
		if constexpr (requires { cfg->pre_csc; })
		{
			cfg->pre_csc[i] = csc;
			cfg->pre_csc[i].pad[0] = cfg->pre_csc[i].pad[1] = 0;
		}
	});
}

BeCcmConfig BackendConfigAdapter::get_pre_csc(unsigned int i) const
{
	return visit([i](auto *cfg) -> BeCcmConfig {
		if constexpr (requires { cfg->pre_csc; })
			return cfg->pre_csc[i];
		else
			return {};
	});
}

void BackendConfigAdapter::set_post_csc(unsigned int i, const BeCcmConfig &csc)
{
	visit([&csc, i](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			cfg->csc[i] = csc;
			cfg->csc[i].pad[0] = cfg->csc[i].pad[1] = 0;
		}
		else
		{
			cfg->post_csc[i] = csc;
			cfg->post_csc[i].pad[0] = cfg->post_csc[i].pad[1] = 0;
		}
	});
}

BeCcmConfig BackendConfigAdapter::get_post_csc(unsigned int i) const
{
	return visit([i](auto *cfg) -> BeCcmConfig {
		if constexpr (is_v1<decltype(cfg)>)
			return cfg->csc[i];
		else
			return cfg->post_csc[i];
	});
}

void BackendConfigAdapter::set_downscale(unsigned int i, const BeDownscaleConfig &downscale)
{
	visit([&downscale, i](auto *cfg) {
		cfg->downscale[i].scale_factor_h = downscale.scale_factor_h;
		cfg->downscale[i].scale_factor_v = downscale.scale_factor_v;
		cfg->downscale[i].scale_recip_h = downscale.scale_recip_h;
		cfg->downscale[i].scale_recip_v = downscale.scale_recip_v;
	});

	// Store the non-register config as well!
	non_reg_->downscale[i] = downscale;
}

BeDownscaleConfig BackendConfigAdapter::get_downscale(unsigned int i) const
{
	return non_reg_->downscale[i];
}

void BackendConfigAdapter::set_output_format(unsigned int i, const BeOutputFormatConfig &output_format)
{
	visit([&output_format, i](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			cfg->output_format[i].image = output_format.image;
			cfg->output_format[i].transform = output_format.transform;
			cfg->output_format[i].lo = output_format.lo;
			cfg->output_format[i].lo2 = output_format.lo2;
			cfg->output_format[i].hi = output_format.hi;
			cfg->output_format[i].hi2 = output_format.hi2;
			cfg->output_format[i].pad[0] = cfg->output_format[i].pad[1] = cfg->output_format[i].pad[2] = 0;
		}
		else
			cfg->output_format[i] = output_format;
	});
}

BeOutputFormatConfig BackendConfigAdapter::get_output_format(unsigned int i) const
{
	return visit([i](auto *cfg) -> BeOutputFormatConfig {
		if constexpr (is_v1<decltype(cfg)>)
		{
			BeOutputFormatConfig output_format {};
			output_format.image = cfg->output_format[i].image;
			output_format.transform = cfg->output_format[i].transform;
			output_format.lo = cfg->output_format[i].lo;
			output_format.lo2 = cfg->output_format[i].lo2;
			output_format.hi = cfg->output_format[i].hi;
			output_format.hi2 = cfg->output_format[i].hi2;
			return output_format;
		}
		else
			return cfg->output_format[i];
	});
}

void BackendConfigAdapter::set_tdn(const BeTdnConfig &tdn)
{
	visit([&tdn](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			cfg->tdn.black_level = tdn.black_level;
			cfg->tdn.ratio = tdn.ratio;
			cfg->tdn.noise_constant = tdn.noise_constant;
			cfg->tdn.noise_slope = tdn.noise_slope;
			cfg->tdn.threshold = tdn.threshold;
			cfg->tdn.reset = tdn.reset;
		}
		else
			cfg->tdn = tdn;
		cfg->tdn.pad = 0;
	});
}

BeTdnConfig BackendConfigAdapter::get_tdn() const
{
	return visit([](auto *cfg) -> BeTdnConfig {
		if constexpr (is_v1<decltype(cfg)>)
		{
			BeTdnConfig tdn {};
			tdn.black_level = cfg->tdn.black_level;
			tdn.ratio = cfg->tdn.ratio;
			tdn.noise_constant = cfg->tdn.noise_constant;
			tdn.noise_slope = cfg->tdn.noise_slope;
			tdn.threshold = cfg->tdn.threshold;
			tdn.reset = cfg->tdn.reset;
			return tdn;
		}
		else
			return cfg->tdn;
	});
}

void BackendConfigAdapter::set_stitch(const BeStitchConfig &stitch)
{
	visit([&stitch](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			cfg->stitch.threshold_lo = stitch.threshold_lo;
			cfg->stitch.threshold_diff_power = stitch.threshold_diff_power;
			cfg->stitch.exposure_ratio = stitch.exposure_ratio;
			cfg->stitch.motion_threshold_256 = stitch.motion_threshold_256;
			cfg->stitch.motion_threshold_recip = stitch.motion_threshold_recip;
		}
		else
		{
			cfg->stitch.black_level = stitch.black_level;
			cfg->stitch.ratio = stitch.ratio;
			cfg->stitch.noise_constant = stitch.noise_constant;
			cfg->stitch.noise_slope = stitch.noise_slope;
			cfg->stitch.reset = stitch.reset;
			cfg->stitch.apply_threshold = stitch.apply_threshold;
			cfg->stitch.swap_inputs = stitch.swap_inputs;
			cfg->stitch.saturation_threshold = stitch.saturation_threshold;
			cfg->stitch.threshold = stitch.threshold;
		}
		cfg->stitch.pad = 0;
	});
}

BeStitchConfig BackendConfigAdapter::get_stitch() const
{
	return visit([](auto *cfg) -> BeStitchConfig {
		BeStitchConfig stitch;
		if constexpr (is_v1<decltype(cfg)>)
		{
			stitch.threshold_lo = cfg->stitch.threshold_lo;
			stitch.threshold_diff_power = cfg->stitch.threshold_diff_power;
			stitch.exposure_ratio = cfg->stitch.exposure_ratio;
			stitch.motion_threshold_256 = cfg->stitch.motion_threshold_256;
			stitch.motion_threshold_recip = cfg->stitch.motion_threshold_recip;
		}
		else
		{
			stitch.black_level = cfg->stitch.black_level;
			stitch.ratio = cfg->stitch.ratio;
			stitch.noise_constant = cfg->stitch.noise_constant;
			stitch.noise_slope = cfg->stitch.noise_slope;
			stitch.threshold = cfg->stitch.threshold;
			stitch.reset = cfg->stitch.reset;
			stitch.apply_threshold = cfg->stitch.apply_threshold;
			stitch.swap_inputs = cfg->stitch.swap_inputs;
			stitch.saturation_threshold = cfg->stitch.saturation_threshold;
		}
		return stitch;
	});
}

void BackendConfigAdapter::set_tonemap(const BeTonemapConfig &tonemap)
{
	visit([&tonemap](auto *cfg) {
		if constexpr (requires { cfg->tonemap; })
			cfg->tonemap = tonemap;
	});
}

BeTonemapConfig BackendConfigAdapter::get_tonemap() const
{
	return visit([](auto *cfg) -> BeTonemapConfig {
		if constexpr (requires { cfg->tonemap; })
			return cfg->tonemap;
		else
			return {};
	});
}

void BackendConfigAdapter::set_tonemap2(const BeTonemap2Config &tonemap)
{
	visit([&tonemap](auto *cfg) {
		if constexpr (requires { cfg->tonemap2; })
		{
			cfg->tonemap2 = tonemap;
			cfg->tonemap2.pad0[0] = cfg->tonemap2.pad0[1] = 0;
			cfg->tonemap2.pad1[0] = cfg->tonemap2.pad1[1] = 0;
			cfg->tonemap2.pad2[0] = cfg->tonemap2.pad2[1] = cfg->tonemap2.pad2[2] = 0;
			cfg->tonemap2.pad3 = 0;
		}
	});
}

BeTonemap2Config BackendConfigAdapter::get_tonemap2() const
{
	return visit([](auto *cfg) -> BeTonemap2Config {
		if constexpr (requires { cfg->tonemap2; })
			return cfg->tonemap2;
		else
			return {};
	});
}

void BackendConfigAdapter::set_tonemap_format(ImageFormatConfig const &format)
{
	visit([&format](auto *cfg) {
		if constexpr (requires { cfg->tonemap_format; })
			cfg->tonemap_format = format;
	});
}

ImageFormatConfig BackendConfigAdapter::get_tonemap_format() const
{
	return visit([](auto *cfg) -> ImageFormatConfig {
		if constexpr (requires { cfg->tonemap_format; })
			return cfg->tonemap_format;
		else
			return {};
	});
}

void BackendConfigAdapter::set_demosaic(const BeDemosaicConfig &demosaic)
{
	visit([&demosaic](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			cfg->demosaic.sharper = demosaic.sharper;
			cfg->demosaic.fc_mode = demosaic.fc_mode;
			cfg->demosaic.pad[0] = cfg->demosaic.pad[1] = 0;
		}
		else
		{
			cfg->demosaic = demosaic;
			cfg->demosaic.pad0[0] = cfg->demosaic.pad0[1] = 0;
			cfg->demosaic.pad1 = 0;
		}
	});
}

BeDemosaicConfig BackendConfigAdapter::get_demosaic() const
{
	return visit([](auto *cfg) -> BeDemosaicConfig {
		if constexpr (is_v1<decltype(cfg)>)
		{
			BeDemosaicConfig demosaic {};
			demosaic.sharper = cfg->demosaic.sharper;
			demosaic.fc_mode = cfg->demosaic.fc_mode;
			return demosaic;
		}
		else
			return cfg->demosaic;
	});
}

// v1 has a single gamma block, so only index 0 exists there.
void BackendConfigAdapter::set_gamma(unsigned int i, const BeGammaConfig &gamma)
{
	visit([&gamma, i](auto *cfg) {
		if constexpr (is_v1<decltype(cfg)>)
		{
			if (i != 0)
				return;
			for (unsigned int j = 0; j < PISP_BE_GAMMA_LUT_SIZE; j++)
				cfg->gamma.lut[j] = gamma.lut[j];
		}
		else
		{
			for (unsigned int j = 0; j < PISP_BE_GAMMA2_LUT_SIZE; j++)
				cfg->gamma[i].lut[j] = gamma.lut[j] & 0xffff;
			cfg->gamma[i].pad = 0;
		}
	});
}

BeGammaConfig BackendConfigAdapter::get_gamma(unsigned int i) const
{
	return visit([i](auto *cfg) -> BeGammaConfig {
		BeGammaConfig gamma;
		if constexpr (is_v1<decltype(cfg)>)
		{
			if (i != 0)
				return gamma;
			for (unsigned int j = 0; j < PISP_BE_GAMMA_LUT_SIZE; j++)
				gamma.lut[j] = cfg->gamma.lut[j];
		}
		else
		{
			for (unsigned int j = 0; j < PISP_BE_GAMMA2_LUT_SIZE; j++)
				gamma.lut[j] = cfg->gamma[i].lut[j] & 0xffff;
		}
		return gamma;
	});
}

void BackendConfigAdapter::set_resample(unsigned int i, const BeResampleConfig &resample)
{
	visit([&resample, i](auto *cfg) {
		cfg->resample[i].scale_factor_h = resample.scale_factor_h;
		cfg->resample[i].scale_factor_v = resample.scale_factor_v;

		if constexpr (is_v1<decltype(cfg)>)
		{
			if (resample.coef)
				std::memcpy(cfg->resample[i].coef, resample.coef->data(), sizeof(cfg->resample[i].coef));
		}
		else
		{
			// Separate H/V coefficients fall back to the common ones if not given.
			auto const &coef_h = resample.coef_h ? resample.coef_h : resample.coef;
			auto const &coef_v = resample.coef_v ? resample.coef_v : resample.coef;

			if (coef_h)
				std::memcpy(cfg->resample[i].coef_h, coef_h->data(), sizeof(cfg->resample[i].coef_h));
			if (coef_v)
				std::memcpy(cfg->resample[i].coef_v, coef_v->data(), sizeof(cfg->resample[i].coef_v));
		}
	});

	// Store the non-register config as well!
	non_reg_->resample[i] = resample;
}

BeResampleConfig BackendConfigAdapter::get_resample(unsigned int i) const
{
	return non_reg_->resample[i];
}

void BackendConfigAdapter::set_crop(unsigned int i, BeCropConfig const &crop)
{
	// Not a hardware register set.
	non_reg_->crop[i] = crop;
}

BeCropConfig BackendConfigAdapter::get_crop(unsigned int i) const
{
	return non_reg_->crop[i];
}

void BackendConfigAdapter::set_dedistort(const BeDedistortionConfig &dedistort)
{
	visit([&dedistort, this](auto *cfg) {
		if constexpr (requires { cfg->dedistort; })
		{
			cfg->dedistort.grid_step_x = dedistort.grid_step_x;
			cfg->dedistort.grid_step_y = dedistort.grid_step_y;
			cfg->dedistort.pad = 0;
			std::memcpy(cfg->dedistort.lut, dedistort.lut, sizeof(dedistort.lut));
			non_reg_->dedistort = dedistort;
		}
	});
}

BeDedistortionConfig BackendConfigAdapter::get_dedistort() const
{
	return non_reg_->dedistort;
}

void BackendConfigAdapter::set_output_compress(unsigned int i, const BeCompressConfig &compress)
{
	visit([&compress, i](auto *cfg) {
		if constexpr (requires { cfg->compress; })
		{
			cfg->compress[i] = compress;
			cfg->compress[i].pad = 0;
		}
	});
}

BeCompressConfig BackendConfigAdapter::get_output_compress(unsigned int i) const
{
	return visit([i](auto *cfg) -> BeCompressConfig {
		if constexpr (requires { cfg->compress; })
			return cfg->compress[i];
		else
			return {};
	});
}
