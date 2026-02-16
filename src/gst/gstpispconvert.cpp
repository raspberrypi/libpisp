/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026 Raspberry Pi Ltd
 *
 * gstpispconvert.cpp - GStreamer element for PiSP hardware conversion
 */

#include "gstpispconvert.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>

#include <unistd.h>

#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

#include "common/logging.hpp"
#include "common/utils.hpp"
#include "helpers/v4l2_device.hpp"
#include "variants/variant.hpp"

using BufferRef = libpisp::helpers::BufferRef;
using Buffer = libpisp::helpers::Buffer;

GST_DEBUG_CATEGORY_STATIC(gst_pisp_convert_debug);
#define GST_CAT_DEFAULT gst_pisp_convert_debug

/* Supported GStreamer formats */
#define PISP_FORMATS "{ RGB, RGBx, BGRx, I420, YV12, Y42B, Y444, YUY2, UYVY, NV12, NV12_128C8, NV12_10LE32_128C8 }"
/* Supported DRM fourccs */
#define PISP_DRM_FORMATS "{ RG24, XB24, XR24, YU12, YV12, YU16, YU24, YUYV, UYVY, NV12, NV12:0x0700000000000004, P030:0x0700000000000004 }"

#define PISP_SRC_CAPS \
	"video/x-raw(memory:DMABuf), format=(string)DMA_DRM, drm-format=(string)" PISP_DRM_FORMATS \
	", width=(int)[1,32768], height=(int)[1,32768], framerate=(fraction)[0/1,2147483647/1]" \
	";" \
	GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_DMABUF, PISP_FORMATS) ";" \
	GST_VIDEO_CAPS_MAKE(PISP_FORMATS)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink", GST_PAD_SINK, GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		/* DMA-DRM format (GStreamer 1.24+) */
		"video/x-raw(memory:DMABuf), format=(string)DMA_DRM, drm-format=(string)" PISP_DRM_FORMATS
		", width=(int)[1,32768], height=(int)[1,32768], framerate=(fraction)[0/1,2147483647/1]"
		";" /* Regular dmabuf with standard formats */
		/* System memory */
		GST_VIDEO_CAPS_MAKE(PISP_FORMATS)));

static GstStaticPadTemplate src0_template = GST_STATIC_PAD_TEMPLATE(
	"src0", GST_PAD_SRC, GST_PAD_ALWAYS,
	GST_STATIC_CAPS(PISP_SRC_CAPS));

static GstStaticPadTemplate src1_template = GST_STATIC_PAD_TEMPLATE(
	"src1", GST_PAD_SRC, GST_PAD_ALWAYS,
	GST_STATIC_CAPS(PISP_SRC_CAPS));

#define gst_pisp_convert_parent_class parent_class
G_DEFINE_TYPE(GstPispConvert, gst_pisp_convert, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE(pispconvert, "pispconvert", GST_RANK_PRIMARY, GST_TYPE_PISP_CONVERT);

/* Bidirectional mapping between GstVideoFormat and PiSP format strings */
static const std::map<GstVideoFormat, std::string> gst_pisp_format_map = {
	{ GST_VIDEO_FORMAT_RGB, "RGB888" },
	{ GST_VIDEO_FORMAT_RGBx, "RGBX8888" },
	{ GST_VIDEO_FORMAT_BGRx, "XRGB8888" },
	{ GST_VIDEO_FORMAT_I420, "YUV420P" },
	{ GST_VIDEO_FORMAT_YV12, "YVU420P" },
	{ GST_VIDEO_FORMAT_Y42B, "YUV422P" },
	{ GST_VIDEO_FORMAT_Y444, "YUV444P" },
	{ GST_VIDEO_FORMAT_YUY2, "YUYV" },
	{ GST_VIDEO_FORMAT_UYVY, "UYVY" },
	{ GST_VIDEO_FORMAT_NV12, "YUV420SP" },
	{ GST_VIDEO_FORMAT_NV12_128C8, "YUV420SP_COL128" },
	{ GST_VIDEO_FORMAT_NV12_10LE32_128C8, "YUV420SP10_COL128" },
};

/* Bidirectional mapping between DRM fourcc and PiSP format strings */
static const std::map<std::string, std::string> drm_pisp_format_map = {
	{ "RG24", "RGB888" },
	{ "BG24", "RGB888" },
	{ "XB24", "RGBX8888" },
	{ "XR24", "XRGB8888" },
	{ "YU12", "YUV420P" },
	{ "YV12", "YVU420P" },
	{ "YU16", "YUV422P" },
	{ "YU24", "YUV444P" },
	{ "YUYV", "YUYV" },
	{ "UYVY", "UYVY" },
	{ "NV12", "YUV420SP" },
	{ "NV12:0x0700000000000004", "YUV420SP_COL128" },
	{ "P030:0x0700000000000004", "YUV420SP10_COL128" },
};

static const char *gst_format_to_pisp(GstVideoFormat format)
{
	auto it = gst_pisp_format_map.find(format);
	return it != gst_pisp_format_map.end() ? it->second.c_str() : nullptr;
}

static GstVideoFormat pisp_to_gst_video_format(const char *pisp_format)
{
	if (!pisp_format)
		return GST_VIDEO_FORMAT_UNKNOWN;

	for (const auto &[gst_fmt, pisp_fmt] : gst_pisp_format_map)
	{
		if (g_str_equal(pisp_format, pisp_fmt.c_str()))
			return gst_fmt;
	}
	return GST_VIDEO_FORMAT_UNKNOWN;
}

static const char *drm_format_to_pisp(const gchar *drm_format)
{
	if (!drm_format)
		return nullptr;

	auto it = drm_pisp_format_map.find(drm_format);
	return it != drm_pisp_format_map.end() ? it->second.c_str() : nullptr;
}

/* Helper function to check if a PiSP format string is YUV */
static bool is_yuv_format(const char *format)
{
	if (!format)
		return false;
	return format[0] == 'Y' || format[0] == 'U';
}

/* Map GStreamer colorimetry to PiSP colour space string.
 * Returns nullptr if the colorimetry is unknown/unspecified. */
static const char *colorimetry_to_pisp(const GstVideoColorimetry *colorimetry)
{
	if (!colorimetry || colorimetry->matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN)
		return nullptr;

	bool full_range = colorimetry->range == GST_VIDEO_COLOR_RANGE_0_255;

	if (colorimetry->matrix == GST_VIDEO_COLOR_MATRIX_BT2020)
		return full_range ? "bt2020_full" : "bt2020";
	if (colorimetry->matrix == GST_VIDEO_COLOR_MATRIX_BT709)
		return full_range ? "rec709_full" : "rec709";
	if (colorimetry->matrix == GST_VIDEO_COLOR_MATRIX_BT601)
		return full_range ? "jpeg" : "smpte170m";

	return nullptr;
}

/* Configure colour space conversion blocks for the backend */
static uint32_t configure_colour_conversion(libpisp::BackEnd *backend, const char *in_format,
											const char *in_colorspace, const char *out_format,
											const char *out_colorspace, unsigned int output_index)
{
	uint32_t rgb_enables = 0;

	/* YUV->RGB conversion on input */
	if (is_yuv_format(in_format))
	{
		pisp_be_ccm_config csc;
		backend->InitialiseYcbcrInverse(csc, in_colorspace);
		backend->SetCcm(csc);
		rgb_enables |= PISP_BE_RGB_ENABLE_CCM;
	}

	/* RGB->YUV conversion on output */
	if (is_yuv_format(out_format))
	{
		pisp_be_ccm_config csc;
		backend->InitialiseYcbcr(csc, out_colorspace);
		backend->SetCsc(output_index, csc);
		rgb_enables |= PISP_BE_RGB_ENABLE_CSC(output_index);
	}
	else if (g_str_equal(out_format, "RGB888") ||
			 g_str_equal(out_format, "RGBX8888") ||
			 g_str_equal(out_format, "XRGB8888"))
	{
		/* R/B channel swap to match GStreamer/DRM byte ordering */
		pisp_be_ccm_config csc = {};
		csc.coeffs[2] = csc.coeffs[4] = csc.coeffs[6] = 1 << 10;
		backend->SetCsc(output_index, csc);
		rgb_enables |= PISP_BE_RGB_ENABLE_CSC(output_index);
	}

	return rgb_enables;
}

/* GObject vmethod implementations */
static void gst_pisp_convert_finalize(GObject *object);

/* GstElement vmethod implementations */
static GstStateChangeReturn gst_pisp_convert_change_state(GstElement *element, GstStateChange transition);

/* Pad functions */
static gboolean gst_pisp_convert_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);
static GstFlowReturn gst_pisp_convert_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);
static gboolean gst_pisp_convert_src_event(GstPad *pad, GstObject *parent, GstEvent *event);
static gboolean gst_pisp_convert_src_query(GstPad *pad, GstObject *parent, GstQuery *query);
static gboolean gst_pisp_convert_sink_query(GstPad *pad, GstObject *parent, GstQuery *query);

/* Internal functions */
static gboolean gst_pisp_convert_start(GstPispConvert *self);
static gboolean gst_pisp_convert_stop(GstPispConvert *self);
static gboolean gst_pisp_convert_configure(GstPispConvert *self);
static gboolean gst_pisp_convert_setup_output_pool(GstPispConvert *self, guint index);

enum
{
	PROP_0,
	PROP_OUTPUT_BUFFER_COUNT,
	PROP_CROP,
	PROP_CROP0,
	PROP_CROP1,
	N_PROPERTIES
};

static void gst_pisp_convert_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_pisp_convert_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

/* Initialize the class */
static void gst_pisp_convert_class_init(GstPispConvertClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gobject_class->finalize = gst_pisp_convert_finalize;
	gobject_class->set_property = gst_pisp_convert_set_property;
	gobject_class->get_property = gst_pisp_convert_get_property;

	gst_element_class_set_static_metadata(element_class, "PiSP Hardware Image Converter",
										  "Filter/Converter/Video/Scaler",
										  "Hardware accelerated format conversion and scaling using libpisp",
										  "Raspberry Pi");

	gst_element_class_add_static_pad_template(element_class, &sink_template);
	gst_element_class_add_static_pad_template(element_class, &src0_template);
	gst_element_class_add_static_pad_template(element_class, &src1_template);

	g_object_class_install_property(
		gobject_class, PROP_OUTPUT_BUFFER_COUNT,
		g_param_spec_uint("output-buffer-count", "Output buffer count",
						  "Number of backend buffers to allocate (round-robin)", 1, 32, 4,
						  static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(
		gobject_class, PROP_CROP,
		g_param_spec_string("crop", "Crop region for all outputs",
							"Crop region as 'x,y,width,height' applied to all outputs (0,0,0,0 = full input)",
							"0,0,0,0",
							static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(
		gobject_class, PROP_CROP0,
		g_param_spec_string("crop0", "Crop region for output 0",
							"Crop region as 'x,y,width,height' (0,0,0,0 = no crop / full input)",
							"0,0,0,0",
							static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(
		gobject_class, PROP_CROP1,
		g_param_spec_string("crop1", "Crop region for output 1",
							"Crop region as 'x,y,width,height' (0,0,0,0 = no crop / full input)",
							"0,0,0,0",
							static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	element_class->change_state = GST_DEBUG_FUNCPTR(gst_pisp_convert_change_state);

	GST_DEBUG_CATEGORY_INIT(gst_pisp_convert_debug, "pispconvert", 0, "PiSP hardware converter");
}

static void gst_pisp_convert_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstPispConvert *self = GST_PISP_CONVERT(object);

	switch (prop_id)
	{
	case PROP_OUTPUT_BUFFER_COUNT:
		self->priv->output_buffer_count = g_value_get_uint(value);
		break;
	case PROP_CROP:
	case PROP_CROP0:
	case PROP_CROP1:
	{
		const gchar *str = g_value_get_string(value);
		guint x, y, w, h;
		if (str && sscanf(str, "%u,%u,%u,%u", &x, &y, &w, &h) == 4)
		{
			pisp_be_crop_config crop_cfg = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
			if (prop_id == PROP_CROP)
			{
				self->priv->crop[0] = crop_cfg;
				self->priv->crop[1] = crop_cfg;
			}
			else
			{
				guint idx = (prop_id == PROP_CROP0) ? 0 : 1;
				self->priv->crop[idx] = crop_cfg;
			}
		}
		else
			GST_WARNING_OBJECT(self, "Invalid %s format '%s', expected 'x,y,width,height'", pspec->name, str);

		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gst_pisp_convert_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstPispConvert *self = GST_PISP_CONVERT(object);

	switch (prop_id)
	{
	case PROP_OUTPUT_BUFFER_COUNT:
		g_value_set_uint(value, self->priv->output_buffer_count);
		break;
	case PROP_CROP:
	case PROP_CROP0:
	case PROP_CROP1:
	{
		guint idx = (prop_id == PROP_CROP1) ? 1 : 0;
		gchar *str = g_strdup_printf("%u,%u,%u,%u", self->priv->crop[idx].offset_x, self->priv->crop[idx].offset_y,
									 self->priv->crop[idx].width, self->priv->crop[idx].height);
		g_value_take_string(value, str);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* Initialize the element instance */
static void gst_pisp_convert_init(GstPispConvert *self)
{
	/* Create and configure sink pad */
	self->sinkpad = gst_pad_new_from_static_template(&sink_template, "sink");
	gst_pad_set_chain_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_pisp_convert_chain));
	gst_pad_set_event_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_pisp_convert_sink_event));
	gst_pad_set_query_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_pisp_convert_sink_query));
	gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);

	/* Create and configure src0 pad (primary output) */
	self->srcpad[0] = gst_pad_new_from_static_template(&src0_template, "src0");
	gst_pad_set_event_function(self->srcpad[0], GST_DEBUG_FUNCPTR(gst_pisp_convert_src_event));
	gst_pad_set_query_function(self->srcpad[0], GST_DEBUG_FUNCPTR(gst_pisp_convert_src_query));
	gst_element_add_pad(GST_ELEMENT(self), self->srcpad[0]);

	/* Create and configure src1 pad (secondary output) */
	self->srcpad[1] = gst_pad_new_from_static_template(&src1_template, "src1");
	gst_pad_set_event_function(self->srcpad[1], GST_DEBUG_FUNCPTR(gst_pisp_convert_src_event));
	gst_pad_set_query_function(self->srcpad[1], GST_DEBUG_FUNCPTR(gst_pisp_convert_src_query));
	gst_element_add_pad(GST_ELEMENT(self), self->srcpad[1]);

	/* Initialize private data */
	self->priv = new GstPispConvertPrivate();
	self->priv->backend_device = nullptr;
	self->priv->backend = nullptr;
	self->priv->media_dev_path = nullptr;
	self->priv->configured = FALSE;
	self->priv->dmabuf_allocator = gst_dmabuf_allocator_new();
	self->priv->use_dmabuf_input = FALSE;

	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		self->priv->out_width[i] = 0;
		self->priv->out_height[i] = 0;
		self->priv->out_stride[i] = 0;
		self->priv->out_hw_stride[i] = 0;
		self->priv->out_format[i] = nullptr;
		self->priv->output_enabled[i] = FALSE;
		self->priv->use_dmabuf_output[i] = FALSE;
		self->priv->output_pool[i] = nullptr;
		self->priv->src_caps[i] = nullptr;
	}

	self->priv->pending_segment = nullptr;
	self->priv->output_buffer_count = 4;
	self->priv->buffer_slice_index = 0;
}

static void gst_pisp_convert_finalize(GObject *object)
{
	GstPispConvert *self = GST_PISP_CONVERT(object);

	if (self->priv)
	{
		if (self->priv->dmabuf_allocator)
		{
			gst_object_unref(self->priv->dmabuf_allocator);
			self->priv->dmabuf_allocator = nullptr;
		}

		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (self->priv->output_pool[i])
			{
				gst_object_unref(self->priv->output_pool[i]);
				self->priv->output_pool[i] = nullptr;
			}
			if (self->priv->src_caps[i])
			{
				gst_caps_unref(self->priv->src_caps[i]);
				self->priv->src_caps[i] = nullptr;
			}
		}

		if (self->priv->pending_segment)
		{
			gst_event_unref(self->priv->pending_segment);
			self->priv->pending_segment = nullptr;
		}

		g_free(self->priv->media_dev_path);
		delete self->priv;
		self->priv = nullptr;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/*
 * Parse output caps for a specific output index.
 * Returns TRUE if caps are valid and were parsed successfully.
 */
static gboolean parse_output_caps(GstPispConvert *self, guint index, GstCaps *caps)
{
	GstVideoInfo out_info;

	if (!caps || gst_caps_is_empty(caps))
		return FALSE;

	GstCapsFeatures *out_features = gst_caps_get_features(caps, 0);
	self->priv->use_dmabuf_output[index] = out_features &&
										   gst_caps_features_contains(out_features, GST_CAPS_FEATURE_MEMORY_DMABUF);

	GstStructure *out_structure = gst_caps_get_structure(caps, 0);
	const gchar *out_format_str = gst_structure_get_string(out_structure, "format");
	gboolean out_is_drm = out_format_str && g_str_equal(out_format_str, "DMA_DRM");

	if (out_is_drm)
	{
		const gchar *drm_format = gst_structure_get_string(out_structure, "drm-format");
		gst_structure_get_int(out_structure, "width", (gint *)&self->priv->out_width[index]);
		gst_structure_get_int(out_structure, "height", (gint *)&self->priv->out_height[index]);
		self->priv->out_format[index] = drm_format_to_pisp(drm_format);
		self->priv->out_stride[index] = 0;

		GstVideoColorimetry colorimetry = {};
		const gchar *colorimetry_str = gst_structure_get_string(out_structure, "colorimetry");
		if (colorimetry_str)
			gst_video_colorimetry_from_string(&colorimetry, colorimetry_str);
		self->priv->out_colorspace[index] = colorimetry_to_pisp(&colorimetry);

		GST_INFO_OBJECT(self, "Output%u DMA-DRM format: drm-format=%s, pisp=%s, colorspace=%s (matrix=%d, range=%d)",
						index, drm_format, self->priv->out_format[index], self->priv->out_colorspace[index],
						colorimetry.matrix, colorimetry.range);
	}
	else
	{
		if (!gst_video_info_from_caps(&out_info, caps))
		{
			GST_ERROR_OBJECT(self, "Failed to parse output%u caps", index);
			return FALSE;
		}
		self->priv->out_width[index] = GST_VIDEO_INFO_WIDTH(&out_info);
		self->priv->out_height[index] = GST_VIDEO_INFO_HEIGHT(&out_info);
		self->priv->out_stride[index] = GST_VIDEO_INFO_PLANE_STRIDE(&out_info, 0);
		self->priv->out_format[index] = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&out_info));
		self->priv->out_colorspace[index] = colorimetry_to_pisp(&GST_VIDEO_INFO_COLORIMETRY(&out_info));
		GST_INFO_OBJECT(self, "Output%u format: pisp=%s, colorspace=%s (matrix=%d, range=%d)", index,
						self->priv->out_format[index], self->priv->out_colorspace[index],
						GST_VIDEO_INFO_COLORIMETRY(&out_info).matrix, GST_VIDEO_INFO_COLORIMETRY(&out_info).range);
	}

	if (!self->priv->out_format[index])
	{
		GST_ERROR_OBJECT(self, "Unsupported output%u format", index);
		return FALSE;
	}

	self->priv->output_enabled[index] = TRUE;
	return TRUE;
}

/*
 * Parse input caps from the sink pad.
 */
static gboolean parse_input_caps(GstPispConvert *self, GstCaps *caps)
{
	GstVideoInfo in_info;

	GST_DEBUG_OBJECT(self, "parse_input_caps: caps=%" GST_PTR_FORMAT, caps);

	GstCapsFeatures *in_features = gst_caps_get_features(caps, 0);
	self->priv->use_dmabuf_input = in_features &&
								   gst_caps_features_contains(in_features, GST_CAPS_FEATURE_MEMORY_DMABUF);
	GST_DEBUG_OBJECT(self, "in_features=%p, use_dmabuf_input=%d", in_features, self->priv->use_dmabuf_input);

	GstStructure *in_structure = gst_caps_get_structure(caps, 0);
	const gchar *in_format_str = gst_structure_get_string(in_structure, "format");
	gboolean in_is_drm = in_format_str && g_str_equal(in_format_str, "DMA_DRM");

	if (in_is_drm)
	{
		const gchar *drm_format = gst_structure_get_string(in_structure, "drm-format");
		gst_structure_get_int(in_structure, "width", (gint *)&self->priv->in_width);
		gst_structure_get_int(in_structure, "height", (gint *)&self->priv->in_height);
		self->priv->in_format = drm_format_to_pisp(drm_format);
		self->priv->in_stride = 0;

		GstVideoColorimetry colorimetry = {};
		const gchar *colorimetry_str = gst_structure_get_string(in_structure, "colorimetry");
		if (colorimetry_str)
			gst_video_colorimetry_from_string(&colorimetry, colorimetry_str);
		self->priv->in_colorspace = colorimetry_to_pisp(&colorimetry);

		GST_INFO_OBJECT(self, "Input DMA-DRM format: drm-format=%s, pisp=%s, colorspace=%s (matrix=%d, range=%d)",
						drm_format, self->priv->in_format, self->priv->in_colorspace,
						colorimetry.matrix, colorimetry.range);
	}
	else
	{
		if (!gst_video_info_from_caps(&in_info, caps))
		{
			GST_ERROR_OBJECT(self, "Failed to parse input caps");
			return FALSE;
		}
		self->priv->in_width = GST_VIDEO_INFO_WIDTH(&in_info);
		self->priv->in_height = GST_VIDEO_INFO_HEIGHT(&in_info);
		self->priv->in_stride = GST_VIDEO_INFO_PLANE_STRIDE(&in_info, 0);
		self->priv->in_format = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&in_info));
		self->priv->in_colorspace = colorimetry_to_pisp(&GST_VIDEO_INFO_COLORIMETRY(&in_info));
		GST_INFO_OBJECT(self, "Input format: pisp=%s, colorspace=%s (matrix=%d, range=%d)",
						self->priv->in_format, self->priv->in_colorspace,
						GST_VIDEO_INFO_COLORIMETRY(&in_info).matrix, GST_VIDEO_INFO_COLORIMETRY(&in_info).range);
	}

	if (!self->priv->in_format)
	{
		GST_ERROR_OBJECT(self, "Unsupported input format");
		return FALSE;
	}

	return TRUE;
}

/* Helper functions for dmabuf support */
static gboolean gst_buffer_is_dmabuf(GstBuffer *buffer)
{
	GstMemory *mem;

	if (gst_buffer_n_memory(buffer) == 0)
		return FALSE;

	mem = gst_buffer_peek_memory(buffer, 0);
	return gst_is_dmabuf_memory(mem);
}

/* Helper function to create Buffer object from GStreamer dmabuf buffer */
static std::optional<libpisp::helpers::Buffer> gst_to_libpisp_buffer(GstBuffer *buffer)
{
	std::array<int, 3> fds = { -1, -1, -1 };
	std::array<size_t, 3> sizes = { 0, 0, 0 };
	guint n_mem = gst_buffer_n_memory(buffer);

	for (guint i = 0; i < std::min(n_mem, 3u); i++)
	{
		GstMemory *mem = gst_buffer_peek_memory(buffer, i);
		if (!gst_is_dmabuf_memory(mem))
			return std::nullopt;

		int raw = gst_dmabuf_memory_get_fd(mem);
		fds[i] = dup(raw);
		if (fds[i] < 0)
		{
			for (guint j = 0; j < i; j++)
			{
				if (fds[j] >= 0)
					close(fds[j]);
			}
			return std::nullopt;
		}
		sizes[i] = mem->size;
	}

	return libpisp::helpers::Buffer(fds, sizes);
}

/* Create GstBuffer (dmabuf) from libpisp backend Buffer; dups FDs so backend keeps its ref */
static GstBuffer *libpisp_to_gst_dmabuf(const Buffer &buffer, GstAllocator *dmabuf_allocator)
{
	GstBuffer *gstbuf = gst_buffer_new();
	if (!gstbuf)
		return nullptr;

	for (unsigned int p = 0; p < 3; p++)
	{
		int fd = buffer.Fd()[p];
		size_t size = buffer.Size()[p];
		if (fd < 0 || size == 0)
			continue;

		int dup_fd = dup(fd);
		if (dup_fd < 0)
		{
			GST_WARNING("dup(plane %u) failed: %s", p, strerror(errno));
			gst_buffer_unref(gstbuf);
			return nullptr;
		}

		GstMemory *mem = gst_dmabuf_allocator_alloc(dmabuf_allocator, dup_fd, size);
		if (!mem)
		{
			close(dup_fd);
			gst_buffer_unref(gstbuf);
			return nullptr;
		}
		gst_buffer_append_memory(gstbuf, mem);
	}

	return gstbuf;
}

/* Attach GstVideoMeta with the correct hardware stride to a dmabuf output buffer */
static void add_video_meta(GstBuffer *buffer, const char *pisp_format, guint width, guint height, guint hw_stride)
{
	GstVideoFormat gst_fmt = pisp_to_gst_video_format(pisp_format);
	if (gst_fmt == GST_VIDEO_FORMAT_UNKNOWN)
		return;

	GstVideoInfo vinfo;
	gst_video_info_set_format(&vinfo, gst_fmt, width, height);

	gsize offsets[GST_VIDEO_MAX_PLANES] = {};
	gint strides[GST_VIDEO_MAX_PLANES] = {};
	gsize offset = 0;
	guint n_planes = GST_VIDEO_INFO_N_PLANES(&vinfo);
	GstMemory *mem;

	for (guint p = 0; p < n_planes; p++)
	{
		offsets[p] = offset;
		strides[p] = hw_stride * GST_VIDEO_INFO_PLANE_STRIDE(&vinfo, p) /
					 GST_VIDEO_INFO_PLANE_STRIDE(&vinfo, 0);
		mem = gst_buffer_peek_memory (buffer, p);
		offset += mem->size;
	}

	gst_buffer_add_video_meta_full(buffer, GST_VIDEO_FRAME_FLAG_NONE, gst_fmt,
								   width, height, n_planes, offsets, strides);
}

static void copy_planes(std::array<uint8_t *, 3> src, guint src_stride, std::array<uint8_t *, 3> dst, guint dst_stride,
						guint width, guint height, const char *format)
{
	GST_DEBUG("copy_planes: %ux%u, src_stride=%u, dst_stride=%u, format=%s", width, height, src_stride, dst_stride,
			  format);

	/* YUV420SP_COL128 (NV12 column 128) - special tiled format */
	if (strncmp(format, "YUV420SP_COL128", 15) == 0 || strncmp(format, "YUV420SP10_COL128", 17) == 0)
	{
		guint y_size = GST_VIDEO_TILE_X_TILES(src_stride) * 128 * GST_VIDEO_TILE_Y_TILES(src_stride) * 8;
		memcpy(dst[0], src[0], y_size);

		uint8_t *src_uv = src[1] ? src[1] : src[0] + y_size;
		uint8_t *dst_uv = dst[1] ? dst[1] : dst[0] + y_size;
		memcpy(dst_uv, src_uv, y_size / 2);
		return;
	}

	/* Planar YUV formats: YUV420P, YVU420P, YUV422P, YUV444P */
	if (is_yuv_format(format) && strstr(format, "P") != nullptr)
	{
		/* Copy Y plane line by line */
		for (guint y = 0; y < height; ++y)
			memcpy(dst[0] + y * dst_stride, src[0] + y * src_stride, width);

		/* Determine UV subsampling */
		guint uv_width, uv_height;
		if (strstr(format, "420") != nullptr)
		{
			uv_width = width / 2;
			uv_height = height / 2;
		}
		else if (strstr(format, "422") != nullptr)
		{
			uv_width = width / 2;
			uv_height = height;
		}
		else /* 444 */
		{
			uv_width = width;
			uv_height = height;
		}

		guint src_uv_stride = (uv_width == width) ? src_stride : src_stride / 2;
		guint dst_uv_stride = (uv_width == width) ? dst_stride : dst_stride / 2;

		/* Calculate plane pointers if not explicitly provided (single contiguous buffer) */
		uint8_t *src_u = src[1] ? src[1] : src[0] + src_stride * height;
		uint8_t *src_v = src[2] ? src[2] : src_u + src_uv_stride * uv_height;
		uint8_t *dst_u = dst[1] ? dst[1] : dst[0] + dst_stride * height;
		uint8_t *dst_v = dst[2] ? dst[2] : dst_u + dst_uv_stride * uv_height;

		/* Copy U and V planes */
		for (guint y = 0; y < uv_height; ++y)
		{
			memcpy(dst_u + y * dst_uv_stride, src_u + y * src_uv_stride, uv_width);
			memcpy(dst_v + y * dst_uv_stride, src_v + y * src_uv_stride, uv_width);
		}
		return;
	}

	/* Packed formats: YUYV, UYVY (2 bpp), RGB888 (3 bpp), RGBX (4 bpp) */
	guint bytes_per_pixel = is_yuv_format(format) ? 2 : (strstr(format, "RGB888") != nullptr) ? 3 : 4;
	guint line_stride = width * bytes_per_pixel;

	for (guint y = 0; y < height; ++y)
		memcpy(dst[0] + y * dst_stride, src[0] + y * src_stride, line_stride);
}

static void copy_buffer_to_pisp(GstBuffer *gstbuf, std::array<uint8_t *, 3> &mem, guint width, guint height,
								guint gst_stride, guint hw_stride, const char *format)
{
	GstMapInfo map;
	gst_buffer_map(gstbuf, &map, GST_MAP_READ);

	/* GstBuffer is always contiguous - planes calculated from offsets */
	std::array<uint8_t *, 3> src = { map.data, nullptr, nullptr };

	copy_planes(src, gst_stride, mem, hw_stride, width, height, format);

	gst_buffer_unmap(gstbuf, &map);
}

static void copy_pisp_to_buffer(const std::array<uint8_t *, 3> &mem, GstBuffer *gstbuf, guint width, guint height,
								guint gst_stride, guint hw_stride, const char *format)
{
	GstMapInfo map;
	gst_buffer_map(gstbuf, &map, GST_MAP_WRITE);

	/* GstBuffer is always contiguous - planes calculated from offsets */
	std::array<uint8_t *, 3> dst = { map.data, nullptr, nullptr };

	copy_planes(const_cast<std::array<uint8_t *, 3> &>(mem), hw_stride, dst, gst_stride, width, height, format);

	gst_buffer_unmap(gstbuf, &map);
}

/*
 * Configure the PiSP backend for the current input/output settings.
 */
static gboolean gst_pisp_convert_configure(GstPispConvert *self)
{
	if (!self->priv->backend_device || !self->priv->backend)
	{
		GST_ERROR_OBJECT(self, "Backend not initialized");
		return FALSE;
	}

	/* Check which outputs are enabled */
	gboolean any_output = FALSE;
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		self->priv->output_enabled[i] = gst_pad_is_linked(self->srcpad[i]);
		if (self->priv->output_enabled[i])
			any_output = TRUE;
	}

	if (!any_output)
	{
		GST_ERROR_OBJECT(self, "No outputs are linked");
		return FALSE;
	}

	/* Get output caps from linked pads */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (!self->priv->output_enabled[i])
			continue;

		GstCaps *peer_caps = gst_pad_peer_query_caps(self->srcpad[i], nullptr);
		if (!peer_caps || gst_caps_is_empty(peer_caps))
		{
			if (peer_caps)
				gst_caps_unref(peer_caps);
			GST_ERROR_OBJECT(self, "Failed to get caps for output%d", i);
			return FALSE;
		}

		GstCaps *fixed_caps = gst_caps_fixate(peer_caps);
		if (!parse_output_caps(self, i, fixed_caps))
		{
			gst_caps_unref(fixed_caps);
			return FALSE;
		}

		/* Store caps and send downstream */
		if (self->priv->src_caps[i])
			gst_caps_unref(self->priv->src_caps[i]);
		self->priv->src_caps[i] = fixed_caps;

		gst_pad_push_event(self->srcpad[i], gst_event_new_caps(fixed_caps));
	}

	/* Forward pending segment event now that caps have been sent */
	if (self->priv->pending_segment)
	{
		GST_DEBUG_OBJECT(self, "Forwarding stored segment event");
		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (self->priv->output_enabled[i])
				gst_pad_push_event(self->srcpad[i], gst_event_ref(self->priv->pending_segment));
		}
		gst_event_unref(self->priv->pending_segment);
		self->priv->pending_segment = nullptr;
	}

	try
	{
		pisp_be_global_config global;
		self->priv->backend->GetGlobal(global);
		global.bayer_enables = 0;
		global.rgb_enables = PISP_BE_RGB_ENABLE_INPUT;

		/* Configure input format */
		pisp_image_format_config input_cfg = {};
		input_cfg.width = self->priv->in_width;
		input_cfg.height = self->priv->in_height;
		input_cfg.format = libpisp::get_pisp_image_format(self->priv->in_format);
		if (!input_cfg.format)
		{
			GST_ERROR_OBJECT(self, "Failed to get input format");
			return FALSE;
		}
		libpisp::compute_stride(input_cfg);
		self->priv->in_hw_stride = input_cfg.stride;
		self->priv->backend->SetInputFormat(input_cfg);

		GST_INFO_OBJECT(self, "Input: %ux%u %s (stride: gst=%u hw=%u) colorspace %s", self->priv->in_width, self->priv->in_height,
						self->priv->in_format, self->priv->in_stride, self->priv->in_hw_stride, self->priv->in_colorspace);

		if (!self->priv->in_colorspace)
			self->priv->in_colorspace = "jpeg";

		/* Configure each enabled output - first pass: formats and enables */
		pisp_be_output_format_config output_cfg[PISP_NUM_OUTPUTS] = {};
		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (!self->priv->output_enabled[i])
				continue;

			global.rgb_enables |= PISP_BE_RGB_ENABLE_OUTPUT(i);

			output_cfg[i].image.width = self->priv->out_width[i];
			output_cfg[i].image.height = self->priv->out_height[i];
			output_cfg[i].image.format = libpisp::get_pisp_image_format(self->priv->out_format[i]);
			if (!output_cfg[i].image.format)
			{
				GST_ERROR_OBJECT(self, "Failed to get output%d format", i);
				return FALSE;
			}
			libpisp::compute_optimal_stride(output_cfg[i].image, true);
			self->priv->out_hw_stride[i] = output_cfg[i].image.stride;
			self->priv->backend->SetOutputFormat(i, output_cfg[i]);

			if ((g_str_equal(self->priv->out_format[i], "RGBX8888") ||
				 g_str_equal(self->priv->out_format[i], "XRGB8888")) && !self->priv->variant->BackendRGB32Supported(0))
				GST_WARNING_OBJECT(self, "pisp_be HW does not support 32-bit RGB output, the image will be corrupt.");

			if (!self->priv->out_colorspace[i])
				self->priv->out_colorspace[i] = self->priv->in_colorspace;

			global.rgb_enables |= configure_colour_conversion(self->priv->backend.get(),
								self->priv->in_format, self->priv->in_colorspace,
								self->priv->out_format[i], self->priv->out_colorspace[i], i);

			GST_INFO_OBJECT(self, "Output%d: %ux%u %s (stride: gst=%u hw=%u) colorspace %s", i, self->priv->out_width[i],
							self->priv->out_height[i], self->priv->out_format[i], self->priv->out_stride[i],
							self->priv->out_hw_stride[i], self->priv->out_colorspace[i]);
		}

		self->priv->backend->SetGlobal(global);

		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (!self->priv->output_enabled[i])
				continue;

			pisp_be_crop_config crop = self->priv->crop[i];

			/* Default to full input if width/height not specified */
			if (!crop.width)
				crop.width = input_cfg.width;
			if (!crop.height)
				crop.height = input_cfg.height;

			/* Clip crop region to fit within input */
			if (crop.offset_x >= input_cfg.width)
				crop.offset_x = 0;
			if (crop.offset_y >= input_cfg.height)
				crop.offset_y = 0;
			if (crop.offset_x + crop.width > input_cfg.width)
				crop.width = input_cfg.width - crop.offset_x;
			if (crop.offset_y + crop.height > input_cfg.height)
				crop.height = input_cfg.height - crop.offset_y;

			GST_INFO_OBJECT(self, "Crop%u: offset=(%u,%u) size=%ux%u", i, crop.offset_x, crop.offset_y, crop.width, crop.height);

			/* Only call SetCrop if parameters changed */
			if (memcmp(&crop, &self->priv->applied_crop[i], sizeof(crop)) != 0)
			{
				self->priv->backend->SetCrop(i, crop);
				self->priv->applied_crop[i] = crop;
			}

			/* Only call SetSmartResize if parameters changed */
			libpisp::BackEnd::SmartResize smart_resize = { (uint16_t)output_cfg[i].image.width,
														   (uint16_t)output_cfg[i].image.height };
			if (memcmp(&smart_resize, &self->priv->applied_smart_resize[i], sizeof(smart_resize)) != 0)
			{
				self->priv->backend->SetSmartResize(i, smart_resize);
				self->priv->applied_smart_resize[i] = smart_resize;
			}
		}

		/* Prepare the hardware configuration */
		pisp_be_tiles_config config = {};
		self->priv->backend->Prepare(&config);
		self->priv->backend_device->Setup(config, self->priv->output_buffer_count);

		self->priv->pisp_buffers = self->priv->backend_device->GetBuffers();
		self->priv->buffer_slice_index = 0;
		self->priv->configured = TRUE;

		GST_INFO_OBJECT(self, "Backend configured successfully with %u output(s), %u buffer(s)",
						self->priv->output_enabled[1] ? 2 : 1, self->priv->output_buffer_count);
	}
	catch (const std::exception &e)
	{
		GST_ERROR_OBJECT(self, "Failed to configure backend: %s", e.what());
		return FALSE;
	}

	return TRUE;
}

/*
 * Setup output buffer pool for the specified output index.
 */
static gboolean gst_pisp_convert_setup_output_pool(GstPispConvert *self, guint index)
{
	if (self->priv->output_pool[index])
	{
		gst_buffer_pool_set_active(self->priv->output_pool[index], FALSE);
		gst_object_unref(self->priv->output_pool[index]);
		self->priv->output_pool[index] = nullptr;
	}

	if (!self->priv->src_caps[index])
		return FALSE;

	GstBufferPool *pool = gst_video_buffer_pool_new();
	GstStructure *config = gst_buffer_pool_get_config(pool);

	/* Calculate buffer size */
	gsize size;
	GstStructure *structure = gst_caps_get_structure(self->priv->src_caps[index], 0);
	const gchar *format_str = gst_structure_get_string(structure, "format");

	if (format_str && g_str_equal(format_str, "DMA_DRM"))
	{
		size = self->priv->out_width[index] * self->priv->out_height[index] * 4;
	}
	else
	{
		GstVideoInfo vinfo;
		if (!gst_video_info_from_caps(&vinfo, self->priv->src_caps[index]))
		{
			gst_object_unref(pool);
			return FALSE;
		}
		size = GST_VIDEO_INFO_SIZE(&vinfo);
	}

	gst_buffer_pool_config_set_params(config, self->priv->src_caps[index], size, 4, 0);
	gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);

	if (!gst_buffer_pool_set_config(pool, config))
	{
		GST_ERROR_OBJECT(self, "Failed to set buffer pool config for output%u", index);
		gst_object_unref(pool);
		return FALSE;
	}

	if (!gst_buffer_pool_set_active(pool, TRUE))
	{
		GST_ERROR_OBJECT(self, "Failed to activate buffer pool for output%u", index);
		gst_object_unref(pool);
		return FALSE;
	}

	self->priv->output_pool[index] = pool;
	GST_DEBUG_OBJECT(self, "Created buffer pool for output%u", index);
	return TRUE;
}

/*
 * Chain function - process incoming buffer and push to output pads.
 */
static GstFlowReturn gst_pisp_convert_chain(GstPad *pad [[maybe_unused]], GstObject *parent, GstBuffer *inbuf)
{
	GstPispConvert *self = GST_PISP_CONVERT(parent);
	GstFlowReturn ret = GST_FLOW_OK;
	GstBuffer *outbuf[PISP_NUM_OUTPUTS] = { nullptr, nullptr };

	/* Configure on first buffer if not already configured */
	if (!self->priv->configured)
	{
		if (!gst_pisp_convert_configure(self))
		{
			gst_buffer_unref(inbuf);
			return GST_FLOW_ERROR;
		}

		/* Setup output buffer pools (only for non-dmabuf outputs) */
		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (self->priv->output_enabled[i] && !self->priv->use_dmabuf_output[i])
			{
				if (!gst_pisp_convert_setup_output_pool(self, i))
				{
					gst_buffer_unref(inbuf);
					return GST_FLOW_ERROR;
				}
			}
		}
	}

	/* Acquire output buffers from pool only for non-dmabuf outputs */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (!self->priv->output_enabled[i] || self->priv->use_dmabuf_output[i])
			continue;

		GstFlowReturn acquire_ret = gst_buffer_pool_acquire_buffer(self->priv->output_pool[i], &outbuf[i], nullptr);
		if (acquire_ret != GST_FLOW_OK)
		{
			GST_ERROR_OBJECT(self, "Failed to acquire buffer for output%d", i);
			gst_buffer_unref(inbuf);
			for (unsigned int j = 0; j < i; j++)
			{
				if (outbuf[j])
					gst_buffer_unref(outbuf[j]);
			}
			return acquire_ret;
		}
	}

	gboolean input_is_dmabuf = gst_buffer_is_dmabuf(inbuf);
	GST_DEBUG_OBJECT(self, "input_is_dmabuf=%d, use_dmabuf_input=%d", input_is_dmabuf, self->priv->use_dmabuf_input);

	/* Round-robin slice from allocated backend buffers */
	guint index = self->priv->output_buffer_count ? (self->priv->buffer_slice_index % self->priv->output_buffer_count)
												  : 0;
	self->priv->buffer_slice_index++;

	/* Create the buffer slize to send to the hardware */
	std::map<std::string, Buffer> slice;
	for (const auto &[node_name, buffers] : self->priv->pisp_buffers)
		slice.emplace(node_name, buffers[index]);

	/* Prepare input: copy to slice buffer (memcpy path) or get dmabuf (zero-copy path) */
	if (input_is_dmabuf && self->priv->use_dmabuf_input)
	{
		std::optional<Buffer> dmabuf_input = gst_to_libpisp_buffer(inbuf);
		if (!dmabuf_input)
		{
			ret = GST_FLOW_ERROR;
			goto cleanup;
		}
		/* Move so slice takes ownership of the dupped fds; no second dup in copy assignment. */
		slice.at("pispbe-input") = std::move(*dmabuf_input);
		GST_DEBUG_OBJECT(self, "Using zero-copy input path");
	}
	else
	{
		Buffer::Sync s(slice.at("pispbe-input"), Buffer::Sync::Access::ReadWrite);
		const auto &mem = s.Get();
		copy_buffer_to_pisp(inbuf, const_cast<std::array<uint8_t *, 3> &>(mem), self->priv->in_width,
							self->priv->in_height, self->priv->in_stride, self->priv->in_hw_stride,
							self->priv->in_format);
		GST_DEBUG_OBJECT(self, "Using memcpy input path");
	}

	if (self->priv->backend_device->Run(slice))
	{
		GST_ERROR_OBJECT(self, "Hardware conversion failed");
		ret = GST_FLOW_ERROR;
		goto cleanup;
	}

	/* Output: wrap backend DMA as GstBuffer (dmabuf) or copy to pool buffer (memcpy) */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (!self->priv->output_enabled[i])
			continue;

		const std::string node_name("pispbe-output" + std::to_string(i));

		if (self->priv->use_dmabuf_output[i])
		{
			outbuf[i] = libpisp_to_gst_dmabuf(slice.at(node_name), self->priv->dmabuf_allocator);
			if (!outbuf[i])
			{
				GST_ERROR_OBJECT(self, "Failed to wrap backend buffer as dmabuf for output%d", i);
				ret = GST_FLOW_ERROR;
				goto cleanup;
			}

			add_video_meta(outbuf[i], self->priv->out_format[i], self->priv->out_width[i],
						   self->priv->out_height[i], self->priv->out_hw_stride[i]);

			GST_DEBUG_OBJECT(self, "Using zero-copy output%d path", i);
		}
		else
		{
			Buffer::Sync s(slice.at(node_name), Buffer::Sync::Access::Read);
			copy_pisp_to_buffer(s.Get(), outbuf[i], self->priv->out_width[i], self->priv->out_height[i],
								self->priv->out_stride[i], self->priv->out_hw_stride[i], self->priv->out_format[i]);
			GST_DEBUG_OBJECT(self, "Using memcpy output%d path", i);
		}
	}

	/* Copy timestamp and duration from input */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (!self->priv->output_enabled[i] || !outbuf[i])
			continue;

		GST_BUFFER_PTS(outbuf[i]) = GST_BUFFER_PTS(inbuf);
		GST_BUFFER_DTS(outbuf[i]) = GST_BUFFER_DTS(inbuf);
		GST_BUFFER_DURATION(outbuf[i]) = GST_BUFFER_DURATION(inbuf);
	}

	/* Push buffers to output pads */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (!self->priv->output_enabled[i] || !outbuf[i])
			continue;

		GstFlowReturn push_ret = gst_pad_push(self->srcpad[i], outbuf[i]);
		outbuf[i] = nullptr; /* Buffer ownership transferred */

		if (push_ret != GST_FLOW_OK && ret == GST_FLOW_OK)
			ret = push_ret;
	}

	gst_buffer_unref(inbuf);
	return ret;

cleanup:
	gst_buffer_unref(inbuf);
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (outbuf[i])
			gst_buffer_unref(outbuf[i]);
	}
	return ret;
}

/*
 * Start the element - acquire backend device.
 */
static gboolean gst_pisp_convert_start(GstPispConvert *self)
{
	GST_INFO_OBJECT(self, "Starting pispconvert element");

	libpisp::logging_init();

	try
	{
		libpisp::helpers::MediaDevice devices;

		std::string media_dev = devices.Acquire();
		if (media_dev.empty())
		{
			GST_ERROR_OBJECT(self, "Unable to acquire any pisp_be device!");
			return FALSE;
		}

		self->priv->media_dev_path = g_strdup(media_dev.c_str());
		self->priv->backend_device = std::make_unique<libpisp::helpers::BackendDevice>(media_dev);

		if (!self->priv->backend_device->Valid())
		{
			GST_ERROR_OBJECT(self, "Failed to create backend device");
			return FALSE;
		}

		const std::vector<libpisp::PiSPVariant> &variants = libpisp::get_variants();
		const media_device_info info = devices.DeviceInfo(media_dev);

		auto variant_it = std::find_if(variants.begin(), variants.end(),
									   [&info](const auto &v) { return v.BackEndVersion() == info.hw_revision; });

		if (variant_it == variants.end())
		{
			GST_ERROR_OBJECT(self, "Backend hardware could not be identified: %u", info.hw_revision);
			return FALSE;
		}

		self->priv->variant = &(*variant_it);
		self->priv->backend = std::make_unique<libpisp::BackEnd>(libpisp::BackEnd::Config({}), *variant_it);

		GST_INFO_OBJECT(self, "Acquired device %s", media_dev.c_str());
	}
	catch (const std::exception &e)
	{
		GST_ERROR_OBJECT(self, "Failed to start: %s", e.what());
		return FALSE;
	}

	return TRUE;
}

/*
 * Stop the element - release backend device.
 */
static gboolean gst_pisp_convert_stop(GstPispConvert *self)
{
	GST_INFO_OBJECT(self, "Stopping pispconvert element");

	/* Deactivate and release buffer pools */
	for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
	{
		if (self->priv->output_pool[i])
		{
			gst_buffer_pool_set_active(self->priv->output_pool[i], FALSE);
			gst_object_unref(self->priv->output_pool[i]);
			self->priv->output_pool[i] = nullptr;
		}
		if (self->priv->src_caps[i])
		{
			gst_caps_unref(self->priv->src_caps[i]);
			self->priv->src_caps[i] = nullptr;
		}
		self->priv->output_enabled[i] = FALSE;
		self->priv->use_dmabuf_output[i] = FALSE;
	}

	if (self->priv->pending_segment)
	{
		gst_event_unref(self->priv->pending_segment);
		self->priv->pending_segment = nullptr;
	}

	self->priv->backend.reset();
	self->priv->backend_device.reset();

	g_free(self->priv->media_dev_path);
	self->priv->media_dev_path = nullptr;
	self->priv->configured = FALSE;
	self->priv->use_dmabuf_input = FALSE;

	return TRUE;
}

/*
 * State change handler.
 */
static GstStateChangeReturn gst_pisp_convert_change_state(GstElement *element, GstStateChange transition)
{
	GstPispConvert *self = GST_PISP_CONVERT(element);
	GstStateChangeReturn ret;

	switch (transition)
	{
	case GST_STATE_CHANGE_NULL_TO_READY:
		if (!gst_pisp_convert_start(self))
			return GST_STATE_CHANGE_FAILURE;
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition)
	{
	case GST_STATE_CHANGE_READY_TO_NULL:
		gst_pisp_convert_stop(self);
		break;
	default:
		break;
	}

	return ret;
}

/*
 * Sink pad event handler.
 */
static gboolean gst_pisp_convert_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
	GstPispConvert *self = GST_PISP_CONVERT(parent);
	gboolean ret = TRUE;

	GST_TRACE_OBJECT(self, "Received sink event: %s", GST_EVENT_TYPE_NAME(event));

	switch (GST_EVENT_TYPE(event))
	{
	case GST_EVENT_CAPS:
	{
		GstCaps *caps;
		gst_event_parse_caps(event, &caps);

		gchar *caps_str = gst_caps_to_string(caps);
		GST_INFO_OBJECT(self, "Received input caps: %s", caps_str);
		g_free(caps_str);

		if (!parse_input_caps(self, caps))
		{
			GST_ERROR_OBJECT(self, "Failed to parse input caps");
			ret = FALSE;
		}

		/* Mark as needing reconfiguration */
		self->priv->configured = FALSE;

		gst_event_unref(event);
		break;
	}
	case GST_EVENT_SEGMENT:
		/* Store segment to forward after caps are sent */
		if (self->priv->pending_segment)
			gst_event_unref(self->priv->pending_segment);
		self->priv->pending_segment = event;
		GST_DEBUG_OBJECT(self, "Stored segment event for later forwarding");
		break;
	case GST_EVENT_EOS:
	case GST_EVENT_FLUSH_START:
	case GST_EVENT_FLUSH_STOP:
	case GST_EVENT_STREAM_START:
		/* Forward to all linked src pads */
		for (unsigned int i = 0; i < PISP_NUM_OUTPUTS; i++)
		{
			if (gst_pad_is_linked(self->srcpad[i]))
				gst_pad_push_event(self->srcpad[i], gst_event_ref(event));
		}
		gst_event_unref(event);
		break;
	default:
		ret = gst_pad_event_default(pad, parent, event);
		break;
	}

	return ret;
}

/*
 * Source pad event handler.
 */
static gboolean gst_pisp_convert_src_event(GstPad *pad [[maybe_unused]], GstObject *parent, GstEvent *event)
{
	GstPispConvert *self = GST_PISP_CONVERT(parent);

	GST_TRACE_OBJECT(self, "Received src event: %s", GST_EVENT_TYPE_NAME(event));

	/* Forward upstream events to sink pad */
	return gst_pad_push_event(self->sinkpad, event);
}

/*
 * Sink pad query handler.
 */
static gboolean gst_pisp_convert_sink_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
	GstPispConvert *self = GST_PISP_CONVERT(parent);
	gboolean ret = TRUE;

	GST_TRACE_OBJECT(self, "Received sink query: %s", GST_QUERY_TYPE_NAME(query));

	switch (GST_QUERY_TYPE(query))
	{
	case GST_QUERY_ALLOCATION:
	{
		/* Indicate support for VideoMeta (required for DMABuf) */
		gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
		/* Offer dmabuf allocator */
		if (self->priv->dmabuf_allocator)
			gst_query_add_allocation_param(query, self->priv->dmabuf_allocator, nullptr);
		ret = TRUE;
		break;
	}
	case GST_QUERY_CAPS:
	{
		GstCaps *filter, *caps;
		gst_query_parse_caps(query, &filter);

		caps = gst_pad_get_pad_template_caps(pad);
		if (filter)
		{
			GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
			gst_caps_unref(caps);
			caps = intersection;
		}

		gst_query_set_caps_result(query, caps);
		gst_caps_unref(caps);
		ret = TRUE;
		break;
	}
	case GST_QUERY_ACCEPT_CAPS:
	{
		GstCaps *caps;
		gst_query_parse_accept_caps(query, &caps);

		GstCaps *template_caps = gst_pad_get_pad_template_caps(pad);
		gboolean accept = gst_caps_can_intersect(caps, template_caps);
		gst_caps_unref(template_caps);

		gst_query_set_accept_caps_result(query, accept);
		ret = TRUE;
		break;
	}
	default:
		ret = gst_pad_query_default(pad, parent, query);
		break;
	}

	return ret;
}

/*
 * Source pad query handler.
 */
static gboolean gst_pisp_convert_src_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
	GstPispConvert *self = GST_PISP_CONVERT(parent);
	gboolean ret = TRUE;

	GST_TRACE_OBJECT(self, "Received src query: %s", GST_QUERY_TYPE_NAME(query));

	switch (GST_QUERY_TYPE(query))
	{
	case GST_QUERY_CAPS:
	{
		GstCaps *filter, *caps;
		gst_query_parse_caps(query, &filter);

		caps = gst_pad_get_pad_template_caps(pad);
		if (filter)
		{
			GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
			gst_caps_unref(caps);
			caps = intersection;
		}

		gst_query_set_caps_result(query, caps);
		gst_caps_unref(caps);
		ret = TRUE;
		break;
	}
	case GST_QUERY_ACCEPT_CAPS:
	{
		GstCaps *caps;
		gst_query_parse_accept_caps(query, &caps);

		GstCaps *template_caps = gst_pad_get_pad_template_caps(pad);
		gboolean accept = gst_caps_can_intersect(caps, template_caps);
		gst_caps_unref(template_caps);

		gst_query_set_accept_caps_result(query, accept);
		ret = TRUE;
		break;
	}
	default:
		ret = gst_pad_query_default(pad, parent, query);
		break;
	}

	return ret;
}

/* Plugin initialization */
static gboolean plugin_init(GstPlugin *plugin)
{
	return GST_ELEMENT_REGISTER(pispconvert, plugin);
}

#define PACKAGE "pispconvert"
#define VERSION "1.3.0"
#define GST_PACKAGE_NAME "GStreamer PiSP Plugin"
#define GST_PACKAGE_ORIGIN "https://github.com/raspberrypi/libpisp"
#define GST_LICENSE "BSD"

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, pispconvert,
				  "PiSP hardware accelerated format conversion and scaling", plugin_init, VERSION, GST_LICENSE,
				  GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
