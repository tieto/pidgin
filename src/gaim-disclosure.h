/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Gaim is the legal property of its developers, whose names are too numerous
 *  to list here.  Please refer to the COPYRIGHT file distributed with this
 *  source distribution.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */
#ifndef __GAIM_GAIM_DISCLOSURE_H__
#define __GAIM_GAIM_DISCLOSURE_H__

#include "eventloop.h"

#include <gtk/gtkcheckbutton.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#define GAIM_DISCLOSURE_TYPE (gaim_disclosure_get_type ())
#define GAIM_DISCLOSURE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIM_DISCLOSURE_TYPE, GaimDisclosure))
#define GAIM_DISCLOSURE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GAIM_DISCLOSURE_TYPE, GaimDisclosureClass))
#define IS_GAIM_DISCLOSURE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIM_DISCLOSURE_TYPE))
#define IS_GAIM_DISCLOSURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIM_DISCLOSURE_TYPE))

typedef struct _GaimDisclosure GaimDisclosure;
typedef struct _GaimDisclosureClass GaimDisclosureClass;
typedef struct _GaimDisclosurePrivate GaimDisclosurePrivate;

struct _GaimDisclosure {
	GtkCheckButton parent;

	GaimDisclosurePrivate *priv;
};

struct _GaimDisclosureClass {
	GtkCheckButtonClass parent_class;
};

GType gaim_disclosure_get_type (void);
GtkWidget *gaim_disclosure_new (const char *shown, const char *hidden);
void gaim_disclosure_set_container (GaimDisclosure *disclosure,
									GtkWidget *container);

#ifdef __cplusplus
}
#endif

#endif
