/**
 * @file media.h Account API
 * @ingroup core
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef __MEDIA_MANAGER_H_
#define __MEDIA_MANAGER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_FARSIGHT

#include <farsight/farsight.h>
#include <glib.h>
#include <glib-object.h>

#include "connection.h"
#include "media.h"

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA_MANAGER            (purple_media_manager_get_type())
#define PURPLE_MEDIA_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManager))
#define PURPLE_MEDIA_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerClass))
#define PURPLE_IS_MEDIA_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA_MANAGER))
#define PURPLE_IS_MEDIA_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA_MANAGER))
#define PURPLE_MEDIA_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerClass))

typedef struct _PurpleMediaManager PurpleMediaManager;
typedef struct _PurpleMediaManagerClass PurpleMediaManagerClass;
typedef struct _PurpleMediaManagerPrivate PurpleMediaManagerPrivate;

struct _PurpleMediaManagerClass
{
	GObjectClass parent_class;
};

struct _PurpleMediaManager
{
	GObject parent;
	PurpleMediaManagerPrivate *priv;
};

GType purple_media_manager_get_type();
PurpleMediaManager *purple_media_manager_get();

PurpleMedia *purple_media_manager_create_media(PurpleMediaManager *manager,
					       PurpleConnection *gc,
					       const char *screenname);

G_END_DECLS

#endif  /* USE_FARSIGHT */


#endif  /* __MEDIA_MANAGER_H_ */
