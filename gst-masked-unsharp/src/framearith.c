/*
 * Basic frame arithmetics library
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

#include "framearith.h"

#include <glib.h>

void
blend_video_frames (gint w,
                    gint h,
                    guint8 bpp,
                    guint8 *a,
                    guint8 *b,
                    guint8 *dest,
                    guint8 *blend_ratio_image)
{
  gint cw, ch;

  for (ch = 0; ch < h; ch++) {
    for (cw = 0; cw < w; cw++) {
      gint briidx, fidx, weight;
      gint i;

      briidx = ch*w + cw;
      fidx = briidx * bpp;
      weight = blend_ratio_image[briidx];
      for(i=0; i<bpp; i++) {
        dest[fidx+i] = (weight*a[fidx+i] + (255-weight)*b[fidx+i]) / 255;
      }
    }
  }
}

void
subtract_frames (gint w,
                 gint h,
                 guint8 bpp,
                 guint8 *a,
                 guint8 *b,
                 guint8 *dest)
{
  gint cw, ch;

  for (ch = 0; ch < h; ch++) {
    for (cw = 0; cw < w; cw++) {
      gint idx;
      gint i;
      gint result;

      idx = ch*w + cw;

      for(i=0; i<bpp; i++) {
        result = CLAMP(a[idx+i] - b[idx+i], 0, 255);
        dest[idx+i] = (guint8)result;
      }
    }
  }
}
