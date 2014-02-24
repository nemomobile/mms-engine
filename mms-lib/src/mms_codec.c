/*
 *
 *  Multimedia Messaging Service
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  define ssize_t int
#  define write _write
#  include <io.h>
#  define uninitialized_var(x) x
#else
#  include <unistd.h>
#  define uninitialized_var(x) x = x
#endif

#include <glib.h>

#include "wsputil.h"
#include "mms_codec.h"

#define MAX_ENC_VALUE_BYTES 6

#ifdef TEMP_FAILURE_RETRY
#define TFR TEMP_FAILURE_RETRY
#else
#define TFR
#endif

#define QUOTE (127)

enum mms_message_value_bool {
	MMS_MESSAGE_VALUE_BOOL_YES =		128,
	MMS_MESSAGE_VALUE_BOOL_NO =		129,
};

enum header_flag {
	HEADER_FLAG_MANDATORY =			1,
	HEADER_FLAG_ALLOW_MULTI =		2,
	HEADER_FLAG_PRESET_POS =		4,
	HEADER_FLAG_MARKED =			8,
};

enum mms_header {
	MMS_HEADER_BCC =			0x01,
	MMS_HEADER_CC =				0x02,
	MMS_HEADER_CONTENT_LOCATION =		0x03,
	MMS_HEADER_CONTENT_TYPE =		0x04,
	MMS_HEADER_DATE =			0x05,
	MMS_HEADER_DELIVERY_REPORT =		0x06,
	MMS_HEADER_DELIVERY_TIME =		0x07,
	MMS_HEADER_EXPIRY =			0x08,
	MMS_HEADER_FROM =			0x09,
	MMS_HEADER_MESSAGE_CLASS =		0x0a,
	MMS_HEADER_MESSAGE_ID =			0x0b,
	MMS_HEADER_MESSAGE_TYPE =		0x0c,
	MMS_HEADER_MMS_VERSION =		0x0d,
	MMS_HEADER_MESSAGE_SIZE =		0x0e,
	MMS_HEADER_PRIORITY =			0x0f,
	MMS_HEADER_READ_REPORT =		0x10,
	MMS_HEADER_REPORT_ALLOWED =		0x11,
	MMS_HEADER_RESPONSE_STATUS =		0x12,
	MMS_HEADER_RESPONSE_TEXT =		0x13,
	MMS_HEADER_SENDER_VISIBILITY =		0x14,
	MMS_HEADER_STATUS =			0x15,
	MMS_HEADER_SUBJECT =			0x16,
	MMS_HEADER_TO =				0x17,
	MMS_HEADER_TRANSACTION_ID =		0x18,
	MMS_HEADER_RETRIEVE_STATUS =	0x19,
	MMS_HEADER_RETRIEVE_TEXT =		0x1a,
	MMS_HEADER_READ_STATUS =		0x1b,
	__MMS_HEADER_MAX =			0x1c,
	MMS_HEADER_INVALID =			0x80,
};

enum mms_part_header {
	MMS_PART_HEADER_CONTENT_LOCATION =	0x0e,
	MMS_PART_HEADER_CONTENT_DISPOSITION =	0x2e,
	MMS_PART_HEADER_CONTENT_ID =		0x40,
};

/*
 * Reference: IANA http://www.iana.org/assignments/character-sets
 */
static const struct {
	unsigned int mib_enum;
	const char *charset;
} charset_assignments[] = {
	{ 3,    "US-ASCII"          },
	{ 4,    "ISO_8859-1"        },
	{ 5,    "ISO_8859-2"        },
	{ 6,    "ISO_8859-3"        },
	{ 7,    "ISO_8859-4"        },
	{ 8,    "ISO_8859-5"        },
	{ 9,    "ISO_8859-6"        },
	{ 10,   "ISO_8859-7"        },
	{ 11,   "ISO_8859-8"        },
	{ 12,   "ISO_8859-9"        },
	{ 13,   "ISO-8859-10"       },
	{ 17,   "Shift_JIS"         },
	{ 18,   "EUC-JP"            },
	{ 36,   "KS_C_5601-1987"    },
	{ 37,   "ISO-2022-KR"       },
	{ 38,   "EUC-KR"            },
	{ 39,   "ISO-2022-JP"       },
	{ 40,   "ISO-2022-JP-2"     },
	{ 81,   "ISO_8859-6-E"      },
	{ 82,   "ISO_8859-6-I"      },
	{ 84,   "ISO_8859-8-E"      },
	{ 85,   "ISO_8859-8-I"      },
	{ 106,  "UTF-8"             },
	{ 109,  "ISO-8859-13"       },
	{ 110,  "ISO-8859-14"       },
	{ 111,  "ISO-8859-15"       },
	{ 112,  "ISO-8859-16"       },
	{ 113,  "GBK"               },
	{ 114,  "GB18030"           },
	{ 1000, "ISO-10646-UCS-2"   },
	{ 1001, "ISO-10646-UCS-4"   },
	{ 1004, "ISO-10646-J-1"     },
	{ 1012, "UTF-7"             },
	{ 1013, "UTF-16BE"          },
	{ 1014, "UTF-16LE"          },
	{ 1015, "UTF-16"            },
	{ 1017, "UTF-32"            },
	{ 1018, "UTF-32BE"          },
	{ 1019, "UTF-32LE"          },
	{ 2025, "GB2312"            },
	{ 2026, "Big5"              },
	{ 2027, "macintosh"         },
	{ 2084, "KOI8-R"            },
	{ 2109, "windows-874"       },
	{ 2250, "windows-1250"      },
	{ 2251, "windows-1251"      },
	{ 2252, "windows-1252"      },
	{ 2253, "windows-1253"      },
	{ 2254, "windows-1254"      },
	{ 2255, "windows-1255"      },
	{ 2256, "windows-1256"      },
	{ 2257, "windows-1257"      },
	{ 2258, "windows-1258"      }
};

#define FB_SIZE 256

struct file_buffer {
	unsigned char buf[FB_SIZE];
	unsigned int size;
	unsigned int fsize;
	int fd;
};

typedef gboolean (*header_handler)(struct wsp_header_iter *, void *);
typedef gboolean (*header_encoder)(struct file_buffer *, enum mms_header,
									void *);

/*
 * mms_parse_http_content_type() parses HTTP media type as defined
 * in section 3.7 "Media Types" of the HTTP/1.1 specification. The
 * grammar is defined as follows:
 *
 * media-type     = type "/" subtype *( ";" parameter )
 * type           = token
 * subtype        = token
 * parameter      = attribute "=" value
 * attribute      = token
 * value          = token | quoted-string
 * token          = 1*<any CHAR except CTLs or separators>
 * quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
 * qdtext         = <any TEXT except <">>
 * quoted-pair    = "\" CHAR
 *
 * Returns the NULL terminated array of strings which consists of the
 * type/subtype part followed by attribute + value pairs. For example,
 * this string:
 *
 *   "text/html; charset=ISO-8859-4"
 *
 * would be parsed into the followring:
 *
 *   "text/html"
 *   "charset"
 *   "ISO-8859-4"
 *   NULL
 *
 * The caller is responsible for deallocating the returned string
 * with g_strfreev
 */

/*
 * token          = 1*<any CHAR except CTLs or separators>
 * CHAR           = <any US-ASCII character (octets 0 - 127)>
 * CTL            = <any US-ASCII control character
 *                 (octets 0 - 31) and DEL (127)>
 * separators     = "(" | ")" | "<" | ">" | "@"
 *                | "," | ";" | ":" | "\" | <">
 *                | "/" | "[" | "]" | "?" | "="
 *                | "{" | "}" | SP | HT
 */
static const char http_token[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x00    -   0x0f */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x10    -   0x1f */
	0,1,0,1,1,1,1,1,0,0,1,1,0,1,1,0,	/*  !"#$%&'()*+,-./ */
	1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,	/* 0123456789:;<=>? */
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* @ABCDEFGHIJKLMNO */
	1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,	/* PQRSTUVWXYZ[\]^_ */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* `abcdefghijklmno */
	1,1,1,1,1,1,1,1,1,1,1,0,1,0,1,0,	/* pqrstuvwxyz{|}~  */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 	/* 0x80 - 0xFF      */
};

/*
 * quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
 * quoted-pair    = "\" CHAR
 * qdtext         = <any TEXT except <">>
 * TEXT           = <any OCTET except CTLs,
 *                  but including LWS>
 * CTL            = <any US-ASCII control character
 *                 (octets 0 - 31) and DEL (127)>
 */
static const char http_qdtext[] = {
	0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,	/* 0x00 - 0x0f      */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x10 - 0x1f      */
	1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,	/*  !"#$%&'()*+,-./ */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* 0123456789:;<=>? */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* @ABCDEFGHIJKLMNO */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* PQRSTUVWXYZ[\]^_ */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* `abcdefghijklmno */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,	/* pqrstuvwxyz{|}~  */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 	/* 0x80 - 0xFF      */
};

static const char http_space[] = {
	0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,	/* 0x00 - 0x0f      */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0x10 - 0x1f      */
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/*  !"#$%&'()*+,-./ */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 0123456789:;<=>? */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* @ABCDEFGHIJKLMNO */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* PQRSTUVWXYZ[\]^_ */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* `abcdefghijklmno */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* pqrstuvwxyz{|}~  */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 	/* 0x80 - 0xFF      */
};

static const unsigned char *mms_parse_skip_spaces(const unsigned char *ptr)
{
	while (http_space[*ptr]) ptr++;
	return ptr;
}

static gboolean mms_is_http_token(const unsigned char *ptr)
{
	if (ptr && *ptr) {
		while (*ptr) {
			if (!http_token[*ptr++]) {
				return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}

static const unsigned char *mms_parse_http_token(const unsigned char *ptr,
								GString *token)
{
	int len = 0;
	ptr = mms_parse_skip_spaces(ptr);
	while (http_token[*ptr]) {
		g_string_append_c(token, *ptr);
		ptr++;
		len++;
	}
	return len > 0 ? mms_parse_skip_spaces(ptr) : NULL;
}

static const unsigned char *mms_parse_quoted_string(const unsigned char *ptr,
								GString *value)
{
	ptr = mms_parse_skip_spaces(ptr);
	if (*ptr == '"') {
		ptr++;
		while (*ptr == '\\' || http_qdtext[*ptr]) {
			if (*ptr == '\\') {
				ptr++;
				if (*ptr) g_string_append_c(value, *ptr++);
			} else {
				g_string_append_c(value, *ptr++);
			}
		}
		if (*ptr == '"') {
			return mms_parse_skip_spaces(ptr+1);
		}
	}
	return NULL;
}

static void mms_parse_collect_value(GPtrArray *output, GString *value)
{
	g_ptr_array_add(output, g_strdup(value->str));
	g_string_truncate(value, 0);
}

static gboolean mms_parse_parameters(const unsigned char *ptr,
					GString *token, GPtrArray *output)
{
	g_string_truncate(token, 0);
	ptr = mms_parse_skip_spaces(ptr);
	while (ptr && *ptr == ';') {
		if ((ptr = mms_parse_http_token(ptr+1, token)) != NULL) {
			if (*ptr == '=') {
				mms_parse_collect_value(output, token);
				ptr = mms_parse_skip_spaces(ptr+1);
				ptr = (*ptr == '"') ?
					mms_parse_quoted_string(ptr, token) :
					mms_parse_http_token(ptr, token);
				if (ptr) {
					mms_parse_collect_value(output, token);
				}
			} else {
				return FALSE; /* Missing = */
			}

		}
	}
	return ptr && !*ptr;
}

static const unsigned char *mms_parse_type(const unsigned char *ptr,
							GString *token)
{
	ptr = mms_parse_http_token(ptr, token);
	if (ptr && *ptr == '/') {
		g_string_append_c(token, *ptr++);
		return mms_parse_http_token(ptr, token);
	}
	return NULL;
}

char **mms_parse_http_content_type(const char *str)
{
	gboolean ok = FALSE;
	GPtrArray *output = NULL;
	GString *tmp = g_string_new(NULL);
	const unsigned char *ptr = mms_parse_type((unsigned char*)str, tmp);
	if (ptr) {
		output = g_ptr_array_new();
		mms_parse_collect_value(output, tmp);
		ok = mms_parse_parameters(ptr, tmp, output);
	}
	g_string_free(tmp, TRUE);
	if (ok) {
		g_ptr_array_add(output, NULL);
		return (char**)g_ptr_array_free(output, FALSE);
	}
	if (output) {
		g_ptr_array_set_free_func(output, g_free);
		g_ptr_array_free(output, TRUE);
	}
	return NULL;
}

char *mms_unparse_http_content_type(char **ct)
{
	if (ct && ct[0]) {
		int i;
		GString *str = g_string_new(ct[0]);
		for (i = 1; ct[i]; i += 2) {
			const char *val = ct[i+1];
			g_string_append(str, "; ");
			g_string_append(str, ct[i]);
			g_string_append_c(str, '=');
			if (mms_is_http_token((unsigned char*)val)) {
				g_string_append(str, val);
			} else {
				g_string_append_c(str, '"');
				while (*val) {
					const unsigned char uval = *val;
					if (!http_qdtext[uval]) {
						g_string_append_c(str, '\\');
					}
					g_string_append_c(str, *val++);
				}
				g_string_append_c(str, '"');
			}
		}
		return g_string_free(str, FALSE);
	}
	return NULL;
}

static const char *charset_index2string(unsigned int index)
{
	int low = 0;
	int high = G_N_ELEMENTS(charset_assignments) - 1;

	while (low <= high) {
		const int mid = (low + high)/2;
		const unsigned int val = charset_assignments[mid].mib_enum;
		if (val < index) {
			low = mid + 1;
		} else if (val > index) {
			high = mid - 1;
		} else {
			return charset_assignments[mid].charset;
		}
	}

	return NULL;
}

static gboolean extract_short(struct wsp_header_iter *iter, void *user)
{
	unsigned char *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);
	*out = p[0];

	return TRUE;
}

static const char *decode_text(struct wsp_header_iter *iter)
{
	const unsigned char *p;
	unsigned int l;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_TEXT)
		return NULL;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	return wsp_decode_text(p, l, NULL);
}

static gboolean extract_text(struct wsp_header_iter *iter, void *user)
{
	char **out = user;
	const char *text;

	text = decode_text(iter);
	if (text == NULL)
		return FALSE;

	*out = g_strdup(text);

	return TRUE;
}

static gboolean extract_text_array_element(struct wsp_header_iter *iter,
						void *user)
{
	char **out = user;
	const char *element;
	char *tmp;

	element = decode_text(iter);
	if (element == NULL)
		return FALSE;

	if (*out == NULL) {
		*out = g_strdup(element);
		return TRUE;
	}

	tmp = g_strjoin(",", *out, element, NULL);
	if (tmp == NULL)
		return FALSE;

	g_free(*out);

	*out = tmp;

	return TRUE;
}

static char *decode_encoded_string_with_mib_enum(const unsigned char *p,
		unsigned int l)
{
	unsigned int mib_enum;
	unsigned int consumed;
	const char *text;
	const char *from_codeset;
	const char *to_codeset = "UTF-8";
	gsize bytes_read;
	gsize bytes_written;

	if (wsp_decode_integer(p, l, &mib_enum, &consumed) == FALSE)
		return NULL;

	if (mib_enum == 106) {
		/* header is UTF-8 already */
		text = wsp_decode_text(p + consumed, l - consumed, NULL);

		return g_strdup(text);
	}

	/* convert to UTF-8 */
	from_codeset = charset_index2string(mib_enum);
	if (from_codeset == NULL)
		return NULL;

	return g_convert((const char *) p + consumed, l - consumed,
			to_codeset, from_codeset,
			&bytes_read, &bytes_written, NULL);
}

static gboolean extract_encoded_text(struct wsp_header_iter *iter, void *user)
{
	char **out = user;
	const unsigned char *p;
	unsigned int l;
	const char *text;
	char *uninitialized_var(dec_text);

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	switch (wsp_header_iter_get_val_type(iter)) {
	case WSP_VALUE_TYPE_TEXT:
		/* Text-string */
		text = wsp_decode_text(p, l, NULL);
		dec_text = g_strdup(text);
		break;
	case WSP_VALUE_TYPE_LONG:
		/* (Value-len) Char-set Text-string */
		dec_text = decode_encoded_string_with_mib_enum(p, l);
		break;
	case WSP_VALUE_TYPE_SHORT:
		dec_text = NULL;
		break;
	}

	if (dec_text == NULL)
		return FALSE;

	*out = dec_text;

	return TRUE;
}

static gboolean extract_date(struct wsp_header_iter *iter, void *user)
{
	time_t *out = user;
	const unsigned char *p;
	unsigned int l;
	unsigned int i;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_LONG)
		return FALSE;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	if (l > 4)
		return FALSE;

	for (i = 0, *out = 0; i < l; i++)
		*out = *out << 8 | p[i];

	/* It is possible to overflow time_t on 32 bit systems */
	*out = *out & 0x7fffffff;

	return TRUE;
}

static gboolean extract_absolute_relative_date(struct wsp_header_iter *iter,
						void *user)
{
	time_t *out = user;
	const unsigned char *p;
	unsigned int l;
	unsigned int i;
	unsigned int seconds;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_LONG)
		return FALSE;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	if (l < 2 || l > 5)
		return FALSE;

	if (p[0] != 128 && p[0] != 129)
		return FALSE;

	for (i = 2, seconds = 0; i < l; i++)
		seconds = seconds << 8 | p[i];

	if (p[0] == 129) {
		*out = time(NULL);
		*out += seconds;
	} else
		*out = seconds;

	/* It is possible to overflow time_t on 32 bit systems */
	*out = *out & 0x7fffffff;

	return TRUE;
}

static gboolean extract_boolean(struct wsp_header_iter *iter, void *user)
{
	gboolean *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	if (p[0] == MMS_MESSAGE_VALUE_BOOL_YES) {
		*out = TRUE;
		return TRUE;
	} else if (p[0] == MMS_MESSAGE_VALUE_BOOL_NO) {
		*out = FALSE;
		return TRUE;
	} else {
		return TRUE;
	}
}

static gboolean extract_from(struct wsp_header_iter *iter, void *user)
{
	char **out = user;
	const unsigned char *p;
	unsigned int l;
	const char *text;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_LONG)
		return FALSE;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	if (p[0] != 128 && p[0] != 129)
		return FALSE;

	if (p[0] == 129) {
		*out = NULL;
		return TRUE;
	}

	text = wsp_decode_text(p + 1, l - 1, NULL);
	if (text == NULL)
		return FALSE;

	*out = g_strdup(text);

	return TRUE;
}

static gboolean extract_message_class(struct wsp_header_iter *iter, void *user)
{
	char **out = user;
	const unsigned char *p;
	unsigned int l;
	const char *text;

	if (wsp_header_iter_get_val_type(iter) == WSP_VALUE_TYPE_LONG)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	if (wsp_header_iter_get_val_type(iter) == WSP_VALUE_TYPE_SHORT) {
		switch (p[0]) {
		case 128:
			*out = g_strdup(MMS_MESSAGE_CLASS_PERSONAL);
			return TRUE;
		case 129:
			*out = g_strdup(MMS_MESSAGE_CLASS_ADVERTISEMENT);
			return TRUE;
		case 130:
			*out = g_strdup(MMS_MESSAGE_CLASS_INFORMATIONAL);
			return TRUE;
		case 131:
			*out = g_strdup(MMS_MESSAGE_CLASS_AUTO);
			return TRUE;
		default:
			return FALSE;
		}
	}

	l = wsp_header_iter_get_val_len(iter);

	text = wsp_decode_token_text(p, l, NULL);
	if (text == NULL)
		return FALSE;

	*out = g_strdup(text);

	return TRUE;
}

static gboolean extract_sender_visibility(struct wsp_header_iter *iter,
						void *user)
{
	enum mms_message_sender_visibility *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	if (p[0] != 128 && p[0] != 129)
		return FALSE;

	*out = p[0];

	return TRUE;
}

static gboolean extract_priority(struct wsp_header_iter *iter, void *user)
{
	enum mms_message_priority *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	switch (p[0]) {
	case MMS_MESSAGE_PRIORITY_LOW:
	case MMS_MESSAGE_PRIORITY_NORMAL:
	case MMS_MESSAGE_PRIORITY_HIGH:
		*out = p[0];
		return TRUE;
	}

	return FALSE;
}

static gboolean extract_rsp_status(struct wsp_header_iter *iter, void *user)
{
	unsigned char *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	switch (p[0]) {
	case MMS_MESSAGE_RSP_STATUS_OK:
	case MMS_MESSAGE_RSP_STATUS_ERR_UNSUPPORTED_MESSAGE:
	case MMS_MESSAGE_RSP_STATUS_ERR_TRANS_FAILURE:
	case MMS_MESSAGE_RSP_STATUS_ERR_TRANS_NETWORK_PROBLEM:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_FAILURE:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_SERVICE_DENIED:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_MESSAGE_FORMAT_CORRUPT:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_SENDING_ADDRESS_UNRESOLVED:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_CONTENT_NOT_ACCEPTED:
	case MMS_MESSAGE_RSP_STATUS_ERR_PERM_LACK_OF_PREPAID:
		*out = p[0];
		return TRUE;
	}

	return FALSE;
}

static gboolean extract_status(struct wsp_header_iter *iter, void *user)
{
	enum mms_message_delivery_status *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	switch (p[0]) {
	case MMS_MESSAGE_DELIVERY_STATUS_EXPIRED:
	case MMS_MESSAGE_DELIVERY_STATUS_RETRIEVED:
	case MMS_MESSAGE_DELIVERY_STATUS_REJECTED:
	case MMS_MESSAGE_DELIVERY_STATUS_DEFERRED:
	case MMS_MESSAGE_DELIVERY_STATUS_UNRECOGNISED:
	case MMS_MESSAGE_DELIVERY_STATUS_INDETERMINATE:
	case MMS_MESSAGE_DELIVERY_STATUS_FORWARDED:
	case MMS_MESSAGE_DELIVERY_STATUS_UNREACHABLE:
		*out = p[0];
		return TRUE;
	}

	return FALSE;
}

static gboolean extract_retrieve_status(struct wsp_header_iter *iter, void *user)
{
	unsigned char *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	switch (p[0]) {
	case MMS_MESSAGE_RETRIEVE_STATUS_OK:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_TRANS_FAILURE:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_TRANS_MESSAGE_NOT_FOUND:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_TRANS_NETWORK_PROBLEM:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_PERM_FAILURE:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_PERM_SERVICE_DENIED:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_PERM_MESSAGE_NOT_FOUND:
	case MMS_MESSAGE_RETRIEVE_STATUS_ERR_PERM_CONTENT_UNSUPPORTED:
		*out = p[0];
		return TRUE;
	}

	return FALSE;
}

static gboolean extract_read_status(struct wsp_header_iter *iter, void *user)
{
	enum mms_message_read_status *out = user;
	const unsigned char *p;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_SHORT)
		return FALSE;

	p = wsp_header_iter_get_val(iter);

	switch (p[0]) {
	case MMS_MESSAGE_READ_STATUS_READ:
	case MMS_MESSAGE_READ_STATUS_DELETED:
		*out = p[0];
		return TRUE;
	}

	return FALSE;
}

static gboolean extract_unsigned(struct wsp_header_iter *iter, void *user)
{
	unsigned long *out = user;
	const unsigned char *p;
	unsigned int l;
	unsigned int i;

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_LONG)
		return FALSE;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	if (l > sizeof(unsigned long))
		return FALSE;

	for (i = 0, *out = 0; i < l; i++)
		*out = *out << 8 | p[i];

	return TRUE;
}

static header_handler handler_for_type(enum mms_header header)
{
	switch (header) {
	case MMS_HEADER_BCC:
		return &extract_text_array_element;
	case MMS_HEADER_CC:
		return &extract_text_array_element;
	case MMS_HEADER_CONTENT_LOCATION:
		return &extract_text;
	case MMS_HEADER_CONTENT_TYPE:
		return &extract_text;
	case MMS_HEADER_DATE:
		return &extract_date;
	case MMS_HEADER_DELIVERY_REPORT:
		return &extract_boolean;
	case MMS_HEADER_DELIVERY_TIME:
		return &extract_absolute_relative_date;
	case MMS_HEADER_EXPIRY:
		return &extract_absolute_relative_date;
	case MMS_HEADER_FROM:
		return &extract_from;
	case MMS_HEADER_MESSAGE_CLASS:
		return &extract_message_class;
	case MMS_HEADER_MESSAGE_ID:
		return &extract_text;
	case MMS_HEADER_MESSAGE_TYPE:
		return &extract_short;
	case MMS_HEADER_MMS_VERSION:
		return &extract_short;
	case MMS_HEADER_MESSAGE_SIZE:
		return &extract_unsigned;
	case MMS_HEADER_PRIORITY:
		return &extract_priority;
	case MMS_HEADER_READ_REPORT:
		return &extract_boolean;
	case MMS_HEADER_REPORT_ALLOWED:
		return &extract_boolean;
	case MMS_HEADER_RESPONSE_STATUS:
		return &extract_rsp_status;
	case MMS_HEADER_RESPONSE_TEXT:
		return &extract_encoded_text;
	case MMS_HEADER_SENDER_VISIBILITY:
		return &extract_sender_visibility;
	case MMS_HEADER_STATUS:
		return &extract_status;
	case MMS_HEADER_SUBJECT:
		return &extract_encoded_text;
	case MMS_HEADER_TO:
		return &extract_text_array_element;
	case MMS_HEADER_TRANSACTION_ID:
		return &extract_text;
	case MMS_HEADER_RETRIEVE_STATUS:
		return &extract_retrieve_status;
	case MMS_HEADER_RETRIEVE_TEXT:
		return &extract_encoded_text;
	case MMS_HEADER_READ_STATUS:
		return &extract_read_status;
	case MMS_HEADER_INVALID:
	case __MMS_HEADER_MAX:
		return NULL;
	}

	return NULL;
}

struct header_handler_entry {
	int flags;
	void *data;
	int pos;
};

static gboolean mms_parse_headers(struct wsp_header_iter *iter,
					enum mms_header orig_header, ...)
{
	struct header_handler_entry entries[__MMS_HEADER_MAX + 1];
	va_list args;
	const unsigned char *p;
	int i;
	enum mms_header header;

	memset(&entries, 0, sizeof(entries));

	va_start(args, orig_header);
	header = orig_header;

	while (header != MMS_HEADER_INVALID) {
		entries[header].flags = va_arg(args, int);
		entries[header].data = va_arg(args, void *);

		header = va_arg(args, enum mms_header);
	}

	va_end(args);

	for (i = 1; wsp_header_iter_next(iter); i++) {
		unsigned char h;
		header_handler handler;

		/* Skip application headers */
		if (wsp_header_iter_get_hdr_type(iter) !=
				WSP_HEADER_TYPE_WELL_KNOWN)
			continue;

		p = wsp_header_iter_get_hdr(iter);
		h = p[0] & 0x7f;

		/* Stop parsing when we see an unknown header */
		handler = handler_for_type(h);
		if (handler == NULL)
			break;

		/* Unsupported header, skip */
		if (entries[h].data == NULL)
			continue;

		/* Skip multiply present headers unless explicitly requested */
		if ((entries[h].flags & HEADER_FLAG_MARKED) &&
				!(entries[h].flags & HEADER_FLAG_ALLOW_MULTI))
			continue;

		/* Parse the header, stop if we fail to parse it */
		if (handler(iter, entries[h].data) == FALSE)
			break;

		entries[h].pos = i;
		entries[h].flags |= HEADER_FLAG_MARKED;
	}

	for (i = 0; i < __MMS_HEADER_MAX + 1; i++) {
		if ((entries[i].flags & HEADER_FLAG_MANDATORY) &&
				!(entries[i].flags & HEADER_FLAG_MARKED))
			return FALSE;
	}

	/*
	 * Here we check for header positions.  This function assumes that
	 * headers marked with PRESET_POS are in the beginning of the message
	 * and follow the same order as given in the va_arg list.  The headers
	 * marked this way have to be contiguous.
	 */
	for (i = 0; i < __MMS_HEADER_MAX + 1; i++) {
		int check_flags = HEADER_FLAG_PRESET_POS | HEADER_FLAG_MARKED;
		int expected_pos = 1;

		if ((entries[i].flags & check_flags) != check_flags)
			continue;

		va_start(args, orig_header);
		header = orig_header;

		while (header != MMS_HEADER_INVALID && (int)header != i) {
			va_arg(args, int);
			va_arg(args, void *);

			if (entries[header].flags & HEADER_FLAG_MARKED)
				expected_pos += 1;

			header = va_arg(args, enum mms_header);
		}

		va_end(args);

		if (entries[i].pos != expected_pos)
			return FALSE;
	}

	return TRUE;
}

static gboolean decode_notification_ind(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_FROM,
				0, &out->ni.from,
				MMS_HEADER_SUBJECT,
				0, &out->ni.subject,
				MMS_HEADER_MESSAGE_CLASS,
				HEADER_FLAG_MANDATORY, &out->ni.cls,
				MMS_HEADER_MESSAGE_SIZE,
				HEADER_FLAG_MANDATORY, &out->ni.size,
				MMS_HEADER_EXPIRY,
				HEADER_FLAG_MANDATORY, &out->ni.expiry,
				MMS_HEADER_CONTENT_LOCATION,
				HEADER_FLAG_MANDATORY, &out->ni.location,
				MMS_HEADER_INVALID);
}

static gboolean decode_notify_resp_ind(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_STATUS,
				HEADER_FLAG_MANDATORY, &out->nri.notify_status,
				MMS_HEADER_INVALID);
}

static gboolean decode_acknowledge_ind(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_REPORT_ALLOWED,
				0, &out->ai.report,
				MMS_HEADER_INVALID);
}

static gboolean decode_delivery_ind(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_MESSAGE_ID,
				HEADER_FLAG_MANDATORY, &out->di.msgid,
				MMS_HEADER_TO,
				HEADER_FLAG_MANDATORY, &out->di.to,
				MMS_HEADER_DATE,
				HEADER_FLAG_MANDATORY, &out->di.date,
				MMS_HEADER_STATUS,
				HEADER_FLAG_MANDATORY, &out->di.dr_status,
				MMS_HEADER_INVALID);
}

static gboolean decode_read_ind(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_MESSAGE_ID,
				HEADER_FLAG_MANDATORY, &out->ri.msgid,
				MMS_HEADER_TO,
				HEADER_FLAG_MANDATORY, &out->ri.to,
				MMS_HEADER_FROM,
				HEADER_FLAG_MANDATORY, &out->ri.from,
				MMS_HEADER_DATE,
				HEADER_FLAG_MANDATORY, &out->ri.date,
				MMS_HEADER_READ_STATUS,
				HEADER_FLAG_MANDATORY, &out->ri.rr_status,
				MMS_HEADER_INVALID);
}

static const char *decode_attachment_charset(const unsigned char *pdu,
							unsigned int len)
{
	struct wsp_parameter_iter iter;
	struct wsp_parameter param;

	wsp_parameter_iter_init(&iter, pdu, len);

	while (wsp_parameter_iter_next(&iter, &param)) {
		if (param.type == WSP_PARAMETER_TYPE_CHARSET)
			return param.text;
	}

	return NULL;
}

static gboolean extract_quoted_string(struct wsp_header_iter *iter, char **out)
{
	const unsigned char *p;
	unsigned int l;
	const char *text;

	p = wsp_header_iter_get_val(iter);
	l = wsp_header_iter_get_val_len(iter);

	if (wsp_header_iter_get_val_type(iter) != WSP_VALUE_TYPE_TEXT)
		return FALSE;

	text = wsp_decode_quoted_string(p, l, NULL);

	if (text == NULL)
		return FALSE;

	*out = g_strdup(text);

	return TRUE;
}

static gboolean attachment_parse_headers(struct wsp_header_iter *iter,
						struct mms_attachment *part)
{
	while (wsp_header_iter_next(iter)) {
		const unsigned char *hdr = wsp_header_iter_get_hdr(iter);
		unsigned char h;

		/* Skip application headers */
		if (wsp_header_iter_get_hdr_type(iter) !=
				WSP_HEADER_TYPE_WELL_KNOWN)
			continue;

		h = hdr[0] & 0x7f;

		switch (h) {
		case MMS_PART_HEADER_CONTENT_ID:
			if (!extract_quoted_string(iter, &part->content_id))
				return FALSE;
			break;
		case MMS_PART_HEADER_CONTENT_LOCATION:
			if (!extract_text(iter, &part->content_location))
				return FALSE;
			break;
		}
	}

	return TRUE;
}

static void free_attachment(gpointer data, gpointer user_data)
{
	struct mms_attachment *attach = data;

	g_free(attach->content_type);
	g_free(attach->content_id);
	g_free(attach->content_location);

	g_free(attach);
}

static gboolean mms_parse_attachments(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	struct wsp_multipart_iter mi;
	const void *ct;
	unsigned int ct_len;
	unsigned int consumed;

	if (wsp_multipart_iter_init(&mi, iter, &ct, &ct_len) == FALSE)
		return FALSE;

	while (wsp_multipart_iter_next(&mi) == TRUE) {
		struct mms_attachment *part;
		struct wsp_header_iter hi;
		const void *mimetype;
		const char *charset;

		ct = wsp_multipart_iter_get_content_type(&mi);
		ct_len = wsp_multipart_iter_get_content_type_len(&mi);

		if (wsp_decode_content_type(ct, ct_len, &mimetype,
						&consumed, NULL) == FALSE)
			return FALSE;

		charset = decode_attachment_charset(
					(const unsigned char *)ct + consumed,
					ct_len - consumed);

		wsp_header_iter_init(&hi, wsp_multipart_iter_get_hdr(&mi),
					wsp_multipart_iter_get_hdr_len(&mi),
					0);

		part = g_try_new0(struct mms_attachment, 1);
		if (part == NULL)
			return FALSE;

		if (attachment_parse_headers(&hi, part) == FALSE) {
			free_attachment(part, NULL);
			return FALSE;
		}

		if (wsp_header_iter_at_end(&hi) == FALSE) {
			free_attachment(part, NULL);
			return FALSE;
		}

		if (charset == NULL)
			part->content_type = g_strdup(mimetype);
		else
			part->content_type = g_strconcat(mimetype, ";charset=",
								charset, NULL);

		part->length = wsp_multipart_iter_get_body_len(&mi);
		part->offset = (const unsigned char *)
					wsp_multipart_iter_get_body(&mi) -
					wsp_header_iter_get_pdu(iter);

		out->attachments = g_slist_prepend(out->attachments, part);
	}

	if (wsp_multipart_iter_close(&mi, iter) == FALSE)
		return FALSE;

	out->attachments = g_slist_reverse(out->attachments);

	return TRUE;
}

static gboolean decode_retrieve_conf(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	if (mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_PRESET_POS, &out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_FROM,
				0, &out->rc.from,
				MMS_HEADER_TO,
				HEADER_FLAG_ALLOW_MULTI, &out->rc.to,
				MMS_HEADER_CC,
				HEADER_FLAG_ALLOW_MULTI, &out->rc.cc,
				MMS_HEADER_SUBJECT,
				0, &out->rc.subject,
				MMS_HEADER_MESSAGE_CLASS,
				0, &out->rc.cls,
				MMS_HEADER_PRIORITY,
				0, &out->rc.priority,
				MMS_HEADER_MESSAGE_ID,
				0, &out->rc.msgid,
				MMS_HEADER_DATE,
				HEADER_FLAG_MANDATORY, &out->rc.date,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	if (wsp_header_iter_at_end(iter) == TRUE)
		return TRUE;

	if (wsp_header_iter_is_multipart(iter) == FALSE)
		return FALSE;

	if (mms_parse_attachments(iter, out) == FALSE)
		return FALSE;

	if (wsp_header_iter_at_end(iter) == FALSE)
		return FALSE;

	return TRUE;
}

static gboolean decode_send_conf(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	return mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_RESPONSE_STATUS,
				HEADER_FLAG_MANDATORY, &out->sc.rsp_status,
				MMS_HEADER_MESSAGE_ID,
				0, &out->sc.msgid,
				MMS_HEADER_INVALID);
}

static gboolean decode_send_req(struct wsp_header_iter *iter,
                        struct mms_message *out)
{
	if (mms_parse_headers(iter, MMS_HEADER_TRANSACTION_ID,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->transaction_id,
				MMS_HEADER_MMS_VERSION,
				HEADER_FLAG_MANDATORY | HEADER_FLAG_PRESET_POS,
				&out->version,
				MMS_HEADER_TO,
				HEADER_FLAG_ALLOW_MULTI, &out->sr.to,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	if (wsp_header_iter_at_end(iter) == TRUE)
		return TRUE;

	if (wsp_header_iter_is_multipart(iter) == FALSE)
		return FALSE;

	if (mms_parse_attachments(iter, out) == FALSE)
		return FALSE;

	if (wsp_header_iter_at_end(iter) == FALSE)
		return FALSE;

	return TRUE;
}

#define CHECK_WELL_KNOWN_HDR(hdr)			\
	if (wsp_header_iter_next(&iter) == FALSE)	\
		return FALSE;				\
							\
	if (wsp_header_iter_get_hdr_type(&iter) !=	\
			WSP_HEADER_TYPE_WELL_KNOWN)	\
		return FALSE;				\
							\
	p = wsp_header_iter_get_hdr(&iter);		\
							\
	if ((p[0] & 0x7f) != hdr)			\
		return FALSE				\

gboolean mms_message_decode(const unsigned char *pdu,
                unsigned int len, struct mms_message *out)
{
	unsigned int flags = 0;
	struct wsp_header_iter iter;
	const unsigned char *p;
	unsigned char octet;

	memset(out, 0, sizeof(*out));

	flags |= WSP_HEADER_ITER_FLAG_REJECT_CP;
	flags |= WSP_HEADER_ITER_FLAG_DETECT_MMS_MULTIPART;
	wsp_header_iter_init(&iter, pdu, len, flags);

	CHECK_WELL_KNOWN_HDR(MMS_HEADER_MESSAGE_TYPE);

	if (extract_short(&iter, &octet) == FALSE)
		return FALSE;

	out->type = octet;

	switch (out->type) {
	case MMS_MESSAGE_TYPE_SEND_REQ:
		return decode_send_req(&iter, out);
	case MMS_MESSAGE_TYPE_SEND_CONF:
		return decode_send_conf(&iter, out);
	case MMS_MESSAGE_TYPE_NOTIFICATION_IND:
		return decode_notification_ind(&iter, out);
	case MMS_MESSAGE_TYPE_NOTIFYRESP_IND:
		return decode_notify_resp_ind(&iter, out);
	case MMS_MESSAGE_TYPE_RETRIEVE_CONF:
		return decode_retrieve_conf(&iter, out);
	case MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND:
		return decode_acknowledge_ind(&iter, out);
	case MMS_MESSAGE_TYPE_DELIVERY_IND:
		return decode_delivery_ind(&iter, out);
	case MMS_MESSAGE_TYPE_READ_REC_IND:
	case MMS_MESSAGE_TYPE_READ_ORIG_IND:
		return decode_read_ind(&iter, out);
	}

	return FALSE;
}

void mms_message_free(struct mms_message *msg)
{
	switch (msg->type) {
	case MMS_MESSAGE_TYPE_SEND_REQ:
		g_free(msg->sr.to);
		g_free(msg->sr.content_type);
		break;
	case MMS_MESSAGE_TYPE_SEND_CONF:
		g_free(msg->sc.msgid);
		break;
	case MMS_MESSAGE_TYPE_NOTIFICATION_IND:
		g_free(msg->ni.from);
		g_free(msg->ni.subject);
		g_free(msg->ni.cls);
		g_free(msg->ni.location);
		break;
	case MMS_MESSAGE_TYPE_NOTIFYRESP_IND:
		break;
	case MMS_MESSAGE_TYPE_RETRIEVE_CONF:
		g_free(msg->rc.from);
		g_free(msg->rc.to);
		g_free(msg->rc.cc);
		g_free(msg->rc.subject);
		g_free(msg->rc.cls);
		g_free(msg->rc.msgid);
		break;
	case MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND:
		break;
	case MMS_MESSAGE_TYPE_DELIVERY_IND:
		g_free(msg->di.msgid);
		g_free(msg->di.to);
		break;
	case MMS_MESSAGE_TYPE_READ_REC_IND:
	case MMS_MESSAGE_TYPE_READ_ORIG_IND:
		g_free(msg->ri.msgid);
		g_free(msg->ri.to);
		g_free(msg->ri.from);
		break;
	}

	g_free(msg->transaction_id);

	if (msg->attachments != NULL) {
		g_slist_foreach(msg->attachments, free_attachment, NULL);
		g_slist_free(msg->attachments);
	}

	g_free(msg);
}

static void fb_init(struct file_buffer *fb, int fd)
{
	fb->size = 0;
	fb->fsize = 0;
	fb->fd = fd;
}

static gboolean fb_flush(struct file_buffer *fb)
{
	unsigned int size;
	ssize_t len;

	if (fb->size == 0)
		return TRUE;

	len = write(fb->fd, fb->buf, fb->size);
	if (len < 0)
		return FALSE;

	size = len;

	if (size != fb->size)
		return FALSE;

	fb->fsize += size;

	fb->size = 0;

	return TRUE;
}

static unsigned int fb_get_file_size(struct file_buffer *fb)
{
	return fb->fsize + fb->size;
}

static void *fb_request(struct file_buffer *fb, unsigned int count)
{
	if (fb->size + count < FB_SIZE) {
		void *ptr = fb->buf + fb->size;
		fb->size += count;
		return ptr;
	}

	if (fb_flush(fb) == FALSE)
		return NULL;

	if (count > FB_SIZE)
		return NULL;

	fb->size = count;

	return fb->buf;
}

static void *fb_request_field(struct file_buffer *fb, unsigned char token,
					unsigned int len)
{
	unsigned char *ptr;

	ptr = fb_request(fb, len + 1);
	if (ptr == NULL)
		return NULL;

	ptr[0] = token | 0x80;

	return ptr + 1;
}

static gboolean fb_copy(struct file_buffer *fb, const void *buf, unsigned int c)
{
	unsigned int written;
	ssize_t len;

	if (fb_flush(fb) == FALSE)
		return FALSE;

	len = TFR(write(fb->fd, buf, c));
	if (len < 0)
		return FALSE;

	written = len;

	if (written != c)
		return FALSE;

	fb->fsize += written;

	return TRUE;
}

static gboolean fb_put_value_length(struct file_buffer *fb, unsigned int val)
{
	unsigned int count;

	if (fb->size + MAX_ENC_VALUE_BYTES > FB_SIZE) {
		if (fb_flush(fb) == FALSE)
			return FALSE;
	}

	if (wsp_encode_value_length(val, fb->buf + fb->size, FB_SIZE - fb->size,
					&count) == FALSE)
		return FALSE;

	fb->size += count;

	return TRUE;
}

static gboolean fb_put_uintvar(struct file_buffer *fb, unsigned int val)
{
	unsigned int count;

	if (fb->size + MAX_ENC_VALUE_BYTES > FB_SIZE) {
		if (fb_flush(fb) == FALSE)
			return FALSE;
	}

	if (wsp_encode_uintvar(val, fb->buf + fb->size, FB_SIZE - fb->size,
					&count) == FALSE)
		return FALSE;

	fb->size += count;

	return TRUE;
}

static gboolean encode_short(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char *ptr;
	unsigned int *wk = user;

	ptr = fb_request_field(fb, header, 1);
	if (ptr == NULL)
		return FALSE;

	*ptr = *wk | 0x80;

	return TRUE;
}

static gboolean encode_boolean(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char *ptr;
	gboolean *value = user;

	ptr = fb_request_field(fb, header, 1);
	if (ptr == NULL)
		return FALSE;

	*ptr = *value ? MMS_MESSAGE_VALUE_BOOL_YES : MMS_MESSAGE_VALUE_BOOL_NO;

	return TRUE;
}

static gboolean encode_from(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char *ptr;
	char **text = user;

	if (strlen(*text) > 0)
		return FALSE;

	/* From: header token + value length + Insert-address-token */
	ptr = fb_request_field(fb, header, 2);
	if (ptr == NULL)
		return FALSE;

	ptr[0] = 1;
	ptr[1] = '\x81';

	return TRUE;
}

static gboolean encode_date(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	time_t date = *((time_t *)user);
	guint8 octets[sizeof(date)];
	guint8 *ptr;
	int i, len;

	/*
	 * Date: Long-integer
	 * In seconds from 1970-01-01, 00:00:00 GMT.
	 *
	 * Long-integer = Short-length Multi-octet-integer
	 * Short-length = <Any octet 0-30>
	 * Multi-octet-integer = 1*30 OCTET
	 *
	 * The content octets shall be an unsigned integer value with the
	 * most significant octet encoded first (big-endian representation).
	 * The minimum number of octets must be used to encode the value.
	 */
	for (i=sizeof(date)-1; i>=0; i--, date >>= 8) {
		/* Most significant byte first */
		octets[i] = (guint8)date;
	}
	/* Skip most significant zeros */
	for (i=0; i<(int)(sizeof(date)-1) && !octets[i]; i++);
	len = sizeof(date) - i;

	ptr = fb_request_field(fb, header, len+1);
	if (ptr == NULL)
		return FALSE;

	ptr[0] = (guint8)len;
	memcpy(ptr+1, octets+i, len);
	return TRUE;
}

static gboolean encode_text(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char *ptr;
	char **text = user;
	unsigned int len;

	len = strlen(*text) + 1;
	if ((*text)[0] & 0x80) len++;

	ptr = fb_request_field(fb, header, len);
	if (ptr == NULL)
		return FALSE;

	if ((*text)[0] & 0x80) *ptr++ = QUOTE;
	strcpy(ptr, *text);

	return TRUE;
}

static gboolean encode_quoted_string(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char *ptr;
	char **text = user;
	unsigned int len;

	len = strlen(*text) + 1;

	ptr = fb_request_field(fb, header, len + 3);
	if (ptr == NULL)
		return FALSE;

	ptr[0] = '"';
	ptr[1] = '<';
	strcpy(ptr + 2, *text);
	ptr[len + 1] = '>';
	ptr[len + 2] = '\0';

	return TRUE;
}

static gboolean encode_text_array_element(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	char **text = user;
	char **tos;
	int i;

	tos = g_strsplit(*text, ",", 0);

	for (i = 0; tos[i] != NULL; i++) {
		if (encode_text(fb, header, &tos[i]) == FALSE) {
			g_strfreev(tos);
			return FALSE;
		}
	}

	g_strfreev(tos);

	return TRUE;
}

struct content_type_par {
	unsigned char token;
	const char* value;
};

static gboolean encode_content_type(struct file_buffer *fb,
				enum mms_header header, void *user)
{
	int i;
	char *ptr;
	char **hdr = user;
	unsigned int len;
	unsigned int ct;
	unsigned int ct_len;
	const char *ct_str;
	int npar = 0;
	struct content_type_par *par = NULL;
	char **parsed = mms_parse_http_content_type(*hdr);
	gboolean ok = FALSE;

	if (parsed == NULL)
		return FALSE;

	ct_str = parsed[0];
	if (parsed[1])
		par = g_new(struct content_type_par, g_strv_length(parsed)/2);

	if (wsp_get_well_known_content_type(ct_str, &ct) == TRUE)
		ct_len = 1;
	else
		ct_len = strlen(ct_str) + 1;

	len = ct_len;

	for (i = 1; parsed[i]; i += 2) {
		const char *attribute = parsed[i];
		const char *value = parsed[i+1];
		unsigned char token;

		if (g_ascii_strcasecmp(attribute, "type") == 0) {
			token = WSP_PARAMETER_TYPE_CONTENT_TYPE;
		} else if (g_ascii_strcasecmp(attribute, "start") == 0) {
			token = WSP_PARAMETER_TYPE_START_DEFUNCT;
		} else {
			continue;
		}

		len += 1 + strlen(value) + 1;
		par[npar].token = token;
		par[npar].value = value;
		npar++;
	}

	if (len == 1) {
		ok = encode_short(fb, header, &ct);
		goto done;
	}

	ptr = fb_request(fb, 1);
	if (ptr == NULL)
		goto done;

	*ptr = header | 0x80;

	/* Encode content type value length */
	if (fb_put_value_length(fb, len) == FALSE)
		goto done;

	/* Encode content type including parameters */
	ptr = fb_request(fb, ct_len);
	if (ptr == NULL)
		goto done;

	if (ct_len == 1)
		*ptr = ct | 0x80;
	else
		strcpy(ptr, ct_str);

	for (i = 0; i < npar; i++) {
		unsigned int len = strlen(par[i].value) + 1;
		ptr = fb_request_field(fb, par[i].token, len);
		if (ptr == NULL)
			goto done;

		strcpy(ptr, par[i].value);
	}

	ok = TRUE;

done:
	g_free(par);
	g_strfreev(parsed);
	return ok;
}

static header_encoder encoder_for_type(enum mms_header header)
{
	switch (header) {
	case MMS_HEADER_BCC:
		return &encode_text_array_element;
	case MMS_HEADER_CC:
		return &encode_text_array_element;
	case MMS_HEADER_CONTENT_LOCATION:
		return NULL;
	case MMS_HEADER_CONTENT_TYPE:
		return &encode_content_type;
	case MMS_HEADER_DATE:
		return &encode_date;
	case MMS_HEADER_DELIVERY_REPORT:
		return &encode_boolean;
	case MMS_HEADER_DELIVERY_TIME:
		return NULL;
	case MMS_HEADER_EXPIRY:
		return NULL;
	case MMS_HEADER_FROM:
		return &encode_from;
	case MMS_HEADER_MESSAGE_CLASS:
		return NULL;
	case MMS_HEADER_MESSAGE_ID:
		return &encode_text;
	case MMS_HEADER_MESSAGE_TYPE:
		return &encode_short;
	case MMS_HEADER_MMS_VERSION:
		return &encode_short;
	case MMS_HEADER_MESSAGE_SIZE:
		return NULL;
	case MMS_HEADER_PRIORITY:
		return NULL;
	case MMS_HEADER_READ_REPORT:
		return &encode_boolean;
	case MMS_HEADER_REPORT_ALLOWED:
		return &encode_boolean;
	case MMS_HEADER_RESPONSE_STATUS:
		return NULL;
	case MMS_HEADER_RESPONSE_TEXT:
		return NULL;
	case MMS_HEADER_SENDER_VISIBILITY:
		return NULL;
	case MMS_HEADER_STATUS:
		return &encode_short;
	case MMS_HEADER_SUBJECT:
		return NULL;
	case MMS_HEADER_TO:
		return &encode_text_array_element;
	case MMS_HEADER_TRANSACTION_ID:
		return &encode_text;
	case MMS_HEADER_RETRIEVE_STATUS:
		return &encode_short;
	case MMS_HEADER_RETRIEVE_TEXT:
		return NULL;
	case MMS_HEADER_READ_STATUS:
		return &encode_short;
	case MMS_HEADER_INVALID:
	case __MMS_HEADER_MAX:
		return NULL;
	}

	return NULL;
}

static gboolean mms_encode_send_req_part_header(struct mms_attachment *part,
						struct file_buffer *fb)
{
	int i;
	char *ptr;
	unsigned int len;
	unsigned int ct;
	unsigned int ct_len;
	unsigned int cs_len;
	const char *ct_str;
	const char *uninitialized_var(cs_str);
	unsigned int ctp_len;
	unsigned int cid_len;
	unsigned int cloc_len;
	unsigned int cd_len;
	unsigned char ctp_val[MAX_ENC_VALUE_BYTES];
	unsigned char cs_val[MAX_ENC_VALUE_BYTES];
	unsigned char cd_val[MAX_ENC_VALUE_BYTES];
	unsigned int cs;
	char **parsed = mms_parse_http_content_type(part->content_type);
	gboolean ok = FALSE;
	gboolean is_smil;

	if (parsed == NULL)
		return FALSE;

	ct_str = parsed[0];
	is_smil = strcmp(ct_str, "application/smil") == 0;

	if (wsp_get_well_known_content_type(ct_str, &ct) == TRUE)
		ct_len = 1;
	else
		ct_len = strlen(ct_str) + 1;

	len = ct_len;

	cs_len = 0;

	for (i = 1; parsed[i]; i += 2) {
		const char *key = parsed[i];

		if (g_ascii_strcasecmp("charset", key) == 0) {
			cs_str = parsed[i+1];

			len += 1;

			if (wsp_get_well_known_charset(cs_str, &cs) == FALSE)
				goto done;

			if (wsp_encode_integer(cs, cs_val, MAX_ENC_VALUE_BYTES,
							&cs_len) == FALSE)
				goto done;

			len += cs_len;
		}
	}

	if (wsp_encode_value_length(len, ctp_val, MAX_ENC_VALUE_BYTES,
							&ctp_len) == FALSE)
		goto done;

	len += ctp_len;

	/* Compute content-id header length : token + (Quoted String) */
	if (part->content_id != NULL) {
		cid_len = 1 + strlen(part->content_id) + 3 + 1;
		len += cid_len;
	} else
		cid_len = 0;

	/* Compute content-location header length : text-string */
	if (part->content_location != NULL) {
		cloc_len = strlen(part->content_location);
		if (part->content_location[0] & 0x80) cloc_len++;
		len += cloc_len + 2;
	} else
		cloc_len = 0;

	/* Compute content-disposition length */
	if (!is_smil && part->content_location != NULL) {
		cd_len = 2 + cloc_len + 1;
		if (wsp_encode_value_length(cd_len, cd_val, MAX_ENC_VALUE_BYTES,
							&cd_len) == FALSE)
			goto done;
		len += 1 + cd_len + 2 + cloc_len + 1;
	} else {
		cd_len = 0;
	}

	/* Encode total headers length */
	if (fb_put_uintvar(fb, len) == FALSE)
		goto done;

	/* Encode data length */
	if (fb_put_uintvar(fb, part->length) == FALSE)
		goto done;

	/* Encode content-type */
	ptr = fb_request(fb, ctp_len);
	if (ptr == NULL)
		goto done;

	memcpy(ptr, &ctp_val, ctp_len);

	ptr = fb_request(fb, ct_len);
	if (ptr == NULL)
		goto done;

	if (ct_len == 1)
		ptr[0] = ct | 0x80;
	else
		strcpy(ptr, ct_str);

	/* Encode "charset" param */
	if (cs_len > 0) {
		ptr = fb_request_field(fb, WSP_PARAMETER_TYPE_CHARSET, cs_len);
		if (ptr == NULL)
			goto done;

		memcpy(ptr, &cs_val, cs_len);
	}

	/* Encode content-id */
	if (part->content_id != NULL) {
		if (encode_quoted_string(fb, MMS_PART_HEADER_CONTENT_ID,
						&part->content_id) == FALSE)
			goto done;
	}

	/* Encode content-location */
	if (part->content_location != NULL) {
		if (encode_text(fb, MMS_PART_HEADER_CONTENT_LOCATION,
					&part->content_location) == FALSE)
			goto done;
	}

	/* Encode content-disposition */
	if (cd_len) {
		ptr = fb_request_field(fb, MMS_PART_HEADER_CONTENT_DISPOSITION,
							cd_len + cloc_len + 3);
		if (ptr == NULL)
			goto done;

		memcpy(ptr, &cd_val, cd_len);
		ptr += cd_len;
		*ptr++ = 0x82; /* Inline = <Octet 130> */
		*ptr++ = WSP_PARAMETER_TYPE_FILENAME_DEFUNCT | 0x80;
		strcpy(ptr, part->content_location);
		ptr[cloc_len] = 0;
	}

	ok = TRUE;

done:
	g_strfreev(parsed);
	return ok;
}

static gboolean mms_encode_send_req_part(struct mms_attachment *part,
						struct file_buffer *fb)
{
	if (mms_encode_send_req_part_header(part, fb) == FALSE)
		return FALSE;

	part->offset = fb_get_file_size(fb);

	return fb_copy(fb, part->data, part->length);
}

static gboolean mms_encode_headers(struct file_buffer *fb,
					enum mms_header orig_header, ...)
{
	va_list args;
	void *data;
	enum mms_header header;
	header_encoder encoder;

	va_start(args, orig_header);
	header = orig_header;

	while (header != MMS_HEADER_INVALID) {
		data = va_arg(args, void *);

		encoder = encoder_for_type(header);
		if (encoder == NULL)
			return FALSE;

		if (data && encoder(fb, header, data) == FALSE)
			return FALSE;

		header = va_arg(args, enum mms_header);
	}

	va_end(args);

	return TRUE;
}

static gboolean mms_encode_notify_resp_ind(struct mms_message *msg,
							struct file_buffer *fb)
{
	if (mms_encode_headers(fb, MMS_HEADER_MESSAGE_TYPE, &msg->type,
				MMS_HEADER_TRANSACTION_ID, &msg->transaction_id,
				MMS_HEADER_MMS_VERSION, &msg->version,
				MMS_HEADER_STATUS, &msg->nri.notify_status,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	return fb_flush(fb);
}


static gboolean mms_encode_acknowledge_ind(struct mms_message *msg,
							struct file_buffer *fb)
{
	if (mms_encode_headers(fb, MMS_HEADER_MESSAGE_TYPE, &msg->type,
				MMS_HEADER_TRANSACTION_ID, &msg->transaction_id,
				MMS_HEADER_MMS_VERSION, &msg->version,
				MMS_HEADER_REPORT_ALLOWED, &msg->ai.report,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	return fb_flush(fb);
}

static gboolean mms_encode_read_rec_ind(struct mms_message *msg,
							struct file_buffer *fb)
{
	const char *empty_from = "";
	if (mms_encode_headers(fb, MMS_HEADER_MESSAGE_TYPE, &msg->type,
				MMS_HEADER_MMS_VERSION, &msg->version,
				MMS_HEADER_MESSAGE_ID, &msg->ri.msgid,
				MMS_HEADER_TO, &msg->ri.to,
				MMS_HEADER_FROM, &empty_from,
				MMS_HEADER_DATE, &msg->ri.date,
				MMS_HEADER_READ_STATUS, &msg->ri.rr_status,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	return fb_flush(fb);
}

static gboolean mms_encode_send_req(struct mms_message *msg,
							struct file_buffer *fb)
{
	const char *empty_from = "";
	GSList *item;

	if (mms_encode_headers(fb, MMS_HEADER_MESSAGE_TYPE, &msg->type,
				MMS_HEADER_TRANSACTION_ID, &msg->transaction_id,
				MMS_HEADER_MMS_VERSION, &msg->version,
				MMS_HEADER_FROM, &empty_from,
				MMS_HEADER_TO, &msg->sr.to,
				MMS_HEADER_DELIVERY_REPORT, &msg->sr.dr,
				MMS_HEADER_CONTENT_TYPE, &msg->sr.content_type,
				MMS_HEADER_INVALID) == FALSE)
		return FALSE;

	if (msg->attachments == NULL)
		goto done;

	if (fb_put_uintvar(fb, g_slist_length(msg->attachments)) == FALSE)
		return FALSE;

	for (item = msg->attachments; item != NULL; item = g_slist_next(item)) {
		if (mms_encode_send_req_part(item->data, fb) == FALSE)
			return FALSE;
	}

done:
	return fb_flush(fb);
}

gboolean mms_message_encode(struct mms_message *msg, int fd)
{
	struct file_buffer fb;

	fb_init(&fb, fd);

	switch (msg->type) {
	case MMS_MESSAGE_TYPE_SEND_REQ:
		return mms_encode_send_req(msg, &fb);
	case MMS_MESSAGE_TYPE_SEND_CONF:
	case MMS_MESSAGE_TYPE_NOTIFICATION_IND:
		return FALSE;
	case MMS_MESSAGE_TYPE_NOTIFYRESP_IND:
		return mms_encode_notify_resp_ind(msg, &fb);
	case MMS_MESSAGE_TYPE_RETRIEVE_CONF:
		return FALSE;
	case MMS_MESSAGE_TYPE_ACKNOWLEDGE_IND:
		return mms_encode_acknowledge_ind(msg, &fb);
	case MMS_MESSAGE_TYPE_DELIVERY_IND:
		return FALSE;
	case MMS_MESSAGE_TYPE_READ_REC_IND:
		return mms_encode_read_rec_ind(msg, &fb);
	case MMS_MESSAGE_TYPE_READ_ORIG_IND:
		return FALSE;
	}

	return FALSE;
}
