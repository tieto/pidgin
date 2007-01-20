/**
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
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>

#include "debug.h"
#include "internal.h"
#include "pluginpref.h"
#include "prefs.h"

struct _GaimPluginPrefFrame
{
	GList *prefs;
};

struct _GaimPluginPref
{
	char *name;
	char *label;

	GaimPluginPrefType type;

	int min;
	int max;
	GList *choices;
	unsigned int max_length;
	gboolean masked;
	GaimStringFormatType format;
};

GaimPluginPrefFrame *
gaim_plugin_pref_frame_new()
{
	GaimPluginPrefFrame *frame;

	frame = g_new0(GaimPluginPrefFrame, 1);

	return frame;
}

void
gaim_plugin_pref_frame_destroy(GaimPluginPrefFrame *frame)
{
	g_return_if_fail(frame != NULL);

	g_list_foreach(frame->prefs, (GFunc)gaim_plugin_pref_destroy, NULL);
	g_list_free(frame->prefs);
	g_free(frame);
}

void
gaim_plugin_pref_frame_add(GaimPluginPrefFrame *frame, GaimPluginPref *pref)
{
	g_return_if_fail(frame != NULL);
	g_return_if_fail(pref  != NULL);

	frame->prefs = g_list_append(frame->prefs, pref);
}

GList *
gaim_plugin_pref_frame_get_prefs(GaimPluginPrefFrame *frame)
{
	g_return_val_if_fail(frame        != NULL, NULL);
	g_return_val_if_fail(frame->prefs != NULL, NULL);

	return frame->prefs;
}

GaimPluginPref *
gaim_plugin_pref_new()
{
	GaimPluginPref *pref;

	pref = g_new0(GaimPluginPref, 1);

	return pref;
}

GaimPluginPref *
gaim_plugin_pref_new_with_name(const char *name)
{
	GaimPluginPref *pref;

	g_return_val_if_fail(name != NULL, NULL);

	pref = g_new0(GaimPluginPref, 1);
	pref->name = g_strdup(name);

	return pref;
}

GaimPluginPref *
gaim_plugin_pref_new_with_label(const char *label)
{
	GaimPluginPref *pref;

	g_return_val_if_fail(label != NULL, NULL);

	pref = g_new0(GaimPluginPref, 1);
	pref->label = g_strdup(label);

	return pref;
}

GaimPluginPref *
gaim_plugin_pref_new_with_name_and_label(const char *name, const char *label)
{
	GaimPluginPref *pref;

	g_return_val_if_fail(name  != NULL, NULL);
	g_return_val_if_fail(label != NULL, NULL);

	pref = g_new0(GaimPluginPref, 1);
	pref->name = g_strdup(name);
	pref->label = g_strdup(label);

	return pref;
}

void
gaim_plugin_pref_destroy(GaimPluginPref *pref)
{
	g_return_if_fail(pref != NULL);

	g_free(pref->name);
	g_free(pref->label);
	g_list_free(pref->choices);
	g_free(pref);
}

void
gaim_plugin_pref_set_name(GaimPluginPref *pref, const char *name)
{
	g_return_if_fail(pref != NULL);
	g_return_if_fail(name != NULL);

	g_free(pref->name);
	pref->name = g_strdup(name);
}

const char *
gaim_plugin_pref_get_name(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, NULL);

	return pref->name;
}

void
gaim_plugin_pref_set_label(GaimPluginPref *pref, const char *label)
{
	g_return_if_fail(pref  != NULL);
	g_return_if_fail(label != NULL);

	g_free(pref->label);
	pref->label = g_strdup(label);
}

const char *
gaim_plugin_pref_get_label(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, NULL);

	return pref->label;
}

void
gaim_plugin_pref_set_bounds(GaimPluginPref *pref, int min, int max)
{
	int tmp;

	g_return_if_fail(pref       != NULL);
	g_return_if_fail(pref->name != NULL);

	if (gaim_prefs_get_type(pref->name) != GAIM_PREF_INT)
	{
		gaim_debug_info("pluginpref",
				"gaim_plugin_pref_set_bounds: %s is not an integer pref\n",
				pref->name);
		return;
	}

	if (min > max)
	{
		tmp = min;
		min = max;
		max = tmp;
	}

	pref->min = min;
	pref->max = max;
}

void gaim_plugin_pref_get_bounds(GaimPluginPref *pref, int *min, int *max)
{
	g_return_if_fail(pref       != NULL);
	g_return_if_fail(pref->name != NULL);

	if (gaim_prefs_get_type(pref->name) != GAIM_PREF_INT)
	{
		gaim_debug(GAIM_DEBUG_INFO, "pluginpref",
				"gaim_plugin_pref_get_bounds: %s is not an integer pref\n",
				pref->name);
		return;
	}

	*min = pref->min;
	*max = pref->max;
}

void
gaim_plugin_pref_set_type(GaimPluginPref *pref, GaimPluginPrefType type)
{
	g_return_if_fail(pref != NULL);

	pref->type = type;
}

GaimPluginPrefType
gaim_plugin_pref_get_type(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, GAIM_PLUGIN_PREF_NONE);

	return pref->type;
}

void
gaim_plugin_pref_add_choice(GaimPluginPref *pref, const char *label, gpointer choice)
{
	g_return_if_fail(pref  != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(choice || gaim_prefs_get_type(pref->name) == GAIM_PREF_INT);

	pref->choices = g_list_append(pref->choices, (gpointer)label);
	pref->choices = g_list_append(pref->choices, choice);
}

GList *
gaim_plugin_pref_get_choices(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, NULL);

	return pref->choices;
}

void
gaim_plugin_pref_set_max_length(GaimPluginPref *pref, unsigned int max_length)
{
	g_return_if_fail(pref != NULL);

	pref->max_length = max_length;
}

unsigned int
gaim_plugin_pref_get_max_length(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, 0);

	return pref->max_length;
}

void
gaim_plugin_pref_set_masked(GaimPluginPref *pref, gboolean masked)
{
	g_return_if_fail(pref != NULL);

	pref->masked = masked;
}

gboolean
gaim_plugin_pref_get_masked(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, FALSE);

	return pref->masked;
}

void
gaim_plugin_pref_set_format_type(GaimPluginPref *pref, GaimStringFormatType format)
{
	g_return_if_fail(pref != NULL);
	g_return_if_fail(pref->type == GAIM_PLUGIN_PREF_STRING_FORMAT);

	pref->format = format;
}

GaimStringFormatType
gaim_plugin_pref_get_format_type(GaimPluginPref *pref)
{
	g_return_val_if_fail(pref != NULL, 0);

	if (pref->type != GAIM_PLUGIN_PREF_STRING_FORMAT)
		return GAIM_STRING_FORMAT_TYPE_NONE;

	return pref->format;
}

