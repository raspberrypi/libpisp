/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2025, Raspberry Pi Ltd
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <variant>

#include "bitfield.hpp"
#include "pisp_be_config.h"

namespace libpisp
{

enum class RgbEnable : uint32_t
{
	None			= 0x00000000,
	Input			= 0x00000001,
	Dedistort		= 0x00000002,
	Ccm				= 0x00000004,
	SatControl		= 0x00000008,
	Ycbcr			= 0x00000010,
	FalseColour		= 0x00000020,
	Sharpen			= 0x00000040,
	YcbcrInverse	= 0x00000080,
	PreCsc0			= 0x00000400,
	PreCsc1			= 0x00000800,
	Gamma0			= 0x00001000,
	Gamma1			= 0x00002000,
	PostCsc0		= 0x00004000,
	PostCsc1		= 0x00008000,
	Downscale0		= 0x00010000,
	Downscale1		= 0x00020000,
	Resample0		= 0x00040000,
	Resample1		= 0x00080000,
	Output0			= 0x00100000,
	Output1			= 0x00200000,
	Compress0		= 0x00400000,
	Compress1		= 0x00800000,
};

CREATE_BITFIELD(RgbEnables, RgbEnable);

enum class BayerEnable : uint32_t
{
	None 			= 0x00000000,
	Input			= 0x00000001,
	Decompress		= 0x00000002,
	Dpc				= 0x00000004,
	Geq				= 0x00000008,
	TdnInput		= 0x00000010,
	TdnDecompress	= 0x00000020,
	Tdn				= 0x00000040,
	TdnCompress		= 0x00000080,
	TdnOutput		= 0x00000100,
	Sdn				= 0x00000200,
	Blc				= 0x00000400,
	StitchInput		= 0x00000800,
	StitchDecompress = 0x00001000,
	Stitch			= 0x00002000,
	StitchCompress	= 0x00004000,
	StitchOutput	= 0x00008000,
	Wbg				= 0x00010000,
	Cdn				= 0x00020000,
	Lsc				= 0x00040000,
	Tonemap			= 0x00080000,
	Cac				= 0x00100000,
	Debin			= 0x00200000,
	Demosaic		= 0x00400000,
	TonemapInput	= 0x00800000,
	Transpose		= 0x01000000,
};

CREATE_BITFIELD(BayerEnables, BayerEnable);

enum class DirtyFlag : uint32_t
{
	None		= 0x00000000,
	Crop		= 0x00000001,
	Global		= 0x00000002,
	ShFcCombine	= 0x00000004,
};

CREATE_BITFIELD(DirtyFlags, DirtyFlag);

template <typename T>
concept EnumType = requires {
	requires std::is_enum_v<T>;
};

template <EnumType T>
T getFlagForBranch(const T field, unsigned int branch)
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(field) << branch);
}

class BackEnd;

class BackendConfigAdapter
{
public:
	BackendConfigAdapter(std::variant<pisp_be_config *, pisp_be_config2 *> config)
		: config_(config)
	{
	}
	
	template <typename F>
	auto visit(F &&f)
	{
		return std::visit(std::forward<F>(f), config_);
	}

	template <typename F>
	auto visit(F &&f) const
	{
		return std::visit(std::forward<F>(f), config_);
	}

	// global config is special case between variants!
	struct Global
	{
		BayerEnables bayerEnables;
		RgbEnables rgbEnables;
		uint8_t bayerOrder;
	};

	void setGlobal(const Global &global);
	Global getGlobal() const;

	void setInputFormat(const pisp_image_format_config &inputFormat);
	pisp_image_format_config getInputFormat() const;

	void setDecompress(const pisp_decompress_config &decompress);
	pisp_decompress_config getDecompress() const;

	void setDpc(const pisp_be_dpc_config &dpc);
	pisp_be_dpc_config getDpc() const;

	void setGeq(const pisp_be_geq_config &geq);
	pisp_be_geq_config getGeq() const;

	void setTdnInputFormat(const pisp_image_format_config &tdnInputFormat);
	pisp_image_format_config getTdnInputFormat() const;

	void setTdnDecompress(const pisp_decompress_config &tdnDecompress);
	pisp_decompress_config getTdnDecompress() const;

	void setTdnCompress(const pisp_compress_config &tdnCompress);
	pisp_compress_config getTdnCompress() const;

	void setTdnOutputFormat(const pisp_image_format_config &tdnOutputFormat);
	pisp_image_format_config getTdnOutputFormat() const;

	void setSdn(const pisp_be_sdn_config &sdn);
	pisp_be_sdn_config getSdn() const;

	void setBlc(const pisp_bla_config &blc);
	pisp_bla_config getBlc() const;

	void setStitchInputFormat(const pisp_image_format_config &stitchInputFormat);
	pisp_image_format_config getStitchInputFormat() const;

	void setStitchDecompress(const pisp_decompress_config &stitchDecompress);
	pisp_decompress_config getStitchDecompress() const;

	void setStitchCompress(const pisp_compress_config &stitchCompress);
	pisp_compress_config getStitchCompress() const;

	void setStitchOutputFormat(const pisp_image_format_config &stitchOutputFormat);
	pisp_image_format_config getStitchOutputFormat() const;

	void setLsc(const pisp_be_lsc_config &lsc);
	pisp_be_lsc_config getLsc() const;

	void setWbg(const pisp_wbg_config &wbg);
	pisp_wbg_config getWbg() const;

	void setCdn(const pisp_be_cdn_config &cdn);
	pisp_be_cdn_config getCdn() const;

	void setCac(const pisp_be_cac_config &cac);
	pisp_be_cac_config getCac() const;

	void setDebin(const pisp_be_debin_config &debin);
	pisp_be_debin_config getDebin() const;

	void setCcm(const pisp_be_ccm_config &ccm);
	pisp_be_ccm_config getCcm() const;

	void setSatControl(const pisp_be_sat_control_config &satControl);
	pisp_be_sat_control_config getSatControl() const;

	void setYcbcr(const pisp_be_ccm_config &ycbcr);
	pisp_be_ccm_config getYcbcr() const;

	void setFalseColour(const pisp_be_false_colour_config &falseColour);
	pisp_be_false_colour_config getFalseColour() const;

	void setSharpen(const pisp_be_sharpen_config &sharpen);
	pisp_be_sharpen_config getSharpen() const;

	void setShFcCombine(const pisp_be_sh_fc_combine_config &shFcCombine);
	pisp_be_sh_fc_combine_config getShFcCombine() const;

	void setYcbcrInverse(const pisp_be_ccm_config &ycbcrInverse);
	pisp_be_ccm_config getYcbcrInverse() const;

	// Array fields
	void setCsc(unsigned int i, const pisp_be_ccm_config &csc);
	pisp_be_ccm_config getCsc(unsigned int i) const;

	void setDownscale(unsigned int i, const pisp_be_downscale_config &downscale);
	pisp_be_downscale_config getDownscale(unsigned int i) const;

	void setOutputFormat(unsigned int i, const pisp_be_output_format_config &outputFormat);
	pisp_be_output_format_config getOutputFormat(unsigned int i) const;

	// Variant-specific fields - use visit
	void setTdn(const pisp_be_tdn_config &tdn);
	pisp_be_tdn_config getTdn() const;

	void setStitch(const pisp_be_stitch_config &stitch);
	pisp_be_stitch_config getStitch() const;

	void setTonemap(const pisp_be_tonemap_config &tonemap);
	pisp_be_tonemap_config getTonemap() const;

	void setDemosaic(const pisp_be_demosaic_config &demosaic);
	pisp_be_demosaic_config getDemosaic() const;

	void setGamma(const pisp_be_gamma_config &gamma);
	pisp_be_gamma_config getGamma() const;

	void setResample(unsigned int i, const pisp_be_resample_config &resample);
	pisp_be_resample_config getResample(unsigned int i) const;

	uint32_t rgbEnablesToHw(RgbEnables rgbEnables) const;
	RgbEnables hwToRgbEnables(uint32_t hwEnables) const;
	uint32_t bayerEnablesToHw(BayerEnables bayerEnables) const;
	BayerEnables hwToBayerEnables(uint32_t hwEnables) const;
	Global hwToGlobal(const pisp_be_global_config &hwGlobal) const;
	pisp_be_global_config globalToHw(const Global &global) const;

private:
	std::variant<pisp_be_config *, pisp_be_config2 *> config_;
};

} // namespace libpisp

