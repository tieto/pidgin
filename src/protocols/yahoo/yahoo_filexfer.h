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
 */

/**
 * Process ymsg file receive invites.
 */
void yahoo_process_filetransfer(GaimConnection *gc, struct yahoo_packet *pkt);

/**
 * Send a file.
 *
 * @param gc The GaimConnection handle.
 * @param who Who are we sending it to?
 * @param file What file?
 */
void yahoo_send_file(GaimConnection *gc, const char *who, const char *file);

/**
 * Sends a file, that the user chooses after this call.
 *
 * @param gc The GaimConnection.
 * @param who Who are we going to send a file to?
 */
void yahoo_ask_send_file(GaimConnection *gc, const char *who);
