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
 * SECTION:element-maskedunsharp
 *
 * This element takes a video frame on its frame sink pad, and a grayscale mask
 * on its mask sink pad. Global sharpening is performed on the video frame,
 * then the original and sharpened frames are blended, using the mask frame as
 * the blend ratio. Effectively, a brighter pixel in the mask image means
 * stronger sharpening on the corresponding frame pixel.
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
#include <string.h>
#include "gstmaskedunsharp.h"
#include "framearith.h"
GST_DEBUG_CATEGORY_STATIC (gst_masked_unsharp_debug);
#define GST_CAT_DEFAULT gst_masked_unsharp_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0, PROP_SILENT, PROP_SIGMA
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate fsink_factory =
GST_STATIC_PAD_TEMPLATE ("fsink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
        "format=(fourcc)AYUV")
);

static GstStaticPadTemplate msink_factory =
GST_STATIC_PAD_TEMPLATE ("msink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-gray, "
        "bpp=(int)8, "
        "depth=(int)8")
);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
        "format=(fourcc)AYUV")
);

GST_BOILERPLATE (GstMaskedUnsharp, gst_masked_unsharp, GstElement,
    GST_TYPE_ELEMENT)
;

static void
gst_masked_unsharp_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_masked_unsharp_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void
gst_masked_unsharp_finalize(GObject *object);

static gboolean
gst_masked_unsharp_set_frame_caps(GstPad * pad, GstCaps * caps);
static GstStateChangeReturn
gst_masked_unsharp_change_state(GstElement *element, GstStateChange transition);
static GstFlowReturn
gst_masked_unsharp_collect_func(GstCollectPads *pads, gpointer user_data);
static gboolean
gst_masked_unsharp_sink_event (GstPad  *pad, GstEvent *event);
static gboolean
gst_masked_unsharp_src_event (GstPad  *pad, GstEvent *event);

/* GObject vmethod implementations */

static void
gst_masked_unsharp_base_init(gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class, "MaskedUnsharp",
      "Filter/Effect/Video", "Selective sharpening with external mask.",
      "Roland Elek <elek.roland@gmail.com>");

  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&src_factory));
  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&msink_factory));
  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&fsink_factory));
}

/* initialize the maskedunsharp's class */
static void
gst_masked_unsharp_class_init(GstMaskedUnsharpClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_masked_unsharp_set_property;
  gobject_class->get_property = gst_masked_unsharp_get_property;
  gobject_class->finalize = gst_masked_unsharp_finalize;

  g_object_class_install_property(gobject_class, PROP_SILENT,
      g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_SIGMA,
      g_param_spec_double("sigma", "Sigma", "Sigma value used for sharpening. This "\
          "value is negated before passing to the internal gaussianblur "\
          "element, therefore a positive value means sharpen, and a negative "\
          "value means blur.",
          -20.0, 20.0, 6.0, G_PARAM_READWRITE));

  gstelement_class->change_state = gst_masked_unsharp_change_state;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_masked_unsharp_init(GstMaskedUnsharp * filter,
    GstMaskedUnsharpClass * gclass)
{
  GstPad *tee_sinkpad, *tee_src0, *tee_src1, *origq_sink, *sharpq_sink;
  GstPad *gaussblur_src, *origq_src;
  GstGhostPad *framesink;
  GstBin *unsharp_bin;
  GstElement *tee, *frame_queue, *sharp_queue;
  GstPad *unsharp_bin_sinkpad, *unsharp_bin_origpad, *unsharp_bin_sharppad;

  /* Sharpening bin sink pad given as target later */
  filter->framesink = gst_ghost_pad_new_no_target_from_template("fsink",
      gst_static_pad_template_get(&fsink_factory));
  gst_pad_set_setcaps_function(filter->framesink,
      gst_masked_unsharp_set_frame_caps);
  gst_pad_set_event_function (filter->framesink, gst_masked_unsharp_sink_event);

  filter->masksink = gst_pad_new_from_static_template(&msink_factory, "msink");
  gst_pad_set_event_function (filter->masksink, gst_masked_unsharp_sink_event);

  filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
  gst_pad_set_event_function (filter->srcpad, gst_masked_unsharp_src_event);

  gst_element_add_pad(GST_ELEMENT (filter), filter->framesink);
  gst_element_add_pad(GST_ELEMENT (filter), filter->masksink);
  gst_element_add_pad(GST_ELEMENT (filter), filter->srcpad);

  /* Set up internal data */

  filter->width = filter->height = 0;

  /* Set up the bin for global sharpening */

  filter->unsharp_bin = gst_bin_new("unsharp-bin");
  filter->gaussblur = gst_element_factory_make("gaussianblur", "sharpen");
  gst_object_ref (filter->gaussblur); /* Keep things clean and hold a reference
                                       * for setting the sigma property at
                                       * runtime.
                                       */
  tee = gst_element_factory_make("tee", "tee");
  frame_queue = gst_element_factory_make("queue", "frame-queue");
  sharp_queue = gst_element_factory_make("queue", "sharp-queue");

  unsharp_bin = GST_BIN (filter->unsharp_bin);
  gst_bin_add_many(unsharp_bin, filter->gaussblur, tee,
      frame_queue, sharp_queue, NULL);
  gst_element_link_many(sharp_queue, filter->gaussblur, NULL);
  tee_sinkpad = gst_element_get_static_pad(tee, "sink");

  tee_src0 = gst_element_get_request_pad(tee, "src%d");
  tee_src1 = gst_element_get_request_pad(tee, "src%d");
      //FIXME: do this with new function

  origq_sink = gst_element_get_static_pad(frame_queue, "sink");
  sharpq_sink = gst_element_get_static_pad(sharp_queue, "sink");
  origq_src = gst_element_get_static_pad(frame_queue, "src");
  gaussblur_src = gst_element_get_static_pad(filter->gaussblur, "src");

  gst_pad_link(tee_src0, sharpq_sink);
  gst_pad_link(tee_src1, origq_sink);

  unsharp_bin_sinkpad = gst_ghost_pad_new("sinkpad", tee_sinkpad);
  unsharp_bin_origpad = gst_ghost_pad_new("origpad", origq_src);
  unsharp_bin_sharppad = gst_ghost_pad_new("sharppad", gaussblur_src);

  framesink = GST_GHOST_PAD (filter->framesink);
  gst_ghost_pad_set_target(framesink, unsharp_bin_sinkpad);

  gst_object_ref (unsharp_bin_origpad);
  gst_object_ref (unsharp_bin_sharppad);

  gst_element_add_pad(filter->unsharp_bin, unsharp_bin_sinkpad);
  gst_element_add_pad(filter->unsharp_bin, unsharp_bin_origpad);
  gst_element_add_pad(filter->unsharp_bin, unsharp_bin_sharppad);

  gst_object_unref(tee_sinkpad);
  gst_object_unref(tee_src0);
  gst_object_unref(tee_src1);
  gst_object_unref(origq_sink);
  gst_object_unref(sharpq_sink);
  gst_object_unref(gaussblur_src);
  gst_object_unref(origq_src);

  gst_element_set_parent (filter->unsharp_bin, GST_OBJECT (filter));

  /* Set up the CollectPads
   * The main idea here is creating two additional pads to be able to link the
   * source pads of the sharpening bin to a CollectPads.
   */

  filter->framesink_internal = gst_pad_new_from_static_template(&fsink_factory,
      "fsinki");
  filter->sharpsink_internal = gst_pad_new_from_static_template(&fsink_factory,
      "ssinki");
  gst_pad_activate_push(filter->framesink_internal, TRUE);
  gst_pad_activate_push(filter->sharpsink_internal, TRUE);
  gst_pad_link(unsharp_bin_origpad, filter->framesink_internal);
  gst_pad_link(unsharp_bin_sharppad, filter->sharpsink_internal);

  gst_object_unref (unsharp_bin_origpad);
  gst_object_unref (unsharp_bin_sharppad);

  filter->input = gst_collect_pads_new();
  gst_collect_pads_set_function(filter->input, gst_masked_unsharp_collect_func,
      filter);
  filter->framesink_cdata = gst_collect_pads_add_pad(filter->input,
      filter->framesink_internal, sizeof(GstCollectData));
  filter->sharp_cdata = gst_collect_pads_add_pad(filter->input,
      filter->sharpsink_internal, sizeof(GstCollectData));
  filter->masksink_cdata = gst_collect_pads_add_pad(filter->input,
      filter->masksink, sizeof(GstCollectData));

  /* Set up default property values */

  filter->silent = FALSE;
  g_object_set(G_OBJECT (filter->gaussblur), "sigma", (gdouble) -6.0, NULL);

}

static void
gst_masked_unsharp_finalize (GObject *object) {
  GstMaskedUnsharp *filter = GST_MASKEDUNSHARP (object);

  gst_object_unref(filter->input);
  gst_object_unref(filter->gaussblur);

  G_OBJECT_CLASS (parent_class)->finalize(object);
}

static void
gst_masked_unsharp_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMaskedUnsharp *filter = GST_MASKEDUNSHARP (object);

  switch (prop_id)
    {
  case PROP_SILENT:
    filter->silent = g_value_get_boolean(value);
    break;
  case PROP_SIGMA:
    g_object_set (filter->gaussblur, "sigma", -1.0 * g_value_get_double(value), NULL);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
    }
}

static void
gst_masked_unsharp_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMaskedUnsharp *filter = GST_MASKEDUNSHARP (object);
  gdouble sigma=0.0;

  switch (prop_id)
    {
  case PROP_SILENT:
    g_value_set_boolean(value, filter->silent);
    break;
  case PROP_SIGMA:
    g_object_get(filter->gaussblur, "sigma", &sigma, NULL);
    g_value_set_double(value, -sigma);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
    }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_masked_unsharp_set_frame_caps(GstPad * pad, GstCaps * caps)
{
  GstMaskedUnsharp *filter;
  GstPad *otherpad;

  GstStructure *capstruct = gst_caps_get_structure(caps, 0);
  const gchar *mimetype;
  guint32 format;

  mimetype = gst_structure_get_name(capstruct);
  if (strcmp(mimetype, "video/x-raw-yuv") != 0)
    {
      return FALSE;
    }

  gst_structure_get_fourcc(capstruct, "format", &format);

  if (format != GST_MAKE_FOURCC('A', 'Y', 'U', 'V'))
    {
      return FALSE;
    }

  filter = GST_MASKEDUNSHARP (gst_pad_get_parent (pad));

  gst_structure_get_int(capstruct, "width", &filter->width);
  gst_structure_get_int(capstruct, "height", &filter->height);

  otherpad = filter->srcpad;
  gst_object_unref(filter);

  return gst_pad_set_caps(otherpad, caps);
}

static gboolean
gst_masked_unsharp_sink_event (GstPad  *pad, GstEvent *event) {
  GstMaskedUnsharp *filter;
  gboolean ret;

  filter = GST_MASKEDUNSHARP (gst_pad_get_parent (pad));

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
gst_masked_unsharp_src_event (GstPad  *pad, GstEvent *event) {
   GstMaskedUnsharp *filter;
   gboolean ret;

   filter = GST_MASKEDUNSHARP (gst_pad_get_parent (pad));

   GST_DEBUG ("the src got an event of type %s\n", GST_EVENT_TYPE_NAME (event) );

   gst_event_ref (event);
   ret = gst_pad_push_event (filter->framesink, event);
   ret = ret && gst_pad_push_event (filter->masksink, event);

   gst_object_unref (filter);
   return ret;
}

static GstFlowReturn
gst_masked_unsharp_collect_func(GstCollectPads *pads, gpointer user_data)
{
  GstBuffer *framebuf, *sharpbuf, *maskbuf, *destbuf;
  guint8 *framedata, *maskdata, *sharpdata, *destdata;

  GstMaskedUnsharp *filter;

  gint width, height;

  GST_DEBUG("collect callback called back");

  filter = GST_MASKEDUNSHARP(pads->user_data);

  width = filter->width;
  height = filter->height;

  framebuf = gst_collect_pads_pop(pads, filter->framesink_cdata);
  maskbuf = gst_collect_pads_pop(pads, filter->masksink_cdata);
  sharpbuf = gst_collect_pads_pop(pads, filter->sharp_cdata);

  framedata = GST_BUFFER_DATA (framebuf);
  maskdata = GST_BUFFER_DATA (maskbuf);
  sharpdata = GST_BUFFER_DATA (sharpbuf);

  destbuf = gst_buffer_copy(framebuf);

  destbuf = gst_buffer_make_writable (destbuf);
  destdata = GST_BUFFER_DATA (destbuf);

  blend_video_frames(width, height, 4, sharpdata, framedata, destdata,
      maskdata);

  gst_buffer_unref(maskbuf);
  gst_buffer_unref(framebuf);

  return gst_pad_push(filter->srcpad, destbuf);
}

static GstStateChangeReturn
gst_masked_unsharp_change_state(GstElement *element, GstStateChange transition)
{

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstMaskedUnsharp *filter = GST_MASKEDUNSHARP (element);

  switch (transition)
    {
  case GST_STATE_CHANGE_READY_TO_PAUSED:
    gst_collect_pads_start(filter->input);
    break;
  default:
    break;
    }

  //XXX: good to change unsharp bin state here?
  ret = gst_element_sync_state_with_parent (filter->unsharp_bin);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      GST_ERROR("unsharp bin failed to change state\n");
      return ret;
    }

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
maskedunsharp_init(GstPlugin * maskedunsharp)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_masked_unsharp_debug, "maskedunsharp",
      0, "Selective sharpening with external mask");

  return gst_element_register(maskedunsharp, "maskedunsharp", GST_RANK_NONE,
      GST_TYPE_MASKEDUNSHARP);
}
/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstmaskedunsharp"
#endif
/* gstreamer looks for this structure to register maskedunsharps */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "maskedunsharp",
    "Selective sharpening with external mask",
    maskedunsharp_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
