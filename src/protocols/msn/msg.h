/**
 * @file msg.h Message functions
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _MSN_MSG_H_
#define _MSN_MSG_H_

/**
 * Writes a message to the server.
 *
 * @param fd   The file descriptor.
 * @param data The data to write.
 * @param len  The length of the data
 *
 * @return The number of bytes written.
 */
int msn_write(int fd, void *data, int len);

#endif /* _MSN_MSG_H_ */
