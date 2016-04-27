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

#if GST_CHECK_VERSION(1,0,0)
#include <gst/video/videooverlay.h>
#else
#include <gst/interfaces/propertyprobe.h>
#endif

/* container window for showing a stand-alone configurator */
static GtkWidget *window = NULL;

static PurpleMediaElementInfo *old_video_src = NULL, *old_video_sink = NULL,
		*old_audio_src = NULL, *old_audio_sink = NULL;

static const gchar *AUDIO_SRC_PLUGINS[] = {
	"alsasrc",	"ALSA",
	/* "esdmon",	"ESD", ? */
	"osssrc",	"OSS",
	"pulsesrc",	"PulseAudio",
	"sndiosrc",	"sndio",
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
	"sndiosink",	"sndio",
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
#if !GST_CHECK_VERSION(1,0,0)
	GstPropertyProbe *probe;
	const GParamSpec *pspec;
#endif

	ret = g_list_prepend(ret, (gpointer)_("Default"));
	ret = g_list_prepend(ret, "");

	if (!strcmp(element_name, "<custom>") || (*element_name == '\0')) {
		return g_list_reverse(ret);
	}

	element = gst_element_factory_make(element_name, "test");
	if(!element) {
		purple_debug_info("vvconfig", "'%s' - unable to find element\n", element_name);
		return g_list_reverse(ret);
	}

	klass = G_OBJECT_GET_CLASS (element);
	if(!klass) {
		purple_debug_info("vvconfig", "'%s' - unable to find G_Object Class\n", element_name);
		return g_list_reverse(ret);
	}

#if GST_CHECK_VERSION(1,0,0)
	purple_debug_info("vvconfig", "'%s' - gstreamer-1.0 doesn't suport "
		"property probing\n", element_name);
#else
	if (!g_object_class_find_property(klass, "device") ||
			!GST_IS_PROPERTY_PROBE(element) ||
			!(probe = GST_PROPERTY_PROBE(element)) ||
			!(pspec = gst_property_probe_get_property(probe, "device"))) {
		purple_debug_info("vvconfig", "'%s' - no device\n", element_name);
	} else {
		gsize n;
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
			/* GValueArray is in gstreamer-0.10 API */
			device = g_value_array_get_nth(array, n);
G_GNUC_END_IGNORE_DEPRECATIONS
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
#endif
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
#if GST_CHECK_VERSION(1,0,0)
		if (gst_registry_check_feature_version(gst_registry_get(),
				plugins[0], 0, 0, 0)) {
#else
		if (gst_default_registry_check_feature_version(
				plugins[0], 0, 0, 0)) {
#endif
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
config_destroy(GtkObject *w, gpointer nul)
{
	purple_debug_info("vvconfig", "closing vv configuration window\n");
	window = NULL;
}

static void
config_close(GtkObject *w, gpointer nul)
{
	gtk_widget_destroy(GTK_WIDGET(window));
}

typedef GtkWidget *(*FrameCreateCb)(PurplePlugin *plugin);

static void
show_config(PurplePluginAction *action)
{
	if (!window) {
		FrameCreateCb create_frame = action->user_data;
		GtkWidget *vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
		GtkWidget *hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
		GtkWidget *config_frame = create_frame(NULL);
		GtkWidget *close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

		gtk_container_add(GTK_CONTAINER(vbox), config_frame);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		window = pidgin_create_window(action->label,
			PIDGIN_HIG_BORDER, NULL, FALSE);
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

static GstElement *
create_pipeline()
{
	GstElement *pipeline = gst_pipeline_new("voicetest");
	GstElement *src = create_audio_src(NULL, NULL, NULL);
	GstElement *sink = create_audio_sink(NULL, NULL, NULL);
	GstElement *volume = gst_element_factory_make("volume", "volume");
	GstElement *level = gst_element_factory_make("level", "level");
	GstElement *valve = gst_element_factory_make("valve", "valve");

	gst_bin_add_many(GST_BIN(pipeline), src, volume, level, valve, sink, NULL);
	gst_element_link_many(src, volume, level, valve, sink, NULL);

	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

	return pipeline;
}

static void
on_volume_change_cb(GtkRange *range, GstBin *pipeline)
{
	GstElement *volume;

	g_return_if_fail(pipeline != NULL);

	volume = gst_bin_get_by_name(pipeline, "volume");
	g_object_set(volume, "volume", gtk_range_get_value(range) / 10.0, NULL);
}

static gdouble
gst_msg_db_to_percent(GstMessage *msg, gchar *value_name)
{
	const GValue *list;
	const GValue *value;
	gdouble value_db;
	gdouble percent;

	list = gst_structure_get_value(
				gst_message_get_structure(msg), value_name);
#if GST_CHECK_VERSION(1,0,0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	value = g_value_array_get_nth(g_value_get_boxed(list), 0);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
	value = gst_value_list_get_value(list, 0);
#endif
	value_db = g_value_get_double(value);
	percent = pow(10, value_db / 20);
	return (percent > 1.0) ? 1.0 : percent;
}

typedef struct
{
	GtkProgressBar *level;
	GtkRange *threshold;
} BusCbCtx;

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, BusCbCtx *ctx)
{
	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
		gst_structure_has_name(gst_message_get_structure(msg), "level")) {

		GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
		gchar *name = gst_element_get_name(src);

		if (!strcmp(name, "level")) {
			gdouble percent;
			gdouble threshold;
			GstElement *valve;

			percent = gst_msg_db_to_percent(msg, "rms");
			gtk_progress_bar_set_fraction(ctx->level, percent * 5);

			percent = gst_msg_db_to_percent(msg, "decay");
			threshold = gtk_range_get_value(ctx->threshold) / 100.0;
			valve = gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(src)), "valve");
			g_object_set(valve, "drop", (percent < threshold), NULL);
			g_object_set(ctx->level,
					"text", (percent < threshold) ? _("DROP") : " ", NULL);
		}

		g_free(name);
	}

	return TRUE;
}

static void
voice_test_frame_destroy_cb(GtkObject *w, GstElement *pipeline)
{
	g_return_if_fail(GST_IS_ELEMENT(pipeline));

	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
}

static void
volume_scale_destroy_cb(GtkRange *volume, gpointer nul)
{
	purple_prefs_set_int("/purple/media/audio/volume/input",
			gtk_range_get_value(volume));
}

static gchar*
threshold_value_format_cb(GtkScale *scale, gdouble value)
{
	return g_strdup_printf ("%.*f%%", gtk_scale_get_digits(scale), value);
}

static void
threshold_scale_destroy_cb(GtkRange *threshold, gpointer nul)
{
	purple_prefs_set_int("/purple/media/audio/silence_threshold",
			gtk_range_get_value(threshold));
}

static GtkWidget *
get_voice_test_frame(PurplePlugin *plugin)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	GtkWidget *level = gtk_progress_bar_new();
	GtkWidget *volume = gtk_hscale_new_with_range(0, 100, 1);
	GtkWidget *threshold = gtk_hscale_new_with_range(0, 100, 1);
	GtkWidget *label;
	GtkTable *table = GTK_TABLE(gtk_table_new(2, 2, FALSE));

	GstElement *pipeline;
	GstBus *bus;
	BusCbCtx *ctx;

	g_object_set(vbox, "width-request", 500, NULL);

	gtk_table_set_row_spacings(table, PIDGIN_HIG_BOX_SPACE);
	gtk_table_set_col_spacings(table, PIDGIN_HIG_BOX_SPACE);

	label = gtk_label_new(_("Volume:"));
	g_object_set(label, "xalign", 0.0, NULL);
	gtk_table_attach(table, label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults(table, volume, 1, 2, 0, 1);
	label = gtk_label_new(_("Silence threshold:"));
	g_object_set(label, "xalign", 0.0, "yalign", 1.0, NULL);
	gtk_table_attach(table, label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, threshold, 1, 2, 1, 2);

	gtk_container_add(GTK_CONTAINER(vbox), level);
	gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(table));
	gtk_widget_show_all(vbox);

	pipeline = create_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	ctx = g_new(BusCbCtx, 1);
	ctx->level = GTK_PROGRESS_BAR(level);
	ctx->threshold = GTK_RANGE(threshold);
	g_signal_connect_data(bus, "message", G_CALLBACK(gst_bus_cb),
			ctx, (GClosureNotify)g_free, 0);
	gst_object_unref(bus);

	g_signal_connect(volume, "value-changed",
			(GCallback)on_volume_change_cb, pipeline);

	gtk_range_set_value(GTK_RANGE(volume),
			purple_prefs_get_int("/purple/media/audio/volume/input"));
	gtk_widget_set(volume, "draw-value", FALSE, NULL);

	gtk_range_set_value(GTK_RANGE(threshold),
			purple_prefs_get_int("/purple/media/audio/silence_threshold"));

	g_signal_connect(vbox, "destroy",
			G_CALLBACK(voice_test_frame_destroy_cb), pipeline);
	g_signal_connect(volume, "destroy",
			G_CALLBACK(volume_scale_destroy_cb), NULL);
	g_signal_connect(threshold, "format-value",
			G_CALLBACK(threshold_value_format_cb), NULL);
	g_signal_connect(threshold, "destroy",
			G_CALLBACK(threshold_scale_destroy_cb), NULL);

	return vbox;
}

static GList *
actions(PurplePlugin *plugin, gpointer context)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("Input and Output Settings"),
		show_config);
	act->user_data = get_plugin_config_frame;
	l = g_list_append(l, act);

	act = purple_plugin_action_new(_("Microphone Test"),
		show_config);
	act->user_data = get_voice_test_frame;
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
