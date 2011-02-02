/*
 * Configures microphones and webcams for voice and video
 * Copyright (C) 2009 Mike Ruprecht <cmaiku@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */
#include "internal.h"

#include "debug.h"
#include "mediamanager.h"
#include "media-gst.h"
#include "version.h"
#include "gtkplugin.h"
#include "gtkutils.h"
#include "gtkprefs.h"

#include <gst/interfaces/propertyprobe.h>

/* container window for showing a stand-alone configurator */
static GtkWidget *window = NULL;

static PurpleMediaElementInfo *old_video_src = NULL, *old_video_sink = NULL,
		*old_audio_src = NULL, *old_audio_sink = NULL;

static const gchar *AUDIO_SRC_PLUGINS[] = {
	"alsasrc",	"ALSA",
	/* "esdmon",	"ESD", ? */
	"osssrc",	"OSS",
	"pulsesrc",	"PulseAudio",
	/* "audiotestsrc wave=silence", "Silence", */
	"audiotestsrc",	"Test Sound",
	NULL
};

static const gchar *AUDIO_SINK_PLUGINS[] = {
	"alsasink",	"ALSA",
	"artsdsink",	"aRts",
	"esdsink",	"ESD",
	"osssink",	"OSS",
	"pulsesink",	"PulseAudio",
	NULL
};

static const gchar *VIDEO_SRC_PLUGINS[] = {
	"videotestsrc",	"Test Input",
	"dshowvideosrc","DirectDraw",
	"ksvideosrc",	"KS Video",
	"qcamsrc",	"Quickcam",
	"v4lsrc",	"Video4Linux",
	"v4l2src",	"Video4Linux2",
	"v4lmjpegsrc",	"Video4Linux MJPEG",
	NULL
};

static const gchar *VIDEO_SINK_PLUGINS[] = {
	/* "aasink",	"AALib", Didn't work for me */
	"directdrawsink","DirectDraw",
	"glimagesink",	"OpenGL",
	"ximagesink",	"X Window System",
	"xvimagesink",	"X Window System (Xv)",
	NULL
};

static GList *
get_element_devices(const gchar *element_name)
{
	GList *ret = NULL;
	GstElement *element;
	GObjectClass *klass;
	GstPropertyProbe *probe;
	const GParamSpec *pspec;

	ret = g_list_prepend(ret, (gpointer)_("Default"));
	ret = g_list_prepend(ret, "");

	if (!strcmp(element_name, "<custom>") || (*element_name == '\0')) {
		return g_list_reverse(ret);
	}

	element = gst_element_factory_make(element_name, "test");
	klass = G_OBJECT_GET_CLASS (element);
	if (!g_object_class_find_property(klass, "device") ||
			!GST_IS_PROPERTY_PROBE(element) ||
			!(probe = GST_PROPERTY_PROBE(element)) ||
			!(pspec = gst_property_probe_get_property(probe, "device"))) {
		purple_debug_info("vvconfig", "'%s' - no device\n", element_name);
	} else {
		gint n;
		GValueArray *array;

		/* Set autoprobe[-fps] to FALSE to avoid delays when probing. */
		if (g_object_class_find_property (klass, "autoprobe")) {
			g_object_set (G_OBJECT (element), "autoprobe", FALSE, NULL);
			if (g_object_class_find_property (klass, "autoprobe-fps")) {
				g_object_set (G_OBJECT (element), "autoprobe-fps", FALSE, NULL);
			}
		}

		array = gst_property_probe_probe_and_get_values (probe, pspec);
		if (array == NULL) {
			purple_debug_info("vvconfig", "'%s' has no devices\n", element_name);
			return g_list_reverse(ret);
		}

		for (n=0; n < array->n_values; ++n) {
			GValue *device;
			const gchar *name;
			const gchar *device_name;

			device = g_value_array_get_nth(array, n);
			g_object_set_property(G_OBJECT(element), "device", device);
			if (gst_element_set_state(element, GST_STATE_READY)
					!= GST_STATE_CHANGE_SUCCESS) {
				purple_debug_warning("vvconfig",
						"Error changing state of %s\n",
						element_name);
				continue;
			}

			g_object_get(G_OBJECT(element), "device-name", &name, NULL);
			device_name = g_value_get_string(device);
			if (name == NULL)
				name = _("Unknown");
			purple_debug_info("vvconfig", "Found device %s : %s for %s\n",
					device_name, name, element_name);
			ret = g_list_prepend(ret, (gpointer)name);
			ret = g_list_prepend(ret, (gpointer)device_name);
			gst_element_set_state(element, GST_STATE_NULL);
		}
	}
	gst_object_unref(element);

	return g_list_reverse(ret);
}

static GList *
get_element_plugins(const gchar **plugins)
{
	GList *ret = NULL;

	ret = g_list_prepend(ret, "Default");
	ret = g_list_prepend(ret, "");
	for (; plugins[0] && plugins[1]; plugins += 2) {
		if (gst_default_registry_check_feature_version(
				plugins[0], 0, 0, 0)) {
			ret = g_list_prepend(ret, (gpointer)plugins[1]);
			ret = g_list_prepend(ret, (gpointer)plugins[0]);
		}
	}
	ret = g_list_reverse(ret);
	return ret;
}

static void
device_changed_cb(const gchar *name, PurplePrefType type,
		gconstpointer value, gpointer data)
{
	GtkSizeGroup *sg = data;
	GtkWidget *parent, *widget;
	GSList *widgets;
	GList *devices;
	GValue gvalue;
	gint position;
	gchar *label, *pref;

	widgets = gtk_size_group_get_widgets(GTK_SIZE_GROUP(sg));
	for (; widgets; widgets = g_slist_next(widgets)) {
		const gchar *widget_name =
				gtk_widget_get_name(GTK_WIDGET(widgets->data));
		if (!strcmp(widget_name, name)) {
			gchar *temp_str;
			gchar delimiters[3] = {0, 0, 0};
			const gchar *text;
			gint keyval, pos;

			widget = widgets->data;
			/* Get label with _ from the GtkLabel */
			text = gtk_label_get_text(GTK_LABEL(widget));
			keyval = gtk_label_get_mnemonic_keyval(GTK_LABEL(widget));
			delimiters[0] = g_ascii_tolower(keyval);
			delimiters[1] = g_ascii_toupper(keyval);
			pos = strcspn(text, delimiters);
			if (pos != -1) {
				temp_str = g_strndup(text, pos);
				label = g_strconcat(temp_str, "_",
						text + pos, NULL);
				g_free(temp_str);
			} else {
				label = g_strdup(text);
			}
			break;
		}
	}

	if (widgets == NULL)
		return;

	parent = gtk_widget_get_parent(widget);
	widget = parent;
	parent = gtk_widget_get_parent(GTK_WIDGET(widget));
	gvalue.g_type = 0;
	g_value_init(&gvalue, G_TYPE_INT);
	gtk_container_child_get_property(GTK_CONTAINER(parent),
			GTK_WIDGET(widget), "position", &gvalue);
	position = g_value_get_int(&gvalue);
	g_value_unset(&gvalue);
	gtk_widget_destroy(widget);

	pref = g_strdup(name);
	strcpy(pref + strlen(pref) - strlen("plugin"), "device");
	devices = get_element_devices(value);
	if (g_list_find_custom(devices, purple_prefs_get_string(pref),
			(GCompareFunc)strcmp) == NULL)
		purple_prefs_set_string(pref, g_list_next(devices)->data);
	widget = pidgin_prefs_dropdown_from_list(parent,
			label, PURPLE_PREF_STRING,
			pref, devices);
	g_list_free(devices);
	g_signal_connect_swapped(widget, "destroy",
			G_CALLBACK(g_free), pref);
	g_free(label);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);
	gtk_widget_set_name(widget, name);
	gtk_size_group_add_widget(sg, widget);
	gtk_box_reorder_child(GTK_BOX(parent),
			gtk_widget_get_parent(GTK_WIDGET(widget)), position);
}

static void
get_plugin_frame(GtkWidget *parent, GtkSizeGroup *sg,
		const gchar *name, const gchar *plugin_label,
		const gchar **plugin_strs, const gchar *plugin_pref,
		const gchar *device_label, const gchar *device_pref)
{
	GtkWidget *vbox, *widget;
	GList *plugins, *devices;

	vbox = pidgin_make_frame(parent, name);

	/* Setup plugin preference */
	plugins = get_element_plugins(plugin_strs);
	widget = pidgin_prefs_dropdown_from_list(vbox, plugin_label,
			PURPLE_PREF_STRING, plugin_pref, plugins);
	g_list_free(plugins);
	gtk_size_group_add_widget(sg, widget);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);

	/* Setup device preference */
	devices = get_element_devices(purple_prefs_get_string(plugin_pref));
	if (g_list_find_custom(devices, purple_prefs_get_string(device_pref),
			(GCompareFunc) strcmp) == NULL)
		purple_prefs_set_string(device_pref, g_list_next(devices)->data);
	widget = pidgin_prefs_dropdown_from_list(vbox, device_label,
			PURPLE_PREF_STRING, device_pref, devices);
	g_list_free(devices);
	gtk_widget_set_name(widget, plugin_pref);
	gtk_size_group_add_widget(sg, widget);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);

	purple_prefs_connect_callback(vbox, plugin_pref,
			device_changed_cb, sg);
	g_signal_connect_swapped(vbox, "destroy",
			G_CALLBACK(purple_prefs_disconnect_by_handle), vbox);
}

static GtkWidget *
get_plugin_config_frame(PurplePlugin *plugin) {
	GtkWidget *notebook, *vbox_audio, *vbox_video;
	GtkSizeGroup *sg;

	notebook = gtk_notebook_new();
	gtk_container_set_border_width(GTK_CONTAINER(notebook),
			PIDGIN_HIG_BORDER);
	gtk_widget_show(notebook);

	vbox_audio = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	vbox_video = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			vbox_audio, gtk_label_new(_("Audio")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			vbox_video, gtk_label_new(_("Video")));
	gtk_container_set_border_width(GTK_CONTAINER (vbox_audio),
			PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER (vbox_video),
			PIDGIN_HIG_BORDER);

	gtk_widget_show(vbox_audio);
	gtk_widget_show(vbox_video);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	get_plugin_frame(vbox_audio, sg, _("Output"), _("_Plugin"), AUDIO_SINK_PLUGINS,
			"/plugins/core/vvconfig/audio/sink/plugin", _("_Device"),
			"/plugins/core/vvconfig/audio/sink/device");
	get_plugin_frame(vbox_audio, sg, _("Input"), _("P_lugin"), AUDIO_SRC_PLUGINS,
			"/plugins/core/vvconfig/audio/src/plugin", _("D_evice"),
			"/plugins/core/vvconfig/audio/src/device");

	get_plugin_frame(vbox_video, sg, _("Output"), _("_Plugin"), VIDEO_SINK_PLUGINS,
			"/plugins/gtk/vvconfig/video/sink/plugin", _("_Device"),
			"/plugins/gtk/vvconfig/video/sink/device");
	get_plugin_frame(vbox_video, sg, _("Input"), _("P_lugin"), VIDEO_SRC_PLUGINS,
			"/plugins/core/vvconfig/video/src/plugin", _("D_evice"),
			"/plugins/core/vvconfig/video/src/device");

	return notebook;
}

static GstElement *
create_video_src(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	const gchar *plugin = purple_prefs_get_string(
			"/plugins/core/vvconfig/video/src/plugin");
	const gchar *device = purple_prefs_get_string(
			"/plugins/core/vvconfig/video/src/device");
	GstElement *ret;

	if (plugin[0] == '\0')
		return purple_media_element_info_call_create(old_video_src,
				media, session_id, participant);

	ret = gst_element_factory_make(plugin, "vvconfig-videosrc");
	if (device[0] != '\0')
		g_object_set(G_OBJECT(ret), "device", device, NULL);
	if (!strcmp(plugin, "videotestsrc"))
		g_object_set(G_OBJECT(ret), "is-live", 1, NULL);
	return ret;
}

static GstElement *
create_video_sink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	const gchar *plugin = purple_prefs_get_string(
			"/plugins/gtk/vvconfig/video/sink/plugin");
	const gchar *device = purple_prefs_get_string(
			"/plugins/gtk/vvconfig/video/sink/device");
	GstElement *ret;

	if (plugin[0] == '\0')
		return purple_media_element_info_call_create(old_video_sink,
				media, session_id, participant);

	ret = gst_element_factory_make(plugin, NULL);
	if (device[0] != '\0')
		g_object_set(G_OBJECT(ret), "device", device, NULL);
	return ret;
}

static GstElement *
create_audio_src(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	const gchar *plugin = purple_prefs_get_string(
			"/plugins/core/vvconfig/audio/src/plugin");
	const gchar *device = purple_prefs_get_string(
			"/plugins/core/vvconfig/audio/src/device");
	GstElement *ret;

	if (plugin[0] == '\0')
		return purple_media_element_info_call_create(old_audio_src,
				media, session_id, participant);

	ret = gst_element_factory_make(plugin, NULL);
	if (device[0] != '\0')
		g_object_set(G_OBJECT(ret), "device", device, NULL);
	return ret;
}

static GstElement *
create_audio_sink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	const gchar *plugin = purple_prefs_get_string(
			"/plugins/core/vvconfig/audio/sink/plugin");
	const gchar *device = purple_prefs_get_string(
			"/plugins/core/vvconfig/audio/sink/device");
	GstElement *ret;

	if (plugin[0] == '\0')
		return purple_media_element_info_call_create(old_audio_sink,
				media, session_id, participant);

	ret = gst_element_factory_make(plugin, NULL);
	if (device[0] != '\0')
		g_object_set(G_OBJECT(ret), "device", device, NULL);
	return ret;
}

static void
set_element_info_cond(PurpleMediaElementInfo *old_info,
		PurpleMediaElementInfo *new_info, const gchar *id)
{
	gchar *element_id = purple_media_element_info_get_id(old_info);
	if (!strcmp(element_id, id))
		purple_media_manager_set_active_element(
				purple_media_manager_get(), new_info);
	g_free(element_id);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurpleMediaManager *manager;
	PurpleMediaElementInfo *video_src, *video_sink,
			*audio_src, *audio_sink;

	/* Disable the plugin if the UI doesn't support VV */
	if (purple_media_manager_get_ui_caps(purple_media_manager_get()) ==
			PURPLE_MEDIA_CAPS_NONE)
		return FALSE;

	purple_prefs_add_none("/plugins/core/vvconfig");
	purple_prefs_add_none("/plugins/core/vvconfig/audio");
	purple_prefs_add_none("/plugins/core/vvconfig/audio/src");
	purple_prefs_add_string("/plugins/core/vvconfig/audio/src/plugin", "");
	purple_prefs_add_string("/plugins/core/vvconfig/audio/src/device", "");
	purple_prefs_add_none("/plugins/core/vvconfig/audio/sink");
	purple_prefs_add_string("/plugins/core/vvconfig/audio/sink/plugin", "");
	purple_prefs_add_string("/plugins/core/vvconfig/audio/sink/device", "");
	purple_prefs_add_none("/plugins/core/vvconfig/video");
	purple_prefs_add_none("/plugins/core/vvconfig/video/src");
	purple_prefs_add_string("/plugins/core/vvconfig/video/src/plugin", "");
	purple_prefs_add_string("/plugins/core/vvconfig/video/src/device", "");
	purple_prefs_add_none("/plugins/gtk/vvconfig");
	purple_prefs_add_none("/plugins/gtk/vvconfig/video");
	purple_prefs_add_none("/plugins/gtk/vvconfig/video/sink");
	purple_prefs_add_string("/plugins/gtk/vvconfig/video/sink/plugin", "");
	purple_prefs_add_string("/plugins/gtk/vvconfig/video/sink/device", "");

	video_src = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "vvconfig-videosrc",
			"name", "VV Conf Plugin Video Source",
			"type", PURPLE_MEDIA_ELEMENT_VIDEO
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC
					| PURPLE_MEDIA_ELEMENT_UNIQUE,
			"create-cb", create_video_src, NULL);
	video_sink = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "vvconfig-videosink",
			"name", "VV Conf Plugin Video Sink",
			"type", PURPLE_MEDIA_ELEMENT_VIDEO
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_video_sink, NULL);
	audio_src = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "vvconfig-audiosrc",
			"name", "VV Conf Plugin Audio Source",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC
					| PURPLE_MEDIA_ELEMENT_UNIQUE,
			"create-cb", create_audio_src, NULL);
	audio_sink = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "vvconfig-audiosink",
			"name", "VV Conf Plugin Audio Sink",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_audio_sink, NULL);

	purple_debug_info("gtkmedia", "Registering media element types\n");
	manager = purple_media_manager_get();

	old_video_src = purple_media_manager_get_active_element(manager,
			PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	old_video_sink = purple_media_manager_get_active_element(manager,
			PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);
	old_audio_src = purple_media_manager_get_active_element(manager,
			PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	old_audio_sink = purple_media_manager_get_active_element(manager,
			PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);

	set_element_info_cond(old_video_src, video_src, "pidgindefaultvideosrc");
	set_element_info_cond(old_video_sink, video_sink, "pidgindefaultvideosink");
	set_element_info_cond(old_audio_src, audio_src, "pidgindefaultaudiosrc");
	set_element_info_cond(old_audio_sink, audio_sink, "pidgindefaultaudiosink");

	return TRUE;
}

static void
config_destroy(GtkWidget *w, gpointer nul)
{
	purple_debug_info("vvconfig", "closing vv configuration window\n");
	window = NULL;
}

static void
config_close(GtkWidget *w, gpointer nul)
{
	gtk_widget_destroy(GTK_WIDGET(window));
}

static void
show_config(PurplePluginAction *action)
{
	if (!window) {
		GtkWidget *vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
		GtkWidget *hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
		GtkWidget *config_frame = get_plugin_config_frame(NULL);
		GtkWidget *close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

		gtk_container_add(GTK_CONTAINER(vbox), config_frame);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		window = pidgin_create_window(_("Voice/Video Settings"),
			PIDGIN_HIG_BORDER, NULL, TRUE);
		g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(config_destroy), NULL);
		g_signal_connect(G_OBJECT(close), "clicked",
		    G_CALLBACK(config_close), NULL);
		gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, PIDGIN_HIG_BORDER);
		gtk_container_add(GTK_CONTAINER(window), vbox);
		gtk_widget_show(GTK_WIDGET(close));
		gtk_widget_show(GTK_WIDGET(vbox));
		gtk_widget_show(GTK_WIDGET(hbox));
	}
	gtk_window_present(GTK_WINDOW(window));
}


static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("Voice and Video Settings"),
		show_config);
	l = g_list_append(l, act);

	return l;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	PurpleMediaManager *manager = purple_media_manager_get();
	purple_media_manager_set_active_element(manager, old_video_src);
	purple_media_manager_set_active_element(manager, old_video_sink);
	purple_media_manager_set_active_element(manager, old_audio_src);
	purple_media_manager_set_active_element(manager, old_audio_sink);
	return TRUE;
}

static PidginPluginUiInfo ui_info = {
	get_plugin_config_frame,
	0,   /* page_num (Reserved) */
	/* Padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,                         /**< magic          */
	PURPLE_MAJOR_VERSION,                        /**< major version  */
	PURPLE_MINOR_VERSION,                        /**< minor version  */
	PURPLE_PLUGIN_STANDARD,                      /**< type           */
	PIDGIN_PLUGIN_TYPE,                          /**< ui_requirement */
	0,                                           /**< flags          */
	NULL,                                        /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                     /**< priority       */

	"gtk-maiku-vvconfig",                        /**< id             */
	N_("Voice/Video Settings"),                  /**< name           */
	DISPLAY_VERSION,                             /**< version        */
	N_("Configure your microphone and webcam."), /**< summary        */
	N_("Configure microphone and webcam "
	   "settings for voice/video calls."),       /**< description    */
	"Mike Ruprecht <cmaiku@gmail.com>",          /**< author         */
	PURPLE_WEBSITE,                              /**< homepage       */

	plugin_load,                                 /**< load           */
	plugin_unload,                               /**< unload         */
	NULL,                                        /**< destroy        */

	&ui_info,                                    /**< ui_info        */
	NULL,                                        /**< extra_info     */
	NULL,                                        /**< prefs_info     */
	actions,                                     /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin) {
}

PURPLE_INIT_PLUGIN(vvconfig, init_plugin, info)
