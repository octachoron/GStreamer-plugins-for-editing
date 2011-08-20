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

#ifndef __GST_ADDALPHA_H__
#define __GST_ADDALPHA_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_ADDALPHA \
  (gst_add_alpha_get_type())
#define GST_ADDALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ADDALPHA,GstAddAlpha))
#define GST_ADDALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ADDALPHA,GstAddAlphaClass))
#define GST_IS_ADDALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ADDALPHA))
#define GST_IS_ADDALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ADDALPHA))

typedef struct _GstAddAlpha      GstAddAlpha;
typedef struct _GstAddAlphaClass GstAddAlphaClass;

struct _GstAddAlpha
{
  GstElement element;

  GstPad *framesink, *masksink, *srcpad;
  GstCollectPads *input;
  GstCollectData *framesink_cdata, *masksink_cdata;

  gint width, height;

  gboolean silent;

};

struct _GstAddAlphaClass
{
  GstElementClass parent_class;
};

GType gst_add_alpha_get_type (void);

G_END_DECLS

#define GST_ADDALPHA_FULLY_OPAQUE 255
#define GST_ADDALPHA_FULLY_TRANSPARENT 0


#endif /* __GST_ADDALPHA_H__ */
