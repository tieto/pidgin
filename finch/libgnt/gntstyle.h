/**
 * @file gntstyle.h Style API
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

#include "gnt.h"
#include "gntwm.h"

typedef enum
{
	GNT_STYLE_SHADOW = 0,
	GNT_STYLE_COLOR = 1,
	GNT_STYLE_MOUSE = 2,
	GNT_STYLE_WM = 3,
	GNT_STYLE_REMPOS = 4,
	GNT_STYLES
} GntStyle;

/**
 * 
 * @param filename
 */
void gnt_style_read_configure_file(const char *filename);

const char *gnt_style_get(GntStyle style);

char *gnt_style_get_from_name(const char *group, const char *key);

/**
 * 
 * @param style
 * @param def
 *
 * @return
 */
gboolean gnt_style_get_bool(GntStyle style, gboolean def);

/* This should be called only once for the each type */
/**
 * 
 * @param type
 * @param hash
 */
void gnt_styles_get_keyremaps(GType type, GHashTable *hash);

/**
 * 
 * @param type
 * @param klass
 */
void gnt_style_read_actions(GType type, GntBindableClass *klass);

void gnt_style_read_workspaces(GntWM *wm);

/**
 * 
 */
void gnt_init_styles(void);

/**
 * 
 */
void gnt_uninit_styles(void);

