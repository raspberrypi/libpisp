/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026 Raspberry Pi Ltd
 *
 * gstpispconvert.h - GStreamer element for PiSP hardware conversion
 */

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <memory>
#include <vector>

#include "backend/backend.hpp"
#include "helpers/backend_device.hpp"

G_BEGIN_DECLS

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
	GstBaseTransform base_transform;

	/* Private data */
	GstPispConvertPrivate *priv;
};

struct _GstPispConvertClass
{
	GstBaseTransformClass parent_class;
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

	/* Input/Output format info */
	guint in_width;
	guint in_height;
	guint in_stride; // GStreamer buffer stride
	guint in_hw_stride; // Hardware buffer stride
	const char *in_format;

	guint out_width;
	guint out_height;
	guint out_stride; // GStreamer buffer stride
	guint out_hw_stride; // Hardware buffer stride
	const char *out_format;

	/* dmabuf support */
	GstAllocator *dmabuf_allocator;
	gboolean use_dmabuf_input;
	gboolean use_dmabuf_output;
	gboolean dmabuf_imported;
};

GType gst_pisp_convert_get_type(void);
GST_ELEMENT_REGISTER_DECLARE(pispconvert);

G_END_DECLS

