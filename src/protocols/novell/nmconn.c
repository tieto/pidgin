/*
 * nmconn.c
 *
 * Copyright © 2004 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK OF NOVELL, INC. NO PART OF THIS WORK MAY BE
 * USED, PRACTICED, PERFORMED, COPIED, DISTRIBUTED, REVISED, MODIFIED,
 * TRANSLATED, ABRIDGED, CONDENSED, EXPANDED, COLLECTED, COMPILED, LINKED,
 * RECAST, TRANSFORMED OR ADAPTED WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL,
 * INC. ANY USE OR EXPLOITATION OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 * 
 * AS BETWEEN [GAIM] AND NOVELL, NOVELL GRANTS [GAIM] THE RIGHT TO REPUBLISH
 * THIS WORK UNDER THE GPL (GNU GENERAL PUBLIC LICENSE) WITH ALL RIGHTS AND
 * LICENSES THEREUNDER.  IF YOU HAVE RECEIVED THIS WORK DIRECTLY OR INDIRECTLY
 * FROM [GAIM] AS PART OF SUCH A REPUBLICATION, YOU HAVE ALL RIGHTS AND LICENSES
 * GRANTED BY [GAIM] UNDER THE GPL.  IN CONNECTION WITH SUCH A REPUBLICATION, IF
 * ANYTHING IN THIS NOTICE CONFLICTS WITH THE TERMS OF THE GPL, SUCH TERMS
 * PREVAIL.
 *
 */

#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "nmconn.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define NO_ESCAPE(ch) ((ch == 0x20) || (ch >= 0x30 && ch <= 0x39) || \
					(ch >= 0x41 && ch <= 0x5a) || (ch >= 0x61 && ch <= 0x7a))

/* Read data from conn until the end of a line */
static NMERR_T
read_line(NMConn * conn, char *buff, int len)
{
	NMERR_T rc = NM_OK;
	int total_bytes = 0;

	while ((rc == NM_OK) && (total_bytes < (len - 1))) {
		rc = nm_read_all(conn, &buff[total_bytes], 1);
		if (rc == NM_OK) {
			total_bytes += 1;
			if (buff[total_bytes - 1] == '\n') {
				break;
			}
		}
	}
	buff[total_bytes] = '\0';

	return rc;
}

static char *
url_escape_string(char *src)
{
	guint32 escape = 0;
	char *p;
	char *q;
	char *encoded = NULL;
	int ch;

	static const char hex_table[16] = "0123456789abcdef";

	if (src == NULL) {
		return NULL;
	}

	/* Find number of chars to escape */
	for (p = src; *p != '\0'; p++) {
		ch = (guchar) *p;
		if (!NO_ESCAPE(ch)) {
			escape++;
		}
	}

	encoded = g_malloc((p - src) + (escape * 2) + 1);

	/* Escape the string */
	for (p = src, q = encoded; *p != '\0'; p++) {
		ch = (guchar) * p;
		if (NO_ESCAPE(ch)) {
			if (ch != 0x20) {
				*q = ch;
				q++;
			} else {
				*q = '+';
				q++;
			}
		} else {
			*q = '%';
			q++;
			
			*q = hex_table[ch >> 4];
			q++;
			
			*q = hex_table[ch & 15];
			q++;
		}
	}
	*q = '\0';

	return encoded;
}

static char *
encode_method(guint8 method)
{
	char *str;

	switch (method) {
		case NMFIELD_METHOD_EQUAL:
			str = "G";
			break;
		case NMFIELD_METHOD_UPDATE:
			str = "F";
			break;
		case NMFIELD_METHOD_GTE:
			str = "E";
			break;
		case NMFIELD_METHOD_LTE:
			str = "D";
			break;
		case NMFIELD_METHOD_NE:
			str = "C";
			break;
		case NMFIELD_METHOD_EXIST:
			str = "B";
			break;
		case NMFIELD_METHOD_NOTEXIST:
			str = "A";
			break;
		case NMFIELD_METHOD_SEARCH:
			str = "9";
			break;
		case NMFIELD_METHOD_MATCHBEGIN:
			str = "8";
			break;
		case NMFIELD_METHOD_MATCHEND:
			str = "7";
			break;
		case NMFIELD_METHOD_NOT_ARRAY:
			str = "6";
			break;
		case NMFIELD_METHOD_OR_ARRAY:
			str = "5";
			break;
		case NMFIELD_METHOD_AND_ARRAY:
			str = "4";
			break;
		case NMFIELD_METHOD_DELETE_ALL:
			str = "3";
			break;
		case NMFIELD_METHOD_DELETE:
			str = "2";
			break;
		case NMFIELD_METHOD_ADD:
			str = "1";
			break;
		default:					/* NMFIELD_METHOD_VALID */
			str = "0";
			break;
	}
	
	return str;
}

int
nm_tcp_write(NMConn * conn, const void *buff, int len)
{
	if (conn == NULL || buff == NULL)
		return -1;

	if (!conn->use_ssl)
		return (write(conn->fd, buff, len));
	else if (conn->ssl_conn && conn->ssl_conn->write)
		return (conn->ssl_conn->write(conn->ssl_conn->data, buff, len));
	else
		return -1;
}

int
nm_tcp_read(NMConn * conn, void *buff, int len)
{
	if (conn == NULL || buff == NULL)
		return -1;

	if (!conn->use_ssl)
		return (read(conn->fd, buff, len));
	else if (conn->ssl_conn && conn->ssl_conn->read)
		return (conn->ssl_conn->read(conn->ssl_conn->data, buff, len));
	else
		return -1;
}

NMERR_T
nm_read_all(NMConn * conn, char *buff, int len)
{
	NMERR_T rc = NM_OK;
	int bytes_left = len;
	int bytes_read;
	int total_bytes = 0;
	int retry = 10;

	if (conn == NULL || buff == NULL)
		return NMERR_BAD_PARM;

	/* Keep reading until buffer is full */
	while (bytes_left) {
		bytes_read = nm_tcp_read(conn, &buff[total_bytes], bytes_left);
		if (bytes_read > 0) {
			bytes_left -= bytes_read;
			total_bytes += bytes_read;
		} else {
			if (errno == EAGAIN) {
				if (--retry == 0) {
					rc = NMERR_TCP_READ;
					break;
				}
#ifdef _WIN32
				Sleep(1000);
#else
				usleep(1000);
#endif
			} else {
				rc = NMERR_TCP_READ;
				break;
			}
		}
	}
	return rc;
}

NMERR_T
nm_write_fields(NMConn * conn, NMField * fields)
{
	NMERR_T rc = NM_OK;
	NMField *field;
	char *value = NULL;
	char *method = NULL;
	char buffer[512];
	int ret;
	int bytes_to_send;
	int val = 0;

	if (conn == NULL || fields == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Format each field as valid "post" data and write it out */
	for (field = fields; (rc == NM_OK) && (field->tag); field++) {

		/* We don't currently handle binary types */
		if (field->method == NMFIELD_METHOD_IGNORE ||
			field->type == NMFIELD_TYPE_BINARY) {
			continue;
		}

		/* Write the field tag */
		bytes_to_send = g_snprintf(buffer, sizeof(buffer), "&tag=%s", field->tag);
		ret = nm_tcp_write(conn, buffer, bytes_to_send);
		if (ret < 0) {
			rc = NMERR_TCP_WRITE;
		}

		/* Write the field method */
		if (rc == NM_OK) {
			method = encode_method(field->method);
			bytes_to_send = g_snprintf(buffer, sizeof(buffer), "&cmd=%s", method);
			ret = nm_tcp_write(conn, buffer, bytes_to_send);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}

		/* Write the field value */
		if (rc == NM_OK) {
			switch (field->type) {
				case NMFIELD_TYPE_UTF8:
				case NMFIELD_TYPE_DN:
					
					value = url_escape_string((char *) field->value);
					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%s", value);
					ret = nm_tcp_write(conn, buffer, bytes_to_send);
					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}
					
					g_free(value);
					
					break;
					
				case NMFIELD_TYPE_ARRAY:
				case NMFIELD_TYPE_MV:
					
					val = nm_count_fields((NMField *) field->value);
					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%u", val);
					ret = nm_tcp_write(conn, buffer, bytes_to_send);
					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}
					
					break;
					
				default:
					
					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%u", field->value);
					ret = nm_tcp_write(conn, buffer, bytes_to_send);
					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}
					
					break;
			}
		}
		
		/* Write the field type */
		if (rc == NM_OK) {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer),
									   "&type=%u", field->type);
			ret = nm_tcp_write(conn, buffer, bytes_to_send);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}

		/* If the field is a sub array then post its fields */
		if (rc == NM_OK && val > 0) {
			if (field->type == NMFIELD_TYPE_ARRAY ||
				field->type == NMFIELD_TYPE_MV) {

				rc = nm_write_fields(conn, (NMField *) field->value);

			}
		}
	}

	return rc;
}

NMERR_T
nm_send_request(NMConn * conn, char *cmd, NMField * fields, NMRequest ** req)
{
	NMERR_T rc = NM_OK;
	char buffer[512];
	int bytes_to_send;
	int ret;
	NMField *request = NULL;

	if (conn == NULL || cmd == NULL)
		return NMERR_BAD_PARM;

	/* Write the post */
	bytes_to_send = g_snprintf(buffer, sizeof(buffer),
							   "POST /%s HTTP/1.0\r\n", cmd);
	ret = nm_tcp_write(conn, buffer, bytes_to_send);
	if (ret < 0) {
		rc = NMERR_TCP_WRITE;
	}

	/* Write headers */
	if (rc == NM_OK) {
		if (strcmp("login", cmd) == 0) {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer),
									   "Host: %s:%d\r\n\r\n", conn->addr, conn->port);
			ret = nm_tcp_write(conn, buffer, bytes_to_send);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		} else {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer), "\r\n");
			ret = nm_tcp_write(conn, buffer, bytes_to_send);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}
	}

	/* Add the transaction id to the request fields */
	if (rc == NM_OK) {
		request = nm_copy_field_array(fields);
		if (request) {
			char *str = g_strdup_printf("%d", ++(conn->trans_id));
			
			request = nm_add_field(request, NM_A_SZ_TRANSACTION_ID, 0,
								   NMFIELD_METHOD_VALID, 0,
								   (guint32) str, NMFIELD_TYPE_UTF8);
		}
	}

	/* Send the request to the server */
	if (rc == NM_OK) {
		rc = nm_write_fields(conn, request);
	}

	/* Write the CRLF to terminate the data */
	if (rc == NM_OK) {
		ret = nm_tcp_write(conn, "\r\n", strlen("\r\n"));
		if (ret < 0) {
			rc = NMERR_TCP_WRITE;
		}
	}

	/* Create a request struct and return it */
	if (rc == NM_OK) {
		*req = nm_create_request(cmd, conn->trans_id, time(0));
	}

	if (request != NULL) {
		nm_free_fields(&request);
	}

	return rc;
}

NMERR_T
nm_read_header(NMConn * conn)
{
	NMERR_T rc = NM_OK;
	char buffer[512];
	char *ptr = NULL;
	int i;
	char rtn_buf[8];
	int rtn_code = 0;

	if (conn == NULL)
		return NMERR_BAD_PARM;

	*buffer = '\0';
	rc = read_line(conn, buffer, sizeof(buffer));
	if (rc == NM_OK) {

		/* Find the return code */
		ptr = strchr(buffer, ' ');
		if (ptr != NULL) {
			ptr++;

			i = 0;
			while (isdigit(*ptr) && (i < 3)) {
				rtn_buf[i] = *ptr;
				i++;
				ptr++;
			}
			rtn_buf[i] = '\0';
			
			if (i > 0)
				rtn_code = atoi(rtn_buf);
		}
	}

	/* Finish reading header, in the future we might want to do more processing here */
	/* TODO: handle more general redirects in the future */
	while ((rc == NM_OK) && (strcmp(buffer, "\r\n") != 0)) {
		rc = read_line(conn, buffer, sizeof(buffer));
	}

	if (rc == NM_OK && rtn_code == 301) {
		conn->use_ssl = TRUE;
		conn->redirect = TRUE;
		rc = NMERR_SSL_REDIRECT;
	}

	return rc;
}

NMERR_T
nm_read_fields(NMConn * conn, int count, NMField ** fields)
{
	NMERR_T rc = NM_OK;
	guint8 type;
	guint8 method;
	guint32 val;
	char tag[64];
	NMField *sub_fields = NULL;
	char *str = NULL;

	if (conn == NULL || fields == NULL)
		return NMERR_BAD_PARM;

	do {
		if (count !=  -1) {
			count--;
		}

		/* Read the field type, method, and tag */
		rc = nm_read_all(conn, &type, sizeof(type));
		if (rc != NM_OK || type == 0)
			break;

		rc = nm_read_all(conn, &method, sizeof(method));
		if (rc != NM_OK)
			break;

		rc = nm_read_all(conn, (char *) &val, sizeof(val));
		if (rc != NM_OK)
			break;

		if (val > sizeof(tag)) {
			rc = NMERR_PROTOCOL;
			break;
		}

		rc = nm_read_all(conn, tag, val);
		if (rc != NM_OK)
			break;

		if (type == NMFIELD_TYPE_MV || type == NMFIELD_TYPE_ARRAY) {

			/* Read the subarray (first read the number of items in the array) */
			rc = nm_read_all(conn, (char *) &val, sizeof(val));
			if (rc != NM_OK)
				break;

			if (val > 0) {
				rc = nm_read_fields(conn, val, &sub_fields);
				if (rc != NM_OK)
					break;
			}

			*fields = nm_add_field(*fields, tag, 0, method, 0,
								   (guint32) sub_fields, type);

			sub_fields = NULL;

		} else if (type == NMFIELD_TYPE_UTF8 || type == NMFIELD_TYPE_DN) {

			/* Read the string (first read the length) */
			rc = nm_read_all(conn, (char *) &val, sizeof(val));
			if (rc != NM_OK)
				break;

			if (val > 0) {
				str = g_new0(char, val + 1);

				rc = nm_read_all(conn, str, val);
				if (rc != NM_OK)
					break;
			}

			*fields = nm_add_field(*fields, tag, 0, method, 0,
								   (guint32) str, type);
			str = NULL;
		} else {

			/* Read the numerical value */
			rc = nm_read_all(conn, (char *) &val, sizeof(val));
			if (rc != NM_OK)
				break;

			*fields = nm_add_field(*fields, tag, 0, method, 0, val, type);
		}

	} while ((type != 0) && (count != 0));


	if (str != NULL) {
		g_free(str);
	}

	if (sub_fields != NULL) {
		nm_free_fields(&sub_fields);
	}

	return rc;
}

void
nm_conn_add_request_item(NMConn * conn, NMRequest * request)
{
	if (conn == NULL || request == NULL)
		return;

	nm_request_add_ref(request);
	conn->requests = g_slist_append(conn->requests, request);
}

void
nm_conn_remove_request_item(NMConn * conn, NMRequest * request)
{
	if (conn == NULL || request == NULL)
		return;

	conn->requests = g_slist_remove(conn->requests, request);
	nm_release_request(request);
}

NMRequest *
nm_conn_find_request(NMConn * conn, int trans_id)
{
	NMRequest *req = NULL;
	GSList *itr = NULL;

	if (conn == NULL)
		return NULL;

	itr = conn->requests;
	while (itr) {
		req = (NMRequest *) itr->data;
		if (req != NULL && nm_request_get_trans_id(req) == trans_id) {
			return req;
		}
		itr = g_slist_next(itr);
	}
	return NULL;
}

const char *
nm_conn_get_addr(NMConn * conn)
{
	if (conn == NULL)
		return NULL;
	else
		return conn->addr;
}

int
nm_conn_get_port(NMConn * conn)
{
	if (conn == NULL)
		return -1;
	else
		return conn->port;
}
