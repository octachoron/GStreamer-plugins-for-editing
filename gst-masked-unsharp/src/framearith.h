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

#ifndef FRAMEARITH_H
#define FRAMEARITH_H

#include <glib.h>

/* Blend packed pixel frame data byte by byte, with ratio given by
 * blend_ratio_image.
 */
void blend_video_frames (gint w,
                         gint h,
                         guint8 bpp, /* bytes per pixel */
                         guint8 *a,
                         guint8 *b,
                         guint8 *dest,
                         guint8 *blend_ratio_image);

/* Subtract packed pixel frame b from a byte by byte, clamping the result
 * to [0, 255].
 */
void subtract_frames (gint w,
                      gint h,
                      guint8 bpp, /* bytes per pixel */
                      guint8 *a,
                      guint8 *b,
                      guint8 *dest);

#endif
