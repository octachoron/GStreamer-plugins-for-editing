/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 * Copyright (C) 1997 Scott Goehring
 * Copyright (C) 1996 Federico Mena Quintero
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

/**
 * SECTION:element-gimpcontraststretch
 *
 * This plugin is a video contrast stretch filter ported from The GIMP. It
 * transforms RGB frames to the HSV color space, and performs a histogram
 * stretch on the saturation and value channels. Besides adapting to GStreamer
 * buffers, the original code was modified in two ways. First, the RGB-HSV color
 * space algorithms were converted to use integers only, for increased
 * performance. Second, efficiency was further improved by splitting color space
 * conversion from the enhancement itself. This makes it possible to remove a
 * redundant conversion pass.
 *
 * The original description of the ported GIMP plugin "Autostretch HSV 0.10" by
 * Scott Goehring and Federico Mena Quintero is the following.
 *
 * This simple plug-in does an automatic contrast
 * stretch.  For each channel in the image, it finds
 * the minimum and maximum values... it uses those
 * values to stretch the individual histograms to the
 * full contrast range.  For some images it may do
 * just what you want; for others it may be total
 * crap :).  This version differs from Contrast
 * Autostretch in that it works in HSV space, and
 * preserves hue.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! ffmpegcolorspace ! gimpcontraststretch ! ffmpegcolorspace ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstgimpcontraststretch.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_gimpcontraststretch_debug);
#define GST_CAT_DEFAULT gst_gimpcontraststretch_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
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

GST_BOILERPLATE (Gstgimpcontraststretch, gst_gimpcontraststretch, GstElement,
    GST_TYPE_ELEMENT);

static void gst_gimpcontraststretch_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gimpcontraststretch_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gimpcontraststretch_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_gimpcontraststretch_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_gimpcontraststretch_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "gimpcontraststretch",
    "Filter/Video",
    "HSV contrast stretch filter ported from The GIMP.",
    "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the gimpcontraststretch's class */
static void
gst_gimpcontraststretch_class_init (GstgimpcontraststretchClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gimpcontraststretch_set_property;
  gobject_class->get_property = gst_gimpcontraststretch_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_gimpcontraststretch_init (Gstgimpcontraststretch * filter,
    GstgimpcontraststretchClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_gimpcontraststretch_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gimpcontraststretch_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
}

static void
gst_gimpcontraststretch_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstgimpcontraststretch *filter = GST_GIMPCONTRASTSTRETCH (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gimpcontraststretch_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstgimpcontraststretch *filter = GST_GIMPCONTRASTSTRETCH (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_gimpcontraststretch_set_caps (GstPad * pad, GstCaps * caps)
{
  Gstgimpcontraststretch *filter;
  GstPad *otherpad;
  GstStructure *capstruct = gst_caps_get_structure (caps, 0);
  const gchar *mimetype;  

  mimetype = gst_structure_get_name (capstruct);
  if(strcmp(mimetype, "video/x-raw-rgb") != 0) {
    g_print ("No gimpcontraststretch support for %s, only for video/x-raw-rgb.\n",
             mimetype);
    return FALSE;
  }

  filter = GST_GIMPCONTRASTSTRETCH (gst_pad_get_parent (pad));

  gst_structure_get_int (capstruct, "width", &filter->width);
  gst_structure_get_int (capstruct, "height", &filter->height);

  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}



/* ROUND macro from The GIMP */

#define ROUND(x) ((int) ((x) + 0.5))

/* GIMP color space conversion functions, converted to integer only for
 * speed. */

void
gimp_rgb_to_hsv4 (const guint8 *rgb,
                  guint8      *hue,
                  guint8      *saturation,
                  guint8      *value)
{
  guint8 red, green, blue;
  gint h, s, v;
  guint8 min, max;
  guint8 delta;

  red   = rgb[0];
  green = rgb[1];
  blue  = rgb[2];

  h = 0; /* Shut up -Wall */

  if (red > green)
    {
      max = MAX (red,   blue);
      min = MIN (green, blue);
    }
  else
    {
      max = MAX (green, blue);
      min = MIN (red,   blue);
    }

  v = max;

  if (max != 0)
    s = (max - min) * 255 / max;
  else
    s = 0;

  if (s == 0)
    h = 0;
  else
    {
      delta = max - min;

      if (delta == 0)
        delta = 255;

      if (red == max)
        h = (green - blue) * 255 / delta;
      else if (green == max)
        h = 510 + (blue - red) * 255 / delta;
      else if (blue == max)
        h = 1020 + (red - green) * 255 / delta;

      h *= 60;
      h /= 255;

      if (h < 0)
        h += 360;
      else if (h > 360)
        h -= 360;

      h = h * 255 / 360;
    }

  *hue        = h;
  *saturation = s;
  *value      = v;
}

void
gimp_hsv_to_rgb4 (guint8  *rgb,
                  guint8  hue,
                  guint8  saturation,
                  guint8  value)
{
  gint h, s, v;
  gint f, p, q, t;

  if (saturation == 0)
    {
      hue        = value;
      saturation = value;
      value      = value;
    }
  else
    {
      h = hue * 360 / 255; /* [0, 255] to [0, 360] (degrees) */
      s = saturation;
      v = value;


      f = h % 60;
      p = (v * (255 - s)) / 255;
      q = v * (255*59 - s * f) / (255*59);
      t = v * ((255*59 - s * (59 - f))) / (255*59);


      switch (h/60)
        {
        case 0:
          hue        = v;
          saturation = t;
          value      = p;
          break;

        case 1:
          hue        = q;
          saturation = v;
          value      = p;
          break;

        case 2:
          hue        = p;
          saturation = v;
          value      = t;
          break;

        case 3:
          hue        = p;
          saturation = q;
          value      = v;
          break;

        case 4:
          hue        = t;
          saturation = p;
          value      = v;
          break;

        case 5:
          hue        = v;
          saturation = p;
          value      = q;
          break;
        }
    }

  rgb[0] = hue;
  rgb[1] = saturation;
  rgb[2] = value;
}


/* Original plugin stuff */

typedef struct {
  guint8 shi;
  guint8 slo;
  guint8 vhi;
  guint8 vlo;
} AutostretchData;

static void
find_max (guchar *src, AutostretchData *data)
{
  guint8 s, v;

  s = src[1];
  v = src[2];

  if (s > data->shi) data->shi = s;
  if (s < data->slo) data->slo = s;
  if (v > data->vhi) data->vhi = v;
  if (v < data->vlo) data->vlo = v;
}

static void
autostretch_hsv_func (guchar *src, guchar *dest,
		      AutostretchData *data)
{
  gint s, v;

  s = src[1];
  v = src[2];  

  if (data->shi != data->slo)
    s = (s - data->slo) * 255 / (data->shi - data->slo);
  if (data->vhi != data->vlo)
    v = (v - data->vlo) * 255 / (data->vhi - data->vlo);

  dest[0] = src[0]; /* Don't touch hue */
  dest[1] = s;
  dest[2] = v;
}



/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gimpcontraststretch_chain (GstPad * pad, GstBuffer * buf)
{
  Gstgimpcontraststretch *filter;
  GstBuffer *wrbuf;
  guint8 *data;

  AutostretchData param;

  gint x, y, idx;

  filter = GST_GIMPCONTRASTSTRETCH (GST_OBJECT_PARENT (pad));

  wrbuf = gst_buffer_make_writable (buf);
  if(buf != wrbuf) {
    gst_buffer_unref (buf);
  }

  data = GST_BUFFER_DATA (wrbuf);


  /* To HSV */

  for (y=0; y < filter->height; y++) {
    for (x=0; x < filter->width * 3; x+=3) {
      idx = y*filter->width*3 + x;
      gimp_rgb_to_hsv4(data + idx, data+idx, data + idx + 1, data + idx + 2);
    }
  }

  /* Find maximum and minimum values and saturations */

  memset (&param, 0, sizeof(AutostretchData));

  for (y=0; y < filter->height; y++) {
    for (x=0; x < filter->width * 3; x+=3) {
      idx = y*filter->width*3 + x;
      find_max (data + idx, &param);                     
    }
  }


  /* Do the actual stretching */

  for (y=0; y < filter->height; y++) {
    for (x=0; x < filter->width * 3; x+=3) {
      idx = y*filter->width*3 + x;
      autostretch_hsv_func (data + idx, data + idx, &param);
    }
  }

  /* Back to RGB */

  for (y=0; y < filter->height; y++) {
    for (x=0; x < filter->width * 3; x+=3) {
      idx = y*filter->width*3 + x;
      gimp_hsv_to_rgb4(data + idx, data[idx], data[idx + 1], data[idx + 2]);
    }
  }

  return gst_pad_push (filter->srcpad, wrbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gimpcontraststretch_init (GstPlugin * gimpcontraststretch)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template gimpcontraststretch' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_gimpcontraststretch_debug, "gimpcontraststretch",
      0, "Template gimpcontraststretch");

  return gst_element_register (gimpcontraststretch, "gimpcontraststretch", GST_RANK_NONE,
      GST_TYPE_GIMPCONTRASTSTRETCH);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgimpcontraststretch"
#endif

/* gstreamer looks for this structure to register gimpcontraststretchs
 *
 * exchange the string 'Template gimpcontraststretch' with your gimpcontraststretch description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gimpcontraststretch",
    "HSV contrast stretch filter ported from The GIMP.",
    gimpcontraststretch_init,
    VERSION,
    "GPL",
    "GStreamer",
    "http://gstreamer.net/"
)
