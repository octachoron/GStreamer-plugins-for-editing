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

#ifndef __GST_GIMPDESPECKLE_H__
#define __GST_GIMPDESPECKLE_H__

#include <gst/gst.h>

/* Filter type bits */
#define FILTER_ADAPTIVE 0x2
#define FILTER_RECURSIVE 0x1


G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_GIMPDESPECKLE \
  (gst_gimp_despeckle_get_type())
#define GST_GIMPDESPECKLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIMPDESPECKLE,GstGimpDespeckle))
#define GST_GIMPDESPECKLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIMPDESPECKLE,GstGimpDespeckleClass))
#define GST_IS_GIMPDESPECKLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIMPDESPECKLE))
#define GST_IS_GIMPDESPECKLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIMPDESPECKLE))

typedef struct _GstGimpDespeckle      GstGimpDespeckle;
typedef struct _GstGimpDespeckleClass GstGimpDespeckleClass;

struct _GstGimpDespeckle
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean silent;

  gint width, height;

  /* Despeckle parameters */
  guint8 despeckle_radius;
  gboolean adaptive;
  gboolean recursive;
  guint8 black_level;
  guint8 white_level;
};

struct _GstGimpDespeckleClass 
{
  GstElementClass parent_class;
};

GType gst_gimp_despeckle_get_type (void);

G_END_DECLS

#endif /* __GST_GIMPDESPECKLE_H__ */
