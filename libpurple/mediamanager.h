/**
 * @file mediamanager.h Media Manager API
 * @ingroup core
 */

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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MEDIA_MANAGER_H_
#define __MEDIA_MANAGER_H_

#ifdef USE_VV

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

/** @copydoc _PurpleMediaManager */
typedef struct _PurpleMediaManager PurpleMediaManager;
/** @copydoc _PurpleMediaManagerClass */
typedef struct _PurpleMediaManagerClass PurpleMediaManagerClass;
/** @copydoc _PurpleMediaManagerPrivate */
typedef struct _PurpleMediaManagerPrivate PurpleMediaManagerPrivate;
/** @copydoc _PurpleMediaElementInfo */
typedef struct _PurpleMediaElementInfo PurpleMediaElementInfo;

/** The media manager class. */
struct _PurpleMediaManagerClass
{
	GObjectClass parent_class;       /**< The parent class. */
};

/** The media manager's data. */
struct _PurpleMediaManager
{
	GObject parent;                  /**< The parent of this manager. */
	PurpleMediaManagerPrivate *priv; /**< Private data for the manager. */
};

struct _PurpleMediaElementInfo
{
	const gchar *id;
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @cname Media Manager API                                              */
/**************************************************************************/
/*@{*/

/**
 * Gets the media manager's GType.
 *
 * @return The media manager's GType.
 */
GType purple_media_manager_get_type(void);

/**
 * Gets the "global" media manager object. It's created if it doesn't already exist.
 *
 * @return The "global" instance of the media manager object.
 */
PurpleMediaManager *purple_media_manager_get(void);

/**
 * Creates a media session.
 *
 * @param manager The media manager to create the session under.
 * @param gc The connection to create the session on.
 * @param conference_type The conference type to feed into Farsight2.
 * @param remote_user The remote user to initiate the session with.
 *
 * @return A newly created media session.
 */
PurpleMedia *purple_media_manager_create_media(PurpleMediaManager *manager,
						PurpleConnection *gc,
						const char *conference_type,
						const char *remote_user,
						gboolean initiator);

/**
 * Gets all of the media sessions.
 *
 * @param manager The media manager to get all of the sessions from.
 *
 * @return A list of all the media sessions.
 */
GList *purple_media_manager_get_media(PurpleMediaManager *manager);

/**
 * Removes a media session from the media manager.
 *
 * @param manager The media manager to remove the media session from.
 * @param media The media session to remove.
 */
void
purple_media_manager_remove_media(PurpleMediaManager *manager,
				  PurpleMedia *media);

/**
 * Returns a GStreamer source or sink for audio or video.
 *
 * @param manager The media manager to use to obtain the source/sink.
 * @param type The type of source/sink to get.
 */
GstElement *purple_media_manager_get_element(PurpleMediaManager *manager,
		PurpleMediaSessionType type);

PurpleMediaElementInfo *purple_media_manager_get_element_info(
		PurpleMediaManager *manager, const gchar *name);
gboolean purple_media_manager_register_element(PurpleMediaManager *manager,
		PurpleMediaElementInfo *info);
gboolean purple_media_manager_unregister_element(PurpleMediaManager *manager,
		const gchar *name);
/*}@*/

#ifdef __cplusplus
}
#endif

G_END_DECLS

#endif  /* USE_VV */


#endif  /* __MEDIA_MANAGER_H_ */
