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

#include <gst/video/video.h>

#include "helpers/backend_device.hpp"
#include "helpers/media_device.hpp"
#include "libpisp/backend/backend.hpp"
#include "libpisp/common/logging.hpp"
#include "libpisp/common/utils.hpp"
#include "libpisp/variants/variant.hpp"

GST_DEBUG_CATEGORY_STATIC(gst_pisp_convert_debug);
#define GST_CAT_DEFAULT gst_pisp_convert_debug

/* Supported formats - matching those from convert.cpp */
static GstStaticPadTemplate sink_template =
	GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
							GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGB, I420, YV12, Y42B, Y444, YUY2, UYVY }")));

static GstStaticPadTemplate src_template =
	GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
							GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGB, I420, YV12, Y42B, Y444, YUY2, UYVY }")));

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

/* Initialize the class */
static void gst_pisp_convert_class_init(GstPispConvertClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->finalize = gst_pisp_convert_finalize;

	gst_element_class_set_static_metadata(element_class, "PiSP Hardware Converter", "Filter/Converter/Video/Scaler",
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

	gst_base_transform_set_in_place(GST_BASE_TRANSFORM(filter), FALSE);
	gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(filter), FALSE);
}

static void gst_pisp_convert_finalize(GObject *object)
{
	GstPispConvert *filter = GST_PISP_CONVERT(object);

	if (filter->priv)
	{
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
	gint i, n;

	caps_str = gst_caps_to_string(caps);
	GST_DEBUG_OBJECT(convert, "transform_caps called, direction: %s, caps: %s",
					 direction == GST_PAD_SINK ? "SINK" : "SRC", caps_str);
	g_free(caps_str);

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

		/* We support arbitrary scaling, so only preserve framerate and PAR */
		GstStructure *new_structure = gst_structure_copy(gst_caps_get_structure(tmp, 0));

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

		gst_caps_append_structure(ret, new_structure);
	}

	gst_caps_unref(tmp);

	if (filter)
	{
		gchar *filter_str = gst_caps_to_string(filter);
		GST_DEBUG_OBJECT(convert, "Applying filter: %s", filter_str);
		g_free(filter_str);

		GstCaps *intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(ret);
		ret = intersection;
	}

	caps_str = gst_caps_to_string(ret);
	GST_DEBUG_OBJECT(convert, "Returning caps: %s", caps_str);
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

	if (!gst_video_info_from_caps(&in_info, incaps))
	{
		GST_ERROR_OBJECT(filter, "Failed to parse input caps");
		return FALSE;
	}

	if (!gst_video_info_from_caps(&out_info, outcaps))
	{
		GST_ERROR_OBJECT(filter, "Failed to parse output caps");
		return FALSE;
	}

	/* Store format information from negotiated caps */
	filter->priv->in_width = GST_VIDEO_INFO_WIDTH(&in_info);
	filter->priv->in_height = GST_VIDEO_INFO_HEIGHT(&in_info);
	filter->priv->in_stride = GST_VIDEO_INFO_PLANE_STRIDE(&in_info, 0);
	filter->priv->in_format = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&in_info));

	filter->priv->out_width = GST_VIDEO_INFO_WIDTH(&out_info);
	filter->priv->out_height = GST_VIDEO_INFO_HEIGHT(&out_info);
	filter->priv->out_stride = GST_VIDEO_INFO_PLANE_STRIDE(&out_info, 0);
	filter->priv->out_format = gst_format_to_pisp(GST_VIDEO_INFO_FORMAT(&out_info));

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

/* Copy data between GStreamer buffer and libpisp buffer */
static void copy_buffer_to_pisp(GstBuffer *gstbuf, std::array<uint8_t *, 3> &mem, guint width, guint height,
								guint gst_stride, guint hw_stride, const char *format)
{
	GST_DEBUG("copy_buffer_to_pisp: width=%u, height=%u, gst_stride=%u, hw_stride=%u, format=%s", width, height,
			  gst_stride, hw_stride, format);

	GstMapInfo map;
	gst_buffer_map(gstbuf, &map, GST_MAP_READ);

	uint8_t *src = map.data;
	uint8_t *dst = mem[0];

	if (strncmp(format, "YUV", 3) == 0)
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

	try
	{
		/* Acquire buffers from the backend device */
		auto buffers = filter->priv->backend_device->AcquireBuffers();

		/* Copy input data to PiSP buffer */
		copy_buffer_to_pisp(inbuf, buffers["pispbe-input"].mem, filter->priv->in_width, filter->priv->in_height,
							filter->priv->in_stride, filter->priv->in_hw_stride, filter->priv->in_format);

		/* Run the hardware conversion */
		int ret = filter->priv->backend_device->Run(buffers);
		if (ret)
		{
			GST_ERROR_OBJECT(filter, "Hardware conversion failed");
			filter->priv->backend_device->ReturnBuffer(buffers);
			return GST_FLOW_ERROR;
		}

		/* Copy output data from PiSP buffer */
		copy_pisp_to_buffer(buffers["pispbe-output0"].mem, outbuf, filter->priv->out_width, filter->priv->out_height,
							filter->priv->out_stride, filter->priv->out_hw_stride, filter->priv->out_format);

		filter->priv->backend_device->ReturnBuffer(buffers);
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

		filter->priv->backend =
			std::make_unique<libpisp::BackEnd>(libpisp::BackEnd::Config({}), *variant_it);

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
