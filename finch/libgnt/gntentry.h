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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
/**
 * SECTION:gntentry
 * @section_id: libgnt-gntentry
 * @short_description: <filename>gntentry.h</filename>
 * @title: Entry
 */

#ifndef GNT_ENTRY_H
#define GNT_ENTRY_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_ENTRY				(gnt_entry_get_type())
#define GNT_ENTRY(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_ENTRY, GntEntry))
#define GNT_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_ENTRY, GntEntryClass))
#define GNT_IS_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_ENTRY))
#define GNT_IS_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_ENTRY))
#define GNT_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_ENTRY, GntEntryClass))

#define GNT_ENTRY_FLAGS(obj)				(GNT_ENTRY(obj)->priv.flags)
#define GNT_ENTRY_SET_FLAGS(obj, flags)		(GNT_ENTRY_FLAGS(obj) |= flags)
#define GNT_ENTRY_UNSET_FLAGS(obj, flags)	(GNT_ENTRY_FLAGS(obj) &= ~(flags))

#define	ENTRY_CHAR		'_'			/* The character to use to fill in the blank places */

typedef struct _GntEntry			GntEntry;
typedef struct _GntEntryPriv		GntEntryPriv;
typedef struct _GntEntryClass	GntEntryClass;
typedef struct _GntEntryKillRing    GntEntryKillRing;
typedef struct _GntEntrySearch		GntEntrySearch;

typedef enum
{
	GNT_ENTRY_FLAG_ALPHA    = 1 << 0,  /* Only alpha */
	GNT_ENTRY_FLAG_INT      = 1 << 1,  /* Only integer */
	GNT_ENTRY_FLAG_NO_SPACE = 1 << 2,  /* No blank space is allowed */
	GNT_ENTRY_FLAG_NO_PUNCT = 1 << 3,  /* No punctuations */
	GNT_ENTRY_FLAG_MASK     = 1 << 4,  /* Mask the inputs */
} GntEntryFlag;

#define GNT_ENTRY_FLAG_ALL    (GNT_ENTRY_FLAG_ALPHA | GNT_ENTRY_FLAG_INT)

struct _GntEntry
{
	GntWidget parent;

	GntEntryFlag flag;

	char *start;
	char *end;
	char *scroll;   /* Current scrolling position */
	char *cursor;   /* Cursor location */
	                /* 0 <= cursor - scroll < widget-width */

	size_t buffer;  /* Size of the buffer */

	int max;        /* 0 means infinite */
	gboolean masked;

	GList *history; /* History of the strings. User can use this by pressing ctrl+up/down */
	int histlength; /* How long can the history be? */

	GList *suggests;    /* List of suggestions */
	gboolean word;      /* Are the suggestions for only a word, or for the whole thing? */
	gboolean always;    /* Should the list of suggestions show at all times, or only on tab-press? */
	GntWidget *ddown;   /* The dropdown with the suggested list */
	GntEntryKillRing *killring; /* Since: 2.3.0 */
	GntEntrySearch *search;		/* Since: 2.7.0 */
};

struct _GntEntryClass
{
	GntWidgetClass parent;

	void (*text_changed)(GntEntry *entry);

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_entry_get_gtype:
 *
 * Returns: GType for GntEntry.
 */
GType gnt_entry_get_type(void);

/**
 * gnt_entry_new:
 * @text:   The text in the new entry box.
 *
 * Create a new GntEntry.
 *
 * Returns:  The newly created entry box.
 */
GntWidget * gnt_entry_new(const char *text);

/**
 * gnt_entry_set_max:
 * @entry:  The entry box.
 * @max:    The maximum length for text. A value of 0 means infinite length.
 *
 * Set the maximum length of the text in the entry box.
 */
void gnt_entry_set_max(GntEntry *entry, int max);

/**
 * gnt_entry_set_text:
 * @entry: The entry box.
 * @text:  The text to set in the box.
 *
 * Set the text in an entry box.
 */
void gnt_entry_set_text(GntEntry *entry, const char *text);

/**
 * gnt_entry_set_flag:
 * @entry:  The entry box.
 * @flag:   The flags to set for the entry box.
 *
 * Set flags an entry box.
 */
void gnt_entry_set_flag(GntEntry *entry, GntEntryFlag flag);

/**
 * gnt_entry_get_text:
 * @entry:  The entry box.
 *
 * Get the text in an entry box.
 *
 * Returns:   The current text in the entry box.
 */
const char *gnt_entry_get_text(GntEntry *entry);

/**
 * gnt_entry_clear:
 * @entry:  The entry box.
 *
 * Clear the text in the entry box.
 */
void gnt_entry_clear(GntEntry *entry);

/**
 * gnt_entry_set_masked:
 * @entry:  The entry box.
 * @set:    %TRUE if the text should be masked, %FALSE otherwise.
 *
 * Set whether the text in the entry box should be masked for display.
 */
void gnt_entry_set_masked(GntEntry *entry, gboolean set);

/**
 * gnt_entry_add_to_history:
 * @entry:  The entry box.
 * @text:   A new entry for the history list.
 *
 * Add a text to the history list for the text. The history length for the
 * entry box needs to be set first by gnt_entry_set_history_length.
 */
void gnt_entry_add_to_history(GntEntry *entry, const char *text);

/**
 * gnt_entry_set_history_length:
 * @entry:  The entry box.
 * @num:    The maximum length of the history, -1 for unlimited.
 *
 * Set the length of history for the entry box.
 */
void gnt_entry_set_history_length(GntEntry *entry, int num);

/**
 * gnt_entry_set_word_suggest:
 * @entry:   The entry box.
 * @word:    %TRUE if the suggestions are for individual words, %FALSE otherwise.
 *
 * Set whether the suggestions are for the entire entry box, or for each
 * individual word in the entry box.
 */
void gnt_entry_set_word_suggest(GntEntry *entry, gboolean word);

/**
 * gnt_entry_set_always_suggest:
 * @entry:    The entry box.
 * @always:   %TRUE if the suggestion list should always be displayed.
 *
 * Set whether to always display the suggestions list, or only when the
 * tab-completion key is pressed (the TAB key, by default).
 */
void gnt_entry_set_always_suggest(GntEntry *entry, gboolean always);

/**
 * gnt_entry_add_suggest:
 * @entry:  The entry box.
 * @text:   An item to add to the suggestion list.
 *
 * Add an item to the suggestion list.
 */
void gnt_entry_add_suggest(GntEntry *entry, const char *text);

/**
 * gnt_entry_remove_suggest:
 * @entry:  The entry box.
 * @text:   The item to remove from the suggestion list.
 *
 * Remove an entry from the suggestion list.
 */
void gnt_entry_remove_suggest(GntEntry *entry, const char *text);

G_END_DECLS

#endif /* GNT_ENTRY_H */
