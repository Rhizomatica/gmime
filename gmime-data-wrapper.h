/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_DATA_WRAPPER_H__
#define __GMIME_DATA_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gmime-content-type.h>
#include <gmime-stream.h>

typedef enum {
        GMIME_PART_ENCODING_DEFAULT,
        GMIME_PART_ENCODING_7BIT,
        GMIME_PART_ENCODING_8BIT,
        GMIME_PART_ENCODING_BASE64,
        GMIME_PART_ENCODING_QUOTEDPRINTABLE,
        GMIME_PART_NUM_ENCODINGS
} GMimePartEncodingType;

typedef struct _GMimeDataWrapper {
	GMimePartEncodingType encoding;
	GMimeStream *stream;
} GMimeDataWrapper;

GMimeDataWrapper *g_mime_data_wrapper_new (void);
GMimeDataWrapper *g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimePartEncodingType encoding);

void g_mime_data_wrapper_destroy (GMimeDataWrapper *wrapper);

void g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);
void g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimePartEncodingType encoding);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_DATA_WRAPPER_H__ */
