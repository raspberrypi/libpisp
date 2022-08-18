#include "backend.h"

#include <string.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "pisp_be_config.h"

#define DEFAULT_CONFIG_FILE "/usr/local/share/libpisp/backend_default_config.json"

// Where it might be helpful we initialise some blocks with the "obvious" default parameters. This saves users the trouble,
// and they can just "enable" the blocks.

namespace {

void initialise_debin(pisp_be_debin_config &debin)
{
	static const int8_t default_coeffs[] = { -7, 105, 35, -5 };
	static_assert(sizeof(default_coeffs) == sizeof(debin.coeffs), "Debin filter size mismatch");

	debin.h_enable = debin.v_enable = 1;
	memcpy(debin.coeffs, default_coeffs, sizeof(debin.coeffs));
}

void initialise_ycbcr(pisp_be_ccm_config &ycbcr)
{
	boost::property_tree::ptree root;
	boost::property_tree::read_json(DEFAULT_CONFIG_FILE, root);
	auto params = root.get_child("ycbcr");

	std::vector<int16_t> coeffs_v;
	for (auto &x : params.get_child("coeffs"))
	{
		coeffs_v.push_back(x.second.get_value<int16_t>());
	}
	int16_t* coeffs = &coeffs_v[0];

	std::vector<int32_t> offsets_v;
	for (auto &x : params.get_child("offsets"))
	{
		offsets_v.push_back(x.second.get_value<int32_t>());
	}
	int32_t* offsets = &offsets_v[0];

	memcpy(ycbcr.coeffs, coeffs, sizeof(ycbcr.coeffs));
	memcpy(ycbcr.offsets, offsets, sizeof(ycbcr.offsets));
}

void initialise_ycbcr_inverse(pisp_be_ccm_config &ycbcr_inverse)
{
	boost::property_tree::ptree root;
	boost::property_tree::read_json(DEFAULT_CONFIG_FILE, root);
	auto params = root.get_child("ycbcr_inverse");

	std::vector<int16_t> coeffs_v;
	for (auto &x : params.get_child("coeffs"))
	{
		coeffs_v.push_back(x.second.get_value<int16_t>());
	}
	int16_t* coeffs = &coeffs_v[0];

	std::vector<int32_t> offsets_v;
	for (auto &x : params.get_child("offsets"))
	{
		offsets_v.push_back(x.second.get_value<int32_t>());
	}
	int32_t* offsets = &offsets_v[0];

	memcpy(ycbcr_inverse.coeffs, coeffs, sizeof(ycbcr_inverse.coeffs));
	memcpy(ycbcr_inverse.offsets, offsets, sizeof(ycbcr_inverse.offsets));
}

const uint32_t gamma_lut[] = {
	// Not so much "the obvious default", just a "sane set of values".
	0x9b00000, 0xa0009b0, 0x97d13b0, 0x74d1d2d, 0x620247a, 0x5aa2a9a, 0x5913044, 0x5fb35d5,
	0x54b3bd0, 0x568411b, 0x5914683, 0x5224c14, 0x49c5136, 0x4c755d2, 0x4fb5a99, 0x4fc5f94,
	0x44b6490, 0x44b68db, 0x44c6d26, 0x3e17172, 0x3d97553, 0x3d9792c, 0x3d97d05, 0x3d980de,
	0x3b384b7, 0x37d886a, 0x37c8be7, 0x37d8f63, 0x33f92e0, 0x2e1961f, 0x2e19900, 0x2e19be1,
	0x5829ec2, 0x52fa444, 0x52fa973, 0x413aea2, 0x3fcb2b5, 0x3d8b6b1, 0x3a6ba89, 0x3a6be2f,
	0x362c1d5, 0x2fec537, 0x2fec835, 0x2a7cb33, 0x225cdda, 0x225cfff, 0x222d224, 0x216d446,
	0x42cd65c, 0x42cda88, 0x344deb4, 0x328e1f8, 0x289e520, 0x289e7a9, 0x279ea32, 0x254ecab,
	0x253eeff, 0x241f152, 0x214f393, 0x214f5a7, 0x212f7bb, 0x20ff9cd, 0x20ffbdc, 0x214fdeb
};

void initialise_gamma(pisp_be_gamma_config &gamma)
{
	static_assert(sizeof(gamma_lut) == sizeof(gamma.lut), "Gamma table size mismatch");
	memcpy(gamma.lut, gamma_lut, sizeof(gamma.lut));
}

const int16_t resample_filters[] = {
	-15, 16, 979, 71, -31, 4,
	-2, -30, 965, 132, -48, 7,
	9, -69, 939, 200, -65, 10,
	18, -99, 901, 273, -83, 14,
	24, -121, 854, 350, -101, 18,
	28, -135, 796, 429, -116, 22,
	30, -143, 731, 509, -129, 26,
	30, -143, 660, 587, -139, 29,
	29, -139, 587, 660, -143, 30,
	26, -129, 509, 731, -143, 30,
	22, -116, 429, 796, -135, 28,
	18, -101, 350, 854, -121, 24,
	14, -83, 273, 901, -99, 18,
	10, -65, 200, 939, -69, 9,
	7, -48, 132, 965, -30, -2,
	4, -31, 71, 979, 16, -15
};

void initialise_resample(pisp_be_resample_config &resample)
{
	static_assert(sizeof(resample_filters) == sizeof(resample.coef), "Resample filter size mismatch");
	memcpy(resample.coef, resample_filters, sizeof(resample.coef));
}

#define Q10(x) ((uint16_t)(x * 1024))
const pisp_be_sharpen_config sharpen_filter = {
	{ -2, -2, -2, -2, -2, // int8_t kernel0[PISP_BE_SHARPEN_SIZE*PISP_BE_SHARPEN_SIZE];
	  -1, -1, -1, -1, -1,
	  6, 6, 6, 6, 6,
	  -1, -1, -1, -1, -1,
	  -2, -2, -2, -2, -2 },
	{ 0, 0, 0 }, // int8_t pad0[3];
	{ -2, -1, 6, -1, -2, // int8_t kernel1[PISP_BE_SHARPEN_SIZE*PISP_BE_SHARPEN_SIZE];
	  -2, -1, 6, -1, -2,
	  -2, -1, 6, -1, -2,
	  -2, -1, 6, -1, -2,
	  -2, -1, 6, -1, -2 },
	{ 0, 0, 0 }, // int8_t pad1[3];
	{ 2, -1, -2, 0, 0, // int8_t kernel2[PISP_BE_SHARPEN_SIZE*PISP_BE_SHARPEN_SIZE];
	  -1, 5, -1, -2, 0,
	  -2, -1, 6, -1, -2,
	  0, -2, -1, 5, -1,
	  0, 0, -2, -1, 2 },
	{ 0, 0, 0 }, // int8_t pad2[3];
	{ 0, 0, -2, -1, 2, // int8_t kernel3[PISP_BE_SHARPEN_SIZE*PISP_BE_SHARPEN_SIZE];
	  0, -2, -1, 5, -1,
	  -2, -1, 6, -1, -2,
	  -1, 5, -1, -2, 0,
	  2, -1, -2, 0, 0 },
	{ 0, 0, 0 }, // int8_t pad3[3];
	{ -1, -2, -2, -2, -1, // int8_t kernel4[PISP_BE_SHARPEN_SIZE*PISP_BE_SHARPEN_SIZE];
	  -2, 2, 2, 2, -2,
	  -2, 2, 12, 2, -2,
	  -2, 2, 2, 2, -2,
	  -1, -2, -2, -2, -1 },
	{ 0, 0, 0 }, // int8_t pad4[3];
	100, // uint16_t threshold_offset0;
	(uint16_t)(0.3 * 2048), // uint16_t threshold_slope0;
	(uint16_t)(1.0 * 512), // uint16_t scale0;
	0, // uint16_t pad5;
	100, // uint16_t threshold_offset1;
	(uint16_t)(0.3 * 2048), // uint16_t threshold_slope1;
	(uint16_t)(1.0 * 512), // uint16_t scale1;
	0, // uint16_t pad6;
	100, // uint16_t threshold_offset2;
	(uint16_t)(0.3 * 2048), // uint16_t threshold_slope2;
	(uint16_t)(1.8 * 512), // uint16_t scale2;
	0, // uint16_t pad7;
	100, // uint16_t threshold_offset3;
	(uint16_t)(0.3 * 2048), // uint16_t threshold_slope3;
	(uint16_t)(1.8 * 512), // uint16_t scale3;
	0, // uint16_t pad8;
	200, // uint16_t threshold_offset4;
	(uint16_t)(0.27 * 2048), // uint16_t threshold_slope4;
	(uint16_t)(1.0 * 512), // uint16_t scale4;
	0, // uint16_t pad9;
	(uint16_t)(0.15 * 512), // uint16_t positive_strength;
	2000, // uint16_t positive_pre_limit;
	{ Q10(0.5), Q10(1.0), Q10(1.5), Q10(2.0), Q10(3.0), Q10(4.0), Q10(5.0), Q10(6.5), Q10(8.0) }, // uint16_t positive_func[PISP_BE_SHARPEN_FUNC_NUM_POINTS];
	4000, // uint16_t positive_limit;
	(uint16_t)(0.2 * 512), // uint16_t negative_strength;
	2000, // uint16_t negative_pre_limit;
	{ Q10(0.5), Q10(0.7), Q10(1.5), Q10(2.0), Q10(3.0), Q10(4.0), Q10(5.0), Q10(6.5), Q10(8.0) }, // uint16_t negative_func[PISP_BE_SHARPEN_FUNC_NUM_POINTS];
	4000, // uint16_t negative_limit;
	0x1f, // uint8_t enables;
	0, // uint8_t white;
	0, // uint8_t black;
	50 // uint8_t grey;
};

void initialise_sharpen(pisp_be_sharpen_config &sharpen, pisp_be_sh_fc_combine_config &shfc)
{
	memcpy(&sharpen, &sharpen_filter, sizeof(sharpen));
	memset(&shfc, 0, sizeof(shfc));
	shfc.y_factor = 0.75 * (1 << 8);
}

} // namespace

namespace PiSP {

void BackEnd::InitialiseConfig()
{
	memset(&be_config_, 0, sizeof(be_config_));
	initialise_debin(be_config_.debin);
	be_config_.dirty_flags_bayer |= PISP_BE_BAYER_ENABLE_DEBIN;

	initialise_ycbcr(be_config_.ycbcr);
	initialise_ycbcr_inverse(be_config_.ycbcr_inverse);
	initialise_gamma(be_config_.gamma);
	initialise_sharpen(be_config_.sharpen, be_config_.sh_fc_combine);
	be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_YCBCR + PISP_BE_RGB_ENABLE_YCBCR_INVERSE +
				      PISP_BE_RGB_ENABLE_GAMMA + PISP_BE_RGB_ENABLE_SHARPEN;

	for (unsigned int i = 0; i < variant_.backEndNumBranches(0); i++) {
		initialise_resample(be_config_.resample[i]);
		be_config_.dirty_flags_rgb |= PISP_BE_RGB_ENABLE_RESAMPLE(i);
	}
}

} // namespace PiSP
