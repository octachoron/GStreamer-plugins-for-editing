/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 * Copyright (C) 1999 Martin Weber
 * Copyright (C) 1996 Federico Mena Quintero
 * Copyright (C) 2011 Roland Elek <elek.roland@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
 * SECTION:element-gimpcolorenhance
 *
 * This plugin is a color enhancement filter ported from The GIMP. Besides
 * adapting to GStreamer buffers, the original code was modified in two ways.
 * First, the RGB-HSV color space conversion algorithms were converted to use
 * integers only, for increased performance. Second, efficiency was further
 * improved by splitting color space conversion from the enhancement itself.
 * This makes it possible to remove a redundant conversion pass.
 *
 * The original description of the ported GIMP plugin "Color Enhance 0.10"
 * by Martin Weber and Federico Mena Quintero is the following.
 * 
 * This simple plug-in does an automatic saturation
 * stretch.  For each channel in the image, it finds
 * the minimum and maximum values... it uses those
 * values to stretch the individual histograms to the
 * full range.  For some images it may do just what
 * you want; for others it may not work that well.
 * This version differs from Contrast Autostretch in
 * that it works in HSV space, and preserves hue.
 * 
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! ffmpegcolorspace ! gimpcolorenhance ! ffmpegcolorspace ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#include "gstgimpcolorenhance.h"

GST_DEBUG_CATEGORY_STATIC (gst_gimp_color_enhance_debug);
#define GST_CAT_DEFAULT gst_gimp_color_enhance_debug

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

GST_BOILERPLATE (GstGimpColorEnhance, gst_gimp_color_enhance, GstElement,
    GST_TYPE_ELEMENT);

static void gst_gimp_color_enhance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gimp_color_enhance_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gimp_color_enhance_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_gimp_color_enhance_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_gimp_color_enhance_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "GimpColorEnhance",
    "Filter/Video",
    "Color enhancement filter ported from The GIMP.",
    "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the gimpcolorenhance's class */
static void
gst_gimp_color_enhance_class_init (GstGimpColorEnhanceClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gimp_color_enhance_set_property;
  gobject_class->get_property = gst_gimp_color_enhance_get_property;

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
gst_gimp_color_enhance_init (GstGimpColorEnhance * filter,
    GstGimpColorEnhanceClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_gimp_color_enhance_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gimp_color_enhance_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
}

static void
gst_gimp_color_enhance_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGimpColorEnhance *filter = GST_GIMPCOLORENHANCE (object);

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
gst_gimp_color_enhance_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGimpColorEnhance *filter = GST_GIMPCOLORENHANCE (object);

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
gst_gimp_color_enhance_set_caps (GstPad * pad, GstCaps * caps)
{
  GstGimpColorEnhance *filter;
  GstPad *otherpad;
  GstStructure *capstruct = gst_caps_get_structure (caps, 0);
  const gchar *mimetype;  

  mimetype = gst_structure_get_name (capstruct);
  if(strcmp(mimetype, "video/x-raw-rgb") != 0) {
    g_print ("No gimpcolorenhance support for %s, only for video/x-raw-rgb.\n",
             mimetype);
    return FALSE;
  }

  filter = GST_GIMPCOLORENHANCE (gst_pad_get_parent (pad));

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


/* The actual enhancement functions, also taken from The GIMP, with changes to
 * optimize for performance. The code was converted to use integers only, and
 * color space conversion for the enhancement was split out to enable removing
 * an extra conversion pass along with the now unnecessary get_v() function.
 */

typedef struct
{
  guint8  vhi;
  guint8  vlo;
} ColorEnhanceParam_t;

static void
find_vhi_vlo (const guint8 *src,
              gpointer      data)
{
  ColorEnhanceParam_t *param = (ColorEnhanceParam_t*) data;

  guint8 v = src[2];

  if (v > param->vhi) param->vhi = v;
  if (v < param->vlo) param->vlo = v;
}

static void
colorspace_prepare (guint8 *src, guint8 *dest)
{
  guint8  h, z, v;
  gint    c, m, y;
  gint    k;
  guint8  map[3];

  c = 255 - src[0];
  m = 255 - src[1];
  y = 255 - src[2];

  k = c;
  if (m < k) k = m;
  if (y < k) k = y;

  map[0] = c - k;
  map[1] = m - k;
  map[2] = y - k;

  gimp_rgb_to_hsv4(map, &h, &z, &v);

  dest[0] = h;
  dest[1] = z;
  dest[2] = v;
  dest[3] = k;
}

static void
colorspace_prepare_reverse (guint8 *src, guint8 *dest)
{
  gint    c, m, y;
  gint    k;
  guint8  map[3];

  gimp_hsv_to_rgb4 (map, src[0], src[1], src[2]);

  c = map[0];
  m = map[1];
  y = map[2];

  k = src[3];

  c += k;
  if (c > 255) c = 255;
  m += k;
  if (m > 255) m = 255;
  y += k;
  if (y > 255) y = 255;

  dest[0] = 255 - c;
  dest[1] = 255 - m;
  dest[2] = 255 - y;
}

static void
enhance_it (const guint8 *src, guint8 *dest, guint8 vlo, guint8 vhi)
{
  guint8 v;

  v = src[2];

  if (vhi != vlo)
    v = (v - vlo) * 255 / (vhi - vlo);

  dest[2] = v;
}



/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gimp_color_enhance_chain (GstPad * pad, GstBuffer * buf)
{
  GstGimpColorEnhance *filter;
  GstBuffer *wrbuf, *hsvbuf;
  guint8 *data, *hsvdata;
  

  ColorEnhanceParam_t param;
  gint x, y;
  gint bytes_x, bytes_x_hsvk;


  filter = GST_GIMPCOLORENHANCE (GST_OBJECT_PARENT (pad));


  wrbuf = gst_buffer_make_writable (buf);
  if(buf != wrbuf) {
    gst_buffer_unref (buf);
  }
  hsvbuf = gst_buffer_new_and_alloc(filter->width * filter->height * 4);

  data = GST_BUFFER_DATA (wrbuf);
  hsvdata = GST_BUFFER_DATA (hsvbuf);

  /* Convert color space first, for speed. */

  bytes_x = filter->width * 3;
  bytes_x_hsvk = filter->width * 4;

  for(y=0; y<filter->height; y++) {
    for(x=0; x<bytes_x; x+=3) {
      colorspace_prepare (data + y*bytes_x + x, hsvdata + y*bytes_x_hsvk + x*4/3);
    }
  }

  /* Find vhi and vlo. */
  memset(&param, 0, sizeof(ColorEnhanceParam_t));

  for(y=0; y<filter->height; y++) {
    for(x=0; x<bytes_x_hsvk; x+=4) {
      find_vhi_vlo (hsvdata + y*bytes_x_hsvk + x, (gpointer)(&param));
    }
  }

  /* Run the actual enhancement. */
  
  for(y=0; y<filter->height; y++) {
    for(x=0; x<bytes_x_hsvk; x+=4) {
      enhance_it (hsvdata + y*bytes_x_hsvk + x, hsvdata + y*bytes_x_hsvk + x, param.vlo,
                  param.vhi);
    }
  }

  /* Convert colorspace back to RGB. */

  for(y=0; y<filter->height; y++) {
    for(x=0; x<bytes_x; x+=3) {
      colorspace_prepare_reverse (hsvdata + y*bytes_x_hsvk + x*4/3, data + y*bytes_x + x);
    }
  }

/*
  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");
*/

  gst_buffer_unref (hsvbuf);

  return gst_pad_push (filter->srcpad, wrbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gimpcolorenhance_init (GstPlugin * gimpcolorenhance) //TODO: fill
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template gimpcolorenhance' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_gimp_color_enhance_debug, "gimpcolorenhance",
      0, "Template gimpcolorenhance");

  return gst_element_register (gimpcolorenhance, "gimpcolorenhance", GST_RANK_NONE,
      GST_TYPE_GIMPCOLORENHANCE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgimpcolorenhance"
#endif

/* gstreamer looks for this structure to register gimpcolorenhances
 *
 * exchange the string 'Template gimpcolorenhance' with your gimpcolorenhance description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gimpcolorenhance",
    "Color enhancement filter ported from The GIMP.",
    gimpcolorenhance_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
