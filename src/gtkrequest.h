/**
 * @file gtkrequest.h GTK+ Request API
 * @ingroup gtkui
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
#ifndef _GAIM_GTK_REQUEST_H_
#define _GAIM_GTK_REQUEST_H_

#include "request.h"

/**
 * Returns the UI operations structure for GTK+ request functions.
 *
 * @return The GTK+ UI request operations structure.
 */
GaimRequestUiOps *gaim_get_gtk_request_ui_ops(void);

#endif /* _GAIM_GTK_REQUEST_H_ */
