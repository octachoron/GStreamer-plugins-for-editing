/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Roland Elek <elek.roland@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GST_GIMPCOLORENHANCE_H__
#define __GST_GIMPCOLORENHANCE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_GIMPCOLORENHANCE \
  (gst_gimp_color_enhance_get_type())
#define GST_GIMPCOLORENHANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIMPCOLORENHANCE,GstGimpColorEnhance))
#define GST_GIMPCOLORENHANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIMPCOLORENHANCE,GstGimpColorEnhanceClass))
#define GST_IS_GIMPCOLORENHANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIMPCOLORENHANCE))
#define GST_IS_GIMPCOLORENHANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIMPCOLORENHANCE))

typedef struct _GstGimpColorEnhance      GstGimpColorEnhance;
typedef struct _GstGimpColorEnhanceClass GstGimpColorEnhanceClass;

struct _GstGimpColorEnhance
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean silent;
  gint width, height;
};

struct _GstGimpColorEnhanceClass 
{
  GstElementClass parent_class;
};

GType gst_gimp_color_enhance_get_type (void);

G_END_DECLS

#endif /* __GST_GIMPCOLORENHANCE_H__ */
