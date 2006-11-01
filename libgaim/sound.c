/*
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
#include "internal.h"

#include "blist.h"
#include "prefs.h"
#include "sound.h"

static GaimSoundUiOps *sound_ui_ops = NULL;

#define STATUS_AVAILABLE 1
#define STATUS_AWAY 2

static gboolean
gaim_sound_play_required(const GaimAccount *account)
{
	gint pref_status = gaim_prefs_get_int("/core/sound/while_status");

	if (pref_status == 3)
	{
		/* Play sounds: Always */
		return TRUE;
	}

	if (account != NULL)
	{
		GaimStatus *status = gaim_account_get_active_status(account);

		if (gaim_status_is_online(status))
		{
			gboolean available = gaim_status_is_available(status);
			return (( available && pref_status == STATUS_AVAILABLE) ||
			        (!available && pref_status == STATUS_AWAY));
		}
	}

	/* We get here a couple of ways.  Either the request has been OK'ed
	 * by gaim_sound_play_event() and we're here because the UI has
	 * called gaim_sound_play_file(), or we're here for something
	 * not related to an account (like testing a sound). */
	return TRUE;
}

void
gaim_sound_play_file(const char *filename, const GaimAccount *account)
{
	if (!gaim_sound_play_required(account))
		return;

	if(sound_ui_ops && sound_ui_ops->play_file)
		sound_ui_ops->play_file(filename);
}

void
gaim_sound_play_event(GaimSoundEventID event, const GaimAccount *account)
{
	if (!gaim_sound_play_required(account))
		return;

	if(sound_ui_ops && sound_ui_ops->play_event) {
		int plugin_return;

		plugin_return = GPOINTER_TO_INT(gaim_signal_emit_return_1(
			gaim_sounds_get_handle(), "playing-sound-event",
			event, account));

		if (plugin_return)
			return;
		else
			sound_ui_ops->play_event(event);
	}
}

void
gaim_sound_set_ui_ops(GaimSoundUiOps *ops)
{
	if(sound_ui_ops && sound_ui_ops->uninit)
		sound_ui_ops->uninit();

	sound_ui_ops = ops;

	if(sound_ui_ops && sound_ui_ops->init)
		sound_ui_ops->init();
}

GaimSoundUiOps *
gaim_sound_get_ui_ops(void)
{
	return sound_ui_ops;
}

void
gaim_sound_init()
{
	void *handle = gaim_sounds_get_handle();

	/**********************************************************************
	 * Register signals
	**********************************************************************/

	gaim_signal_register(handle, "playing-sound-event",
	                     gaim_marshal_BOOLEAN__INT_POINTER,
	                     gaim_value_new(GAIM_TYPE_BOOLEAN), 2,
	                     gaim_value_new(GAIM_TYPE_INT),
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_ACCOUNT));

	gaim_prefs_add_none("/core/sound");
	gaim_prefs_add_int("/core/sound/while_status", STATUS_AVAILABLE);
}

void
gaim_sound_uninit()
{
	if(sound_ui_ops && sound_ui_ops->uninit)
		sound_ui_ops->uninit();

	gaim_signals_unregister_by_instance(gaim_sounds_get_handle());
}

void *
gaim_sounds_get_handle()
{
	static int handle;

	return &handle;
}
