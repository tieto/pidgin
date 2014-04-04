/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_REMOTE_SMILEY_H_
#define _PURPLE_REMOTE_SMILEY_H_

#include <glib-object.h>

#include "imgstore.h"
#include "util.h"

typedef struct _PurpleRemoteSmiley PurpleRemoteSmiley;
typedef struct _PurpleRemoteSmileyClass PurpleRemoteSmileyClass;

#define PURPLE_TYPE_REMOTE_SMILEY            (purple_remote_smiley_get_type())
#define PURPLE_REMOTE_SMILEY(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_REMOTE_SMILEY, PurpleRemoteSmiley))
#define PURPLE_REMOTE_SMILEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_REMOTE_SMILEY, PurpleRemoteSmileyClass))
#define PURPLE_IS_REMOTE_SMILEY(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_REMOTE_SMILEY))
#define PURPLE_IS_REMOTE_SMILEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_REMOTE_SMILEY))
#define PURPLE_REMOTE_SMILEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_REMOTE_SMILEY, PurpleRemoteSmileyClass))

/**
 * PurpleRemoteSmiley:
 *
 * A smiley that was sent by remote client, most probably your buddy.
 */
struct _PurpleRemoteSmiley
{
	/*< private >*/
	PurpleSmiley parent;
};

struct _PurpleRemoteSmileyClass
{
	/*< private >*/
	PurpleSmileyClass parent_class;

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_remote_smiley_get_type:
 *
 * Returns: The #GType for a remote smiley.
 */
GType
purple_remote_smiley_get_type(void);

/******************************************************************************
 * Remote smiley API
 ******************************************************************************/



G_END_DECLS

#endif /* _PURPLE_REMOTE_SMILEY_H_ */

