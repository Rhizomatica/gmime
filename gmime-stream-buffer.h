/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_STREAM_BUFFER_H__
#define __GMIME_STREAM_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "gmime-stream.h"

typedef enum {
	GMIME_STREAM_BUFFER_BUFFER  = 0x00,
	GMIME_STREAM_BUFFER_NEWLINE,
	
	GMIME_STREAM_BUFFER_READ    = 0x00,
	GMIME_STREAM_BUFFER_WRITE   = 0xf0,

	GMIME_STREAM_BUFFER_MODE    = 0xf0,
} GMimeStreamBufferMode;

typedef struct _GMimeStreamBuffer {
	GMimeStream parent;
	
	GMimeStream *source;
	
	unsigned char *buffer;
	unsigned char *bufptr;
	unsigned char *bufend;
	size_t buflen;
	
	GMimeStreamBufferMode mode;
} GMimeStreamBuffer;

#define GMIME_STREAM_BUFFER_TYPE g_str_hash ("GMimeStreamBuffer")
#define GMIME_IS_STREAM_BUFFER(stream) (((GMimeStream *) stream)->type == GMIME_STREAM_BUFFER_TYPE)
#define GMIME_STREAM_BUFFER(stream) ((GMimeStreamBuffer *) stream)

GMimeStream *g_mime_stream_buffer_new (GMimeStream *source, GMimeStreamBufferMode mode);


ssize_t g_mime_stream_buffer_gets (GMimeStreamBuffer *stream, char *buf, size_t max);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_BUFFER_H__ */
