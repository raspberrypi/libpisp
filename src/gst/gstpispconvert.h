/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026 Raspberry Pi Ltd
 *
 * gstpispconvert.h - GStreamer element for PiSP hardware conversion
 */

#pragma once

#include <gst/gst.h>
#include <map>
#include <memory>
#include <vector>

#include "backend/backend.hpp"
#include "helpers/backend_device.hpp"

G_BEGIN_DECLS

#define PISP_NUM_OUTPUTS 2

#define GST_TYPE_PISP_CONVERT (gst_pisp_convert_get_type())
#define GST_PISP_CONVERT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PISP_CONVERT, GstPispConvert))
#define GST_PISP_CONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PISP_CONVERT, GstPispConvertClass))
#define GST_IS_PISP_CONVERT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PISP_CONVERT))
#define GST_IS_PISP_CONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PISP_CONVERT))

typedef struct _GstPispConvert GstPispConvert;
typedef struct _GstPispConvertClass GstPispConvertClass;
typedef struct _GstPispConvertPrivate GstPispConvertPrivate;

struct _GstPispConvert
{
	GstElement base_element;

	/* Pads */
	GstPad *sinkpad;
	GstPad *srcpad[PISP_NUM_OUTPUTS]; /* src0 and src1 */

	/* Private data */
	GstPispConvertPrivate *priv;
};

struct _GstPispConvertClass
{
	GstElementClass parent_class;
};

struct _GstPispConvertPrivate
{
	/* C++ objects */
	std::unique_ptr<libpisp::helpers::BackendDevice> backend_device;
	std::unique_ptr<libpisp::BackEnd> backend;

	/* Device info */
	char *media_dev_path;

	/* Configuration */
	gboolean configured;

	/* Crop settings per output */
	pisp_be_crop_config crop[PISP_NUM_OUTPUTS];

	/* Cached crop/resize settings (to avoid redundant API calls) */
	pisp_be_crop_config applied_crop[PISP_NUM_OUTPUTS];
	libpisp::BackEnd::SmartResize applied_smart_resize[PISP_NUM_OUTPUTS];

	/* Input format info */
	guint in_width;
	guint in_height;
	guint in_stride; // GStreamer buffer stride
	guint in_hw_stride; // Hardware buffer stride
	const char *in_format;

	/* Output format info - arrays for dual outputs */
	guint out_width[PISP_NUM_OUTPUTS];
	guint out_height[PISP_NUM_OUTPUTS];
	guint out_stride[PISP_NUM_OUTPUTS]; // GStreamer buffer stride
	guint out_hw_stride[PISP_NUM_OUTPUTS]; // Hardware buffer stride
	const char *out_format[PISP_NUM_OUTPUTS];
	gboolean output_enabled[PISP_NUM_OUTPUTS]; // Track which outputs are active

	/* dmabuf support */
	GstAllocator *dmabuf_allocator;
	gboolean use_dmabuf_input;
	gboolean use_dmabuf_output[PISP_NUM_OUTPUTS];

	/* Buffer pools for outputs */
	GstBufferPool *output_pool[PISP_NUM_OUTPUTS];

	/* Backend buffers: all refs per node (from GetBuffers), round-robin slice index */
	std::map<std::string, std::vector<libpisp::helpers::BufferRef>> pisp_buffers;
	guint buffer_slice_index;
	guint output_buffer_count;

	/* Caps tracking */
	GstCaps *src_caps[PISP_NUM_OUTPUTS];

	/* Pending segment event (to be forwarded after caps) */
	GstEvent *pending_segment;
};

GType gst_pisp_convert_get_type(void);
GST_ELEMENT_REGISTER_DECLARE(pispconvert);

G_END_DECLS

