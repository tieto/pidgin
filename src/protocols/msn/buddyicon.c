/**
 * @file buddyicon.c Buddy icon support
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
 */
#include "msn.h"
#include "buddyicon.h"

#define PACKET_LENGTH 1500

#define MAX_BUDDY_ICON_FILE_SIZE 8192

static const char alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static char *
base64_enc(const char *data, int len)
{
	char *dest;
	char *buf;

	buf = dest = g_malloc(4 * len / 3 + 4);

	/* Encode 3 bytes at a time */
	while (len >= 3) {
		buf[0] = alphabet[(data[0] >> 2) & 0x3F];
		buf[1] = alphabet[((data[0] << 4) & 0x30) | ((data[1] >> 4) & 0x0F)];
		buf[2] = alphabet[((data[1] << 2) & 0x3C) | ((data[2] >> 6) & 0x03)];
		buf[3] = alphabet[data[2] & 0x3F];
		data += 3;
		buf += 4;
		len -= 3;
	}

	if (len > 0) {
		buf[0] = alphabet[(data[0] >> 2) & 0x3F];
		buf[1] = alphabet[(data[0] << 4) & 0x30];

		if (len > 1) {
			buf[1] += (data[1] >> 4) & 0x0F;
			buf[2] = alphabet[(data[1] << 2) & 0x3C];
		}

		else
			buf[2] = '=';

		buf[3] = '=';
		buf += 4;
	}

	*buf = '\0';

	return dest;
}

static gboolean
get_buddy_icon_info(GaimAccount *account, char **base64,
					char **md5sum, int *file_size, int *base64_size)
{
	FILE *fp;
	struct stat sb;
	md5_state_t st;
	md5_byte_t di[16];
	const char *icon;

	if (base64      != NULL) *base64      = NULL;
	if (md5sum      != NULL) *md5sum      = NULL;
	if (file_size   != NULL) *file_size   = 0;
	if (base64_size != NULL) *base64_size = 0;

	icon = gaim_account_get_buddy_icon(account);

	if (!stat(icon, &sb)) {
		if (file_size != NULL)
			*file_size = sb.st_size;

		if ((fp = fopen(icon, "rb")) != NULL) {
			char *buf = g_malloc(sb.st_size + 1);
			char *temp;

			fread(buf, 1, sb.st_size, fp);

			buf[sb.st_size] = '\0';

			temp = base64_enc(buf, sb.st_size);

			if (base64_size != NULL)
				*base64_size = strlen(temp);

			if (base64 != NULL)
				*base64 = temp;
			else
				g_free(temp);

			if (md5sum != NULL) {
				char buf2[3];
				int i;

				md5_init(&st);
				md5_append(&st, (const md5_byte_t *)buf, sb.st_size);
				md5_finish(&st, di);

				*md5sum = g_new0(char, 33);

				for (i = 0; i < 16; i++) {
					g_snprintf(buf2, sizeof(buf2), "%02x", di[i]);
					strcat(*md5sum, buf2);
				}
			}

			g_free(buf);

			fclose(fp);
		}
		else {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Cannot open buddy icon file!\n");

			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
send_icon_data(MsnSwitchBoard *swboard, MsnBuddyIconXfer *buddyicon)
{
	GaimConnection *gc = swboard->servconn->session->account->gc;
	char buf[MSN_BUF_LEN];
	MsnMessage *msg;
	int len;

	len = MIN(PACKET_LENGTH - 4,
			  buddyicon->total_size - buddyicon->bytes_xfer);

	strcpy(buf, "ICON");

	strncat(buf, buddyicon->data + buddyicon->bytes_xfer, len);

	msg = msn_message_new();
	msn_message_set_content_type(msg, "application/x-buddyicon");
	msn_message_set_receiver(msg, buddyicon->user);
	msn_message_set_charset(msg, NULL);
	msn_message_set_attr(msg, "User-Agent", NULL);

	msn_message_set_body(msg, buf);

	if (!msn_switchboard_send_msg(swboard, msg)) {
		msn_message_destroy(msg);

		msn_buddy_icon_xfer_destroy(swboard->buddy_icon_xfer);
		swboard->buddy_icon_xfer = NULL;

		gaim_connection_error(gc, _("Write error"));

		return FALSE;
	}

	msn_message_destroy(msg);

	buddyicon->bytes_xfer += len;

	if (buddyicon->bytes_xfer == buddyicon->total_size) {
		msg = msn_message_new();
		msn_message_set_content_type(msg, "application/x-buddyicon");
		msn_message_set_receiver(msg, buddyicon->user);
		msn_message_set_charset(msg, NULL);
		msn_message_set_attr(msg, "User-Agent", NULL);

		msn_message_set_body(msg, "Command: COMPLETE\r\n");

		msn_switchboard_send_msg(swboard, msg);

		msn_buddy_icon_xfer_destroy(swboard->buddy_icon_xfer);
		swboard->buddy_icon_xfer = NULL;
	}

	return TRUE;
}

static gboolean
process_invite(MsnServConn *servconn, const MsnMessage *msg)
{
	MsnSession *session = servconn->session;
	GaimConnection *gc = session->account->gc;
	MsnMessage *new_msg;
	MsnSwitchBoard *swboard;
	MsnBuddyIconXfer *buddyicon;
	struct buddy *b;
	GHashTable *table;
	const char *command;

	table = msn_message_get_hashtable_from_body(msg);

	command = g_hash_table_lookup(table, "Command");

	if (command == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Missing Command from buddy icon message.\n");
		return TRUE;
	}

	if (!strcmp(command, "INVITE")) {
		MsnUser *user;
		const char *md5sum = g_hash_table_lookup(table, "MD5SUM");
		const char *size_s = g_hash_table_lookup(table, "File-Size");
		const char *base64_size_s = g_hash_table_lookup(table, "Base64-Size");
		const char *passport;

		if (md5sum == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Missing MD5SUM from buddy icon message.\n");

			return TRUE;
		}

		if (size_s == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Missing File-Size from buddy icon message.\n");

			return TRUE;
		}

		if (base64_size_s == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Missing Bas64-Size from buddy icon message.\n");

			return TRUE;
		}

		if (atoi(size_s) > MAX_BUDDY_ICON_FILE_SIZE) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "User tried to send a buddy icon over 8KB! "
					   "Not accepting.");
			return TRUE;
		}

		user = msn_message_get_sender(msg);

		passport = msn_user_get_passport(user);

		/* See if we actually need a new icon. */
		if ((b = gaim_find_buddy(gc->account, passport)) != NULL) {
			const char *cur_md5sum;

			cur_md5sum = gaim_buddy_get_setting(b, "icon_checksum");

			if (cur_md5sum != NULL && !strcmp(cur_md5sum, md5sum))
				return TRUE;
		}

		/* Send a request for transfer. */
		new_msg = msn_message_new();
		msn_message_set_content_type(new_msg, "application/x-buddyicon");
		msn_message_set_receiver(new_msg, user);
		msn_message_set_charset(new_msg, NULL);
		msn_message_set_attr(new_msg, "User-Agent", NULL);

		msn_message_set_body(new_msg, "Command: REQUEST\r\n");

		if ((swboard = msn_session_open_switchboard(session)) == NULL) {
			msn_message_destroy(new_msg);

			gaim_connection_error(gc, _("Write error"));

			return FALSE;
		}

		swboard->hidden = TRUE;
		msn_switchboard_set_user(swboard, user);
		msn_switchboard_send_msg(swboard, new_msg);

		msn_message_destroy(new_msg);

		buddyicon = swboard->buddy_icon_xfer = msn_buddy_icon_xfer_new();

		buddyicon->user = user;
		msn_user_ref(buddyicon->user);

		buddyicon->md5sum     = g_strdup(md5sum);
		buddyicon->total_size = atoi(base64_size_s);
		buddyicon->file_size  = atoi(size_s);

		buddyicon->data = g_malloc(buddyicon->total_size + 1);
	}
	else if (!strcmp(command, "REQUEST")) {
		swboard = (MsnSwitchBoard *)servconn->data;

		swboard->hidden = TRUE;

		swboard->buddy_icon_xfer = buddyicon = msn_buddy_icon_xfer_new();

		if (!get_buddy_icon_info(gc->account,
								 &buddyicon->data,
								 &buddyicon->md5sum,
								 &buddyicon->file_size,
								 &buddyicon->total_size)) {

			msn_buddy_icon_xfer_destroy(buddyicon);

			new_msg = msn_message_new();
			msn_message_set_content_type(new_msg, "application/x-buddyicon");
			msn_message_set_receiver(new_msg, msn_message_get_sender(msg));
			msn_message_set_charset(new_msg, NULL);
			msn_message_set_attr(new_msg, "User-Agent", NULL);

			msn_message_set_body(new_msg, "Command: CANCEL\r\n");

			if ((swboard = msn_session_open_switchboard(session)) == NULL) {
				msn_message_destroy(new_msg);

				gaim_connection_error(gc, _("Write error"));

				return FALSE;
			}

			swboard->hidden = TRUE;

			msn_switchboard_send_msg(swboard, new_msg);

			msn_message_destroy(new_msg);

			msn_switchboard_destroy(swboard);
		}

		return send_icon_data(swboard, buddyicon);
	}
	else if (!strcmp(command, "ACK")) {
		swboard = (MsnSwitchBoard *)servconn->data;

		buddyicon = swboard->buddy_icon_xfer;

		if (buddyicon != NULL)
			return send_icon_data(swboard, buddyicon);
	}
	else if (!strcmp(command, "COMPLETE")) {
		const char *passport;
		char *icon;
		int icon_len;

		swboard = (MsnSwitchBoard *)servconn->data;

		buddyicon = swboard->buddy_icon_xfer;

		passport = msn_user_get_passport(buddyicon->user);
		swboard->hidden = TRUE;

		frombase64(buddyicon->data, &icon, &icon_len);

		if ((b = gaim_find_buddy(gc->account, passport)) != NULL) {
			gaim_buddy_set_setting(b, "icon_checksum", buddyicon->md5sum);
			gaim_blist_save();
		}

		set_icon_data(gc, passport, icon, icon_len);

		g_free(icon);

		msn_buddy_icon_xfer_destroy(swboard->buddy_icon_xfer);
		swboard->buddy_icon_xfer = NULL;

		msn_switchboard_destroy(swboard);
	}
	else if (!strcmp(command, "CANCEL")) {
		swboard = (MsnSwitchBoard *)servconn->data;

		msn_buddy_icon_xfer_destroy(swboard->buddy_icon_xfer);
		swboard->buddy_icon_xfer = NULL;

		msn_switchboard_destroy(swboard);
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Unknown buddy icon message command: %s\n", command);
	}

	return TRUE;
}

static gboolean
process_data(MsnServConn *servconn, const MsnMessage *msg)
{
	GaimConnection *gc = servconn->session->account->gc;
	MsnSwitchBoard *swboard;
	MsnBuddyIconXfer *buddyicon;
	MsnMessage *ack_msg;
	const char *data;
	int len;

	swboard = (MsnSwitchBoard *)servconn->data;
	buddyicon = swboard->buddy_icon_xfer;

	data = msn_message_get_body(msg) + 4;

	len = strlen(data);

	/* Copy the data into our buffer. */
	strncpy(buddyicon->data + buddyicon->bytes_xfer, data,
			buddyicon->total_size - buddyicon->bytes_xfer);

	buddyicon->bytes_xfer += len;

	/* Acknowledge this data. */
	ack_msg = msn_message_new();
	msn_message_set_content_type(ack_msg, "application/x-buddyicon");
	msn_message_set_receiver(ack_msg, msn_message_get_sender(msg));
	msn_message_set_charset(ack_msg, NULL);
	msn_message_set_attr(ack_msg, "User-Agent", NULL);
	msn_message_set_body(ack_msg, "Command: ACK\r\n");

	if (!msn_switchboard_send_msg(swboard, ack_msg)) {
		msn_message_destroy(ack_msg);

		msn_buddy_icon_xfer_destroy(swboard->buddy_icon_xfer);
		swboard->buddy_icon_xfer = NULL;

		gaim_connection_error(gc, _("Write error"));

		return FALSE;
	}

	msn_message_destroy(ack_msg);

	return TRUE;
}

MsnBuddyIconXfer *
msn_buddy_icon_xfer_new(void)
{
	return g_new0(MsnBuddyIconXfer, 1);
}

void
msn_buddy_icon_xfer_destroy(MsnBuddyIconXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (xfer->user != NULL)
		msn_user_unref(xfer->user);

	if (xfer->data != NULL)
		g_free(xfer->data);

	g_free(xfer);
}

gboolean
msn_buddy_icon_msg(MsnServConn *servconn, MsnMessage *msg)
{
	if (!strncmp(msn_message_get_body(msg), "ICON", 4))
		return process_data(servconn, msg);
	else
		return process_invite(servconn, msg);
}

void
msn_buddy_icon_invite(MsnSwitchBoard *swboard)
{
	GaimAccount *account = swboard->servconn->session->account;
	GaimConnection *gc = account->gc;
	MsnMessage *msg;
	char buf[MSN_BUF_LEN];
	char *md5sum;
	int file_size, base64_size;

	g_return_if_fail(swboard != NULL);

	if (gaim_account_get_buddy_icon(account) == NULL)
		return; /* We don't have an icon to send. */

	if (!get_buddy_icon_info(account, NULL, &md5sum,
							   &file_size, &base64_size)) {
		return;
	}

	if (file_size > MAX_BUDDY_ICON_FILE_SIZE) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "The buddy icon is too large to send. Must be no more "
				   "than %d bytes!\n", MAX_BUDDY_ICON_FILE_SIZE);
		return;
	}

	msg = msn_message_new();
	msn_message_set_content_type(msg, "application/x-buddyicon");
	msn_message_set_receiver(msg, msn_message_get_sender(msg));
	msn_message_set_charset(msg, NULL);
	msn_message_set_attr(msg, "User-Agent", NULL);

	g_snprintf(buf, sizeof(buf),
			   "Command: INVITE\r\n"
			   "MD5SUM: %s\r\n"
			   "File-Size: %d\r\n"
			   "Base64-Size: %d\r\n",
			   md5sum, file_size, base64_size);

	g_free(md5sum);

	msn_message_set_body(msg, buf);

	if (!msn_switchboard_send_msg(swboard, msg)) {
		msn_message_destroy(msg);

		gaim_connection_error(gc, _("Write error"));

		return;
	}

	msn_message_destroy(msg);
}
