/**
 * @file gntmedia.h GNT Media API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_MEDIA_H
#define GNT_MEDIA_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_VV

#include <glib-object.h>
#include "gntbox.h"

G_BEGIN_DECLS

#define FINCH_TYPE_MEDIA            (finch_media_get_type())
#define FINCH_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FINCH_TYPE_MEDIA, FinchMedia))
#define FINCH_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FINCH_TYPE_MEDIA, FinchMediaClass))
#define FINCH_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FINCH_TYPE_MEDIA))
#define FINCH_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FINCH_TYPE_MEDIA))
#define FINCH_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), FINCH_TYPE_MEDIA, FinchMediaClass))

typedef struct _FinchMedia FinchMedia;
typedef struct _FinchMediaClass FinchMediaClass;
typedef struct _FinchMediaPrivate FinchMediaPrivate;
typedef enum _FinchMediaState FinchMediaState;

struct _FinchMediaClass
{
	GntBoxClass parent_class;
};

struct _FinchMedia
{
	GntBox parent;
	FinchMediaPrivate *priv;
};

GType finch_media_get_type(void);

GntWidget *finch_media_new(PurpleMedia *media);

void finch_media_manager_init(void);

void finch_media_manager_uninit(void);

G_END_DECLS

#endif /* USE_VV */

#endif /* GNT_MEDIA_H */

