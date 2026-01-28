/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026 Raspberry Pi Ltd
 *
 * gstpispconvert.cpp - GStreamer element for PiSP hardware conversion
 */

#include "gstpispconvert.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>

#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

#include "common/logging.hpp"
#include "common/utils.hpp"
#include "helpers/v4l2_device.hpp"
#include "variants/variant.hpp"

GST_DEBUG_CATEGORY_STATIC(gst_pisp_convert_debug);
#define GST_CAT_DEFAULT gst_pisp_convert_debug

/* Supported formats - matching those from convert.cpp */
#define PISP_FORMATS "{ RGB, I420, YV12, Y42B, Y444, YUY2, UYVY, NV12_128C8 }"
#define PISP_DRM_FORMATS "{ RGB888, YUV420, YVU420, YUV422, YUV444, YUYV, UYVY, NV12 }"

static GstStaticPadTemplate sink_template =
	GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
							GST_STATIC_CAPS(
								/* DMA-DRM format (GStreamer 1.24+) */
								"video/x-raw(memory:DMABuf), format=(string)DMA_DRM, drm-format=(string)" PISP_DRM_FORMATS
								", width=(int)[1,32768], height=(int)[1,32768], framerate=(fraction)[0/1,2147483647/1]"
								";" /* Regular dmabuf with standard formats */
								GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_DMABUF, PISP_FORMATS)
								";" /* System memory */
								GST_VIDEO_CAPS_MAKE(PISP_FORMATS)));

static GstStaticPadTemplate src_template =
	GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
							GST_STATIC_CAPS(
								/* DMA-DRM format (GStreamer 1.24+) */
								"video/x-raw(memory:DMABuf), format=(string)DMA_DRM, drm-format=(string)" PISP_DRM_FORMATS
								", width=(int)[1,32768], height=(int)[1,32768], framerate=(fraction)[0/1,2147483647/1]"
								";" /* Regular dmabuf with standard formats */
								GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_DMABUF, PISP_FORMATS)
								";" /* System memory */
								GST_VIDEO_CAPS_MAKE(PISP_FORMATS)));

#define gst_pisp_convert_parent_class parent_class
G_DEFINE_TYPE(GstPispConvert, gst_pisp_convert, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE(pispconvert, "pispconvert", GST_RANK_PRIMARY, GST_TYPE_PISP_CONVERT);

/* Helper function to map GStreamer format to PiSP format string */
static const char *gst_format_to_pisp(GstVideoFormat format)
{
	switch (format)
	{
	case GST_VIDEO_FORMAT_RGB:
		return "RGB888";
	case GST_VIDEO_FORMAT_I420:
		return "YUV420P";
	case GST_VIDEO_FORMAT_YV12:
		return "YVU420P";
	case GST_VIDEO_FORMAT_Y42B:
		return "YUV422P";
	case GST_VIDEO_FORMAT_Y444:
		return "YUV444P";
	case GST_VIDEO_FORMAT_YUY2:
		return "YUYV";
	case GST_VIDEO_FORMAT_UYVY:
		return "UYVY";
	//case GST_VIDEO_FORMAT_NV12_128C8:
	//	return "YUV420SP_COL128";
	default:
		return nullptr;
	}
}

/* Helper function to map DRM format string to PiSP format string */
static const char *drm_format_to_pisp(const gchar *drm_format)
{
	if (!drm_format)
		return nullptr;

	/* Map common DRM fourcc names to PiSP formats */
	if (g_str_equal(drm_format, "RGB888") || g_str_equal(drm_format, "BGR888"))
		return "RGB888";
	else if (g_str_equal(drm_format, "YUV420") || g_str_equal(drm_format, "YU12"))
		return "YUV420P";
	else if (g_str_equal(drm_format, "YVU420") || g_str_equal(drm_format, "YV12"))
		return "YVU420P";
	else if (g_str_equal(drm_format, "YUV422"))
		return "YUV422P";
	else if (g_str_equal(drm_format, "YUV444"))
		return "YUV444P";
	else if (g_str_equal(drm_format, "YUYV"))
		return "YUYV";
	else if (g_str_equal(drm_format, "UYVY"))
		return "UYVY";
	else if (g_str_equal(drm_format, "NV12"))
		return "YUV420SP";

	return nullptr;
}

/* GObject vmethod implementations */
static void gst_pisp_convert_finalize(GObject *object);

/* GstBaseTransform vmethod implementations */
static GstCaps *gst_pisp_convert_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps,
												GstCaps *filter);
static gboolean gst_pisp_convert_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_pisp_convert_get_unit_size(GstBaseTransform *trans, GstCaps *caps, gsize *size);
static GstFlowReturn gst_pisp_convert_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);
static gboolean gst_pisp_convert_start(GstBaseTransform *trans);
static gboolean gst_pisp_convert_stop(GstBaseTransform *trans);
static gboolean gst_pisp_convert_decide_allocation(GstBaseTransform *trans, GstQuery *query);
static gboolean gst_pisp_convert_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query, GstQuery *query);

/* Initialize the class */
static void gst_pisp_convert_class_init(GstPispConvertClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->finalize = gst_pisp_convert_finalize;

	gst_element_class_set_static_metadata(element_class,
										  "PiSP Hardware Image Converter",
										  "Filter/Converter/Video/Scaler",
										  "Hardware accelerated format conversion and scaling using libpisp",
										  "Raspberry Pi");

	gst_element_class_add_static_pad_template(element_class, &sink_template);
	gst_element_class_add_static_pad_template(element_class, &src_template);

	transform_class->transform_caps = GST_DEBUG_FUNCPTR(gst_pisp_convert_transform_caps);
	transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_pisp_convert_set_caps);
	transform_class->get_unit_size = GST_DEBUG_FUNCPTR(gst_pisp_convert_get_unit_size);
	transform_class->transform = GST_DEBUG_FUNCPTR(gst_pisp_convert_transform);
	transform_class->start = GST_DEBUG_FUNCPTR(gst_pisp_convert_start);
	transform_class->stop = GST_DEBUG_FUNCPTR(gst_pisp_convert_stop);
	transform_class->decide_allocation = GST_DEBUG_FUNCPTR(gst_pisp_convert_decide_allocation);
	transform_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_pisp_convert_propose_allocation);

	GST_DEBUG_CATEGORY_INIT(gst_pisp_convert_debug, "pispconvert", 0, "PiSP hardware converter");
}

/* Initialize the element instance */
static void gst_pisp_convert_init(GstPispConvert *filter)
{
	filter->priv = new GstPispConvertPrivate();
	filter->priv->backend_device = nullptr;
	filter->priv->backend = nullptr;
	filter->priv->media_dev_path = nullptr;
	filter->priv->configured = FALSE;
	filter->priv->dmabuf_allocator = gst_dmabuf_allocator_new();
	filter->priv->use_dmabuf_input = FALSE;
	filter->priv->use_dmabuf_output = FALSE;
	filter->priv->dmabuf_imported = FALSE;

	gst_base_transform_set_in_place(GST_BASE_TRANSFORM(filter), FALSE);
	gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(filter), FALSE);
}

static void gst_pisp_convert_finalize(GObject *object)
{
	GstPispConvert *filter = GST_PISP_CONVERT(object);

	if (filter->priv)
	{
		if (filter->priv->dmabuf_allocator)
		{
			gst_object_unref(filter->priv->dmabuf_allocator);
			filter->priv->dmabuf_allocator = nullptr;
		}
		g_free(filter->priv->media_dev_path);
		delete filter->priv;
		filter->priv = nullptr;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstCaps *gst_pisp_convert_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps,
												GstCaps *filter)
{
	GstPispConvert *convert = GST_PISP_CONVERT(trans);
	GstCaps *ret, *tmp;
	GstStructure *structure;
	gchar *caps_str;
	guint i, n;

	caps_str = gst_caps_to_string(caps);
	GST_INFO_OBJECT(convert, "transform_caps called, direction: %s, caps: %s",
					 direction == GST_PAD_SINK ? "SINK" : "SRC", caps_str);
	g_free(caps_str);
	
	/* Log caps features */
	for (i = 0; i < gst_caps_get_size(caps); i++)
	{
		GstCapsFeatures *features = gst_caps_get_features(caps, i);
		if (features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF))
		{
			GST_INFO_OBJECT(convert, "Input caps[%d] contains memory:DMABuf feature", i);
		}
	}

	/* Return template caps (we can scale to any size) */
	if (direction == GST_PAD_SINK)
	{
		tmp = gst_static_pad_template_get_caps(&src_template);
	}
	else
	{
		tmp = gst_static_pad_template_get_caps(&sink_template);
	}

	ret = gst_caps_new_empty();

	n = gst_caps_get_size(caps);
	for (i = 0; i < n; i++)
	{
		structure = gst_caps_get_structure(caps, i);

		/* Preserve caps features (e.g., memory:DMABuf) */
		GstCapsFeatures *features = gst_caps_get_features(caps, i);
		gboolean has_dmabuf = features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF);
		
		/* For each structure in template caps, create a new one with preserved features */
		for (guint j = 0; j < gst_caps_get_size(tmp); j++)
		{
			GstStructure *tmpl_structure = gst_caps_get_structure(tmp, j);
			GstStructure *new_structure = gst_structure_copy(tmpl_structure);

			/* Preserve framerate and pixel-aspect-ratio, but allow any width/height */
			if (gst_structure_has_field(structure, "framerate"))
			{
				const GValue *framerate = gst_structure_get_value(structure, "framerate");
				gst_structure_set_value(new_structure, "framerate", framerate);
			}
			if (gst_structure_has_field(structure, "pixel-aspect-ratio"))
			{
				const GValue *par = gst_structure_get_value(structure, "pixel-aspect-ratio");
				gst_structure_set_value(new_structure, "pixel-aspect-ratio", par);
			}

			/* Check if template structure is DMA_DRM (requires dmabuf feature) */
			const gchar *tmpl_format = gst_structure_get_string(tmpl_structure, "format");
			gboolean is_drm_template = tmpl_format && g_str_equal(tmpl_format, "DMA_DRM");
			
			/* Preserve the caps features (like memory:DMABuf) */
			if (features)
			{
				GstCapsFeatures *new_features = gst_caps_features_copy(features);
				gst_caps_append_structure_full(ret, gst_structure_copy(new_structure), new_features);
				GST_INFO_OBJECT(convert, "Preserving caps features from input");
			}
			
			/* For DMA_DRM templates, always add them with dmabuf feature (even if input doesn't have it)
			 * This allows accepting dmabuf input and copying to system memory output */
			if (is_drm_template && !has_dmabuf)
			{
				GstCapsFeatures *dmabuf_features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_DMABUF, NULL);
				gst_caps_append_structure_full(ret, gst_structure_copy(new_structure), dmabuf_features);
				GST_INFO_OBJECT(convert, "Adding DMA_DRM with dmabuf feature for input");
			}
			
			/* Also offer system memory output (we can always copy from dmabuf to system memory) */
			if (has_dmabuf)
			{
				/* If template structure is DMA_DRM, skip it for system memory (DMA_DRM requires dmabuf) */
				if (!is_drm_template)
				{
					gst_caps_append_structure(ret, new_structure);
					GST_INFO_OBJECT(convert, "Also offering system memory output");
				}
				else
				{
					gst_structure_free(new_structure);
				}
			}
			else if (!features)
			{
				/* For system memory input, add non-DRM templates */
				if (!is_drm_template)
				{
					gst_caps_append_structure(ret, new_structure);
				}
				else
				{
					gst_structure_free(new_structure);
				}
			}
			else
			{
				gst_structure_free(new_structure);
			}
		}
	}

	gst_caps_unref(tmp);

	if (filter)
	{
		gchar *filter_str = gst_caps_to_string(filter);
		GST_INFO_OBJECT(convert, "Applying filter: %s", filter_str);
		g_free(filter_str);

		GstCaps *intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(ret);
		ret = intersection;
	}

	caps_str = gst_caps_to_string(ret);
	GST_INFO_OBJECT(convert, "Returning caps: %s", caps_str);
	
	/* Log features in returned caps */
	for (i = 0; i < gst_caps_get_size(ret); i++)
	{
		GstCapsFeatures *features = gst_caps_get_features(ret, i);
		if (features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF))
		{
			GST_INFO_OBJECT(convert, "Output caps[%d] contains memory:DMABuf feature", i);
		}
		else if (features)
		{
			gchar *feat_str = gst_caps_features_to_string(features);
			GST_INFO_OBJECT(convert, "Output caps[%d] features: %s", i, feat_str);
			g_free(feat_str);
		}
		else
		{
			GST_INFO_OBJECT(convert, "Output caps[%d] has no features (system memory)", i);
		}
	}
	g_free(caps_str);

	return ret;
}

static gboolean gst_pisp_convert_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);
	GstVideoInfo in_info, out_info;
	gchar *incaps_str, *outcaps_str;

	incaps_str = gst_caps_to_string(incaps);
	outcaps_str = gst_caps_to_string(outcaps);
	GST_INFO_OBJECT(filter, "set_caps called with incaps: %s, outcaps: %s", incaps_str, outcaps_str);
	g_free(incaps_str);
	g_free(outcaps_str);

	/* Check for dmabuf support in caps */
	GstCapsFeatures *in_features = gst_caps_get_features(incaps, 0);
	GstCapsFeatures *out_features = gst_caps_get_features(outcaps, 0);

	/* Log features detail */
	if (in_features)
	{
		gchar *feat_str = gst_caps_features_to_string(in_features);
		GST_INFO_OBJECT(filter, "Input caps features: %s", feat_str);
		g_free(feat_str);
	}
	else
	{
		GST_INFO_OBJECT(filter, "Input caps has no features (system memory)");
	}
	
	if (out_features)
	{
		gchar *feat_str = gst_caps_features_to_string(out_features);
		GST_INFO_OBJECT(filter, "Output caps features: %s", feat_str);
		g_free(feat_str);
	}
	else
	{
		GST_INFO_OBJECT(filter, "Output caps has no features (system memory)");
	}

	filter->priv->use_dmabuf_input = in_features &&
									 gst_caps_features_contains(in_features, GST_CAPS_FEATURE_MEMORY_DMABUF);
	filter->priv->use_dmabuf_output = out_features &&
									  gst_caps_features_contains(out_features, GST_CAPS_FEATURE_MEMORY_DMABUF);

	GST_INFO_OBJECT(filter, "dmabuf support - input: %s, output: %s", filter->priv->use_dmabuf_input ? "YES" : "NO",
					filter->priv->use_dmabuf_output ? "YES" : "NO");

	/* Reset dmabuf import flag since we're reconfiguring */
	filter->priv->dmabuf_imported = FALSE;

	/* Check if input uses DMA-DRM format */
	GstStructure *in_structure = gst_caps_get_structure(incaps, 0);
	const gchar *in_format_str = gst_structure_get_string(in_structure, "format");
	gboolean in_is_drm = in_format_str && g_str_equal(in_format_str, "DMA_DRM");

	if (in_is_drm)
	{
		/* Parse DMA-DRM caps */
		const gchar *drm_format = gst_structure_get_string(in_structure, "drm-format");
		gst_structure_get_int(in_structure, "width", (gint *)&filter->priv->in_width);
		gst_structure_get_int(in_structure, "height", (gint *)&filter->priv->in_height);
		filter->priv->in_format = drm_format_to_pisp(drm_format);
		filter->priv->in_stride = 0; // Will be determined by dmabuf
		GST_INFO_OBJECT(filter, "Input DMA-DRM format: drm-format=%s, pisp=%s", drm_format, filter->priv->in_format);
	}
	else
	{
		/* Parse regular video caps */
		if (!gst_video_info_from_caps(&in_info, incaps))
		{
			GST_ERROR_OBJECT(filter, "Failed to parse input caps");
			return FALSE;
		}
		filter->priv->in_width = GST_VIDEO_INFO_WIDTH(&in_info);
		filter->priv->in_height = GST_VIDEO_INFO_HEIGHT(&in_info);
		filter->priv->in_stride = GST_VIDEO_INFO_PLANE_STRIDE(&in_info, 0);
		filter->priv->in_format = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&in_info));
	}

	/* Check if output uses DMA-DRM format */
	GstStructure *out_structure = gst_caps_get_structure(outcaps, 0);
	const gchar *out_format_str = gst_structure_get_string(out_structure, "format");
	gboolean out_is_drm = out_format_str && g_str_equal(out_format_str, "DMA_DRM");

	if (out_is_drm)
	{
		/* Parse DMA-DRM caps */
		const gchar *drm_format = gst_structure_get_string(out_structure, "drm-format");
		gst_structure_get_int(out_structure, "width", (gint *)&filter->priv->out_width);
		gst_structure_get_int(out_structure, "height", (gint *)&filter->priv->out_height);
		filter->priv->out_format = drm_format_to_pisp(drm_format);
		filter->priv->out_stride = 0; // Will be determined by dmabuf
		GST_INFO_OBJECT(filter, "Output DMA-DRM format: drm-format=%s, pisp=%s", drm_format, filter->priv->out_format);
	}
	else
	{
		/* Parse regular video caps */
		if (!gst_video_info_from_caps(&out_info, outcaps))
		{
			GST_ERROR_OBJECT(filter, "Failed to parse output caps");
			return FALSE;
		}
		filter->priv->out_width = GST_VIDEO_INFO_WIDTH(&out_info);
		filter->priv->out_height = GST_VIDEO_INFO_HEIGHT(&out_info);
		filter->priv->out_stride = GST_VIDEO_INFO_PLANE_STRIDE(&out_info, 0);
		filter->priv->out_format = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&out_info));
	}

	if (!filter->priv->in_format || !filter->priv->out_format)
	{
		GST_ERROR_OBJECT(filter, "Unsupported format");
		return FALSE;
	}

	GST_INFO_OBJECT(filter, "Configured: %ux%u %s -> %ux%u %s", filter->priv->in_width, filter->priv->in_height,
					filter->priv->in_format, filter->priv->out_width, filter->priv->out_height,
					filter->priv->out_format);

	/* Configure the backend */
	try
	{
		if (!filter->priv->backend_device)
		{
			GST_ERROR_OBJECT(filter, "Backend not initialized");
			return FALSE;
		}

		pisp_be_global_config global;
		filter->priv->backend->GetGlobal(global);
		global.bayer_enables = 0;
		global.rgb_enables = PISP_BE_RGB_ENABLE_INPUT + PISP_BE_RGB_ENABLE_OUTPUT0;

		/* Configure input format */
		pisp_image_format_config i = {};
		i.width = filter->priv->in_width;
		i.height = filter->priv->in_height;
		i.format = libpisp::get_pisp_image_format(filter->priv->in_format);
		if (!i.format)
		{
			GST_ERROR_OBJECT(filter, "Failed to get input format");
			return FALSE;
		}
		libpisp::compute_optimal_stride(i);
		filter->priv->in_hw_stride = i.stride; // Store hardware stride
		GST_INFO_OBJECT(filter, "Input format config: width=%u, height=%u, stride=%u, stride2=%u, format=0x%08x",
						i.width, i.height, i.stride, i.stride2, i.format);
		GST_INFO_OBJECT(filter, "Input strides: GStreamer=%u, Hardware=%u", filter->priv->in_stride,
						filter->priv->in_hw_stride);
		filter->priv->backend->SetInputFormat(i);

		/* Configure output format */
		pisp_be_output_format_config o = {};
		o.image.width = filter->priv->out_width;
		o.image.height = filter->priv->out_height;
		o.image.format = libpisp::get_pisp_image_format(filter->priv->out_format);
		if (!o.image.format)
		{
			GST_ERROR_OBJECT(filter, "Failed to get output format");
			return FALSE;
		}
		libpisp::compute_optimal_stride(o.image, true);
		filter->priv->out_hw_stride = o.image.stride; // Store hardware stride
		GST_INFO_OBJECT(filter, "Output format config: width=%u, height=%u, stride=%u, stride2=%u, format=0x%08x",
						o.image.width, o.image.height, o.image.stride, o.image.stride2, o.image.format);
		GST_INFO_OBJECT(filter, "Output strides: GStreamer=%u, Hardware=%u", filter->priv->out_stride,
						filter->priv->out_hw_stride);
		filter->priv->backend->SetOutputFormat(0, o);

		/* Configure color space conversion if needed */
		if (filter->priv->in_format[0] >= 'U')
		{ // YUV input
			pisp_be_ccm_config csc;
			filter->priv->backend->InitialiseYcbcrInverse(csc, "jpeg");
			filter->priv->backend->SetCcm(csc);
			global.rgb_enables |= PISP_BE_RGB_ENABLE_CCM;
		}

		if (filter->priv->out_format[0] >= 'U')
		{ // YUV output
			pisp_be_ccm_config csc;
			filter->priv->backend->InitialiseYcbcr(csc, "jpeg");
			filter->priv->backend->SetCsc(0, csc);
			global.rgb_enables |= PISP_BE_RGB_ENABLE_CSC0;
		}

		filter->priv->backend->SetGlobal(global);
		filter->priv->backend->SetCrop(0, { 0, 0, i.width, i.height });
		filter->priv->backend->SetSmartResize(0, { o.image.width, o.image.height });

		/* Prepare the hardware configuration */
		pisp_be_tiles_config config = {};
		filter->priv->backend->Prepare(&config);

		filter->priv->backend_device->Setup(config);
		filter->priv->configured = TRUE;

		GST_INFO_OBJECT(filter, "Backend configured successfully");
	}
	catch (const std::exception &e)
	{
		GST_ERROR_OBJECT(filter, "Failed to configure backend: %s", e.what());
		return FALSE;
	}

	return TRUE;
}

static gboolean gst_pisp_convert_get_unit_size(GstBaseTransform *trans, GstCaps *caps, gsize *size)
{
	GstPispConvert *convert = GST_PISP_CONVERT(trans);
	GstVideoInfo info;
	gchar *caps_str;

	caps_str = gst_caps_to_string(caps);
	GST_DEBUG_OBJECT(convert, "get_unit_size called with caps: %s", caps_str);
	g_free(caps_str);

	if (!gst_video_info_from_caps(&info, caps))
	{
		GST_ERROR_OBJECT(convert, "Failed to parse caps in get_unit_size");
		return FALSE;
	}

	/* Since transform_caps sets the correct dimensions, just use GST_VIDEO_INFO_SIZE */
	*size = GST_VIDEO_INFO_SIZE(&info);
	GST_DEBUG_OBJECT(convert, "Returning unit size: %zu bytes (dimensions: %dx%d)", *size, GST_VIDEO_INFO_WIDTH(&info),
					 GST_VIDEO_INFO_HEIGHT(&info));
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

static std::optional<libpisp::helpers::V4l2Device::Buffer> gst_buffer_to_dmabuf_buffer(GstBuffer *buffer,
																					   GstVideoInfo *info)
{
	libpisp::helpers::V4l2Device::Buffer buf = {};
	guint n_planes = GST_VIDEO_INFO_N_PLANES(info);

	/* For planar formats, GStreamer may use separate memory blocks per plane,
	 * but we typically get one contiguous dmabuf */
	guint n_mem = gst_buffer_n_memory(buffer);

	if (n_mem == 1)
	{
		/* Single dmabuf for all planes (most common case) */
		GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
		if (!gst_is_dmabuf_memory(mem))
			return std::nullopt;

		gint fd = gst_dmabuf_memory_get_fd(mem);
		buf.fd[0] = fd;
		buf.size[0] = mem->size;

		/* For planar formats, calculate offsets */
		if (n_planes > 1)
		{
			buf.fd[1] = fd;
			buf.fd[2] = fd;
			/* Sizes are accumulated in the single buffer */
		}
	}
	else
	{
		/* Multiple dmabuf memory blocks */
		for (guint i = 0; i < std::min(n_mem, 3u); i++)
		{
			GstMemory *mem = gst_buffer_peek_memory(buffer, i);
			if (!gst_is_dmabuf_memory(mem))
				return std::nullopt;

			buf.fd[i] = gst_dmabuf_memory_get_fd(mem);
			buf.size[i] = mem->size;
		}
	}

	return buf;
}

[[maybe_unused]]
static GstBuffer *create_dmabuf_buffer(const libpisp::helpers::V4l2Device::Buffer &hw_buffer, GstAllocator *allocator,
									   GstVideoInfo *info)
{
	GstBuffer *buffer = gst_buffer_new();
	guint n_planes = GST_VIDEO_INFO_N_PLANES(info);

	/* Check if we have a single contiguous buffer or separate plane buffers */
	gboolean separate_planes = (n_planes > 1 && hw_buffer.fd[0] != hw_buffer.fd[1]);

	if (separate_planes)
	{
		/* Separate dmabuf per plane */
		for (guint i = 0; i < n_planes && i < 3; i++)
		{
			if (hw_buffer.fd[i] >= 0)
			{
				GstMemory *mem = gst_dmabuf_allocator_alloc(allocator, dup(hw_buffer.fd[i]), hw_buffer.size[i]);
				gst_buffer_append_memory(buffer, mem);
			}
		}
	}
	else
	{
		/* Single contiguous dmabuf for all planes */
		if (hw_buffer.fd[0] >= 0)
		{
			gsize total_size = hw_buffer.size[0];
			GstMemory *mem = gst_dmabuf_allocator_alloc(allocator, dup(hw_buffer.fd[0]), total_size);
			gst_buffer_append_memory(buffer, mem);
		}
	}

	/* Add video meta with actual strides and offsets */
	gsize offsets[GST_VIDEO_MAX_PLANES] = { 0 };
	gint strides[GST_VIDEO_MAX_PLANES] = { 0 };

	for (guint i = 0; i < n_planes; i++)
	{
		strides[i] = GST_VIDEO_INFO_PLANE_STRIDE(info, i);
		offsets[i] = GST_VIDEO_INFO_PLANE_OFFSET(info, i);
	}

	gst_buffer_add_video_meta_full(buffer, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_INFO_FORMAT(info),
								   GST_VIDEO_INFO_WIDTH(info), GST_VIDEO_INFO_HEIGHT(info), n_planes, offsets, strides);

	return buffer;
}

/* Copy data between GStreamer buffer and libpisp buffer */
[[maybe_unused]]
static void copy_buffer_to_pisp(GstBuffer *gstbuf, std::array<uint8_t *, 3> &mem, guint width, guint height,
								guint gst_stride, guint hw_stride, const char *format)
{
	GST_DEBUG("copy_buffer_to_pisp: width=%u, height=%u, gst_stride=%u, hw_stride=%u, format=%s", width, height,
			  gst_stride, hw_stride, format);

	GstMapInfo map;
	gst_buffer_map(gstbuf, &map, GST_MAP_READ);

	uint8_t *src = map.data;
	uint8_t *dst = mem[0];


	if (strncmp(format, "YUV420SP_COL128", 15) == 0)
	{
		uint8_t *uv = mem[1];
		memcpy(dst, src, ((width +127) / 128) * hw_stride);
		//memcpy(mem[1], src + (((width + 127) / 128) * hw_stride), ((width + 127) / 128) * hw_stride / 2);
		memset(uv, 0x20, ((width / 128) + 1) * hw_stride / 8 );
		uv += ((width / 128) + 1) * hw_stride / 8;
		memset(uv, 0x40, ((width / 128) + 1) * hw_stride / 8 );
		uv += ((width / 128) + 1) * hw_stride / 8;
		memset(uv, 0x80, ((width / 128) + 1) * hw_stride / 8 );
		uv += ((width / 128) + 1) * hw_stride / 8;
		memset(uv, 0xc0, ((width / 128) + 1) * hw_stride / 8 );
		uv += ((width / 128) + 1) * hw_stride / 8;
	}
	else if (strncmp(format, "YUV", 3) == 0)
	{
		/* Handle YUV planar formats - copy line by line for Y plane */
		for (guint y = 0; y < height; ++y)
		{
			memcpy(dst + y * hw_stride, src + y * gst_stride, width);
		}

		/* Copy UV planes if planar format */
		if (strstr(format, "420") != nullptr)
		{
			guint uv_width = width / 2;
			guint uv_height = height / 2;
			guint uv_gst_stride = gst_stride / 2;
			guint uv_hw_stride = hw_stride / 2;

			uint8_t *src_u = src + gst_stride * height;
			uint8_t *src_v = src_u + uv_gst_stride * uv_height;
			uint8_t *dst_u = mem[1] ? mem[1] : dst + hw_stride * height;
			uint8_t *dst_v = mem[2] ? mem[2] : dst_u + uv_hw_stride * uv_height;

			for (guint y = 0; y < uv_height; ++y)
			{
				memcpy(dst_u + y * uv_hw_stride, src_u + y * uv_gst_stride, uv_width);
				memcpy(dst_v + y * uv_hw_stride, src_v + y * uv_gst_stride, uv_width);
			}
		}
	}
	else
	{
		/* RGB or packed formats - copy line by line */
		guint bytes_per_pixel = (strstr(format, "RGB888") != nullptr) ? 3 : 4;
		guint line_bytes = width * bytes_per_pixel;

		for (guint y = 0; y < height; ++y)
		{
			memcpy(dst + y * hw_stride, src + y * gst_stride, line_bytes);
		}
	}

	gst_buffer_unmap(gstbuf, &map);
}

[[maybe_unused]]
static void copy_pisp_to_buffer(std::array<uint8_t *, 3> &mem, GstBuffer *gstbuf, guint width, guint height,
								guint gst_stride, guint hw_stride, const char *format)
{
	GST_DEBUG("copy_pisp_to_buffer: width=%u, height=%u, gst_stride=%u, hw_stride=%u, format=%s", width, height,
			  gst_stride, hw_stride, format);

	GstMapInfo map;
	gst_buffer_map(gstbuf, &map, GST_MAP_WRITE);

	uint8_t *src = mem[0];
	uint8_t *dst = map.data;

	if (strncmp(format, "YUV", 3) == 0)
	{
		/* Handle YUV planar formats - copy line by line for Y plane */
		for (guint y = 0; y < height; ++y)
			memcpy(dst + y * gst_stride, src + y * hw_stride, width);

		/* Copy UV planes if planar format */
		if (strstr(format, "420") != nullptr)
		{
			guint uv_width = width / 2;
			guint uv_height = height / 2;
			guint uv_gst_stride = gst_stride / 2;
			guint uv_hw_stride = hw_stride / 2;

			uint8_t *src_u = mem[1] ? mem[1] : src + hw_stride * height;
			uint8_t *src_v = mem[2] ? mem[2] : src_u + uv_hw_stride * uv_height;
			uint8_t *dst_u = dst + gst_stride * height;
			uint8_t *dst_v = dst_u + uv_gst_stride * uv_height;

			for (guint y = 0; y < uv_height; ++y)
			{
				memcpy(dst_u + y * uv_gst_stride, src_u + y * uv_hw_stride, uv_width);
				memcpy(dst_v + y * uv_gst_stride, src_v + y * uv_hw_stride, uv_width);
			}
		}
	}
	else
	{
		/* RGB or packed formats - copy line by line */
		guint bytes_per_pixel = (strstr(format, "RGB888") != nullptr) ? 3 : 4;
		guint line_bytes = width * bytes_per_pixel;

		for (guint y = 0; y < height; ++y)
			memcpy(dst + y * gst_stride, src + y * hw_stride, line_bytes);
	}

	gst_buffer_unmap(gstbuf, &map);
}

static GstFlowReturn gst_pisp_convert_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);

	if (!filter->priv->configured)
	{
		GST_ERROR_OBJECT(filter, "Not configured");
		return GST_FLOW_ERROR;
	}

	gboolean input_is_dmabuf = gst_buffer_is_dmabuf(inbuf);
	gboolean output_is_dmabuf = gst_buffer_is_dmabuf(outbuf);

	GST_DEBUG_OBJECT(filter, "Transform: input_dmabuf=%d output_dmabuf=%d", input_is_dmabuf, output_is_dmabuf);

	try
	{
		std::map<std::string, libpisp::helpers::V4l2Device::Buffer> buffers;
		gboolean using_dmabuf = (input_is_dmabuf && filter->priv->use_dmabuf_input) ||
								(output_is_dmabuf && filter->priv->use_dmabuf_output);

		/* On first dmabuf frame, import external buffers into V4L2 device */
		if (using_dmabuf && !filter->priv->dmabuf_imported)
		{
			GST_INFO_OBJECT(filter, "First dmabuf frame - importing external buffers");

			/* Import input dmabuf */
			if (input_is_dmabuf && filter->priv->use_dmabuf_input)
			{
				GstVideoInfo in_info;
				GstCaps *incaps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(trans));
				gst_video_info_from_caps(&in_info, incaps);
				gst_caps_unref(incaps);

				auto dmabuf_buffer = gst_buffer_to_dmabuf_buffer(inbuf, &in_info);
				if (dmabuf_buffer)
				{
					/* Free the MMAP buffers allocated by Setup() */
					filter->priv->backend_device->Node("pispbe-input").ReleaseBuffers();
					
					/* Import the external dmabuf into V4L2 device */
					std::vector<libpisp::helpers::V4l2Device::Buffer> import_bufs = { *dmabuf_buffer };
					filter->priv->backend_device->Node("pispbe-input").ImportBuffers(import_bufs);
					
					GST_INFO_OBJECT(filter, "Imported input dmabuf (fd=%d)", dmabuf_buffer->fd[0]);
				}
				else
				{
					GST_WARNING_OBJECT(filter, "Failed to extract dmabuf from input, falling back to memcpy");
					input_is_dmabuf = FALSE;
					using_dmabuf = FALSE;
				}
			}

			/* Import output dmabuf */
			if (output_is_dmabuf && filter->priv->use_dmabuf_output)
			{
				GstVideoInfo out_info;
				GstCaps *outcaps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SRC_PAD(trans));
				gst_video_info_from_caps(&out_info, outcaps);
				gst_caps_unref(outcaps);

				auto dmabuf_buffer = gst_buffer_to_dmabuf_buffer(outbuf, &out_info);
				if (dmabuf_buffer)
				{
					/* Free the MMAP buffers allocated by Setup() */
					filter->priv->backend_device->Node("pispbe-output0").ReleaseBuffers();
					
					/* Import the external dmabuf into V4L2 device */
					std::vector<libpisp::helpers::V4l2Device::Buffer> import_bufs = { *dmabuf_buffer };
					filter->priv->backend_device->Node("pispbe-output0").ImportBuffers(import_bufs);
					
					GST_INFO_OBJECT(filter, "Imported output dmabuf (fd=%d)", dmabuf_buffer->fd[0]);
				}
				else
				{
					GST_WARNING_OBJECT(filter, "Failed to extract dmabuf from output, falling back to memcpy");
					output_is_dmabuf = FALSE;
					using_dmabuf = FALSE;
				}
			}

			filter->priv->dmabuf_imported = using_dmabuf;
		}

		/* Get hardware buffers - either MMAP (for memcpy) or imported dmabuf */
		if (filter->priv->dmabuf_imported)
		{
			/* Use imported dmabuf buffers */
			buffers["pispbe-input"] = filter->priv->backend_device->Node("pispbe-input").Buffers()[0];
			buffers["pispbe-output0"] = filter->priv->backend_device->Node("pispbe-output0").Buffers()[0];
			GST_DEBUG_OBJECT(filter, "Using imported dmabuf buffers");
		}
		else
		{
			/* Use MMAP buffers for memcpy path */
			buffers = filter->priv->backend_device->GetBufferSlice();
			GST_DEBUG_OBJECT(filter, "Using MMAP buffers");
		}

		/* Handle input buffer - dmabuf or memcpy path */
		if (!input_is_dmabuf || !filter->priv->use_dmabuf_input)
		{
			buffers["pispbe-input"].RwSyncStart();
			/* Memcpy input: copy data to hardware buffer */
			copy_buffer_to_pisp(inbuf, buffers["pispbe-input"].mem, filter->priv->in_width, filter->priv->in_height,
								filter->priv->in_stride, filter->priv->in_hw_stride, filter->priv->in_format);
			buffers["pispbe-input"].RwSyncEnd();
			GST_DEBUG_OBJECT(filter, "Using memcpy input path");
		}
		else
		{
			GST_DEBUG_OBJECT(filter, "Using zero-copy input (dmabuf fd=%d)", buffers["pispbe-input"].fd[0]);
		}

		/* Output buffer already in buffers map */

		/* Run the hardware conversion */
		int ret = filter->priv->backend_device->Run(buffers);
		if (ret)
		{
			GST_ERROR_OBJECT(filter, "Hardware conversion failed");
			return GST_FLOW_ERROR;
		}

		/* Copy output data if using memcpy path */
		if (!output_is_dmabuf || !filter->priv->use_dmabuf_output)
		{
			buffers["pispbe-output0"].ReadSyncStart();
			copy_pisp_to_buffer(buffers["pispbe-output0"].mem, outbuf, filter->priv->out_width,
								filter->priv->out_height, filter->priv->out_stride, filter->priv->out_hw_stride,
								filter->priv->out_format);
			buffers["pispbe-output0"].ReadSyncEnd();								
			GST_DEBUG_OBJECT(filter, "Using memcpy output path");
		}
		else
		{
			GST_DEBUG_OBJECT(filter, "Using zero-copy output (dmabuf fd=%d)", buffers["pispbe-output0"].fd[0]);
		}
	}
	catch (const std::exception &e)
	{
		GST_ERROR_OBJECT(filter, "Transform failed: %s", e.what());
		return GST_FLOW_ERROR;
	}

	return GST_FLOW_OK;
}

static gboolean gst_pisp_convert_start(GstBaseTransform *trans)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);

	GST_INFO_OBJECT(filter, "Starting pispconvert element");

	libpisp::logging_init();

	try
	{
		libpisp::helpers::MediaDevice devices;

		/* Acquire a PiSP backend device */
		std::string media_dev = devices.Acquire();

		if (media_dev.empty())
		{
			GST_ERROR_OBJECT(filter, "Unable to acquire any pisp_be device!");
			return FALSE;
		}

		filter->priv->media_dev_path = g_strdup(media_dev.c_str());
		filter->priv->backend_device = std::make_unique<libpisp::helpers::BackendDevice>(media_dev);

		if (!filter->priv->backend_device->Valid())
		{
			GST_ERROR_OBJECT(filter, "Failed to create backend device");
			return FALSE;
		}

		/* Identify the hardware variant */
		const std::vector<libpisp::PiSPVariant> &variants = libpisp::get_variants();
		const media_device_info info = devices.DeviceInfo(media_dev);

		auto variant_it = std::find_if(variants.begin(), variants.end(),
									   [&info](const auto &v) { return v.BackEndVersion() == info.hw_revision; });

		if (variant_it == variants.end())
		{
			GST_ERROR_OBJECT(filter, "Backend hardware could not be identified: %u", info.hw_revision);
			return FALSE;
		}

		filter->priv->backend = std::make_unique<libpisp::BackEnd>(libpisp::BackEnd::Config({}), *variant_it);

		GST_INFO_OBJECT(filter, "Acquired device %s", media_dev.c_str());
	}
	catch (const std::exception &e)
	{
		GST_ERROR_OBJECT(filter, "Failed to start: %s", e.what());
		return FALSE;
	}

	return TRUE;
}

static gboolean gst_pisp_convert_stop(GstBaseTransform *trans)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);

	filter->priv->backend.reset();
	filter->priv->backend_device.reset();

	g_free(filter->priv->media_dev_path);
	filter->priv->media_dev_path = nullptr;
	filter->priv->configured = FALSE;
	filter->priv->use_dmabuf_input = FALSE;
	filter->priv->use_dmabuf_output = FALSE;
	filter->priv->dmabuf_imported = FALSE;

	return TRUE;
}

static gboolean gst_pisp_convert_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query [[maybe_unused]],
													GstQuery *query)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);

	GST_INFO_OBJECT(filter, "propose_allocation called");
	
	/* Log what caps are being allocated for */
	GstCaps *caps;
	gst_query_parse_allocation(query, &caps, NULL);
	if (caps)
	{
		gchar *caps_str = gst_caps_to_string(caps);
		GST_INFO_OBJECT(filter, "propose_allocation for caps: %s", caps_str);
		g_free(caps_str);
	}

	/* Propose dmabuf allocator for upstream elements */
	if (filter->priv->dmabuf_allocator)
	{
		gst_query_add_allocation_param(query, filter->priv->dmabuf_allocator, nullptr);
		GST_DEBUG_OBJECT(filter, "Proposed dmabuf allocator to upstream");
	}

	/* Add dmabuf memory feature support */
	if (caps)
	{
		GstCaps *dmabuf_caps = gst_caps_copy(caps);
		GstCapsFeatures *features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_DMABUF, nullptr);

		for (guint i = 0; i < gst_caps_get_size(dmabuf_caps); i++)
		{
			gst_caps_set_features(dmabuf_caps, i, gst_caps_features_copy(features));
		}

		gst_caps_features_free(features);
		gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);

		GST_DEBUG_OBJECT(filter, "Proposed dmabuf caps to upstream");
		gst_caps_unref(dmabuf_caps);
	}

	return TRUE;
}

static gboolean gst_pisp_convert_decide_allocation(GstBaseTransform *trans, GstQuery *query)
{
	GstPispConvert *filter = GST_PISP_CONVERT(trans);
	GstAllocator *allocator = nullptr;
	GstAllocationParams params;
	GstBufferPool *pool = nullptr;
	guint size, min, max;
	GstStructure *config;
	GstCaps *caps;
	gboolean update_pool;

	GST_DEBUG_OBJECT(filter, "decide_allocation called");

	gst_query_parse_allocation(query, &caps, nullptr);

	if (!caps)
	{
		GST_ERROR_OBJECT(filter, "No caps in allocation query");
		return FALSE;
	}

	/* Check if downstream supports dmabuf */
	GstCapsFeatures *features = gst_caps_get_features(caps, 0);
	gboolean use_dmabuf = features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF);

	GST_INFO_OBJECT(filter, "Downstream %s dmabuf", use_dmabuf ? "supports" : "doesn't support");

	/* Get allocator from query */
	if (gst_query_get_n_allocation_params(query) > 0)
	{
		gst_query_parse_nth_allocation_param(query, 0, &allocator, &params);
	}
	else
	{
		gst_allocation_params_init(&params);
	}

	/* If downstream wants dmabuf but didn't provide allocator, use ours */
	if (use_dmabuf && (!allocator || !GST_IS_DMABUF_ALLOCATOR(allocator)))
	{
		if (allocator)
			gst_object_unref(allocator);
		allocator = GST_ALLOCATOR(gst_object_ref(filter->priv->dmabuf_allocator));
		GST_DEBUG_OBJECT(filter, "Using our dmabuf allocator for output");
	}

	/* Set or update allocator in query */
	if (gst_query_get_n_allocation_params(query) > 0)
	{
		gst_query_set_nth_allocation_param(query, 0, allocator, &params);
	}
	else
	{
		gst_query_add_allocation_param(query, allocator, &params);
	}

	/* Handle buffer pool */
	if (gst_query_get_n_allocation_pools(query) > 0)
	{
		gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
		update_pool = TRUE;
	}
	else
	{
		pool = nullptr;
		size = 0;
		min = 0;
		max = 0;
		update_pool = FALSE;
	}

	if (!pool)
	{
		pool = gst_video_buffer_pool_new();
	}

	/* Calculate buffer size if not provided */
	if (size == 0)
	{
		GstVideoInfo vinfo;
		if (gst_video_info_from_caps(&vinfo, caps))
		{
			size = GST_VIDEO_INFO_SIZE(&vinfo);
		}
		else
		{
			GST_ERROR_OBJECT(filter, "Failed to get video info from caps for buffer size");
			gst_object_unref(pool);
			if (allocator)
				gst_object_unref(allocator);
			return FALSE;
		}
	}

	/* Ensure minimum buffers */
	if (min == 0)
		min = 2;
	if (max == 0)
		max = 0; /* 0 means unlimited */

	config = gst_buffer_pool_get_config(pool);
	gst_buffer_pool_config_set_params(config, caps, size, min, max);
	gst_buffer_pool_config_set_allocator(config, allocator, &params);

	/* Add video meta support */
	gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);

	if (!gst_buffer_pool_set_config(pool, config))
	{
		GST_ERROR_OBJECT(filter, "Failed to set buffer pool config");
		gst_object_unref(pool);
		if (allocator)
			gst_object_unref(allocator);
		return FALSE;
	}

	if (update_pool)
	{
		gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
	}
	else
	{
		gst_query_add_allocation_pool(query, pool, size, min, max);
	}

	gst_object_unref(pool);
	if (allocator)
		gst_object_unref(allocator);

	GST_DEBUG_OBJECT(filter, "Allocation decided: pool=%p, size=%u, min=%u, max=%u, dmabuf=%d", pool, size, min, max,
					 use_dmabuf);

	return TRUE;
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
