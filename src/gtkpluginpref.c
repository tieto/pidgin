/**
 * @file gtkpluginpref.c GTK+ Plugin preferences
 * @ingroup gtkui
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
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "debug.h"
#include "internal.h"
#include "pluginpref.h"
#include "prefs.h"

#include "gtkpluginpref.h"
#include "gtkprefs.h"
#include "gtkutils.h"

static gboolean
entry_cb(GtkWidget *entry, gpointer data) {
	char *pref = data;

	gaim_prefs_set_string(pref, gtk_entry_get_text(GTK_ENTRY(entry)));

	return FALSE;
}

static void
make_string_pref(GtkWidget *parent, GaimPluginPref *pref, GtkSizeGroup *sg) {
	GtkWidget *hbox, *gtk_label, *entry;
	gchar *pref_name, *pref_label;

	pref_name = gaim_plugin_pref_get_name(pref);
	pref_label = gaim_plugin_pref_get_label(pref);

	switch(gaim_plugin_pref_get_type(pref)) {
		case GAIM_PLUGIN_PREF_CHOICE:
			gtk_label = gaim_gtk_prefs_dropdown_from_list(parent, pref_label,
											  GAIM_PREF_STRING, pref_name,
											  gaim_plugin_pref_get_choices(pref));
			gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);

			if(sg)
				gtk_size_group_add_widget(sg, gtk_label);

			break;
		case GAIM_PLUGIN_PREF_NONE:
		default:
			hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
			gtk_widget_show(hbox);
			gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);

			gtk_label = gtk_label_new_with_mnemonic(pref_label);
			gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);
			gtk_widget_show(gtk_label);
			gtk_box_pack_start(GTK_BOX(hbox), gtk_label, FALSE, FALSE, 0);

			if(sg)
				gtk_size_group_add_widget(sg, gtk_label);

			entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), gaim_prefs_get_string(pref_name));
			gtk_entry_set_max_length(GTK_ENTRY(entry),
									 gaim_plugin_pref_get_max_length(pref));
			gtk_entry_set_visibility(GTK_ENTRY(entry),
									 !gaim_plugin_pref_get_masked(pref));
			g_signal_connect(G_OBJECT(entry), "changed",
							 G_CALLBACK(entry_cb),
							 (gpointer)pref_name);
			gtk_label_set_mnemonic_widget(GTK_LABEL(gtk_label), entry);
			gtk_widget_show(entry);
			gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);

			break;
	}
}

static void
make_int_pref(GtkWidget *parent, GaimPluginPref *pref, GtkSizeGroup *sg) {
	GtkWidget *gtk_label;
	gchar *pref_name, *pref_label;
	gint max, min;

	pref_name = gaim_plugin_pref_get_name(pref);
	pref_label = gaim_plugin_pref_get_label(pref);

	switch(gaim_plugin_pref_get_type(pref)) {
		case GAIM_PLUGIN_PREF_CHOICE:
			gtk_label = gaim_gtk_prefs_dropdown_from_list(parent, pref_label,
					GAIM_PREF_INT, pref_name, gaim_plugin_pref_get_choices(pref));
			gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);

			if(sg)
				gtk_size_group_add_widget(sg, gtk_label);

			break;
		case GAIM_PLUGIN_PREF_NONE:
		default:
			gaim_plugin_pref_get_bounds(pref, &min, &max);
			gaim_gtk_prefs_labeled_spin_button(parent, pref_label,
					pref_name, min, max, sg);
			break;
	}
}


static void
make_info_pref(GtkWidget *parent, GaimPluginPref *pref) {
	GtkWidget *gtk_label = gtk_label_new(gaim_plugin_pref_get_label(pref));
	gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(gtk_label), TRUE);
	gtk_box_pack_start(GTK_BOX(parent), gtk_label, FALSE, FALSE, 0);
	gtk_widget_show(gtk_label);
}


GtkWidget *
gaim_gtk_plugin_pref_create_frame(GaimPluginPrefFrame *frame) {
	GaimPluginPref *pref;
	GtkWidget *ret, *parent;
	GtkSizeGroup *sg;
	GList *pp;
	gchar *name, *label;

	g_return_val_if_fail(frame, NULL);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	parent = ret = gtk_vbox_new(FALSE, 16);
	gtk_container_set_border_width(GTK_CONTAINER(ret), GAIM_HIG_BORDER);
	gtk_widget_show(ret);

	for(pp = gaim_plugin_pref_frame_get_prefs(frame);
		pp != NULL;
		pp = pp->next)
	{
		pref = (GaimPluginPref *)pp->data;

		name = gaim_plugin_pref_get_name(pref);
		label = gaim_plugin_pref_get_label(pref);

		if(name == NULL) {
			if(label == NULL)
				continue;

			if(gaim_plugin_pref_get_type(pref) == GAIM_PLUGIN_PREF_INFO) {
				make_info_pref(parent, pref);
			} else {
				parent = gaim_gtk_make_frame(ret, label);
				gtk_widget_show(parent);
			}

			continue;
		}

		switch(gaim_prefs_get_type(name)) {
			case GAIM_PREF_BOOLEAN:
				gaim_gtk_prefs_checkbox(label, name, parent);
				break;
			case GAIM_PREF_INT:
				make_int_pref(parent, pref, sg);
				break;
			case GAIM_PREF_STRING:
				make_string_pref(parent, pref, sg);
				break;
			default:
				break;
		}
	}

	return ret;
}
