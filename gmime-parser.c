/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *           Charles Kerr <charles@rebelbase.com>
 *
 *  Copyright 2000, 2001 Helix Code, Inc. (www.helixcode.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-parser.h"
#include "gmime-utils.h"
#include "gmime-header.h"
#include <string.h>
#include <ctype.h>

#define d(x) (x)

enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID
};

static gchar *content_headers[] = {
	"Content-Type:",
	"Content-Transfer-Encoding:",
	"Content-Disposition:",
	"Content-Description:",
	"Content-Location:",
	"Content-Md5:",
	"Content-Id:",
	NULL
};

static void
header_unfold (gchar *header)
{
	/* strip all \n's and replace tabs with spaces - this should
           undo any header folding */
	gchar *src, *dst;
	
	for (src = dst = header; *src; src++) {
		if (*src != '\n')
			*dst++ = *src != '\t' ? *src : ' ';
	}
	*dst = '\0';
}

static int
content_header (const gchar *field)
{
	int i;
	
	for (i = 0; content_headers[i]; i++)
		if (!g_strncasecmp (field, content_headers[i], strlen (content_headers[i])))
			return i;
	
	return -1;
}

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

static const gchar *
g_strstrbound (const gchar *haystack, const gchar *needle, const gchar *end)
{
	gboolean matches = FALSE;
	const gchar * ptr;
	guint nlen;
	
	nlen = strlen (needle);
	ptr = haystack;
	
	while (ptr + nlen <= end) {
		if (!strncmp (ptr, needle, nlen)) {
			matches = TRUE;
			break;
		}
		ptr++;
	}
	
	if (matches)
		return ptr;
	else
		return NULL;
}

static const gchar *
find_header_part_end (const gchar* in, guint inlen)
{
	const gchar * pch;
	const gchar * hdr_end = NULL;

	g_return_val_if_fail (in!=NULL, NULL);

	if (*in == '\n') /* if the beginning is a '\n' there are no content headers */
		hdr_end = in;
	else if ((pch = g_strstrbound (in, "\n\n", in+inlen)) != NULL)
		hdr_end = pch;
	else if ((pch = g_strstrbound (in, "\n\r\n", in+inlen)) != NULL)
		hdr_end = pch;

	return hdr_end;
}

#define GMIME_PARSER_MAX_LINE_WIDTH 1024


/**
 * get_header_block_from_file: Get the header block from a message.
 * @fp: file pointer pointing to the beginning of the file block.
 *
 * This will read all of the headers into an unparsed GArray and
 * leave fp_in pointing at the message body that comes after the
 * headers.
 **/
static GArray *
get_header_block_from_file (FILE *fp)
{
	GArray *a;
	gchar buf [GMIME_PARSER_MAX_LINE_WIDTH];
	
	g_return_val_if_fail (fp != NULL, NULL);
	
	a = g_array_new (TRUE, FALSE, 1);
	for (;;) {
		gchar *pch;
		
		pch = fgets (buf, sizeof (buf), fp);
		if (pch == NULL) {
			/* eof reached before end-of-headers! */
			g_array_free (a, TRUE);
			a = NULL;
			break;
		}
		
		/* empty line -- end of headers */
		if (!strcmp (buf,"\n"))
			break;
		
		g_array_append_vals (a, buf, strlen (buf));
	}
	
	return a;
}


/**
 * parse_content_heaaders:
 * @headers: content header string
 * @inlen: length of the header block.
 * @mime_part: mime part to populate with the information we get from the Content-* headers.
 * @is_multipart: set to TRUE if the part is a multipart, FALSE otherwise (to be set by function)
 * @boundary: multipart boundary string (to be set by function)
 * @end_boundary: multipart end boundary string (to be set by function)
 *
 * Parse a header block for content information.
 */
static void
parse_content_headers (const gchar *headers, gint inlen,
                       GMimePart *mime_part, gboolean *is_multipart,
                       gchar **boundary, gchar **end_boundary)
{
	const gchar *inptr = headers;
	const gchar *inend = inptr + inlen;
	
	*boundary = NULL;
	*end_boundary = NULL;
	
	while (inptr < inend) {
		const gint type = content_header (inptr);
		const gchar *hvalptr;
		const gchar *hvalend;
		gchar *value;
		
		if (type == -1) {
			if (!(hvalptr = memchr (inptr, ':', inend - inptr)))
				break;
			hvalptr++;
		} else {
			hvalptr = inptr + strlen (content_headers[type]);
		}
		
		for (hvalend = hvalptr; hvalend < inend; hvalend++)
			if (*hvalend == '\n' && !isblank (*(hvalend + 1)))
				break;
		
		value = g_strndup (hvalptr, (gint) (hvalend - hvalptr));
		
		header_unfold (value);
		g_strstrip (value);
		
		switch (type) {
		case CONTENT_DESCRIPTION: {
			gchar *description = g_mime_utils_8bit_header_decode (value);
			
			g_strstrip (description);
			g_mime_part_set_content_description (mime_part, description);
			g_free (description);
			
			break;
		}
		case CONTENT_LOCATION:
			g_mime_part_set_content_location (mime_part, value);
			break;
		case CONTENT_MD5:
			g_mime_part_set_content_md5 (mime_part, value);
			break;
		case CONTENT_ID:
			g_mime_part_set_content_id (mime_part, value);
			break;
		case CONTENT_TRANSFER_ENCODING:
			g_mime_part_set_encoding (mime_part, g_mime_part_encoding_from_string (value));
			break;
		case CONTENT_TYPE: {
			GMimeContentType *mime_type;
			
			mime_type = g_mime_content_type_new_from_string (value);
			
			*is_multipart = g_mime_content_type_is_type (mime_type, "multipart", "*");
			if (*is_multipart) {
				const gchar *b;
				
				b = g_mime_content_type_get_parameter (mime_type, "boundary");
				if (b != NULL) {
					/* create our temp boundary vars */
					*boundary = g_strdup_printf ("--%s\n", b);
					*end_boundary = g_strdup_printf ("--%s--\n", b);
				} else {
					g_warning ("Invalid MIME structure: boundary not found for multipart"
						   " - defaulting to text/plain.");
					
					/* let's continue onward as if this was not a multipart */
					g_mime_content_type_destroy (mime_type);
					mime_type = g_mime_content_type_new ("text", "plain");
					is_multipart = FALSE;
				}
			}
			g_mime_part_set_content_type (mime_part, mime_type);
			
			break;
		}
		case CONTENT_DISPOSITION: {
			gchar *disposition, *ptr;
			
			/* get content disposition part */
			for (ptr = value; *ptr && *ptr != ';'; ptr++); /* find ; or \0 */
			disposition = g_strndup (value, (gint)(ptr - value));
			g_strstrip (disposition);
			g_mime_part_set_content_disposition (mime_part, disposition);
			g_free (disposition);
			
			/* parse the parameters, if any */
			while (*ptr == ';') {
				gchar *pname, *pval;
				
				/* get the param name */
				for (pname = ptr + 1; *pname && !isspace ((int)*pname); pname++);
				for (ptr = pname; *ptr && *ptr != '='; ptr++);
				pname = g_strndup (pname, (gint) (ptr - pname));
				g_strstrip (pname);
				
				/* convert param name to lowercase */
				g_strdown (pname);
				
				/* skip any whitespace */
				for (pval = ptr + 1; *pval && isspace ((int) *pval); pval++);
				
				if (*pval == '"') {
					/* value is in quotes */
					pval++;
					for (ptr = pval; *ptr; ptr++)
						if (*ptr == '"' && *(ptr - 1) != '\\')
							break;
					pval = g_strndup (pval, (gint) (ptr - pval));
					g_strstrip (pval);
					g_mime_utils_unquote_string (pval);
					
					for ( ; *ptr && *ptr != ';'; ptr++);
				} else {
					/* value is not in quotes */
					for (ptr = pval; *ptr && *ptr != ';'; ptr++);
					pval = g_strndup (pval, (gint) (ptr - pval));
					g_strstrip (pval);
				}
				
				g_mime_part_add_content_disposition_parameter (mime_part, pname, pval);
				
				g_free (pname);
				g_free (pval);
			}
			
			break;
		}
		default:
			/* ignore this header */
			break;
		}
		
		g_free (value);
		inptr = hvalend + 1;
	}
}

typedef enum {
	PARSER_EOF,
	PARSER_BOUNDARY,
	PARSER_END_BOUNDARY,
	PARSER_LINE
} ParserState;

static ParserState
get_next_line (gchar *buf, guint buf_len, FILE *fp, const gchar *boundary, const gchar *end_boundary)
{
	ParserState state;
	
	*buf = '\0';
	if (fgets (buf, buf_len, fp) == NULL)
		state = PARSER_EOF;
	else if (boundary != NULL && !strcmp (buf, boundary))
		state = PARSER_BOUNDARY;
	else if (end_boundary != NULL && !strcmp (buf, end_boundary))
		state = PARSER_END_BOUNDARY;
	else
		state = PARSER_LINE;
	
	return state;
}

/**
 * g_mime_parser_construct_part_from_file: Construct a GMimePart object
 * @in: raw MIME Part data
 *
 * Returns a GMimePart object based on the data.
 **/
static GMimePart *
g_mime_parser_construct_part_from_file (const gchar   *headers,
                                        guint          headers_len,
                                        FILE          *fp,
                                        const gchar   *parent_boundary,
                                        const gchar   *parent_end_boundary,
                                        ParserState   *setme_state)
{
	gchar *boundary;
	gchar *end_boundary;
	gboolean is_multipart;
	GMimePart *mime_part;
	
	g_return_val_if_fail (headers != NULL, NULL);
	g_return_val_if_fail (headers_len > 0, NULL);
	g_return_val_if_fail (fp != NULL, NULL);
	g_return_val_if_fail (setme_state != NULL, NULL);
	
	/* Headers */
	boundary = NULL;
	end_boundary = NULL;
	is_multipart = FALSE;
	mime_part = g_mime_part_new ();
	parse_content_headers (headers, headers_len, mime_part, &is_multipart, &boundary, &end_boundary);
	
	/* Body */
	if (is_multipart && boundary != NULL && end_boundary != NULL) {
		/* is a multipart */

		/* get all the subparts */
		gchar buf[GMIME_PARSER_MAX_LINE_WIDTH];
		
		for (;;) {
			/* get the next line, we're looking for the beginning of a subpart */
			ParserState ps = get_next_line (buf, sizeof (buf), fp, parent_boundary,
							parent_end_boundary);
			if (ps != PARSER_LINE) {
				*setme_state = ps;
				break;
			}
			
			/* is the beginning of a subpart? */
			if (strcmp (buf, boundary))
				continue;
			
			/* add subparts as long as we keep getting boundaries */
			for (;;) {
				GArray *h = get_header_block_from_file (fp);
				if (h != NULL) {
					ParserState ps = 0;
					GMimePart *part;
					
					part = g_mime_parser_construct_part_from_file (h->data, h->len,
										       fp, boundary,
										       end_boundary, &ps);
					g_array_free (h, TRUE);
					if (part != NULL)
						g_mime_part_add_subpart (mime_part, part);
					if (ps != PARSER_BOUNDARY)
						break;
				}
			}
		}
	} else {
		/* a single part */
		GMimePartEncodingType encoding = g_mime_part_get_encoding (mime_part);
		gchar buf [GMIME_PARSER_MAX_LINE_WIDTH];
		
		/* keep reading lines until we reach a boundary or EOF, we're populating a part */
		for (;;) {
			ParserState ps = get_next_line (buf, sizeof (buf), fp,
							parent_boundary,
							parent_end_boundary);
			if (ps == PARSER_LINE) {
				if (*buf != '\0')
					g_mime_part_append_pre_encoded_content (mime_part, buf,
										strlen (buf), encoding);
			} else {
				*setme_state = ps;
				break;
			}
		}
	}
	
	g_free (boundary);	
	g_free (end_boundary);	
	
	return mime_part;
}


/* we pass the length here so that we can avoid dup'ing in the caller
   as mime parts can be BIG (tm) */

/**
 * g_mime_parser_construct_part: Construct a GMimePart object
 * @in: raw MIME Part data
 * @inlen: raw MIME Part data length
 *
 * Returns a GMimePart object based on the data.
 **/
GMimePart *
g_mime_parser_construct_part (const gchar *in, guint inlen)
{
	GMimePart *mime_part;
	gchar *boundary;
	gchar *end_boundary;
	gboolean is_multipart;
	const gchar *inptr;
	const gchar *inend = in + inlen;
	const gchar *hdr_end;
	
	g_return_val_if_fail (in != NULL, NULL);
	g_return_val_if_fail (inlen != 0, NULL);
	
	/* Headers */
	/* if the beginning of the input is a '\n' then there are no content headers */
	hdr_end = find_header_part_end (in, inlen);
	if (!hdr_end)
		return NULL;
	
	mime_part = g_mime_part_new ();
	is_multipart = FALSE;
	parse_content_headers (in, hdr_end - in, mime_part,
			       &is_multipart, &boundary, &end_boundary);
	
	/* Body */
	inptr = hdr_end;
	
	if (is_multipart && boundary && end_boundary) {
		/* get all the subparts */
		GMimePart *subpart;
		const gchar *part_begin;
		const gchar *part_end;
		
		part_begin = g_strstrbound (inptr, boundary, inend);
		while (part_begin && part_begin < inend) {
			/* make sure we're not looking at the end boundary */
			if (!strncmp (part_begin, end_boundary, strlen (end_boundary)))
				break;
			
			/* find the end of this part */
			part_end = g_strstrbound (part_begin + strlen (boundary), boundary, inend);
			if (!part_end || part_end >= inend) {
				part_end = g_strstrbound (part_begin + strlen (boundary),
							  end_boundary, inend);
				if (!part_end || part_end >= inend)
					part_end = inend;
			}
			
			/* get the subpart */
			part_begin += strlen (boundary);
			subpart = g_mime_parser_construct_part (part_begin, (guint) (part_end - part_begin));
			g_mime_part_add_subpart (mime_part, subpart);
			
			/* the next part begins where the last one left off */
			part_begin = part_end;
		}
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		const gchar *content = NULL;
		guint len = 0;
		
		/* from here to the end is the content */
		if (inptr < inend) {
			for (inptr++; inptr < inend && isspace ((int)*inptr); inptr++);
			len = inend - inptr;
			content = inptr;
			
			/* trim off excess trailing \n's */
			inend = content + len;
			while (len > 2 && *(inend - 1) == '\n' && *(inend - 2) == '\n') {
				inend--;
				len--;
			}
		}
		
		encoding = g_mime_part_get_encoding (mime_part);
		
		if (len > 0)
			g_mime_part_set_pre_encoded_content (mime_part, content, len, encoding);
	}
	
	return mime_part;
}

enum {
	HEADER_FROM = 0,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_MESSAGE_ID,
	HEADER_UNKNOWN
};

static char *fields[] = {
	"From:",
	"Reply-To:",
	"To:",
	"Cc:",
	"Bcc:",
	"Subject:",
	"Date:",
	"Message-Id:",
	NULL
};

static gboolean
special_header (const gchar *field)
{
	return (!g_strcasecmp (field, "MIME-Version:") || content_header (field) != -1);
}

static void
construct_headers (GMimeMessage *message, const gchar *headers, gint inlen, gboolean save_extra_headers)
{
	gchar *field, *value, *raw, *q;
	gchar *inptr, *inend;
	time_t date;
	int offset = 0;
	int i;
	
	inptr = (gchar *) headers;
	inend = inptr + inlen;
	
	for ( ; inptr < inend; inptr++) {
		for (i = 0; fields[i]; i++)
			if (!g_strncasecmp (fields[i], inptr, strlen (fields[i])))
				break;
		
		if (!fields[i]) {
			field = inptr;
			for (q = field; q < inend && *q != ':'; q++);
			field = g_strndup (field, (gint) (q - field + 1));
			g_strstrip (field);
		} else {
			field = g_strdup (fields[i]);
		}
		
		value = inptr + strlen (field);
		for (q = value; q < inend; q++)
			if (*q == '\n' && !isblank (*(q + 1)))
				break;
		
		value = g_strndup (value, (gint) (q - value));
		g_strstrip (value);
		header_unfold (value);
		
		switch (i) {
		case HEADER_FROM:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_sender (message, raw);
			g_free (raw);
			break;
		case HEADER_REPLY_TO:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_reply_to (message, raw);
			g_free (raw);
			break;
		case HEADER_TO:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_TO, value);
			break;
		case HEADER_CC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_CC, value);
			break;
		case HEADER_BCC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_BCC, value);
			break;
		case HEADER_SUBJECT:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_subject (message, raw);
			g_free (raw);
			break;
		case HEADER_DATE:
			date = g_mime_utils_header_decode_date (value, &offset);
			g_mime_message_set_date (message, date, offset);
			break;
		case HEADER_MESSAGE_ID:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_message_id (message, raw);
			g_free (raw);
			break;
		case HEADER_UNKNOWN:
		default:
			break;
		}
		
		/* possibly save the raw header */
		if ((save_extra_headers || fields[i]) && !special_header (field)) {
			field[strlen (field) - 1] = '\0'; /* kill the ';' */
			g_strstrip (field);
			g_mime_header_set (message->header->headers, field, value);
		}
		
		g_free (field);
		g_free (value);
		
		if (q >= inend)
			break;
		else
			inptr = q;
	}
}


/**
 * g_mime_parser_construct_message: Construct a GMimeMessage object
 * @in: an rfc0822 message stream
 * @inlen: stream length
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (const gchar *in, guint inlen, gboolean save_extra_headers)
{
	GMimeMessage *message = NULL;
	const gchar *hdr_end;
	
	g_return_val_if_fail (in != NULL, NULL);
	
	hdr_end = find_header_part_end (in, inlen);
	if (hdr_end != NULL) {
		GMimePart *part;
		
		message = g_mime_message_new ();
		construct_headers (message, in, hdr_end - in, save_extra_headers);
		part = g_mime_parser_construct_part (in, inlen);
		g_mime_message_set_mime_part (message, part);
	}
	
	return message;
}

/**
 * g_mime_parser_construct_message_from_file: Construct a GMimeMessage object
 * @fp: a file pointer pointing to an rfc0822 message
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message_from_file (FILE *fp, gboolean save_extra_headers)
{
	GMimeMessage *message = NULL;
	GArray *headers;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
	headers = get_header_block_from_file (fp);
	if (headers != NULL) {
		GMimePart *part;
		ParserState state = -1;
		
		message = g_mime_message_new ();
		construct_headers (message, headers->data, headers->len, save_extra_headers);
		part = g_mime_parser_construct_part_from_file (headers->data, headers->len, fp,
							       NULL, NULL, &state);
		g_mime_message_set_mime_part (message, part);
		if (state != PARSER_EOF)
			g_warning ("Didn't reach end of file - parser error?");
		
		g_array_free (headers, TRUE);
	}
	
	return message;
}
