/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Roland Elek <elek.roland@gmail.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MASKEDUNSHARP_H__
#define __GST_MASKEDUNSHARP_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_MASKEDUNSHARP \
  (gst_masked_unsharp_get_type())
#define GST_MASKEDUNSHARP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MASKEDUNSHARP,GstMaskedUnsharp))
#define GST_MASKEDUNSHARP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MASKEDUNSHARP,GstMaskedUnsharpClass))
#define GST_IS_MASKEDUNSHARP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MASKEDUNSHARP))
#define GST_IS_MASKEDUNSHARP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MASKEDUNSHARP))

typedef struct _GstMaskedUnsharp      GstMaskedUnsharp;
typedef struct _GstMaskedUnsharpClass GstMaskedUnsharpClass;

struct _GstMaskedUnsharp
{
  GstElement element;

  GstPad *framesink, *masksink, *srcpad;
  GstPad *framesink_internal, *sharpsink_internal;
  GstCollectPads *input;
  GstCollectData *framesink_cdata, *sharp_cdata, *masksink_cdata;

  gint width, height;

  gint sigma;
  gboolean silent;

  /* Sharpening bin stuff */
  GstElement *unsharp_bin;
  GstElement *gaussblur; /* The bin takes care of the other elements.
                          * We keep gaussblur as an instance member to be able
                          * to set the sigma property at runtime.
                          */

};

struct _GstMaskedUnsharpClass 
{
  GstElementClass parent_class;
};

GType gst_masked_unsharp_get_type (void);

G_END_DECLS

#endif /* __GST_MASKEDUNSHARP_H__ */
