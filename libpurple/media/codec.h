/**
 * @file codec.h Codec for Media API
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_MEDIA_CODEC_H_
#define _PURPLE_MEDIA_CODEC_H_

#include "enum-types.h"

/** An opaque structure representing an audio or video codec. */
typedef struct _PurpleMediaCodec PurpleMediaCodec;

#include "../util.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_MEDIA_CODEC            (purple_media_codec_get_type())
#define PURPLE_IS_MEDIA_CODEC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEDIA_CODEC))
#define PURPLE_IS_MEDIA_CODEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEDIA_CODEC))
#define PURPLE_MEDIA_CODEC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEDIA_CODEC, PurpleMediaCodec))
#define PURPLE_MEDIA_CODEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEDIA_CODEC, PurpleMediaCodec))
#define PURPLE_MEDIA_CODEC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEDIA_CODEC, PurpleMediaCodec))


/**
 * Gets the type of the media codec structure.
 *
 * Returns: The media codec's GType
 */
GType purple_media_codec_get_type(void);

/**
 * Creates a new PurpleMediaCodec instance.
 *
 * @id: Codec identifier.
 * @encoding_name: Name of the media type this encodes.
 * @media_type: PurpleMediaSessionType of this codec.
 * @clock_rate: The clock rate this codec encodes at, if applicable.
 *
 * Returns: The newly created PurpleMediaCodec.
 */
PurpleMediaCodec *purple_media_codec_new(int id, const char *encoding_name,
		PurpleMediaSessionType media_type, guint clock_rate);

/**
 * Gets the codec id.
 *
 * @The: codec to get the id from.
 *
 * Returns: The codec id.
 */
guint purple_media_codec_get_id(PurpleMediaCodec *codec);

/**
 * Gets the encoding name.
 *
 * @The: codec to get the encoding name from.
 *
 * Returns: The encoding name.
 */
gchar *purple_media_codec_get_encoding_name(PurpleMediaCodec *codec);

/**
 * Gets the clock rate.
 *
 * @The: codec to get the clock rate from.
 *
 * Returns: The clock rate.
 */
guint purple_media_codec_get_clock_rate(PurpleMediaCodec *codec);

/**
 * Gets the number of channels.
 *
 * @The: codec to get the number of channels from.
 *
 * Returns: The number of channels.
 */
guint purple_media_codec_get_channels(PurpleMediaCodec *codec);

/**
 * Gets a list of the optional parameters.
 *
 * The list consists of PurpleKeyValuePair's.
 *
 * @The: codec to get the optional parameters from.
 *
 * Returns: The list of optional parameters. The list is owned by the codec and
 *         should not be freed.
 */
GList *purple_media_codec_get_optional_parameters(PurpleMediaCodec *codec);

/**
 * Adds an optional parameter to the codec.
 *
 * @codec: The codec to add the parameter to.
 * @name: The name of the parameter to add.
 * @value: The value of the parameter to add.
 */
void purple_media_codec_add_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value);

/**
 * Removes an optional parameter from the codec.
 *
 * @codec: The codec to remove the parameter from.
 * @param: A pointer to the parameter to remove.
 */
void purple_media_codec_remove_optional_parameter(PurpleMediaCodec *codec,
		PurpleKeyValuePair *param);

/**
 * Gets an optional parameter based on the values given.
 *
 * @codec: The codec to find the parameter in.
 * @name: The name of the parameter to search for.
 * @value: The value to search for or NULL.
 *
 * Returns: The value found or NULL.
 */
PurpleKeyValuePair *purple_media_codec_get_optional_parameter(
		PurpleMediaCodec *codec, const gchar *name,
		const gchar *value);

/**
 * Copies a PurpleMediaCodec object.
 *
 * @codec: The codec to copy.
 *
 * Returns: The copy of the codec.
 */
PurpleMediaCodec *purple_media_codec_copy(PurpleMediaCodec *codec);

/**
 * Copies a GList of PurpleMediaCodec and its contents.
 *
 * @codecs: The list of codecs to be copied.
 *
 * Returns: The copy of the GList.
 */
GList *purple_media_codec_list_copy(GList *codecs);

/**
 * Frees a GList of PurpleMediaCodec and its contents.
 *
 * @codecs: The list of codecs to be freed.
 */
void purple_media_codec_list_free(GList *codecs);

/**
 * Creates a string representation of the codec.
 *
 * @codec: The codec to create the string of.
 *
 * Returns: The new string representation.
 */
gchar *purple_media_codec_to_string(const PurpleMediaCodec *codec);

G_END_DECLS

#endif  /* _PURPLE_MEDIA_CODEC_H_ */

