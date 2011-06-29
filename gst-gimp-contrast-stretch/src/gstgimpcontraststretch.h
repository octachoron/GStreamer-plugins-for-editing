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

#ifndef __GST_GIMPCONTRASTSTRETCH_H__
#define __GST_GIMPCONTRASTSTRETCH_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_GIMPCONTRASTSTRETCH \
  (gst_gimpcontraststretch_get_type())
#define GST_GIMPCONTRASTSTRETCH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIMPCONTRASTSTRETCH,Gstgimpcontraststretch))
#define GST_GIMPCONTRASTSTRETCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIMPCONTRASTSTRETCH,GstgimpcontraststretchClass))
#define GST_IS_GIMPCONTRASTSTRETCH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIMPCONTRASTSTRETCH))
#define GST_IS_GIMPCONTRASTSTRETCH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIMPCONTRASTSTRETCH))

typedef struct _Gstgimpcontraststretch      Gstgimpcontraststretch;
typedef struct _GstgimpcontraststretchClass GstgimpcontraststretchClass;

struct _Gstgimpcontraststretch
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean silent;
  gint width, height;
};

struct _GstgimpcontraststretchClass 
{
  GstElementClass parent_class;
};

GType gst_gimpcontraststretch_get_type (void);

G_END_DECLS

#endif /* __GST_GIMPCONTRASTSTRETCH_H__ */
