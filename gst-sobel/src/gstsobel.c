/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
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
 * SECTION:element-sobel
 *
 * This filter calculates gradient magnitude for every pixel in I420 video
 * frames using the Sobel operator on the luminance channel.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! sobel ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include <string.h>
#include <math.h>

#include "gstsobel.h"

const gint8 sobel_x[3][3] = { {-1, 0, 1 },
                              {-2, 0, 2 },
                              {-1, 0, 1 } };

const gint8 sobel_y[3][3] = { {-1, -2, -1 },
                              { 0,  0,  0 },
                              { 1,  2,  1 } };


GST_DEBUG_CATEGORY_STATIC (gst_sobel_debug);
#define GST_CAT_DEFAULT gst_sobel_debug

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
  PROP_MIRROR,
  PROP_ABS_MAGNITUDE,
  PROP_CLAMP
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
                        "format = (fourcc) I420")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
                        "format = (fourcc) I420")
    );

GST_BOILERPLATE (GstSobel, gst_sobel, GstElement,
    GST_TYPE_ELEMENT);

static void gst_sobel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sobel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_sobel_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_sobel_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_sobel_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "Sobel",
    "Filter/Video",
    "This filter calculates gradient magnitude for every pixel in I420 video frames using the Sobel operator on the luminance channel.",
    "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the sobel's class */
static void
gst_sobel_class_init (GstSobelClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_sobel_set_property;
  gobject_class->get_property = gst_sobel_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MIRROR,
      g_param_spec_boolean ("mirror", "Mirror", "If true, clamp the indices\
between zero and maximum dimension in the gradient calculation, effectively\
mirroring border pixels outside the frame, so that the operator can be applied\
to them. This reduces performance. If false, border pixels will be black.",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ABS_MAGNITUDE,
      g_param_spec_boolean ("abs-magnitude", "Use absolute gradient magnitude",
          "Calculate gradient magnitude using the formula "
          "G = abs(Gx) + abs(Gy), instead of G = sqrt(Gx*Gx + Gy*Gy). "
          "This increases performance at the cost of a little accuracy.",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CLAMP,
      g_param_spec_boolean ("clamp", "Clamp",
          "Instead of range conversion, clamp the output of the operator "
          "inside [0,255].",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_sobel_init (GstSobel * filter,
    GstSobelClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_sobel_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_sobel_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
  filter->mirror = TRUE;
  filter->abs_magnitude = FALSE;
  filter->clamp = FALSE;
}

static void
gst_sobel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSobel *filter = GST_SOBEL (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_MIRROR:
      filter->mirror = g_value_get_boolean (value);
      break;
    case PROP_ABS_MAGNITUDE:
      filter->abs_magnitude = g_value_get_boolean (value);
      break;
    case PROP_CLAMP:
      filter->clamp = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_sobel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSobel *filter = GST_SOBEL (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_MIRROR:
      g_value_set_boolean (value, filter->mirror);
      break;
    case PROP_ABS_MAGNITUDE:
      g_value_set_boolean (value, filter->abs_magnitude);
      break;
    case PROP_CLAMP:
      g_value_set_boolean (value, filter->clamp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_sobel_set_caps (GstPad * pad, GstCaps * caps)
{
  GstSobel *filter;
  GstPad *otherpad;
  GstStructure *capstruct = gst_caps_get_structure (caps, 0);
  const gchar *mimetype;  

  mimetype = gst_structure_get_name (capstruct);
  if(strcmp(mimetype, "video/x-raw-yuv") != 0) {
    g_print ("No sobel support for %s, only for video/x-raw-yuv.\n",
             mimetype);
    return FALSE;
  }

  filter = GST_SOBEL (gst_pad_get_parent (pad));

  gst_structure_get_int (capstruct, "width", &filter->width);
  gst_structure_get_int (capstruct, "height", &filter->height);

  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_sobel_chain (GstPad * pad, GstBuffer * buf)
{
  GstSobel *filter;
  GstBuffer *destbuf;
  gint i, j;
  guint8 *origdata, *newdata;

  gint mi, mj;
  gint g_x, g_y;

  guint8 skip_border = 1;

  gint result;


  filter = GST_SOBEL (GST_OBJECT_PARENT (pad));

  destbuf = gst_buffer_copy (buf);
  destbuf = gst_buffer_make_writable (destbuf);

  origdata = GST_BUFFER_DATA (buf);
  newdata = GST_BUFFER_DATA (destbuf);

  if(filter->mirror == TRUE) {
    skip_border = 0;
  }

  for(i=skip_border; i < (filter->height - skip_border); i++) {
    for(j=skip_border; j < (filter->width - skip_border); j++) {

      /* Set chroma to gray */
      newdata[filter->height*filter->width +
              (filter->width/2*(i/2)+(j/2))] = 127;
      newdata[filter->height*filter->width + 
              (filter->width/2*(i/2)+(j/2)) +
               filter->height*filter->width/4] = 127;

      g_x = 0;
      for(mi = -1; mi <= 1; mi++) {
        for(mj = -1; mj <= 1; mj++) {
            if(filter->mirror == TRUE) {
              g_x += sobel_x[mi+1][mj+1] *
                      origdata[filter->width*(CLAMP(i+mi,0,filter->height-1)) +
                                CLAMP(j+mj,0,filter->width-1)];
            } else {
              g_x += sobel_x[mi+1][mj+1] * origdata[filter->width*(i+mi) +
                                                    (j+mj)];
            }
        }
      }

      g_y = 0;
      for(mi = -1; mi <= 1; mi++) {
        for(mj = -1; mj <= 1; mj++) {
            if(filter->mirror) {
              g_y += sobel_y[mi+1][mj+1] *
                      origdata[filter->width*(CLAMP(i+mi,0,filter->height-1)) +
                                CLAMP(j+mj,0,filter->width-1)];
            } else {
              g_y += sobel_y[mi+1][mj+1] * origdata[filter->width*(i+mi) +
                                                    (j+mj)];
            }
        }
      }

      /* Calculate magnitude, normalize/clamp */
      if (filter->abs_magnitude) {
        result = abs(g_x) + abs(g_y);
        if(!filter->clamp) {
          result = result * 255 / SOBEL_MAX_ABS;
        }
      } else {
        result = sqrt(g_x*g_x + g_y*g_y) * 255;
        if(!filter->clamp) {
          result = result * 255 / SOBEL_MAX_SQRT;
        }
      }
      if (filter->clamp) {
        result = CLAMP (result, 0, 255);
      }

      newdata[i*filter->width+j] = (guint8)result;
    }
  }

  /* Zero out borders if not mirroring border pixels */
  if(filter->mirror == FALSE) {
    for(i=0; i<filter->height; i++) {
      newdata[i*filter->width] = 0;
      newdata[i*filter->width + filter->width - 1] = 0;
    }

    for(j=0; j<filter->width; j++) {
      newdata[j] = 0;
      newdata[(filter->height-1)*filter->width + j] = 0;
    }
  }

  gst_buffer_unref (buf);
  return gst_pad_push (filter->srcpad, destbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
sobel_init (GstPlugin * sobel)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template sobel' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_sobel_debug, "sobel",
      0, "Calculates luminance gradient magnitude using the Sobel operator.");

  return gst_element_register (sobel, "sobel", GST_RANK_NONE,
      GST_TYPE_SOBEL);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstsobel"
#endif

/* gstreamer looks for this structure to register sobels
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "sobel",
    "Calculates luminance gradient magnitude using the Sobel operator.",
    sobel_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
