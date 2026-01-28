/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2026 Raspberry Pi Ltd
 *
 * gstpispconvert.h - GStreamer element for PiSP hardware conversion
 */

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

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

GType gst_pisp_convert_get_type(void);
GST_ELEMENT_REGISTER_DECLARE(pispconvert);

G_END_DECLS
