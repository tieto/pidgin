/**
 * @file gtkpluginpref.c GTK+ Plugin preferences
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "gtkwebview.h"

static gboolean
entry_cb(GtkWidget *entry, gpointer data) {
	char *pref = data;

	purple_prefs_set_string(pref, gtk_entry_get_text(GTK_ENTRY(entry)));

	return FALSE;
}


static void
webview_cb(GtkWebView *webview, gpointer data)
{
	char *pref;
	char *text;

	pref = g_object_get_data(G_OBJECT(webview), "pref-key");
	g_return_if_fail(pref);

	text = gtk_webview_get_body_html(webview);
	purple_prefs_set_string(pref, text);
	g_free(text);
}

static void
make_string_pref(GtkWidget *parent, PurplePluginPref *pref, GtkSizeGroup *sg) {
	GtkWidget *box, *gtk_label, *entry;
	const gchar *pref_name;
	const gchar *pref_label;
	PurpleStringFormatType format;

	pref_name = purple_plugin_pref_get_name(pref);
	pref_label = purple_plugin_pref_get_label(pref);
	format = purple_plugin_pref_get_format_type(pref);

	switch(purple_plugin_pref_get_type(pref)) {
		case PURPLE_PLUGIN_PREF_CHOICE:
			gtk_label = pidgin_prefs_dropdown_from_list(parent, pref_label,
											  PURPLE_PREF_STRING, pref_name,
											  purple_plugin_pref_get_choices(pref));
			gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);

			if(sg)
				gtk_size_group_add_widget(sg, gtk_label);

			break;
		case PURPLE_PLUGIN_PREF_NONE:
		default:
			if (format == PURPLE_STRING_FORMAT_TYPE_NONE)
			{
				entry = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(pref_name));
				gtk_entry_set_max_length(GTK_ENTRY(entry),
									 purple_plugin_pref_get_max_length(pref));
				if (purple_plugin_pref_get_masked(pref))
				{
					gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
				}
				g_signal_connect(G_OBJECT(entry), "changed",
								 G_CALLBACK(entry_cb),
								 (gpointer)pref_name);
				pidgin_add_widget_to_vbox(GTK_BOX(parent), pref_label, sg, entry, TRUE, NULL);
			}
			else
			{
				GtkWidget *hbox;
				GtkWidget *spacer;
				GtkWidget *webview;
				GtkWidget *frame;

				box = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

				gtk_widget_show(box);
				gtk_box_pack_start(GTK_BOX(parent), box, FALSE, FALSE, 0);

				gtk_label = gtk_label_new_with_mnemonic(pref_label);
				gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);
				gtk_widget_show(gtk_label);
				gtk_box_pack_start(GTK_BOX(box), gtk_label, FALSE, FALSE, 0);

				if(sg)
					gtk_size_group_add_widget(sg, gtk_label);

				hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
				gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
				gtk_widget_show(hbox);

				spacer = gtk_label_new("    ");
				gtk_box_pack_start(GTK_BOX(hbox), spacer, FALSE, FALSE, 0);
				gtk_widget_show(spacer);

				if ((format & PURPLE_STRING_FORMAT_TYPE_HTML) != 0) {
					frame = pidgin_create_webview(TRUE, &webview, NULL);
				} else {
					frame = pidgin_create_webview(FALSE, &webview, NULL);
					gtk_webview_set_format_functions(GTK_WEBVIEW(webview), 0);
				}

				if (format & PURPLE_STRING_FORMAT_TYPE_MULTILINE) {
					gchar *tmp = purple_strreplace(purple_prefs_get_string(pref_name), "\n", "<br>");
					gtk_webview_append_html(GTK_WEBVIEW(webview), tmp);
					g_free(tmp);
				} else
					gtk_webview_append_html(GTK_WEBVIEW(webview), purple_prefs_get_string(pref_name));
				gtk_label_set_mnemonic_widget(GTK_LABEL(gtk_label), webview);
				gtk_widget_show_all(frame);
				g_object_set_data(G_OBJECT(webview), "pref-key", (gpointer)pref_name);
				g_signal_connect(G_OBJECT(webview), "changed",
				                 G_CALLBACK(webview_cb), NULL);
				g_signal_connect(G_OBJECT(webview), "format-toggled",
				                 G_CALLBACK(webview_cb), NULL);
				gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
			}

			break;
	}
}

static void
make_int_pref(GtkWidget *parent, PurplePluginPref *pref, GtkSizeGroup *sg) {
	GtkWidget *gtk_label;
	const gchar *pref_name;
	const gchar *pref_label;
	gint max, min;

	pref_name = purple_plugin_pref_get_name(pref);
	pref_label = purple_plugin_pref_get_label(pref);

	switch(purple_plugin_pref_get_type(pref)) {
		case PURPLE_PLUGIN_PREF_CHOICE:
			gtk_label = pidgin_prefs_dropdown_from_list(parent, pref_label,
					PURPLE_PREF_INT, pref_name, purple_plugin_pref_get_choices(pref));
			gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0.5);

			if(sg)
				gtk_size_group_add_widget(sg, gtk_label);

			break;
		case PURPLE_PLUGIN_PREF_NONE:
		default:
			purple_plugin_pref_get_bounds(pref, &min, &max);
			pidgin_prefs_labeled_spin_button(parent, pref_label,
					pref_name, min, max, sg);
			break;
	}
}


static void
make_info_pref(GtkWidget *parent, PurplePluginPref *pref) {
	GtkWidget *gtk_label = gtk_label_new(purple_plugin_pref_get_label(pref));
	gtk_misc_set_alignment(GTK_MISC(gtk_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(gtk_label), TRUE);
	gtk_box_pack_start(GTK_BOX(parent), gtk_label, FALSE, FALSE, 0);
	gtk_widget_show(gtk_label);
}


GtkWidget *
pidgin_plugin_pref_create_frame(PurplePluginPrefFrame *frame) {
	GtkWidget *ret, *parent;
	GtkSizeGroup *sg;
	GList *pp;

	g_return_val_if_fail(frame, NULL);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	parent = ret = gtk_vbox_new(FALSE, 16);
#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);
#endif
	gtk_widget_show(ret);

	for(pp = purple_plugin_pref_frame_get_prefs(frame);
		pp != NULL;
		pp = pp->next)
	{
		PurplePluginPref *pref = (PurplePluginPref *)pp->data;

		const char *name = purple_plugin_pref_get_name(pref);
		const char *label = purple_plugin_pref_get_label(pref);

		if(name == NULL) {
			if(label == NULL)
				continue;

			if(purple_plugin_pref_get_type(pref) == PURPLE_PLUGIN_PREF_INFO) {
				make_info_pref(parent, pref);
			} else {
				parent = pidgin_make_frame(ret, label);
				gtk_widget_show(parent);
			}

			continue;
		}

		switch(purple_prefs_get_type(name)) {
			case PURPLE_PREF_BOOLEAN:
				pidgin_prefs_checkbox(label, name, parent);
				break;
			case PURPLE_PREF_INT:
				make_int_pref(parent, pref, sg);
				break;
			case PURPLE_PREF_STRING:
				make_string_pref(parent, pref, sg);
				break;
			default:
				break;
		}
	}

	g_object_unref(sg);

	return ret;
}
