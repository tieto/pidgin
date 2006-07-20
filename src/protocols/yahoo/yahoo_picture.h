/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */

#ifndef _YAHOO_PICTURE_H_
#define _YAHOO_PICTURE_H_

void yahoo_send_picture_request(GaimConnection *gc, const char *who);
void yahoo_send_picture_info(GaimConnection *gc, const char *who);
void yahoo_send_picture_checksum(GaimConnection *gc);
void yahoo_send_picture_update(GaimConnection *gc, int type);
void yahoo_send_picture_update_to_user(GaimConnection *gc, const char *who, int type);

void yahoo_process_picture(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_picture_update(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_picture_checksum(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_picture_upload(GaimConnection *gc, struct yahoo_packet *pkt);

void yahoo_process_avatar_update(GaimConnection *gc, struct yahoo_packet *pkt);

void yahoo_set_buddy_icon(GaimConnection *gc, const char *iconfile);
void yahoo_buddy_icon_upload(GaimConnection *gc, struct yahoo_buddy_icon_upload_data *d);
void yahoo_buddy_icon_upload_data_free(struct yahoo_buddy_icon_upload_data *d);

#endif /* _YAHOO_PICTURE_H_ */
