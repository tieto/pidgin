/**
 * @file media.h Account API
 * @ingroup core
 *
 * Pidgin 
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef __GTKMEDIA_H_
#define __GTKMEDIA_H_

#ifdef USE_VV

#include <gtk/gtk.h>
#include <glib-object.h>

#include "connection.h"

G_BEGIN_DECLS

#define PIDGIN_TYPE_MEDIA            (pidgin_media_get_type())
#define PIDGIN_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_MEDIA, PidginMedia))
#define PIDGIN_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_MEDIA, PidginMediaClass))
#define PIDGIN_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_MEDIA))
#define PIDGIN_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_MEDIA))
#define PIDGIN_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_MEDIA, PidginMediaClass))

typedef struct _PidginMedia PidginMedia;
typedef struct _PidginMediaClass PidginMediaClass;
typedef struct _PidginMediaPrivate PidginMediaPrivate;

struct _PidginMediaClass
{
	GtkHBoxClass parent_class;
};

struct _PidginMedia
{
	GtkHBox parent;
	PidginMediaPrivate *priv;
};

GType pidgin_media_get_type(void);

GtkWidget *pidgin_media_new(PurpleMedia *media);

G_END_DECLS

#endif  /* USE_VV */


#endif  /* __GTKMEDIA_H_ */
