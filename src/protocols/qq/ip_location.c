/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This code is based on
 *  * qqwry.c, by lvxiang <srm2003@163.com>
 *  * IpChecker.m, by Jeff_Ye
 */

// START OF FILE
/*****************************************************************************/
#include "internal.h"
#include <string.h>		// memset
#include "debug.h"
#include "prefs.h"		// gaim_prefs_get_string

#include "utils.h"
#include "ip_location.h"

#define DEFAULT_IP_LOCATION_FILE  "gaim/QQWry.dat"

typedef struct _ip_finder ip_finder;

// all offset is based the begining of the file
struct _ip_finder {
	guint32 offset_first_start_ip;	// first abs offset of start ip
	guint32 offset_last_start_ip;	// last abs offset of start ip
	guint32 cur_start_ip;	// start ip of current search range
	guint32 cur_end_ip;	// end ip of current search range
	guint32 offset_cur_end_ip;	// where is the current end ip saved
	GIOChannel *io;		// IO Channel to read file
};				// struct _ip_finder

/*****************************************************************************/
// convert 1-4 bytes array to integer.
// Small endian (higher bytes in lower place)
static guint32 _byte_array_to_int(guint8 * ip, gint count)
{
	guint32 ret, i;
	g_return_val_if_fail(count >= 1 && count <= 4, 0);
	ret = ip[0];
	for (i = 0; i < count; i++)
		ret |= ((guint32) ip[i]) << (8 * i);
	return ret;
}				// _byte_array_to_int

/*****************************************************************************/
// read len of bytes to buf, from io at offset
static void _read_from(GIOChannel * io, guint32 offset, guint8 * buf, gint len)
{
	GError *err;
	GIOStatus status;

	err = NULL;
	status = g_io_channel_seek_position(io, offset, G_SEEK_SET, &err);
	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail seek file @offset[%d]: %s", offset, err->message);
		g_error_free(err);
		memset(buf, 0, len);
		return;
	}			// if err

	status = g_io_channel_read_chars(io, buf, len, NULL, &err);
	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail read %d bytes from file: %s", len, err->message);
		g_error_free(err);
		memset(buf, 0, len);
		return;
	}			// if err
}				// _read_from

/*****************************************************************************/
// read len of bytes to buf, from io at offset
static gsize _read_line_from(GIOChannel * io, guint32 offset, gchar ** ret_str)
{
	gsize bytes_read;
	GError *err;
	GIOStatus status;

	err = NULL;
	status = g_io_channel_seek_position(io, offset, G_SEEK_SET, &err);
	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail seek file @offset[%d]: %s", offset, err->message);
		g_error_free(err);
		ret_str = NULL;
		return -1;
	}			// if err

	status = g_io_channel_read_line(io, ret_str, &bytes_read, NULL, &err);
	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail read from file: %s", err->message);
		g_error_free(err);
		ret_str = NULL;
		return -1;
	}			// if err

	return bytes_read;
}				// _read_from

/*****************************************************************************/
// read the string from io, at offset, it may jump several times
// returns the offset of next readable string for area
static guint32 _get_string(GIOChannel * io, guint32 offset, gchar ** ret)
{
	guint8 *buf;
	g_return_val_if_fail(io != NULL, 0);

	buf = g_new0(guint8, 3);
	_read_from(io, offset, buf, 1);

	switch (buf[0]) {	/* fixed by bluestar11 at gmail dot com, 04/12/20 */
	case 0x01:		/* jump together */
	  _read_from(io, offset + 1, buf, 3);
	  return _get_string(io, _byte_array_to_int(buf, 3), ret);
	case 0x02:		/* jump separately */
	   _read_from(io, offset + 1, buf, 3);
	  _get_string(io, _byte_array_to_int(buf, 3), ret);
	  return offset + 4;
	default:
	  _read_line_from(io, offset, ret);
	  return offset + strlen(*ret) + 1;
	} /* switch */
}				// _get_string

/*****************************************************************************/
// extract country and area starting from offset
static void _get_country_city(GIOChannel * io, guint32 offset, gchar ** country, gchar ** area)
{
	guint32 next_offset;
	g_return_if_fail(io != NULL);

	next_offset = _get_string(io, offset, country);
	if (next_offset == 0)
		*area = g_strdup("");
	else
		_get_string(io, next_offset, area);
}				// _get_country_city

/*****************************************************************************/
// set start_ip and end_ip of current range
static void _set_ip_range(gint rec_no, ip_finder * f)
{
	guint8 *buf;
	guint32 offset;

	g_return_if_fail(f != NULL);

	buf = g_newa(guint8, 7);
	offset = f->offset_first_start_ip + rec_no * 7;

	_read_from(f->io, offset, buf, 7);
	f->cur_start_ip = _byte_array_to_int(buf, 4);
	f->offset_cur_end_ip = _byte_array_to_int(buf + 4, 3);

	_read_from(f->io, f->offset_cur_end_ip, buf, 4);
	f->cur_end_ip = _byte_array_to_int(buf, 4);

}				// _set_ip_range

/*****************************************************************************/
// set the country and area for given IP.
// country and area needs to be freed later
gboolean qq_ip_get_location(guint32 ip, gchar ** country, gchar ** area)
{
	gint rec, record_count, B, E;
	guint8 *buf;
	gchar *addr_file;
	ip_finder *f;
	GError *err;
	const char *ip_fn;

	if (ip == 0)
		return FALSE;

	f = g_new0(ip_finder, 1);
	err = NULL;
	ip_fn = gaim_prefs_get_string("/plugins/prpl/qq/ipfile");
	if (ip_fn == NULL || strlen(ip_fn) == 0 || strncmp(ip_fn, "(null)", strlen("(null)")) == 0) {
		addr_file = g_build_filename(DATADIR, DEFAULT_IP_LOCATION_FILE, NULL);
	} else {
		addr_file = g_build_filename(ip_fn, NULL);
	}

	f->io = g_io_channel_new_file(addr_file, "r", &err);
	g_free(addr_file);
	if (err != NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Unable to open (%s): %s\n", addr_file, err->message);
		g_error_free(err);
		return FALSE;
	} else
		g_io_channel_set_encoding(f->io, NULL, NULL);	// set binary

	buf = g_newa(guint8, 4);

	_read_from(f->io, 0, buf, 4);
	f->offset_first_start_ip = _byte_array_to_int(buf, 4);
	_read_from(f->io, 4, buf, 4);
	f->offset_last_start_ip = _byte_array_to_int(buf, 4);

	record_count = (f->offset_last_start_ip - f->offset_first_start_ip) / 7;
	if (record_count <= 1) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "File data error, no records found\n");
		g_io_channel_shutdown(f->io, FALSE, NULL);
		return FALSE;;
	}
	// search for right range
	B = 0;
	E = record_count;
	while (B < E - 1) {
		rec = (B + E) / 2;
		_set_ip_range(rec, f);
		if (ip == f->cur_start_ip) {
			B = rec;
			break;
		}
		if (ip > f->cur_start_ip)
			B = rec;
		else
			E = rec;
	}			// while
	_set_ip_range(B, f);

	if (f->cur_start_ip <= ip && ip <= f->cur_end_ip) {
		_get_country_city(f->io, f->offset_cur_end_ip + 4, country, area);
	} else {		// not in this range... miss
		*country = g_strdup("unkown");
		*area = g_strdup(" ");
	}			// if ip_start<=ip<=ip_end

	g_io_channel_shutdown(f->io, FALSE, NULL);
	return TRUE;

}				// qq_ip_get_location

/*****************************************************************************/
// END OF FILE
