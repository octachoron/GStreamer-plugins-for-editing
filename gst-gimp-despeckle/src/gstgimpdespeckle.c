/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Roland Elek <elek.roland@gmail.com>
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * Copyright (C) 1997-1998 Michael Sweet (mike@easysw.com)
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

/**
 * SECTION:element-gimpdespeckle
 *
 * This is an optionally adaptive and/or recursive median filter ported from
 * The GIMP. It can be used to reduce speckle noise and salt-pepper noise in
 * image frames.
 *
 * The original descripton of Michael Sweet's GIMP plugin is the following.
 * This plug-in selectively performs a median or adaptive box filter on an
 * image.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! ffmpegcolorspace ! gimpdespeckle ! ffmpegcolorspace ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include <string.h>

#include "gstgimpdespeckle.h"


#define SQR(x) ((x) * (x))


GST_DEBUG_CATEGORY_STATIC (gst_gimp_despeckle_debug);
#define GST_CAT_DEFAULT gst_gimp_despeckle_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_RADIUS,
  PROP_ADAPTIVE,
  PROP_RECURSIVE,
  PROP_BLACK_LEVEL,
  PROP_WHITE_LEVEL
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );

GST_BOILERPLATE (GstGimpDespeckle, gst_gimp_despeckle, GstElement,
    GST_TYPE_ELEMENT);

static void gst_gimp_despeckle_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gimp_despeckle_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gimp_despeckle_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_gimp_despeckle_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_gimp_despeckle_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "GimpDespeckle",
    "Filter/Video",
    "Despeckle filter ported from The GIMP",
    "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the gimpdespeckle's class */
static void
gst_gimp_despeckle_class_init (GstGimpDespeckleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gimp_despeckle_set_property;
  gobject_class->get_property = gst_gimp_despeckle_get_property;


  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_RADIUS,
      g_param_spec_int ("despeckle_radius", "Despeckle radius", "Initial radius of the region around the target pixel used for median filtering.",
          0, 255, 1, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ADAPTIVE,
      g_param_spec_boolean ("adaptive", "Adaptive", "If true, analyze the histogram of the region around the target pixel, and adjust despeckle diameter accordingly, without increasing it past the initial value.",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_RECURSIVE,
      g_param_spec_boolean ("recursive", "Recursive", "If true, apply the filter recursively by writing filter results also to source data while filtering. Produces a stronger effect, and in some cases, weird results.",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_BLACK_LEVEL,
      g_param_spec_int ("black_level", "Black level", "Threshold under which pixels are considered completely dark.",
          0, 255, 7, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_WHITE_LEVEL,
      g_param_spec_int ("white_level", "White level", "Threshold over which pixels are considered completely bright.",
          0, 255, 248, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_gimp_despeckle_init (GstGimpDespeckle * filter,
    GstGimpDespeckleClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_gimp_despeckle_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gimp_despeckle_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
  filter->despeckle_radius = 1;
  filter->adaptive = FALSE;
  filter->recursive = FALSE;
  filter->black_level = 7;
  filter->white_level = 248;
}

static void
gst_gimp_despeckle_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGimpDespeckle *filter = GST_GIMPDESPECKLE (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_RADIUS:
      filter->despeckle_radius = g_value_get_int (value);
      break;
    case PROP_ADAPTIVE:
      filter->adaptive = g_value_get_boolean (value);
      break;
    case PROP_RECURSIVE:
      filter->recursive = g_value_get_boolean (value);
      break;
    case PROP_BLACK_LEVEL:
      filter->black_level = g_value_get_boolean (value);
      break;
    case PROP_WHITE_LEVEL:
      filter->white_level = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gimp_despeckle_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGimpDespeckle *filter = GST_GIMPDESPECKLE (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_RADIUS:
      g_value_set_int (value, filter->despeckle_radius);
      break;
    case PROP_ADAPTIVE:
      g_value_set_boolean (value, filter->adaptive);
      break;
    case PROP_RECURSIVE:
      g_value_set_boolean (value, filter->recursive);
      break;
    case PROP_BLACK_LEVEL:
      g_value_set_int (value, filter->black_level);
      break;
    case PROP_WHITE_LEVEL:
      g_value_set_int (value, filter->white_level);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_gimp_despeckle_set_caps (GstPad * pad, GstCaps * caps)
{
  GstGimpDespeckle *filter;
  GstPad *otherpad;
  GstStructure *capstruct = gst_caps_get_structure (caps, 0);
  const gchar *mimetype;  

  mimetype = gst_structure_get_name (capstruct);
  if(strcmp(mimetype, "video/x-raw-rgb") != 0) {
    g_print ("No gimpdespeckle support for %s, only for video/x-raw-rgb.\n",
             mimetype);
    return FALSE;
  }

  filter = GST_GIMPDESPECKLE (gst_pad_get_parent (pad));

  gst_structure_get_int (capstruct, "width", &filter->width);
  gst_structure_get_int (capstruct, "height", &filter->height);

  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* From gimprgb.h */

#define GIMP_RGB_LUMINANCE_RED    (0.2126)
#define GIMP_RGB_LUMINANCE_GREEN  (0.7152)
#define GIMP_RGB_LUMINANCE_BLUE   (0.0722)

#define GIMP_RGB_LUMINANCE(r,g,b) ((r) * GIMP_RGB_LUMINANCE_RED   + \
                                   (g) * GIMP_RGB_LUMINANCE_GREEN + \
                                   (b) * GIMP_RGB_LUMINANCE_BLUE)


/* From GIMP despeckle filter */

#define VALUE_SWAP(a,b)   \
  { register gdouble t = (a); (a) = (b); (b) = t; }
#define POINTER_SWAP(a,b) \
  { register const guchar *t = (a); (a) = (b); (b) = t; }

/*
 * This Quickselect routine is based on the algorithm described in
 * "Numerical recipes in C", Second Edition,
 * Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
 * This code by Nicolas Devillard - 1998. Public domain.
 *
 * Modified to swap pointers: swap is done by comparing luminance
 * value for the pointer to RGB.
 */
static gint
quick_median_select (const guchar **p,
                     guchar        *i,
                     gint           n)
{
  gint low    = 0;
  gint high   = n - 1;
  gint median = (low + high) / 2;

  while (TRUE)
    {
      gint middle, ll, hh;

      if (high <= low) /* One element only */
        return median;

      if (high == low + 1)
        {
          /* Two elements only */
          if (i[low] > i[high])
            {
               VALUE_SWAP (i[low], i[high]);
               POINTER_SWAP (p[low], p[high]);
            }

          return median;
        }

      /* Find median of low, middle and high items; swap into position low */
      middle = (low + high) / 2;

      if (i[middle] > i[high])
        {
           VALUE_SWAP (i[middle], i[high]);
           POINTER_SWAP (p[middle], p[high]);
        }

      if (i[low] > i[high])
        {
           VALUE_SWAP (i[low], i[high]);
           POINTER_SWAP (p[low], p[high]);
        }

      if (i[middle] > i[low])
        {
          VALUE_SWAP (i[middle], i[low]);
          POINTER_SWAP (p[middle], p[low]);
        }

      /* Swap low item (now in position middle) into position (low+1) */
      VALUE_SWAP (i[middle], i[low+1]);
      POINTER_SWAP (p[middle], p[low+1]);

      /* Nibble from each end towards middle, swapping items when stuck */
      ll = low + 1;
      hh = high;

      while (TRUE)
        {
           do ll++;
           while (i[low] > i[ll]);

           do hh--;
           while (i[hh]  > i[low]);

           if (hh < ll)
             break;

           VALUE_SWAP (i[ll], i[hh]);
           POINTER_SWAP (p[ll], p[hh]);
        }

      /* Swap middle item (in position low) back into correct position */
      VALUE_SWAP (i[low], i[hh]);
      POINTER_SWAP (p[low], p[hh]);

      /* Re-set active partition */
      if (hh <= median)
        low = ll;

      if (hh >= median)
        high = hh - 1;
    }
}


static inline guchar
pixel_luminance (const guchar *p,
                 gint          bpp)
{
  switch (bpp)
    {
    case 1:
    case 2:
      return p[0];

    case 3:
    case 4:
      return GIMP_RGB_LUMINANCE (p[0], p[1], p[2]);

    default:
      return 0; /* should not be reached */
    }
}

static void
despeckle_median (guchar   *src,
                  guchar   *dst,
                  gint      width,
                  gint      height,
                  gint      bpp,
                  gboolean  preview,
                  GstGimpDespeckle *filter)
{
  const guchar **buf;
  guchar        *ibuf;
  guint          progress;
  guint          max_progress;
  gint           x, y;
  gint           u, v;
  gint           diameter;
  gint           box;
  gint           pos;

  guint8 radius = filter->despeckle_radius;
  guint8 filter_type = ((guint8)filter->adaptive << 1) |
                          (guint8)filter->recursive;
  guint8 black_level = filter->black_level;
  guint8 white_level = filter->white_level;

  progress     = 0;
  max_progress = width * height;

  diameter = (2 * radius) + 1;
  box      = SQR (diameter);
  /* TODO: possible performance increase if we don't allocate these for every
   * frame, but just once when */
  buf      = g_new (const guchar *, box);
  ibuf     = g_new (guchar, box);


  for (y = 0; y < height; y++)
    {
      gint ymin = MAX (0, y - radius);
      gint ymax = MIN (height - 1, y + radius);

      for (x = 0; x < width; x++)
        {
          gint xmin    = MAX (0, x - radius);
          gint xmax    = MIN (width - 1, x + radius);
          gint hist0   = 0;
          gint hist255 = 0;
          gint med     = -1;

          for (v = ymin; v <= ymax; v++)
            {
              for (u = xmin; u <= xmax; u++)
                {
                  gint value;

                  pos = (u + (v * width)) * bpp;
                  value = pixel_luminance (src + pos, bpp);

                  if (value > black_level && value < white_level)
                    {
                      med++;
                      buf[med]  = src + pos;
                      ibuf[med] = value;
                    }
                  else
                    {
                      if (value <= black_level)
                        hist0++;

                      if (value >= white_level)
                        hist255++;
                    }
                }
             }

           pos = (x + (y * width)) * bpp;

           if (med < 1)
             {
               //pixel_copy (dst + pos, src + pos, bpp);
               memcpy (dst+pos, src+pos, bpp);
             }
           else
             {
               const guchar *pixel;

               pixel = buf[quick_median_select (buf, ibuf, med + 1)];

                if (filter_type & FILTER_RECURSIVE)
                  memcpy (src+pos, pixel, bpp);
                  //pixel_copy (src + pos, pixel, bpp);

               memcpy (dst+pos, pixel, bpp);
               //pixel_copy (dst + pos, pixel, bpp);
             }

          /*
           * Check the histogram and adjust the diameter accordingly...
           */
          if (filter_type & FILTER_ADAPTIVE)
            {
              if (hist0 >= radius || hist255 >= radius)
                {
                  if (radius < diameter / 2)
                    radius++;
                }
              else if (radius > 1)
                {
                  radius--;
                }
            }

        }

    }

  g_free (ibuf);
  g_free (buf);
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gimp_despeckle_chain (GstPad * pad, GstBuffer * buf)
{
  GstGimpDespeckle *filter;
  GstBuffer *cpbuf, *wrbuf;
  guint8 *origdata, *newdata;

  filter = GST_GIMPDESPECKLE (GST_OBJECT_PARENT (pad));

  cpbuf = gst_buffer_copy (buf);
  wrbuf = gst_buffer_make_writable (cpbuf);
  if(wrbuf != cpbuf) {
    gst_buffer_unref(cpbuf);
  }
  cpbuf = gst_buffer_make_writable (buf);
  if(cpbuf != buf) {
    gst_buffer_unref(buf);
  }

  origdata = GST_BUFFER_DATA (buf);
  newdata = GST_BUFFER_DATA (wrbuf);

  despeckle_median (origdata, newdata, filter->width, filter->height, 3,
                      FALSE, filter);

  gst_buffer_unref (cpbuf);
  return gst_pad_push (filter->srcpad, wrbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gimpdespeckle_init (GstPlugin * gimpdespeckle)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template gimpdespeckle' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_gimp_despeckle_debug, "gimpdespeckle",
      0, "GIMP despeckle filter");

  return gst_element_register (gimpdespeckle, "gimpdespeckle", GST_RANK_NONE,
      GST_TYPE_GIMPDESPECKLE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgimpdespeckle"
#endif

/* gstreamer looks for this structure to register gimpdespeckles
 *
 * exchange the string 'Template gimpdespeckle' with your gimpdespeckle description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gimpdespeckle",
    "Despeckle filter ported from The GIMP",
    gimpdespeckle_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
