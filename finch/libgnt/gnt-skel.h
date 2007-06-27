/**
 * @file gnt-skel.h -skel API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#ifndef GNT_SKEL_H
#define GNT_SKEL_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_SKEL				(gnt_skel_get_gtype())
#define GNT_SKEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_SKEL, GntSkel))
#define GNT_SKEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_SKEL, GntSkelClass))
#define GNT_IS_SKEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_SKEL))
#define GNT_IS_SKEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_SKEL))
#define GNT_SKEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_SKEL, GntSkelClass))

#define GNT_SKEL_FLAGS(obj)				(GNT_SKEL(obj)->priv.flags)
#define GNT_SKEL_SET_FLAGS(obj, flags)		(GNT_SKEL_FLAGS(obj) |= flags)
#define GNT_SKEL_UNSET_FLAGS(obj, flags)	(GNT_SKEL_FLAGS(obj) &= ~(flags))

typedef struct _GntSkel			GntSkel;
typedef struct _GntSkelPriv		GntSkelPriv;
typedef struct _GntSkelClass		GntSkelClass;

struct _GntSkel
{
	GntWidget parent;
};

struct _GntSkelClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * 
 *
 * @return
 */
GType gnt_skel_get_gtype(void);

/**
 * 
 *
 * @return
 */
GntWidget * gnt_skel_new();

G_END_DECLS

#endif /* GNT_SKEL_H */
