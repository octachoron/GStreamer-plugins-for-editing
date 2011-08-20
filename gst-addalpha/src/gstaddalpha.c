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
/**
 * SECTION:element-addalpha
 *
 * This element takes an RGB or YUV video stream, and a 8 bit grayscale alpha
 * stream, and combines them into an RGBA or AYUV stream. No colour space
 * conversion is done, therefore if working with RGB, it should be 24 bpp,
 * and in the case of YUV, it must be a raw 4:4:4 packed pixel format.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * FIXME
 * ]|
 * </refsect2>
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <glib.h>
#include "gstaddalpha.h"

GST_DEBUG_CATEGORY_STATIC (gst_add_alpha_debug);
#define GST_CAT_DEFAULT gst_add_alpha_debug

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
static GstStaticPadTemplate fsink_factory =
GST_STATIC_PAD_TEMPLATE ("fsink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv,"
        "format=(fourcc)v308; "
        "video/x-raw-rgb,"
        "depth=24,"
        "bpp=24")
);

static GstStaticPadTemplate msink_factory =
GST_STATIC_PAD_TEMPLATE ("msink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-gray,"
        "bpp=(int)8, "
        "depth=(int)8")
);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv,"
        "format=(fourcc)AYUV; "
        "video/x-raw-rgb,"
        "bpp=32,"
        "depth=32")
);

GST_BOILERPLATE (GstAddAlpha, gst_add_alpha, GstElement,
    GST_TYPE_ELEMENT)
;

static void
gst_add_alpha_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_add_alpha_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void
gst_add_alpha_finalize(GObject *object);

static gboolean
gst_add_alpha_set_frame_sink_caps(GstPad * pad, GstCaps * caps);
static gboolean
gst_add_alpha_set_src_caps(GstPad * pad, GstCaps * caps);
static GstStateChangeReturn
gst_add_alpha_change_state(GstElement *element, GstStateChange transition);
static GstFlowReturn
gst_add_alpha_collect_func(GstCollectPads *pads, gpointer user_data);
static gboolean
gst_add_alpha_sink_event (GstPad  *pad, GstEvent *event);
static gboolean
gst_add_alpha_src_event (GstPad  *pad, GstEvent *event);

/* GObject vmethod implementations */

static void
gst_add_alpha_base_init(gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class, "AddAlpha",
      "Converter/Video",
      "Add a gray stream to an RGB or YUV stream as alpha channel",
      "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&src_factory));
  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&msink_factory));
  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&fsink_factory));
}

/* initialize the addalpha's class */
static void
gst_add_alpha_class_init(GstAddAlphaClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_add_alpha_set_property;
  gobject_class->get_property = gst_add_alpha_get_property;
  gobject_class->finalize = gst_add_alpha_finalize;

  g_object_class_install_property(gobject_class, PROP_SILENT,
      g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_add_alpha_change_state;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_add_alpha_init(GstAddAlpha * filter,
    GstAddAlphaClass * gclass)
{

  filter->framesink = gst_pad_new_from_static_template(&fsink_factory, "fsink");
  gst_pad_set_setcaps_function(filter->framesink,
      gst_add_alpha_set_frame_sink_caps);
  gst_pad_set_event_function (filter->framesink, gst_add_alpha_sink_event);

  filter->masksink = gst_pad_new_from_static_template(&msink_factory, "msink");
  gst_pad_set_event_function (filter->masksink, gst_add_alpha_sink_event);

  filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
  gst_pad_set_setcaps_function(filter->srcpad,
        gst_add_alpha_set_src_caps);
  gst_pad_set_event_function (filter->srcpad, gst_add_alpha_src_event);

  gst_element_add_pad(GST_ELEMENT (filter), filter->framesink);
  gst_element_add_pad(GST_ELEMENT (filter), filter->masksink);
  gst_element_add_pad(GST_ELEMENT (filter), filter->srcpad);

  /* Set up internal data */

  filter->width = filter->height = 0;

  /* Set up the CollectPads */

  filter->input = gst_collect_pads_new();
  gst_collect_pads_set_function(filter->input, gst_add_alpha_collect_func,
      filter);
  filter->framesink_cdata = gst_collect_pads_add_pad(filter->input,
      filter->framesink, sizeof(GstCollectData));
  filter->masksink_cdata = gst_collect_pads_add_pad(filter->input,
      filter->masksink, sizeof(GstCollectData));

  /* Set up default property values */

  filter->silent = FALSE;

}

static void
gst_add_alpha_finalize (GObject *object) {
  GstAddAlpha *filter = GST_ADDALPHA (object);

  gst_object_unref(filter->input);

  G_OBJECT_CLASS (parent_class)->finalize(object);
}

static void
gst_add_alpha_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAddAlpha *filter = GST_ADDALPHA (object);

  switch (prop_id)
    {
  case PROP_SILENT:
    filter->silent = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
    }
}

static void
gst_add_alpha_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAddAlpha *filter = GST_ADDALPHA (object);

  switch (prop_id)
    {
  case PROP_SILENT:
    g_value_set_boolean(value, filter->silent);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_add_alpha_set_frame_sink_caps(GstPad * pad, GstCaps * caps)
{
  GstAddAlpha *filter;
  GstPad *otherpad;
  const gchar *mimetype;
  gboolean ret;
  GstCaps *src_caps;

  GstStructure *capstruct = gst_caps_get_structure(caps, 0);

  filter = GST_ADDALPHA (gst_pad_get_parent (pad));

  mimetype = gst_structure_get_name (capstruct);
  gst_structure_get_int(capstruct, "width", &filter->width);
  gst_structure_get_int(capstruct, "height", &filter->height);

  otherpad = filter->srcpad;


  /* Caps to set - preserve everything from the sink pad caps,
   * only change alpha-related fields.
   */
  src_caps = gst_caps_copy (caps);

  if (g_strcmp0 (mimetype, "video/x-raw-rgb") == 0) {
    gst_caps_set_simple (src_caps, "bpp", G_TYPE_INT, 32,
        "depth", G_TYPE_INT, 32,
        "alpha_mask", G_TYPE_INT, 0xff000000, NULL);
                        /* Alpha is always the first byte for now */
  } else {
    gst_caps_set_simple (src_caps, "format", GST_TYPE_FOURCC, GST_STR_FOURCC ("AYUV"), NULL);
  }

  g_assert (src_caps != NULL); //FIXME: remove?

  ret = gst_pad_set_caps(otherpad, src_caps);

  gst_caps_unref(src_caps);
  gst_object_unref(filter);

  return ret;
}

static gboolean
gst_add_alpha_set_src_caps(GstPad * pad, GstCaps * caps)
{
  GstAddAlpha *filter;
  GstPad *otherpad;
  const gchar *mimetype;
  gboolean ret;
  GstCaps *fsink_caps;

  GstStructure *capstruct = gst_caps_get_structure(caps, 0);

  filter = GST_ADDALPHA (gst_pad_get_parent (pad));

  mimetype = gst_structure_get_name (capstruct);
  gst_structure_get_int(capstruct, "width", &filter->width);
  gst_structure_get_int(capstruct, "height", &filter->height);

  otherpad = filter->framesink;

  if (g_strcmp0 (mimetype, "video/x-raw-rgb") == 0) {
    fsink_caps = gst_caps_new_simple ("video/x-raw-rgb", "bpp", G_TYPE_INT, 24, "depth", G_TYPE_INT, 24, NULL);
  } else {
    fsink_caps = gst_caps_new_simple ("video/x-raw-yuv", "format", GST_TYPE_FOURCC, GST_STR_FOURCC ("v308"), NULL);
  }

  ret = gst_pad_set_caps(otherpad, fsink_caps);

  gst_caps_unref(fsink_caps);
  gst_object_unref(filter);

  return ret;
}

static gboolean
gst_add_alpha_sink_event (GstPad  *pad, GstEvent *event) {
  GstAddAlpha *filter;
  gboolean ret;

  filter = GST_ADDALPHA (gst_pad_get_parent (pad));

  GST_DEBUG ("sink named %s got an event of type %s\n", gst_pad_get_name (pad), GST_EVENT_TYPE_NAME (event) );

  switch (GST_EVENT_TYPE (event)) {
    default:
      ret = gst_pad_push_event (filter->srcpad, event);
      break;
  }


  gst_object_unref (filter);
  return ret;
}

static gboolean
gst_add_alpha_src_event (GstPad  *pad, GstEvent *event) {
   GstAddAlpha *filter;
   gboolean ret;

   filter = GST_ADDALPHA (gst_pad_get_parent (pad));

   GST_DEBUG ("the src got an event of type %s\n", GST_EVENT_TYPE_NAME (event) );

   gst_event_ref (event);
   ret = gst_pad_push_event (filter->framesink, event);
   ret = ret && gst_pad_push_event (filter->masksink, event);

   gst_object_unref (filter);
   return ret;
}

static GstFlowReturn
gst_add_alpha_collect_func(GstCollectPads *pads, gpointer user_data)
{
  GstBuffer *framebuf, *maskbuf, *destbuf;
  guint8 *framedata, *maskdata, *destdata;

  GstAddAlpha *filter;

  gint width, height;
  guint cur_pixel;

  GstFlowReturn buf_alloc_ret;

  GST_DEBUG("collect callback called back");

  filter = GST_ADDALPHA(pads->user_data);

  width = filter->width;
  height = filter->height;

  framebuf = gst_collect_pads_pop(pads, filter->framesink_cdata);
  maskbuf = gst_collect_pads_pop(pads, filter->masksink_cdata);

  framedata = GST_BUFFER_DATA (framebuf);
  maskdata = GST_BUFFER_DATA (maskbuf);

  /* Allocate output buffer. */
  buf_alloc_ret = gst_pad_alloc_buffer_and_set_caps(filter->srcpad,
      GST_BUFFER_OFFSET (maskbuf), width*height*4,
      GST_PAD_CAPS (filter->srcpad), &destbuf);

  GST_DEBUG ("width = %d, height = %d, req size = %u, act size = %u", width, height, width*height*4, GST_BUFFER_SIZE (destbuf));

  /* Handle errors */
  if (buf_alloc_ret != GST_FLOW_OK) {
    gst_buffer_unref(maskbuf);
    gst_buffer_unref(framebuf);

    GST_DEBUG ("could not allocate output buffer");

    return buf_alloc_ret;
  }

  GST_BUFFER_TIMESTAMP (destbuf) = GST_BUFFER_TIMESTAMP (framebuf);
  GST_BUFFER_DURATION (destbuf) = GST_BUFFER_DURATION (framebuf);
  GST_BUFFER_OFFSET (destbuf) = GST_BUFFER_OFFSET (framebuf);
  GST_BUFFER_OFFSET_END (destbuf) = GST_BUFFER_OFFSET_END (framebuf);

  destdata = GST_BUFFER_DATA (destbuf);

  GST_DEBUG ("buffer sizes: frame %u, mask %u, dest %u", GST_BUFFER_SIZE (framebuf), GST_BUFFER_SIZE (maskbuf), GST_BUFFER_SIZE (destbuf));

  /* Do the actual repacking */
  for (cur_pixel = 0; cur_pixel < width*height; cur_pixel++) {
    destdata[cur_pixel * 4] = maskdata[cur_pixel]; /* Copy alpha byte */
    /* Copy original bytes */
    destdata[cur_pixel * 4 + 1] = framedata[cur_pixel * 3];
    destdata[cur_pixel * 4 + 2] = framedata[cur_pixel * 3 + 1];
    destdata[cur_pixel * 4 + 3] = framedata[cur_pixel * 3 + 2];
  }

  gst_buffer_unref(maskbuf);
  gst_buffer_unref(framebuf);

  return gst_pad_push(filter->srcpad, destbuf);
}

static GstStateChangeReturn
gst_add_alpha_change_state(GstElement *element, GstStateChange transition)
{

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstAddAlpha *filter = GST_ADDALPHA (element);

  switch (transition)
  {
  case GST_STATE_CHANGE_READY_TO_PAUSED:
    /* Start the CollectPads now */
    gst_collect_pads_start(filter->input);
    break;
  default:
    break;
  }

  /* Stop the CollectPads in accordance with the docs */
  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
  {
    gst_collect_pads_stop(filter->input);
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state(element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition)
    {
  case GST_STATE_CHANGE_PAUSED_TO_READY:
    break;
  default:
    break;
    }

  return ret;

}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
addalpha_init(GstPlugin * addalpha)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_add_alpha_debug, "addalpha",
      0, "Add a gray stream to an RGB or YUV stream as alpha channel");

  return gst_element_register(addalpha, "addalpha", GST_RANK_NONE,
      GST_TYPE_ADDALPHA);
}
/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstaddalpha"
#endif
/* gstreamer looks for this structure to register addalphas */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "addalpha",
    "Add a gray stream to an RGB or YUV stream as alpha channel",
    addalpha_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
