/**
 * @file e2ee.h End-to-end encryption API
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_E2EE_H_
#define _PURPLE_E2EE_H_

typedef struct _PurpleE2eeState PurpleE2eeState;

typedef struct _PurpleE2eeProvider PurpleE2eeProvider;

typedef enum
{
	PURPLE_E2EE_FEATURE_IM = 0x0001,
	PURPLE_E2EE_FEATURE_CHAT = 0x0002
} PurpleE2eeFeatures;

#include <glib.h>

G_BEGIN_DECLS

/**************************************************************************/
/** @name Encryption states for conversations.                            */
/**************************************************************************/
/*@{*/

PurpleE2eeState *
purple_e2ee_state_new(PurpleE2eeProvider *provider);

void
purple_e2ee_state_ref(PurpleE2eeState *state);

PurpleE2eeState *
purple_e2ee_state_unref(PurpleE2eeState *state);

PurpleE2eeProvider *
purple_e2ee_state_get_provider(PurpleE2eeState *state);

void
purple_e2ee_state_set_name(PurpleE2eeState *state, const gchar *name);

const gchar *
purple_e2ee_state_get_name(PurpleE2eeState *state);

void
purple_e2ee_state_set_stock_icon(PurpleE2eeState *state,
	const gchar *stock_icon);

const gchar *
purple_e2ee_state_get_stock_icon(PurpleE2eeState *state);

/*@}*/


/**************************************************************************/
/** @name Encryption providers API.                                       */
/**************************************************************************/
/*@{*/

PurpleE2eeProvider *
purple_e2ee_provider_new(PurpleE2eeFeatures features);

void
purple_e2ee_provider_free(PurpleE2eeProvider *provider);

gboolean
purple_e2ee_provider_register(PurpleE2eeProvider *provider);

void
purple_e2ee_provider_unregister(PurpleE2eeProvider *provider);

PurpleE2eeProvider *
purple_e2ee_provider_get_main(void);

PurpleE2eeFeatures
purple_e2ee_provider_get_features(PurpleE2eeProvider *provider);

void
purple_e2ee_provider_set_default_state(PurpleE2eeProvider *provider,
	PurpleE2eeState *state);

PurpleE2eeState *
purple_e2ee_provider_get_default_state(PurpleE2eeProvider *provider);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_E2EE_H_ */
