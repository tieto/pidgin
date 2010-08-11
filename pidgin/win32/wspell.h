/*
 * pidgin
 *
 * File: wspell.h
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
 *
 */
#ifndef _WSPELL_H_
#define _WSPELL_H_
#include <gtkspell/gtkspell.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void winpidgin_spell_init(void);

extern GtkSpell* (*wpidginspell_new_attach)(GtkTextView*, const gchar*, GError**);
#define gtkspell_new_attach( view, lang, error ) \
wpidginspell_new_attach( view, lang, error )

extern GtkSpell* (*wpidginspell_get_from_text_view)(GtkTextView*);
#define gtkspell_get_from_text_view( view ) \
wpidginspell_get_from_text_view( view )

extern void (*wpidginspell_detach)(GtkSpell*);
#define gtkspell_detach( spell ) \
wpidginspell_detach( spell )

extern gboolean (*wpidginspell_set_language)(GtkSpell*, const gchar*, GError**);
#define gtkspell_set_language( spell, lang, error ) \
wpidginspell_set_language( spell, lang, error )

extern void (*wpidginspell_recheck_all)(GtkSpell*);
#define gtkspell_recheck_all( spell ) \
wpidginspell_recheck_all( spell )

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WSPELL_H_ */
