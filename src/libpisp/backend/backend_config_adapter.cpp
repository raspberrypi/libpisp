/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */

#include "backend_config_adapter.hpp"

#include <cstring>
#include <map>
#include <stdexcept>

using namespace libpisp;

namespace
{

struct EnableMapping
{
	uint32_t v1;
	uint32_t v2;
};

const std::map<RgbEnable, EnableMapping> rgbEnableMap =
{
	{ RgbEnable::Input,			{ PISP_BE_RGB_ENABLE_INPUT,			PISP_BE_RGB_ENABLE2_INPUT } },
	{ RgbEnable::Dedistort,		{ 0,								PISP_BE_RGB_ENABLE2_DEDISTORT } },
	{ RgbEnable::Ccm,			{ PISP_BE_RGB_ENABLE_CCM,			PISP_BE_RGB_ENABLE2_CCM } },
	{ RgbEnable::SatControl,	{ PISP_BE_RGB_ENABLE_SAT_CONTROL,	PISP_BE_RGB_ENABLE2_SAT_CONTROL } },
	{ RgbEnable::Ycbcr,			{ PISP_BE_RGB_ENABLE_YCBCR,			PISP_BE_RGB_ENABLE2_YCBCR } },
	{ RgbEnable::FalseColour,	{ PISP_BE_RGB_ENABLE_FALSE_COLOUR,	PISP_BE_RGB_ENABLE2_FALSE_COLOUR } },
	{ RgbEnable::Sharpen,		{ PISP_BE_RGB_ENABLE_SHARPEN,		PISP_BE_RGB_ENABLE2_SHARPEN } },
	{ RgbEnable::YcbcrInverse,	{ PISP_BE_RGB_ENABLE_YCBCR_INVERSE,	PISP_BE_RGB_ENABLE2_YCBCR_INVERSE } },
	{ RgbEnable::PreCsc0,		{ 0,								PISP_BE_RGB_ENABLE2_PRE_CSC0 } },
	{ RgbEnable::PreCsc1,		{ 0,								PISP_BE_RGB_ENABLE2_PRE_CSC1 } },
	{ RgbEnable::Gamma0,		{ PISP_BE_RGB_ENABLE_GAMMA,			PISP_BE_RGB_ENABLE2_GAMMA0 } },
	{ RgbEnable::Gamma1,		{ 0,								PISP_BE_RGB_ENABLE2_GAMMA1 } },
	{ RgbEnable::PostCsc0,		{ PISP_BE_RGB_ENABLE_CSC0,			PISP_BE_RGB_ENABLE2_POST_CSC0 } },
	{ RgbEnable::PostCsc1,		{ PISP_BE_RGB_ENABLE_CSC1,			PISP_BE_RGB_ENABLE2_POST_CSC1 } },
	{ RgbEnable::Downscale0,	{ PISP_BE_RGB_ENABLE_DOWNSCALE0,	PISP_BE_RGB_ENABLE2_DOWNSCALE0 } },
	{ RgbEnable::Downscale1,	{ PISP_BE_RGB_ENABLE_DOWNSCALE1,	PISP_BE_RGB_ENABLE2_DOWNSCALE1 } },
	{ RgbEnable::Resample0,		{ PISP_BE_RGB_ENABLE_RESAMPLE0,		PISP_BE_RGB_ENABLE2_RESAMPLE0 } },
	{ RgbEnable::Resample1,		{ PISP_BE_RGB_ENABLE_RESAMPLE1,		PISP_BE_RGB_ENABLE2_RESAMPLE1 } },
	{ RgbEnable::Output0,		{ PISP_BE_RGB_ENABLE_OUTPUT0,		PISP_BE_RGB_ENABLE2_OUTPUT0 } },
	{ RgbEnable::Output1,		{ PISP_BE_RGB_ENABLE_OUTPUT1,		PISP_BE_RGB_ENABLE2_OUTPUT1 } },
	{ RgbEnable::Compress0,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS0 } },
	{ RgbEnable::Compress1,		{ 0,								PISP_BE_RGB_ENABLE2_COMPRESS1 } },
};

const std::map<BayerEnable, EnableMapping> bayerEnableMap =
{
	{ BayerEnable::Input,				{ PISP_BE_BAYER_ENABLE_INPUT,				PISP_BE_BAYER_ENABLE_INPUT } },
	{ BayerEnable::Decompress,			{ PISP_BE_BAYER_ENABLE_DECOMPRESS,			PISP_BE_BAYER_ENABLE_DECOMPRESS } },
	{ BayerEnable::Dpc,					{ PISP_BE_BAYER_ENABLE_DPC,					PISP_BE_BAYER_ENABLE_DPC } },
	{ BayerEnable::Geq,					{ PISP_BE_BAYER_ENABLE_GEQ,					PISP_BE_BAYER_ENABLE_GEQ } },
	{ BayerEnable::TdnInput,			{ PISP_BE_BAYER_ENABLE_TDN_INPUT,			PISP_BE_BAYER_ENABLE_TDN_INPUT } },
	{ BayerEnable::TdnDecompress,		{ PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS,		PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS } },
	{ BayerEnable::Tdn,					{ PISP_BE_BAYER_ENABLE_TDN,					PISP_BE_BAYER_ENABLE_TDN } },
	{ BayerEnable::TdnCompress,			{ PISP_BE_BAYER_ENABLE_TDN_COMPRESS,		PISP_BE_BAYER_ENABLE_TDN_COMPRESS } },
	{ BayerEnable::TdnOutput,			{ PISP_BE_BAYER_ENABLE_TDN_OUTPUT,			PISP_BE_BAYER_ENABLE_TDN_OUTPUT } },
	{ BayerEnable::Sdn,					{ PISP_BE_BAYER_ENABLE_SDN,					PISP_BE_BAYER_ENABLE_SDN } },
	{ BayerEnable::Blc,					{ PISP_BE_BAYER_ENABLE_BLC,					PISP_BE_BAYER_ENABLE_BLC } },
	{ BayerEnable::StitchInput,			{ PISP_BE_BAYER_ENABLE_STITCH_INPUT,		PISP_BE_BAYER_ENABLE_STITCH_INPUT } },
	{ BayerEnable::StitchDecompress,	{ PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS,	PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS } },
	{ BayerEnable::Stitch,				{ PISP_BE_BAYER_ENABLE_STITCH,				PISP_BE_BAYER_ENABLE_STITCH } },
	{ BayerEnable::StitchCompress,		{ PISP_BE_BAYER_ENABLE_STITCH_COMPRESS,		PISP_BE_BAYER_ENABLE_STITCH_COMPRESS } },
	{ BayerEnable::StitchOutput,		{ PISP_BE_BAYER_ENABLE_STITCH_OUTPUT,		PISP_BE_BAYER_ENABLE_STITCH_OUTPUT } },
	{ BayerEnable::Wbg,					{ PISP_BE_BAYER_ENABLE_WBG,					PISP_BE_BAYER_ENABLE_WBG } },
	{ BayerEnable::Cdn,					{ PISP_BE_BAYER_ENABLE_CDN,					PISP_BE_BAYER_ENABLE_CDN } },
	{ BayerEnable::Lsc,					{ PISP_BE_BAYER_ENABLE_LSC,					PISP_BE_BAYER_ENABLE_LSC } },
	{ BayerEnable::Tonemap,				{ PISP_BE_BAYER_ENABLE_TONEMAP,				PISP_BE_BAYER_ENABLE_TONEMAP } },
	{ BayerEnable::Cac,					{ PISP_BE_BAYER_ENABLE_CAC,					PISP_BE_BAYER_ENABLE_CAC } },
	{ BayerEnable::Debin,				{ PISP_BE_BAYER_ENABLE_DEBIN,				PISP_BE_BAYER_ENABLE_DEBIN } },
	{ BayerEnable::Demosaic,			{ PISP_BE_BAYER_ENABLE_DEMOSAIC,			PISP_BE_BAYER_ENABLE_DEMOSAIC } },
	{ BayerEnable::TonemapInput,		{ PISP_BE_BAYER_ENABLE_TONEMAP_INPUT,		PISP_BE_BAYER_ENABLE_TONEMAP_INPUT } },
	{ BayerEnable::Transpose,			{ PISP_BE_BAYER_ENABLE_TRANSPOSE,			PISP_BE_BAYER_ENABLE_TRANSPOSE } },
};

// From https://en.cppreference.com/w/cpp/utility/variant/visit2.html
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

} // namespace

uint32_t BackendConfigAdapter::rgbEnablesToHw(RgbEnables rgbEnables) const
{
	uint32_t hwEnables = 0;

	visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((rgbEnables & enable) && map.v1)
						hwEnables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((rgbEnables & enable) && map.v2)
						hwEnables |= map.v2;
			}
		}
	);

	return hwEnables;
}

uint32_t BackendConfigAdapter::bayerEnablesToHw(BayerEnables bayerEnables) const
{
	uint32_t hwEnables = 0;

	visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((bayerEnables & enable) && map.v1)
						hwEnables |= map.v1;
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((bayerEnables & enable) && map.v2)
						hwEnables |= map.v2;
			}
		}
	);

	return hwEnables;
}

RgbEnables BackendConfigAdapter::hwToRgbEnables(uint32_t hwEnables) const
{
	RgbEnables enables = RgbEnable::None;

	visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((hwEnables & map.v1) && map.v1)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : rgbEnableMap)
					if ((hwEnables & map.v2) && map.v2)
						enables = static_cast<RgbEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		}
	);

	return enables;
}

BayerEnables BackendConfigAdapter::hwToBayerEnables(uint32_t hwEnables) const
{
	BayerEnables enables = BayerEnable::None;

	visit(
		overloaded {
			[&](pisp_be_config *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((hwEnables & map.v1) && map.v1)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			},

			[&](pisp_be_config2 *)
			{
				for (auto &[enable, map] : bayerEnableMap)
					if ((hwEnables & map.v2) && map.v2)
						enables = static_cast<BayerEnables>(static_cast<uint32_t>(enables) | static_cast<uint32_t>(enable));
			}
		}
	);

	return enables;
}

void BackendConfigAdapter::setTile(unsigned int i, const BeTile &tile)
{
	std::visit(overloaded {
		[&tile, i](pisp_tile *tiles) {
			// Common fields
			tiles[i].input_addr_offset = tile.input_addr_offset;
			tiles[i].input_addr_offset2 = tile.input_addr_offset2;
			tiles[i].input_offset_x = tile.input_offset_x;
			tiles[i].input_offset_y = tile.input_offset_y;
			tiles[i].input_width = tile.input_width;
			tiles[i].input_height = tile.input_height;
			tiles[i].tdn_input_addr_offset = tile.tdn_input_addr_offset;
			tiles[i].tdn_output_addr_offset = tile.tdn_output_addr_offset;
			tiles[i].stitch_input_addr_offset = tile.stitch_input_addr_offset;
			tiles[i].stitch_output_addr_offset = tile.stitch_output_addr_offset;
			tiles[i].lsc_grid_offset_x = tile.lsc_grid_offset_x;
			tiles[i].lsc_grid_offset_y = tile.lsc_grid_offset_y;
			tiles[i].cac_grid_offset_x = tile.cac_grid_offset_x;
			tiles[i].cac_grid_offset_y = tile.cac_grid_offset_y;
			std::memcpy(tiles[i].crop_x_start, tile.crop_x_start.data(), sizeof(tile.crop_x_start));
			std::memcpy(tiles[i].crop_x_end, tile.crop_x_end.data(), sizeof(tile.crop_x_end));
			std::memcpy(tiles[i].crop_y_start, tile.crop_y_start.data(), sizeof(tile.crop_y_start));
			std::memcpy(tiles[i].crop_y_end, tile.crop_y_end.data(), sizeof(tile.crop_y_end));
			std::memcpy(tiles[i].resample_in_width, tile.resample_in_width.data(), sizeof(tile.resample_in_width));
			std::memcpy(tiles[i].resample_in_height, tile.resample_in_height.data(), sizeof(tile.resample_in_height));
			std::memcpy(tiles[i].output_offset_x, tile.output_offset_x.data(), sizeof(tile.output_offset_x));
			std::memcpy(tiles[i].output_offset_y, tile.output_offset_y.data(), sizeof(tile.output_offset_y));
			std::memcpy(tiles[i].output_width, tile.output_width.data(), sizeof(tile.output_width));
			std::memcpy(tiles[i].output_height, tile.output_height.data(), sizeof(tile.output_height));
			std::memcpy(tiles[i].output_addr_offset, tile.output_addr_offset.data(), sizeof(tile.output_addr_offset));
			std::memcpy(tiles[i].output_addr_offset2, tile.output_addr_offset2.data(), sizeof(tile.output_addr_offset2));
			
			// v1 specific fields
			tiles[i].edge = tile.edge;
			std::memcpy(tiles[i].downscale_phase_x, tile.downscale_phase_x.data(), sizeof(tile.downscale_phase_x));
			std::memcpy(tiles[i].downscale_phase_y, tile.downscale_phase_y.data(), sizeof(tile.downscale_phase_y));
			std::memcpy(tiles[i].resample_phase_x, tile.resample_phase_x.data(), sizeof(tile.resample_phase_x));
			std::memcpy(tiles[i].resample_phase_y, tile.resample_phase_y.data(), sizeof(tile.resample_phase_y));
		},
		[&tile, i](pisp_tile2 *tiles) {
			// Common fields
			tiles[i].input_addr_offset = tile.input_addr_offset;
			tiles[i].input_addr_offset2 = tile.input_addr_offset2;
			tiles[i].input_offset_x = tile.input_offset_x;
			tiles[i].input_offset_y = tile.input_offset_y;
			tiles[i].input_width = tile.input_width;
			tiles[i].input_height = tile.input_height;
			tiles[i].tdn_input_addr_offset = tile.tdn_input_addr_offset;
			tiles[i].tdn_output_addr_offset = tile.tdn_output_addr_offset;
			tiles[i].stitch_input_addr_offset = tile.stitch_input_addr_offset;
			tiles[i].stitch_output_addr_offset = tile.stitch_output_addr_offset;
			tiles[i].lsc_grid_offset_x = tile.lsc_grid_offset_x;
			tiles[i].lsc_grid_offset_y = tile.lsc_grid_offset_y;
			tiles[i].cac_grid_offset_x = tile.cac_grid_offset_x;
			tiles[i].cac_grid_offset_y = tile.cac_grid_offset_y;
			std::memcpy(tiles[i].crop_x_start, tile.crop_x_start.data(), sizeof(tile.crop_x_start));
			std::memcpy(tiles[i].crop_x_end, tile.crop_x_end.data(), sizeof(tile.crop_x_end));
			std::memcpy(tiles[i].crop_y_start, tile.crop_y_start.data(), sizeof(tile.crop_y_start));
			std::memcpy(tiles[i].crop_y_end, tile.crop_y_end.data(), sizeof(tile.crop_y_end));
			std::memcpy(tiles[i].resample_in_width, tile.resample_in_width.data(), sizeof(tile.resample_in_width));
			std::memcpy(tiles[i].resample_in_height, tile.resample_in_height.data(), sizeof(tile.resample_in_height));
			std::memcpy(tiles[i].output_offset_x, tile.output_offset_x.data(), sizeof(tile.output_offset_x));
			std::memcpy(tiles[i].output_offset_y, tile.output_offset_y.data(), sizeof(tile.output_offset_y));
			std::memcpy(tiles[i].output_width, tile.output_width.data(), sizeof(tile.output_width));
			std::memcpy(tiles[i].output_height, tile.output_height.data(), sizeof(tile.output_height));
			std::memcpy(tiles[i].output_addr_offset, tile.output_addr_offset.data(), sizeof(tile.output_addr_offset));
			std::memcpy(tiles[i].output_addr_offset2, tile.output_addr_offset2.data(), sizeof(tile.output_addr_offset2));
			
			// v2 specific fields
			tiles[i].tonemap_grid_offset_x = tile.tonemap_grid_offset_x;
			tiles[i].tonemap_grid_offset_y = tile.tonemap_grid_offset_y;
			tiles[i].tonemap_lores_offset_x = tile.tonemap_lores_offset_x;
			tiles[i].tonemap_lores_offset_y = tile.tonemap_lores_offset_y;
			tiles[i].dedistort_grid_offset_x = tile.dedistort_grid_offset_x;
			tiles[i].dedistort_grid_offset_y = tile.dedistort_grid_offset_y;

			// v2 has per-output downscale/resample phases, v1/BeTile has per-component
			for (unsigned int j = 0; j < PISP_BACK_END_NUM_OUTPUTS; j++)
			{
				tiles[i].downscale_phase_x[j] = tile.downscale_phase_x[j];
				tiles[i].downscale_phase_y[j] = tile.downscale_phase_y[j];
				tiles[i].resample_phase_x[j] = tile.resample_phase_x[j];
				tiles[i].resample_phase_y[j] = tile.resample_phase_y[j];
			}
		}
	}, tile_);
}


BeTile BackendConfigAdapter::getTile(unsigned int i) const
{
	BeTile tile{};

	std::visit(overloaded {
		[&tile, i](pisp_tile *tiles) {
			// Common fields
			tile.input_addr_offset = tiles[i].input_addr_offset;
			tile.input_addr_offset2 = tiles[i].input_addr_offset2;
			tile.input_offset_x = tiles[i].input_offset_x;
			tile.input_offset_y = tiles[i].input_offset_y;
			tile.input_width = tiles[i].input_width;
			tile.input_height = tiles[i].input_height;
			tile.tdn_input_addr_offset = tiles[i].tdn_input_addr_offset;
			tile.tdn_output_addr_offset = tiles[i].tdn_output_addr_offset;
			tile.stitch_input_addr_offset = tiles[i].stitch_input_addr_offset;
			tile.stitch_output_addr_offset = tiles[i].stitch_output_addr_offset;
			tile.lsc_grid_offset_x = tiles[i].lsc_grid_offset_x;
			tile.lsc_grid_offset_y = tiles[i].lsc_grid_offset_y;
			tile.cac_grid_offset_x = tiles[i].cac_grid_offset_x;
			tile.cac_grid_offset_y = tiles[i].cac_grid_offset_y;
			std::memcpy(tile.crop_x_start.data(), tiles[i].crop_x_start, sizeof(tile.crop_x_start));
			std::memcpy(tile.crop_x_end.data(), tiles[i].crop_x_end, sizeof(tile.crop_x_end));
			std::memcpy(tile.crop_y_start.data(), tiles[i].crop_y_start, sizeof(tile.crop_y_start));
			std::memcpy(tile.crop_y_end.data(), tiles[i].crop_y_end, sizeof(tile.crop_y_end));
			std::memcpy(tile.resample_in_width.data(), tiles[i].resample_in_width, sizeof(tile.resample_in_width));
			std::memcpy(tile.resample_in_height.data(), tiles[i].resample_in_height, sizeof(tile.resample_in_height));
			std::memcpy(tile.output_offset_x.data(), tiles[i].output_offset_x, sizeof(tile.output_offset_x));
			std::memcpy(tile.output_offset_y.data(), tiles[i].output_offset_y, sizeof(tile.output_offset_y));
			std::memcpy(tile.output_width.data(), tiles[i].output_width, sizeof(tile.output_width));
			std::memcpy(tile.output_height.data(), tiles[i].output_height, sizeof(tile.output_height));
			std::memcpy(tile.output_addr_offset.data(), tiles[i].output_addr_offset, sizeof(tile.output_addr_offset));
			std::memcpy(tile.output_addr_offset2.data(), tiles[i].output_addr_offset2, sizeof(tile.output_addr_offset2));
			
			// v1 specific fields
			tile.edge = tiles[i].edge;
			std::memcpy(tile.downscale_phase_x.data(), tiles[i].downscale_phase_x, sizeof(tile.downscale_phase_x));
			std::memcpy(tile.downscale_phase_y.data(), tiles[i].downscale_phase_y, sizeof(tile.downscale_phase_y));
			std::memcpy(tile.resample_phase_x.data(), tiles[i].resample_phase_x, sizeof(tile.resample_phase_x));
			std::memcpy(tile.resample_phase_y.data(), tiles[i].resample_phase_y, sizeof(tile.resample_phase_y));
		},
		[&tile, i](pisp_tile2 *tiles) {
			// Common fields
			tile.input_addr_offset = tiles[i].input_addr_offset;
			tile.input_addr_offset2 = tiles[i].input_addr_offset2;
			tile.input_offset_x = tiles[i].input_offset_x;
			tile.input_offset_y = tiles[i].input_offset_y;
			tile.input_width = tiles[i].input_width;
			tile.input_height = tiles[i].input_height;
			tile.tdn_input_addr_offset = tiles[i].tdn_input_addr_offset;
			tile.tdn_output_addr_offset = tiles[i].tdn_output_addr_offset;
			tile.stitch_input_addr_offset = tiles[i].stitch_input_addr_offset;
			tile.stitch_output_addr_offset = tiles[i].stitch_output_addr_offset;
			tile.lsc_grid_offset_x = tiles[i].lsc_grid_offset_x;
			tile.lsc_grid_offset_y = tiles[i].lsc_grid_offset_y;
			tile.cac_grid_offset_x = tiles[i].cac_grid_offset_x;
			tile.cac_grid_offset_y = tiles[i].cac_grid_offset_y;
			std::memcpy(tile.crop_x_start.data(), tiles[i].crop_x_start, sizeof(tile.crop_x_start));
			std::memcpy(tile.crop_x_end.data(), tiles[i].crop_x_end, sizeof(tile.crop_x_end));
			std::memcpy(tile.crop_y_start.data(), tiles[i].crop_y_start, sizeof(tile.crop_y_start));
			std::memcpy(tile.crop_y_end.data(), tiles[i].crop_y_end, sizeof(tile.crop_y_end));
			std::memcpy(tile.resample_in_width.data(), tiles[i].resample_in_width, sizeof(tile.resample_in_width));
			std::memcpy(tile.resample_in_height.data(), tiles[i].resample_in_height, sizeof(tile.resample_in_height));
			std::memcpy(tile.output_offset_x.data(), tiles[i].output_offset_x, sizeof(tile.output_offset_x));
			std::memcpy(tile.output_offset_y.data(), tiles[i].output_offset_y, sizeof(tile.output_offset_y));
			std::memcpy(tile.output_width.data(), tiles[i].output_width, sizeof(tile.output_width));
			std::memcpy(tile.output_height.data(), tiles[i].output_height, sizeof(tile.output_height));
			std::memcpy(tile.output_addr_offset.data(), tiles[i].output_addr_offset, sizeof(tile.output_addr_offset));
			std::memcpy(tile.output_addr_offset2.data(), tiles[i].output_addr_offset2, sizeof(tile.output_addr_offset2));
			
			// v2 specific fields
			tile.tonemap_grid_offset_x = tiles[i].tonemap_grid_offset_x;
			tile.tonemap_grid_offset_y = tiles[i].tonemap_grid_offset_y;
			tile.tonemap_lores_offset_x = tiles[i].tonemap_lores_offset_x;
			tile.tonemap_lores_offset_y = tiles[i].tonemap_lores_offset_y;
			tile.dedistort_grid_offset_x = tiles[i].dedistort_grid_offset_x;
			tile.dedistort_grid_offset_y = tiles[i].dedistort_grid_offset_y;
			// v2 has per-output downscale/resample phases, v1/BeTile has per-component
			for (unsigned int j = 0; j < PISP_BACK_END_NUM_OUTPUTS; j++) {
				tile.downscale_phase_x[j] = tiles[i].downscale_phase_x[j];
				tile.downscale_phase_y[j] = tiles[i].downscale_phase_y[j];
				tile.resample_phase_x[j] = tiles[i].resample_phase_x[j];
				tile.resample_phase_y[j] = tiles[i].resample_phase_y[j];
			}
		}
	}, tile_);

	return tile;
}

void BackendConfigAdapter::setGlobal(const BeGlobal &global)
{
	visit([&global, this](auto *cfg) {
		cfg->global.bayer_enables = bayerEnablesToHw(global.bayerEnables);
		cfg->global.rgb_enables = rgbEnablesToHw(global.rgbEnables);
		cfg->global.bayer_order = global.bayerOrder;
		cfg->global.pad[0] = cfg->global.pad[1] = cfg->global.pad[2] = 0;
	});
}

BeGlobal BackendConfigAdapter::getGlobal() const
{
	BeGlobal global;

	visit([&global, this](auto *cfg) {
		global.bayerEnables = hwToBayerEnables(cfg->global.bayer_enables);
		global.rgbEnables = hwToRgbEnables(cfg->global.rgb_enables);
		global.bayerOrder = cfg->global.bayer_order;
	});

	return global;
}

// Common field implementations (same type in both variants)
void BackendConfigAdapter::setInputFormat(const ImageFormatConfig &inputFormat)
{
	visit([&inputFormat](auto *cfg) { cfg->input_format = inputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->input_format; });
}

void BackendConfigAdapter::setDecompress(const DecompressConfig &decompress)
{
	visit([&decompress](auto *cfg) { cfg->decompress = decompress; });
}

DecompressConfig BackendConfigAdapter::getDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->decompress; });
}

void BackendConfigAdapter::setDpc(const BeDpcConfig &dpc)
{
	visit([&dpc](auto *cfg) {
		cfg->dpc = dpc;
		cfg->dpc.pad = 0;
	});
}

BeDpcConfig BackendConfigAdapter::getDpc() const
{
	return visit([](auto *cfg) -> BeDpcConfig { return cfg->dpc; });
}

void BackendConfigAdapter::setGeq(const BeGeqConfig &geq)
{
	visit([&geq](auto *cfg) { cfg->geq = geq; });
}

BeGeqConfig BackendConfigAdapter::getGeq() const
{
	return visit([](auto *cfg) -> BeGeqConfig { return cfg->geq; });
}

void BackendConfigAdapter::setTdnInputFormat(const ImageFormatConfig &tdnInputFormat)
{
	visit([&tdnInputFormat](auto *cfg) { cfg->tdn_input_format = tdnInputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getTdnInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_input_format; });
}

void BackendConfigAdapter::setTdnDecompress(const DecompressConfig &tdnDecompress)
{
	visit([&tdnDecompress](auto *cfg) { cfg->tdn_decompress = tdnDecompress; });
}

DecompressConfig BackendConfigAdapter::getTdnDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->tdn_decompress; });
}

void BackendConfigAdapter::setTdnCompress(const CompressConfig &tdnCompress)
{
	visit([&tdnCompress](auto *cfg) { cfg->tdn_compress = tdnCompress; });
}

CompressConfig BackendConfigAdapter::getTdnCompress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->tdn_compress; });
}

void BackendConfigAdapter::setTdnOutputFormat(const ImageFormatConfig &tdnOutputFormat)
{
	visit([&tdnOutputFormat](auto *cfg) { cfg->tdn_output_format = tdnOutputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getTdnOutputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->tdn_output_format; });
}

void BackendConfigAdapter::setSdn(const BeSdnConfig &sdn)
{
	visit([&sdn](auto *cfg) {
		cfg->sdn = sdn;
		cfg->sdn.pad = 0;
	});
}

BeSdnConfig BackendConfigAdapter::getSdn() const
{
	return visit([](auto *cfg) -> BeSdnConfig { return cfg->sdn; });
}

void BackendConfigAdapter::setBlc(const BlaConfig &blc)
{
	visit([&blc](auto *cfg) {
		cfg->blc = blc;
		cfg->blc.pad[0] = cfg->blc.pad[1] = 0;
	});
}

BlaConfig BackendConfigAdapter::getBlc() const
{
	return visit([](auto *cfg) -> BlaConfig { return cfg->blc; });
}

void BackendConfigAdapter::setStitchInputFormat(const ImageFormatConfig &stitchInputFormat)
{
	visit([&stitchInputFormat](auto *cfg) { cfg->stitch_input_format = stitchInputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getStitchInputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_input_format; });
}

void BackendConfigAdapter::setStitchDecompress(const DecompressConfig &stitchDecompress)
{
	visit([&stitchDecompress](auto *cfg) { cfg->stitch_decompress = stitchDecompress; });
}

DecompressConfig BackendConfigAdapter::getStitchDecompress() const
{
	return visit([](auto *cfg) -> DecompressConfig { return cfg->stitch_decompress; });
}

void BackendConfigAdapter::setStitchCompress(const CompressConfig &stitchCompress)
{
	visit([&stitchCompress](auto *cfg) { cfg->stitch_compress = stitchCompress; });
}

CompressConfig BackendConfigAdapter::getStitchCompress() const
{
	return visit([](auto *cfg) -> CompressConfig { return cfg->stitch_compress; });
}

void BackendConfigAdapter::setStitchOutputFormat(const ImageFormatConfig &stitchOutputFormat)
{
	visit([&stitchOutputFormat](auto *cfg) { cfg->stitch_output_format = stitchOutputFormat; });
}

ImageFormatConfig BackendConfigAdapter::getStitchOutputFormat() const
{
	return visit([](auto *cfg) -> ImageFormatConfig { return cfg->stitch_output_format; });
}

void BackendConfigAdapter::setLsc(const BeLscConfig &lsc)
{
	visit([&lsc](auto *cfg) { 
		cfg->lsc.grid_step_x = lsc.grid_step_x;
		cfg->lsc.grid_step_y = lsc.grid_step_y;
		std::memcpy(cfg->lsc.lut_packed, lsc.lut_packed.data(), sizeof(cfg->lsc.lut_packed));
	});

	// Store the extras as well!
	extras_->lsc = lsc;
}

BeLscConfig BackendConfigAdapter::getLsc() const
{
	return extras_->lsc;
}

void BackendConfigAdapter::setWbg(const WbgConfig &wbg)
{
	visit([&wbg](auto *cfg) {
		cfg->wbg = wbg;
		cfg->wbg.pad[0] = cfg->wbg.pad[1] = 0;
	});
}

WbgConfig BackendConfigAdapter::getWbg() const
{
	return visit([](auto *cfg) -> WbgConfig { return cfg->wbg; });
}

void BackendConfigAdapter::setCdn(const BeCdnConfig &cdn)
{
	visit([&cdn](auto *cfg) { cfg->cdn = cdn; });
}

BeCdnConfig BackendConfigAdapter::getCdn() const
{
	return visit([](auto *cfg) -> BeCdnConfig { return cfg->cdn; });
}

void BackendConfigAdapter::setCac(const BeCacConfig &cac)
{
	visit([&cac](auto *cfg) { 
		cfg->cac.grid_step_x = cac.grid_step_x;
		cfg->cac.grid_step_y = cac.grid_step_y;
		std::memcpy(cfg->cac.lut, cac.lut.data(), sizeof(cfg->cac.lut));
	});

	// Store the extras as well!
	extras_->cac = cac;
}

BeCacConfig BackendConfigAdapter::getCac() const
{
	return extras_->cac;
}

void BackendConfigAdapter::setDebin(const BeDebinConfig &debin)
{
	visit([&debin](auto *cfg) {
		cfg->debin = debin;
		cfg->debin.pad[0] = cfg->debin.pad[1] = 0;
	});
}

BeDebinConfig BackendConfigAdapter::getDebin() const
{
	return visit([](auto *cfg) -> BeDebinConfig { return cfg->debin; });
}

void BackendConfigAdapter::setCcm(const BeCcmConfig &ccm)
{
	visit([&ccm](auto *cfg) {
		cfg->ccm = ccm;
		cfg->ccm.pad[0] = cfg->ccm.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getCcm() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ccm; });
}

void BackendConfigAdapter::setSatControl(const BeSatControlConfig &satControl)
{
	visit([&satControl](auto *cfg) {
		cfg->sat_control = satControl;
		cfg->sat_control.pad = 0;
	});
}

BeSatControlConfig BackendConfigAdapter::getSatControl() const
{
	return visit([](auto *cfg) -> BeSatControlConfig { return cfg->sat_control; });
}

void BackendConfigAdapter::setYcbcr(const BeCcmConfig &ycbcr)
{
	visit([&ycbcr](auto *cfg) {
		cfg->ycbcr = ycbcr;
		cfg->ycbcr.pad[0] = cfg->ycbcr.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getYcbcr() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr; });
}

void BackendConfigAdapter::setFalseColour(const BeFalseColourConfig &falseColour)
{
	visit([&falseColour](auto *cfg) {
		cfg->false_colour = falseColour;
		cfg->false_colour.pad[0] = cfg->false_colour.pad[1] = cfg->false_colour.pad[2] = 0;
	});
}

BeFalseColourConfig BackendConfigAdapter::getFalseColour() const
{
	return visit([](auto *cfg) -> BeFalseColourConfig { return cfg->false_colour; });
}

void BackendConfigAdapter::setSharpen(const BeSharpenConfig &sharpen)
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

BeSharpenConfig BackendConfigAdapter::getSharpen() const
{
	return visit([](auto *cfg) -> BeSharpenConfig { return cfg->sharpen; });
}

void BackendConfigAdapter::setShFcCombine(const BeShFcCombineConfig &shFcCombine)
{
	visit([&shFcCombine](auto *cfg) {
		cfg->sh_fc_combine = shFcCombine;
		cfg->sh_fc_combine.pad = 0;
	});
}

BeShFcCombineConfig BackendConfigAdapter::getShFcCombine() const
{
	return visit([](auto *cfg) -> BeShFcCombineConfig { return cfg->sh_fc_combine; });
}

void BackendConfigAdapter::setYcbcrInverse(const BeCcmConfig &ycbcrInverse)
{
	visit([&ycbcrInverse](auto *cfg) {
		cfg->ycbcr_inverse = ycbcrInverse;
		cfg->ycbcr_inverse.pad[0] = cfg->ycbcr_inverse.pad[1] = 0;
	});
}

BeCcmConfig BackendConfigAdapter::getYcbcrInverse() const
{
	return visit([](auto *cfg) -> BeCcmConfig { return cfg->ycbcr_inverse; });
}

void BackendConfigAdapter::setPreCsc(unsigned int i, const BeCcmConfig &csc)
{
	visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) {
			// Throw error? 
		},
		[&csc, i](pisp_be_config2 *cfg) {
			cfg->pre_csc[i] = csc;
			cfg->pre_csc[i].pad[0] = cfg->pre_csc[i].pad[1] = 0;
		}
	});
}

BeCcmConfig BackendConfigAdapter::getPreCsc(unsigned int i) const
{
	return visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) -> BeCcmConfig {
			// Throw error? 
			return {};
		},

		[i](pisp_be_config2 *cfg) -> BeCcmConfig { return cfg->pre_csc[i]; }
	});
}

void BackendConfigAdapter::setPostCsc(unsigned int i, const BeCcmConfig &csc)
{
	visit(overloaded {
		[&csc, i](pisp_be_config *cfg) {
			cfg->csc[i] = csc;
			cfg->csc[i].pad[0] = cfg->csc[i].pad[1] = 0;
		},
		[&csc, i](pisp_be_config2 *cfg) {
			cfg->post_csc[i] = csc;
			cfg->post_csc[i].pad[0] = cfg->post_csc[i].pad[1] = 0;
		}
	});
}

BeCcmConfig BackendConfigAdapter::getPostCsc(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> BeCcmConfig { return cfg->csc[i]; },
		[i](pisp_be_config2 *cfg) -> BeCcmConfig { return cfg->post_csc[i]; }
	});
}

void BackendConfigAdapter::setDownscale(unsigned int i, const BeDownscaleConfig &downscale)
{
	visit([&downscale, i](auto *cfg) {
		cfg->downscale[i].scale_factor_h = downscale.scale_factor_h;
		cfg->downscale[i].scale_factor_v = downscale.scale_factor_v;
		cfg->downscale[i].scale_recip_h = downscale.scale_recip_h;
		cfg->downscale[i].scale_recip_v = downscale.scale_recip_v;
	});

	// Store the extras as well!
	extras_->downscale[i] = downscale;
}

BeDownscaleConfig BackendConfigAdapter::getDownscale(unsigned int i) const
{
	return extras_->downscale[i];
}

void BackendConfigAdapter::setOutputFormat(unsigned int i, const BeOutputFormatConfig &outputFormat)
{
	visit(overloaded {
		[&outputFormat, i](pisp_be_config *cfg) {
			cfg->output_format[i].image = outputFormat.image;
			cfg->output_format[i].transform = outputFormat.transform;
			cfg->output_format[i].lo = outputFormat.lo;
			cfg->output_format[i].lo2 = outputFormat.lo2;
			cfg->output_format[i].hi = outputFormat.hi;
			cfg->output_format[i].hi2 = outputFormat.hi2;
			cfg->output_format[i].pad[0] = cfg->output_format[i].pad[1] = cfg->output_format[i].pad[2] = 0;
		},
		[&outputFormat, i](pisp_be_config2 *cfg) {
			cfg->output_format[i] = outputFormat;
		}
	});
}

BeOutputFormatConfig BackendConfigAdapter::getOutputFormat(unsigned int i) const
{
	return visit(overloaded {
		[i](pisp_be_config *cfg) -> BeOutputFormatConfig {
			BeOutputFormatConfig outputFormat {};
			outputFormat.image = cfg->output_format[i].image;
			outputFormat.transform = cfg->output_format[i].transform;
			outputFormat.lo = cfg->output_format[i].lo;
			outputFormat.lo2 = cfg->output_format[i].lo2;
			outputFormat.hi = cfg->output_format[i].hi;
			outputFormat.hi2 = cfg->output_format[i].hi2;
			return outputFormat;
		},
		[i](pisp_be_config2 *cfg) -> BeOutputFormatConfig { return cfg->output_format[i]; }
	});
}

// Variant-specific implementations
void BackendConfigAdapter::setTdn(const BeTdnConfig &tdn)
{
	visit(overloaded {
		[&tdn](pisp_be_config *cfg) {
			cfg->tdn.black_level = tdn.black_level;
			cfg->tdn.ratio = tdn.ratio;
			cfg->tdn.noise_constant = tdn.noise_constant;
			cfg->tdn.noise_slope = tdn.noise_slope;
			cfg->tdn.reset = tdn.reset;
			cfg->tdn.threshold = tdn.threshold;
			cfg->tdn.pad = 0;
		},
		[&tdn](pisp_be_config2 *cfg) {
			cfg->tdn = tdn;
		}
	});
}

BeTdnConfig BackendConfigAdapter::getTdn() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeTdnConfig {
			BeTdnConfig tdn;
			tdn.black_level = cfg->tdn.black_level;
			tdn.ratio = cfg->tdn.ratio;
			tdn.noise_constant = cfg->tdn.noise_constant;
			tdn.noise_slope = cfg->tdn.noise_slope;
			tdn.reset = cfg->tdn.reset;
			tdn.threshold = cfg->tdn.threshold;
			return tdn;
		},
		[](pisp_be_config2 *cfg) -> BeTdnConfig {
			return cfg->tdn;
		}
	});
}

void BackendConfigAdapter::setStitch(const BeStitchConfig &stitch)
{
	visit(overloaded {
		[&stitch](pisp_be_config *cfg) {
			cfg->stitch.threshold_lo = stitch.threshold_lo;
			cfg->stitch.threshold_diff_power = stitch.threshold_diff_power;
			cfg->stitch.exposure_ratio = stitch.exposure_ratio;
			cfg->stitch.motion_threshold_256 = stitch.motion_threshold_256;
			cfg->stitch.motion_threshold_recip = stitch.motion_threshold_recip;
			cfg->stitch.pad = 0;
		},
		[&stitch](pisp_be_config2 *cfg) {
			cfg->stitch.black_level = stitch.black_level;
			cfg->stitch.ratio = stitch.ratio;
			cfg->stitch.noise_constant = stitch.noise_constant;
			cfg->stitch.noise_slope = stitch.threshold;
			cfg->stitch.reset = stitch.reset;
			cfg->stitch.apply_threshold = stitch.apply_threshold;
			cfg->stitch.swap_inputs = stitch.swap_inputs;
			cfg->stitch.saturation_threshold = stitch.saturation_threshold;
			cfg->stitch.threshold = stitch.threshold;
			cfg->stitch.pad = 0;
		}
	});
}

BeStitchConfig BackendConfigAdapter::getStitch() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeStitchConfig {
			BeStitchConfig stitch;
			stitch.threshold_lo = cfg->stitch.threshold_lo;
			stitch.threshold_diff_power = cfg->stitch.threshold_diff_power;
			stitch.exposure_ratio = cfg->stitch.exposure_ratio;
			stitch.motion_threshold_256 = cfg->stitch.motion_threshold_256;
			stitch.motion_threshold_recip = cfg->stitch.motion_threshold_recip;
			return stitch;
		},
		[](pisp_be_config2 *cfg) -> BeStitchConfig {
			BeStitchConfig stitch;
			stitch.black_level = cfg->stitch.black_level;
			stitch.ratio = cfg->stitch.ratio;
			stitch.noise_constant = cfg->stitch.noise_constant;
			stitch.threshold = cfg->stitch.threshold;
			stitch.reset = cfg->stitch.reset;
			stitch.apply_threshold = cfg->stitch.apply_threshold;
			stitch.swap_inputs = cfg->stitch.swap_inputs;
			stitch.saturation_threshold = cfg->stitch.saturation_threshold;
			return stitch;
		}
	});
}

void BackendConfigAdapter::setTonemap(const BeTonemapConfig &tonemap)
{
	visit(overloaded {
		[&tonemap](pisp_be_config *cfg) {
			cfg->tonemap = tonemap;
		},
		[&tonemap]([[maybe_unused]] pisp_be_config2 *cfg) {
			// Throw exception?
		}
	});
}

BeTonemapConfig BackendConfigAdapter::getTonemap() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeTonemapConfig {
			return cfg->tonemap;
		},
		[]([[maybe_unused]] pisp_be_config2 *cfg) -> BeTonemapConfig {
			// Throw exception?
			return {};
		}
	});
}

void BackendConfigAdapter::setTonemap2(const BeTonemap2Config &tonemap)
{
	visit(overloaded {
		[&tonemap]([[maybe_unused]] pisp_be_config *cfg) {
			// Throw exception?
		},
		[&tonemap](pisp_be_config2 *cfg) {
			cfg->tonemap2 = tonemap;
			cfg->tonemap2.pad0[0] = cfg->tonemap2.pad0[1] = cfg->tonemap2.pad0[2] = 0;
			cfg->tonemap2.pad1[0] = cfg->tonemap2.pad1[1] = 0;
			cfg->tonemap2.pad2[0] = cfg->tonemap2.pad2[1] = cfg->tonemap2.pad2[2] = 0;
			cfg->tonemap2.pad3 = 0;
		}
	});
}

BeTonemap2Config BackendConfigAdapter::getTonemap2() const
{
	return visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) -> BeTonemap2Config {
			// Throw exception?
			return {};
		},
		[](pisp_be_config2 *cfg) -> BeTonemap2Config {
			return cfg->tonemap2;
		}
	});
}

void BackendConfigAdapter::setTonemap2Format(ImageFormatConfig const &format)
{
	visit(overloaded {
		[&format]([[maybe_unused]] pisp_be_config *cfg) {
			// Throw exception?
		},
		[&format](pisp_be_config2 *cfg) {
			cfg->tonemap2_format = format;
		}
	});
}

ImageFormatConfig BackendConfigAdapter::getTonemap2Format() const
{
	return visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) -> ImageFormatConfig {
			// Throw exception?
			return {};
		},
		[](pisp_be_config2 *cfg) -> ImageFormatConfig {
			return cfg->tonemap2_format;
		}
	});
}

void BackendConfigAdapter::setDemosaic(const BeDemosaicConfig &demosaic)
{
	visit(overloaded {
		[&demosaic](pisp_be_config *cfg) {
			cfg->demosaic.sharper = demosaic.sharper;
			cfg->demosaic.fc_mode = demosaic.fc_mode;
			cfg->demosaic.pad[0] = cfg->demosaic.pad[1] = 0;
		},
		[&demosaic](pisp_be_config2 *cfg) {
			cfg->demosaic = demosaic;
			cfg->demosaic.pad[0] = cfg->demosaic.pad[1] = cfg->demosaic.pad[2] = 0;
		}
	});
}

BeDemosaicConfig BackendConfigAdapter::getDemosaic() const
{
	return visit(overloaded {
		[](pisp_be_config *cfg) -> BeDemosaicConfig {
			BeDemosaicConfig demosaic {};
			demosaic.sharper = cfg->demosaic.sharper;
			demosaic.fc_mode = cfg->demosaic.fc_mode;
			return demosaic;
		},
		[](pisp_be_config2 *cfg) -> BeDemosaicConfig {
			return cfg->demosaic;
		}
	});
}

void BackendConfigAdapter::setGamma(unsigned int i, const BeGammaConfig &gamma)
{
	visit(overloaded {
		[&gamma](pisp_be_config *cfg) {
			std::memcpy(cfg->gamma.lut, gamma.lut.data(), sizeof(cfg->gamma.lut));
		},
		[i, &gamma](pisp_be_config2 *cfg) {
			std::memcpy(cfg->gamma[i].lut, gamma.lut.data(), sizeof(cfg->gamma[0].lut));
			cfg->gamma[i].pad = cfg->gamma[1].pad = 0;
		}
	});
}

BeGammaConfig BackendConfigAdapter::getGamma(unsigned int i) const
{
	BeGammaConfig gamma;

	return visit(overloaded {
		[&gamma](pisp_be_config *cfg) -> BeGammaConfig {
			std::memcpy(gamma.lut.data(), cfg->gamma.lut, sizeof(cfg->gamma.lut));
			return gamma;
		},
		[i, &gamma](pisp_be_config2 *cfg) -> BeGammaConfig {
			std::memcpy(gamma.lut.data(), cfg->gamma[i].lut, sizeof(cfg->gamma[i].lut));
			return gamma;
		}
	});
}

void BackendConfigAdapter::setResample(unsigned int i, const BeResampleConfig &resample)
{
	visit(overloaded {
		[&resample, i](pisp_be_config *cfg) {
			cfg->resample[i].scale_factor_h = resample.scale_factor_h;
			cfg->resample[i].scale_factor_v = resample.scale_factor_v;
			std::memcpy(cfg->resample[i].coef, resample.coef.data(), sizeof(cfg->resample[i].coef));
		},

		[&resample, i](pisp_be_config2 *cfg) {
			cfg->resample[i].scale_factor_h = resample.scale_factor_h;
			cfg->resample[i].scale_factor_v = resample.scale_factor_v;
			std::memcpy(cfg->resample[i].coef_h, resample.coef_h.data(), sizeof(cfg->resample[i].coef_h));
			std::memcpy(cfg->resample[i].coef_v, resample.coef_h.data(), sizeof(cfg->resample[i].coef_v));
		}
	});

	// Store the extras as well!
	extras_->resample[i] = resample;
}

BeResampleConfig BackendConfigAdapter::getResample(unsigned int i) const
{
	return extras_->resample[i];
}

void BackendConfigAdapter::setDedistort(const BeDedistortionConfig &dedistort)
{
	visit(overloaded {
		[&dedistort]([[maybe_unused]]pisp_be_config *cfg) {
			// Throw exception?
		},

		[&dedistort, this](pisp_be_config2 *cfg) {
			cfg->dedistort.grid_step_x = dedistort.grid_step_x;
			cfg->dedistort.grid_step_y = dedistort.grid_step_y;
			cfg->dedistort.pad = 0;
			std::memcpy(cfg->dedistort.lut, dedistort.lut.data(), dedistort.lut.size());
			extras_->dedistort = dedistort;
		}
	});
}

BeDedistortionConfig BackendConfigAdapter::getDedistort() const
{
	return extras_->dedistort;
}

void BackendConfigAdapter::setOutputCompress(unsigned int i, const BeCompressConfig &compress)
{
	visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) {
			// Throw exception?
		},

		[&compress, i](pisp_be_config2 *cfg) {
			cfg->compress[i] = compress;
			cfg->compress[i].pad = 0;
		}
	});
}

BeCompressConfig BackendConfigAdapter::getOutputCompress(unsigned int i) const
{
	return visit(overloaded {
		[]([[maybe_unused]] pisp_be_config *cfg) -> BeCompressConfig {
			// Throw exception?
			return {};
		},

		[i](pisp_be_config2 *cfg) {
			return cfg->compress[i];
		}
	});
}