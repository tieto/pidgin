/**
 * @file stringref.h Reference-counted immutable strings
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 *
 */
#ifndef _GAIM_STRINGREF_H_
#define _GAIM_STRINGREF_H_

/**
 * The internal representation of a stringref.
 *
 * @note For this structure to be useful, the string contained within
 * it must be immutable -- for this reason, do _not_ access it
 * directly!
 */
typedef struct _GaimStringref {
	guint32 ref;	/**< The reference count of this string.
					 *   Note that reference counts are only
					 *   31 bits, and the high-order bit
					 *   indicates whether this string is up
					 *   for GC at the next idle handler...
					 *   But you aren't going to touch this
					 *   anyway, right? */
	char value[1];	/**< The string contained in this ref.
					 *   Notice that it is simply "hanging
					 *   off the end" of the ref ... this
					 *   is to save an allocation. */
} GaimStringref;

/**
 * Creates an immutable reference-counted string object.  The newly
 * created object will have a reference count of 1.
 *
 * @param value This will be the value of the string; it will be
 *              duplicated.
 *
 * @return A newly allocated string reference object with a refcount
 *         of 1.
 */
GaimStringref *gaim_stringref_new(const char *value);

/**
 * Creates an immutable reference-counted string object.  The newly
 * created object will have a reference count of zero, and if it is
 * not referenced before the next iteration of the mainloop it will
 * be freed at that time.
 *
 * @param value This will be the value of the string; it will be
 *              duplicated.
 *
 * @return A newly allocated string reference object with a refcount
 *         of zero.
 */
GaimStringref *gaim_stringref_new_noref(const char *value);

/**
 * Creates an immutable reference-counted string object from a printf
 * format specification and arguments.  The created object will have a
 * reference count of 1.
 *
 * @param format A printf-style format specification.
 *
 * @return A newly allocated string reference object with a refcount
 *         of 1.
 */
GaimStringref *gaim_stringref_printf(const char *format, ...);

/**
 * Increase the reference count of the given stringref.
 *
 * @param stringref String to be referenced.
 *
 * @return A pointer to the referenced string.
 */
GaimStringref *gaim_stringref_ref(GaimStringref *stringref);

/**
 * Decrease the reference count of the given stringref.  If this
 * reference count reaches zero, the stringref will be freed; thus
 * you MUST NOT use this string after dereferencing it.
 *
 * @param stringref String to be dereferenced.
 */
void gaim_stringref_unref(GaimStringref *stringref);

/**
 * Retrieve the value of a stringref.
 *
 * @note This value should not be cached or stored in a local variable.
 *       While there is nothing inherently incorrect about doing so, it
 *       is easy to forget that the cached value is in fact a
 *       reference-counted object and accidentally use it after
 *       dereferencing.  This is more problematic for a reference-
 *       counted object than a heap-allocated object, as it may seem to
 *       be valid or invalid nondeterministically based on how many
 *       other references to it exist.
 *
 * @param stringref String reference from which to retrieve the value.
 *
 * @return The contents of the string reference.
 */
const char *gaim_stringref_value(const GaimStringref *stringref);

/**
 * Compare two stringrefs for string equality.  This returns the same
 * value as strcmp would, where <0 indicates that s1 is "less than" s2
 * in the ASCII lexicography, 0 indicates equality, etc.
 *
 * @param s1 The reference string.
 *
 * @param s2 The string to compare against the reference.
 *
 * @return An ordering indication on s1 and s2.
 */
int gaim_stringref_cmp(const GaimStringref *s1, const GaimStringref *s2);

/**
 * Find the length of the string inside a stringref.
 *
 * @param stringref The string in whose length we are interested.
 *
 * @return The length of the string in stringref
 */
size_t gaim_stringref_len(const GaimStringref *stringref);

#endif /* _GAIM_STRINGREF_H_ */
