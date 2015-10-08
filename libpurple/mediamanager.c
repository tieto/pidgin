/**
 * @file mediamanager.c Media Manager API
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

#include "internal.h"

#include "account.h"
#include "debug.h"
#include "media.h"
#include "mediamanager.h"

#ifdef USE_GSTREAMER
#include "marshallers.h"
#include "media-gst.h"
#endif

#ifdef USE_VV
#include <media/backend-fs2.h>

#ifdef HAVE_FARSIGHT
#include <gst/farsight/fs-element-added-notifier.h>
#else
#include <farstream/fs-element-added-notifier.h>
#endif
#ifdef HAVE_MEDIA_APPLICATION
#include <gst/app/app.h>
#endif

#if GST_CHECK_VERSION(1,0,0)
#include <gst/video/videooverlay.h>
#else
#include <gst/interfaces/xoverlay.h>
#endif

/** @copydoc _PurpleMediaManagerPrivate */
typedef struct _PurpleMediaManagerPrivate PurpleMediaManagerPrivate;
/** @copydoc _PurpleMediaOutputWindow */
typedef struct _PurpleMediaOutputWindow PurpleMediaOutputWindow;
/** @copydoc _PurpleMediaManagerPrivate */
typedef struct _PurpleMediaElementInfoPrivate PurpleMediaElementInfoPrivate;

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

struct _PurpleMediaOutputWindow
{
	gulong id;
	PurpleMedia *media;
	gchar *session_id;
	gchar *participant;
	gulong window_id;
	GstElement *sink;
};

struct _PurpleMediaManagerPrivate
{
	GstElement *pipeline;
	PurpleMediaCaps ui_caps;
	GList *medias;
	GList *private_medias;
	GList *elements;
	GList *output_windows;
	gulong next_output_window_id;
	GType backend_type;
	GstCaps *video_caps;

	PurpleMediaElementInfo *video_src;
	PurpleMediaElementInfo *video_sink;
	PurpleMediaElementInfo *audio_src;
	PurpleMediaElementInfo *audio_sink;

#ifdef HAVE_MEDIA_APPLICATION
	/* Application data streams */
	GList *appdata_info; /* holds PurpleMediaAppDataInfo */
	GMutex appdata_mutex;
	guint appdata_cb_token; /* last used read/write callback token */
#endif
};

#ifdef HAVE_MEDIA_APPLICATION
typedef struct {
	PurpleMedia *media;
	GWeakRef media_ref;
	gchar *session_id;
	gchar *participant;
	PurpleMediaAppDataCallbacks callbacks;
	gpointer user_data;
	GDestroyNotify notify;
	GstAppSrc *appsrc;
	GstAppSink *appsink;
	gint num_samples;
	GstSample *current_sample;
	guint sample_offset;
	gboolean writable;
	gboolean connected;
	guint writable_cb_token;
	guint readable_cb_token;
	guint writable_timer_id;
	guint readable_timer_id;
	GCond readable_cond;
} PurpleMediaAppDataInfo;
#endif

#define PURPLE_MEDIA_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerPrivate))
#define PURPLE_MEDIA_ELEMENT_INFO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_ELEMENT_INFO, PurpleMediaElementInfoPrivate))

static void purple_media_manager_class_init (PurpleMediaManagerClass *klass);
static void purple_media_manager_init (PurpleMediaManager *media);
static void purple_media_manager_finalize (GObject *object);
#ifdef HAVE_MEDIA_APPLICATION
static void free_appdata_info_locked (PurpleMediaAppDataInfo *info);
#endif

static GObjectClass *parent_class = NULL;



enum {
	INIT_MEDIA,
	INIT_PRIVATE_MEDIA,
	UI_CAPS_CHANGED,
	LAST_SIGNAL
};
static guint purple_media_manager_signals[LAST_SIGNAL] = {0};
#endif

GType
purple_media_manager_get_type()
{
#ifdef USE_VV
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMediaManagerClass),
			NULL,
			NULL,
			(GClassInitFunc) purple_media_manager_class_init,
			NULL,
			NULL,
			sizeof(PurpleMediaManager),
			0,
			(GInstanceInitFunc) purple_media_manager_init,
			NULL
		};
		type = g_type_register_static(G_TYPE_OBJECT, "PurpleMediaManager", &info, 0);
	}
	return type;
#else
	return G_TYPE_NONE;
#endif
}

#ifdef USE_VV
static void
purple_media_manager_class_init (PurpleMediaManagerClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = purple_media_manager_finalize;

	purple_media_manager_signals[INIT_MEDIA] = g_signal_new ("init-media",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		purple_smarshal_BOOLEAN__OBJECT_POINTER_STRING,
		G_TYPE_BOOLEAN, 3, PURPLE_TYPE_MEDIA,
		G_TYPE_POINTER, G_TYPE_STRING);

	purple_media_manager_signals[INIT_PRIVATE_MEDIA] =
		g_signal_new ("init-private-media",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			0, NULL, NULL,
			purple_smarshal_BOOLEAN__OBJECT_POINTER_STRING,
			G_TYPE_BOOLEAN, 3, PURPLE_TYPE_MEDIA,
			G_TYPE_POINTER, G_TYPE_STRING);

	purple_media_manager_signals[UI_CAPS_CHANGED] = g_signal_new ("ui-caps-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		purple_smarshal_VOID__FLAGS_FLAGS,
		G_TYPE_NONE, 2, PURPLE_MEDIA_TYPE_CAPS,
		PURPLE_MEDIA_TYPE_CAPS);

	g_type_class_add_private(klass, sizeof(PurpleMediaManagerPrivate));
}

static void
purple_media_manager_init (PurpleMediaManager *media)
{
	media->priv = PURPLE_MEDIA_MANAGER_GET_PRIVATE(media);
	media->priv->medias = NULL;
	media->priv->private_medias = NULL;
	media->priv->next_output_window_id = 1;
	media->priv->backend_type = PURPLE_TYPE_MEDIA_BACKEND_FS2;
#ifdef HAVE_MEDIA_APPLICATION
	media->priv->appdata_info = NULL;
	g_mutex_init (&media->priv->appdata_mutex);
#endif

	purple_prefs_add_none("/purple/media");
	purple_prefs_add_none("/purple/media/audio");
	purple_prefs_add_int("/purple/media/audio/silence_threshold", 5);
	purple_prefs_add_none("/purple/media/audio/volume");
	purple_prefs_add_int("/purple/media/audio/volume/input", 10);
	purple_prefs_add_int("/purple/media/audio/volume/output", 10);
}

static void
purple_media_manager_finalize (GObject *media)
{
	PurpleMediaManagerPrivate *priv = PURPLE_MEDIA_MANAGER_GET_PRIVATE(media);
	for (; priv->medias; priv->medias =
			g_list_delete_link(priv->medias, priv->medias)) {
		g_object_unref(priv->medias->data);
	}
	for (; priv->private_medias; priv->private_medias =
			g_list_delete_link(priv->private_medias, priv->private_medias)) {
		g_object_unref(priv->private_medias->data);
	}
	for (; priv->elements; priv->elements =
			g_list_delete_link(priv->elements, priv->elements)) {
		g_object_unref(priv->elements->data);
	}
	if (priv->video_caps)
		gst_caps_unref(priv->video_caps);
#ifdef HAVE_MEDIA_APPLICATION
	if (priv->appdata_info)
		g_list_free_full (priv->appdata_info,
			(GDestroyNotify) free_appdata_info_locked);
	g_mutex_clear (&priv->appdata_mutex);
#endif

	parent_class->finalize(media);
}
#endif

PurpleMediaManager *
purple_media_manager_get()
{
#ifdef USE_VV
	static PurpleMediaManager *manager = NULL;

	if (manager == NULL)
		manager = PURPLE_MEDIA_MANAGER(g_object_new(purple_media_manager_get_type(), NULL));
	return manager;
#else
	return NULL;
#endif
}

#ifdef USE_VV
static gboolean
pipeline_bus_call(GstBus *bus, GstMessage *msg, PurpleMediaManager *manager)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS:
			purple_debug_info("mediamanager", "End of Stream\n");
			break;
		case GST_MESSAGE_ERROR: {
			gchar *debug = NULL;
			GError *err = NULL;

			gst_message_parse_error(msg, &err, &debug);

			purple_debug_error("mediamanager",
					"gst pipeline error: %s\n",
					err->message);
			g_error_free(err);

			if (debug) {
				purple_debug_error("mediamanager",
						"Debug details: %s\n", debug);
				g_free (debug);
			}
			break;
		}
		default:
			break;
	}
	return TRUE;
}
#endif

#ifdef USE_GSTREAMER
GstElement *
purple_media_manager_get_pipeline(PurpleMediaManager *manager)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	if (manager->priv->pipeline == NULL) {
		FsElementAddedNotifier *notifier;
		gchar *filename;
		GError *err = NULL;
		GKeyFile *keyfile;
		GstBus *bus;
		manager->priv->pipeline = gst_pipeline_new(NULL);

		bus = gst_pipeline_get_bus(
				GST_PIPELINE(manager->priv->pipeline));
		gst_bus_add_signal_watch(GST_BUS(bus));
		g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(pipeline_bus_call), manager);
#if GST_CHECK_VERSION(1,0,0)
		gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL, NULL);
#else
		gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL);
#endif
		gst_object_unref(bus);

		filename = g_build_filename(purple_user_dir(),
				"fs-element.conf", NULL);
		keyfile = g_key_file_new();
		if (!g_key_file_load_from_file(keyfile, filename,
				G_KEY_FILE_NONE, &err)) {
			if (err->code == 4)
				purple_debug_info("mediamanager",
						"Couldn't read "
						"fs-element.conf: %s\n",
						err->message);
			else
				purple_debug_error("mediamanager",
						"Error reading "
						"fs-element.conf: %s\n",
						err->message);
			g_error_free(err);
		}
		g_free(filename);

		/* Hack to make alsasrc stop messing up audio timestamps */
		if (!g_key_file_has_key(keyfile,
				"alsasrc", "slave-method", NULL)) {
			g_key_file_set_integer(keyfile,
					"alsasrc", "slave-method", 2);
		}

		notifier = fs_element_added_notifier_new();
		fs_element_added_notifier_add(notifier,
				GST_BIN(manager->priv->pipeline));
		fs_element_added_notifier_set_properties_from_keyfile(
				notifier, keyfile);

		gst_element_set_state(manager->priv->pipeline,
				GST_STATE_PLAYING);
	}

	return manager->priv->pipeline;
#else
	return NULL;
#endif
}
#endif /* USE_GSTREAMER */

static PurpleMedia *
create_media(PurpleMediaManager *manager,
			  PurpleAccount *account,
			  const char *conference_type,
			  const char *remote_user,
			  gboolean initiator,
			  gboolean private)
{
#ifdef USE_VV
	PurpleMedia *media;
	guint signal_id;

	media = PURPLE_MEDIA(g_object_new(purple_media_get_type(),
			     "manager", manager,
			     "account", account,
			     "conference-type", conference_type,
			     "initiator", initiator,
			     NULL));

	signal_id = private ?
			purple_media_manager_signals[INIT_PRIVATE_MEDIA] :
			purple_media_manager_signals[INIT_MEDIA];

	if (g_signal_has_handler_pending(manager, signal_id, 0, FALSE)) {
		gboolean signal_ret;

		g_signal_emit(manager, signal_id, 0, media, account, remote_user,
				&signal_ret);
		if (signal_ret == FALSE) {
			g_object_unref(media);
			return NULL;
		}
	}

	if (private)
		manager->priv->private_medias = g_list_append(
			manager->priv->private_medias, media);
	else
		manager->priv->medias = g_list_append(manager->priv->medias, media);
	return media;
#else
	return NULL;
#endif
}

static GList *
get_media(PurpleMediaManager *manager, gboolean private)
{
#ifdef USE_VV
	if (private)
		return manager->priv->private_medias;
	else
		return manager->priv->medias;
#else
	return NULL;
#endif
}

static GList *
get_media_by_account(PurpleMediaManager *manager,
		PurpleAccount *account, gboolean private)
{
#ifdef USE_VV
	GList *media = NULL;
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	if (private)
		iter = manager->priv->private_medias;
	else
		iter = manager->priv->medias;
	for (; iter; iter = g_list_next(iter)) {
		if (purple_media_get_account(iter->data) == account) {
			media = g_list_prepend(media, iter->data);
		}
	}

	return media;
#else
	return NULL;
#endif
}

void
purple_media_manager_remove_media(PurpleMediaManager *manager, PurpleMedia *media)
{
#ifdef USE_VV
	GList *list;
	GList **medias;

	g_return_if_fail(manager != NULL);

	if ((list = g_list_find(manager->priv->medias, media))) {
		medias = &manager->priv->medias;
	} else if ((list = g_list_find(manager->priv->private_medias, media))) {
		medias = &manager->priv->private_medias;
	}

	if (list) {
		*medias = g_list_delete_link(*medias, list);

#ifdef HAVE_MEDIA_APPLICATION
		g_mutex_lock (&manager->priv->appdata_mutex);
		for (list = manager->priv->appdata_info; list; list = list->next) {
			PurpleMediaAppDataInfo *info = list->data;

			if (info->media == media) {
				manager->priv->appdata_info = g_list_delete_link (
					manager->priv->appdata_info, list);
				free_appdata_info_locked (info);
			}
		}
		g_mutex_unlock (&manager->priv->appdata_mutex);
#endif
	}
#endif
}

PurpleMedia *
purple_media_manager_create_media(PurpleMediaManager *manager,
				  PurpleAccount *account,
				  const char *conference_type,
				  const char *remote_user,
				  gboolean initiator)
{
	return create_media (manager, account, conference_type,
						  remote_user, initiator, FALSE);
}

GList *
purple_media_manager_get_media(PurpleMediaManager *manager)
{
	return get_media (manager, FALSE);
}

GList *
purple_media_manager_get_media_by_account(PurpleMediaManager *manager,
		PurpleAccount *account)
{
	return get_media_by_account (manager, account, FALSE);
}

PurpleMedia *
purple_media_manager_create_private_media(PurpleMediaManager *manager,
				  PurpleAccount *account,
				  const char *conference_type,
				  const char *remote_user,
				  gboolean initiator)
{
	return create_media (manager, account, conference_type,
		remote_user, initiator, TRUE);
}

GList *
purple_media_manager_get_private_media(PurpleMediaManager *manager)
{
	return get_media (manager, TRUE);
}

GList *
purple_media_manager_get_private_media_by_account(PurpleMediaManager *manager,
		PurpleAccount *account)
{
	return get_media_by_account (manager, account, TRUE);
}

#ifdef HAVE_MEDIA_APPLICATION
static void
free_appdata_info_locked (PurpleMediaAppDataInfo *info)
{
	if (info->notify)
		info->notify (info->user_data);

	/* Make sure no other thread is using the structure */
	g_free (info->session_id);
	g_free (info->participant);

	/* This lets the potential read or write callbacks waiting for appdata_mutex
	 * know the info structure has been destroyed. */
	info->readable_cb_token = 0;
	info->writable_cb_token = 0;

	if (info->readable_timer_id) {
		purple_timeout_remove (info->readable_timer_id);
		info->readable_timer_id = 0;
	}

	if (info->writable_timer_id) {
		purple_timeout_remove (info->writable_timer_id);
		info->writable_timer_id = 0;
	}

	if (info->current_sample)
		gst_sample_unref (info->current_sample);
	info->current_sample = NULL;

	/* Unblock any reading thread before destroying the GCond */
	g_cond_broadcast (&info->readable_cond);

	g_cond_clear (&info->readable_cond);

	g_slice_free (PurpleMediaAppDataInfo, info);
}

/*
 * Get an app data info struct associated with a session and lock the mutex
 * We don't want to return an info struct and unlock then it gets destroyed
 * so we need to return it with the lock still taken
 */
static PurpleMediaAppDataInfo *
get_app_data_info_and_lock (PurpleMediaManager *manager,
	PurpleMedia *media, const gchar *session_id, const gchar *participant)
{
	GList *i;

	g_mutex_lock (&manager->priv->appdata_mutex);
	for (i = manager->priv->appdata_info; i; i = i->next) {
		PurpleMediaAppDataInfo *info = i->data;

		if (info->media == media &&
			g_strcmp0 (info->session_id, session_id) == 0 &&
			(participant == NULL ||
				g_strcmp0 (info->participant, participant) == 0)) {
			return info;
		}
	}

	return NULL;
}

/*
 * Get an app data info struct associated with a session and lock the mutex
 * if it doesn't exist, we create it.
 */
static PurpleMediaAppDataInfo *
ensure_app_data_info_and_lock (PurpleMediaManager *manager, PurpleMedia *media,
	const gchar *session_id, const gchar *participant)
{
	PurpleMediaAppDataInfo * info = get_app_data_info_and_lock (manager, media,
		session_id, participant);

	if (info == NULL) {
		info = g_slice_new0 (PurpleMediaAppDataInfo);
		info->media = media;
		g_weak_ref_init (&info->media_ref, media);
		info->session_id = g_strdup (session_id);
		info->participant = g_strdup (participant);
		g_cond_init (&info->readable_cond);
		manager->priv->appdata_info = g_list_prepend (
			manager->priv->appdata_info, info);
	}

	return info;
}
#endif


#ifdef USE_VV
static void
request_pad_unlinked_cb(GstPad *pad, GstPad *peer, gpointer user_data)
{
	GstElement *parent = GST_ELEMENT_PARENT(pad);
	GstIterator *iter;
#if GST_CHECK_VERSION(1,0,0)
	GValue tmp = G_VALUE_INIT;
#endif
	GstPad *remaining_pad;
	GstIteratorResult result;

	gst_element_release_request_pad(parent, pad);

	iter = gst_element_iterate_src_pads(parent);

#if GST_CHECK_VERSION(1,0,0)
	result = gst_iterator_next(iter, &tmp);
#else
	result = gst_iterator_next(iter, (gpointer)&remaining_pad);
#endif

	if (result == GST_ITERATOR_DONE) {
		gst_element_set_locked_state(parent, TRUE);
		gst_element_set_state(parent, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(parent)), parent);
	} else if (result == GST_ITERATOR_OK) {
#if GST_CHECK_VERSION(1,0,0)
		remaining_pad = g_value_get_object(&tmp);
		g_value_reset(&tmp);
#endif
		gst_object_unref(remaining_pad);
	}

	gst_iterator_free(iter);
}

static void
nonunique_src_unlinked_cb(GstPad *pad, GstPad *peer, gpointer user_data)
{
	GstElement *element = GST_ELEMENT_PARENT(pad);
	gst_element_set_locked_state(element, TRUE);
	gst_element_set_state(element, GST_STATE_NULL);
	gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(element)), element);
}

#endif

#ifdef USE_GSTREAMER

void
purple_media_manager_set_video_caps(PurpleMediaManager *manager, GstCaps *caps)
{
#ifdef USE_VV
	if (manager->priv->video_caps)
		gst_caps_unref(manager->priv->video_caps);

	manager->priv->video_caps = caps;

	if (manager->priv->pipeline && manager->priv->video_src) {
		gchar *id = purple_media_element_info_get_id(manager->priv->video_src);
		GstElement *src = gst_bin_get_by_name(GST_BIN(manager->priv->pipeline), id);

		if (src) {
			GstElement *capsfilter = gst_bin_get_by_name(GST_BIN(src), "prpl_video_caps");
			if (capsfilter) {
				g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
				gst_object_unref (capsfilter);
			}
			gst_object_unref (src);
		}

		g_free(id);
	}
#endif
}

GstCaps *
purple_media_manager_get_video_caps(PurpleMediaManager *manager)
{
#ifdef USE_VV
	if (manager->priv->video_caps == NULL)
#if GST_CHECK_VERSION(1,0,0)
		manager->priv->video_caps = gst_caps_from_string("video/x-raw,"
#else
		manager->priv->video_caps = gst_caps_from_string("video/x-raw-yuv,"
#endif
			"width=[250,352], height=[200,288], framerate=[1/1,20/1]");
	return manager->priv->video_caps;
#else
	return NULL;
#endif
}

#ifdef HAVE_MEDIA_APPLICATION
/*
 * Calls the appdata writable callback from the main thread.
 * This needs to grab the appdata lock and make sure it didn't get destroyed
 * before calling the callback.
 */
static gboolean
appsrc_writable (gpointer user_data)
{
	PurpleMediaManager *manager = purple_media_manager_get ();
	PurpleMediaAppDataInfo *info = user_data;
	void (*writable_cb) (PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant, gboolean writable,
		gpointer user_data);
	PurpleMedia *media;
	gchar *session_id;
	gchar *participant;
	gboolean writable;
	gpointer cb_data;
	guint *cb_token_ptr = &info->writable_cb_token;
	guint cb_token = *cb_token_ptr;


	g_mutex_lock (&manager->priv->appdata_mutex);
	if (cb_token == 0 || cb_token != *cb_token_ptr) {
		/* In case info was freed while we were waiting for the mutex to unlock
		 * we still have a pointer to the cb_token which should still be
		 * accessible since it's in the Glib slice allocator. It gets set to 0
		 * just after the timeout is canceled which happens also before the
		 * AppDataInfo is freed, so even if that memory slice gets reused, the
		 * cb_token would be different from its previous value (unless
		 * extremely unlucky). So checking if the value for the cb_token changed
		 * should be enough to prevent any kind of race condition in which the
		 * media/AppDataInfo gets destroyed in one thread while the timeout was
		 * triggered and is waiting on the mutex to get unlocked in this thread
		 */
		g_mutex_unlock (&manager->priv->appdata_mutex);
		return FALSE;
	}
	writable_cb = info->callbacks.writable;
	media = g_weak_ref_get (&info->media_ref);
	session_id = g_strdup (info->session_id);
	participant = g_strdup (info->participant);
	writable = info->writable && info->connected;
	cb_data = info->user_data;

	info->writable_cb_token = 0;
	g_mutex_unlock (&manager->priv->appdata_mutex);


	if (writable_cb && media)
		writable_cb (manager, media, session_id, participant, writable,
			cb_data);

	g_object_unref (media);
	g_free (session_id);
	g_free (participant);

	return FALSE;
}

/*
 * Schedule a writable callback to be called from the main thread.
 * We need to do this because need-data/enough-data signals from appsrc
 * will come from the streaming thread and we need to create
 * a source that we attach to the main context but we can't use
 * g_main_context_invoke since we need to be able to cancel the source if the
 * media gets destroyed.
 * We use a timeout source instead of idle source, so the callback gets a higher
 * priority
 */
static void
call_appsrc_writable_locked (PurpleMediaAppDataInfo *info)
{
	PurpleMediaManager *manager = purple_media_manager_get ();

	/* We already have a writable callback scheduled, don't create another one */
	if (info->writable_cb_token || info->callbacks.writable == NULL)
		return;

	/* We can't use writable_timer_id as a token, because the timeout is added
	 * into libpurple's main event loop, which runs in a different thread than
	 * from where call_appsrc_writable_locked() was called. Consequently, the
	 * callback may run even before purple_timeout_add() returns the timer ID
	 * to us. */
	info->writable_cb_token = ++manager->priv->appdata_cb_token;
	info->writable_timer_id = purple_timeout_add (0, appsrc_writable, info);
}

static void
appsrc_need_data (GstAppSrc *appsrc, guint length, gpointer user_data)
{
	PurpleMediaAppDataInfo *info = user_data;
	PurpleMediaManager *manager = purple_media_manager_get ();

	g_mutex_lock (&manager->priv->appdata_mutex);
	if (!info->writable) {
		info->writable = TRUE;
		/* Only signal writable if we also established a connection */
		if (info->connected)
			call_appsrc_writable_locked (info);
	}
	g_mutex_unlock (&manager->priv->appdata_mutex);
}

static void
appsrc_enough_data (GstAppSrc *appsrc, gpointer user_data)
{
	PurpleMediaAppDataInfo *info = user_data;
	PurpleMediaManager *manager = purple_media_manager_get ();

	g_mutex_lock (&manager->priv->appdata_mutex);
	if (info->writable) {
		info->writable = FALSE;
		call_appsrc_writable_locked (info);
	}
	g_mutex_unlock (&manager->priv->appdata_mutex);
}

static gboolean
appsrc_seek_data (GstAppSrc *appsrc, guint64 offset, gpointer user_data)
{
	return FALSE;
}

static void
appsrc_destroyed (PurpleMediaAppDataInfo *info)
{
	PurpleMediaManager *manager = purple_media_manager_get ();

	g_mutex_lock (&manager->priv->appdata_mutex);
	info->appsrc = NULL;
	if (info->writable) {
		info->writable = FALSE;
		call_appsrc_writable_locked (info);
	}
	g_mutex_unlock (&manager->priv->appdata_mutex);
}

static void
media_established_cb (PurpleMedia *media,const gchar *session_id,
	const gchar *participant, PurpleMediaCandidate *local_candidate,
	PurpleMediaCandidate *remote_candidate, PurpleMediaAppDataInfo *info)
{
	PurpleMediaManager *manager = purple_media_manager_get ();

	g_mutex_lock (&manager->priv->appdata_mutex);
	info->connected = TRUE;
	/* We established the connection, if we were writable, then we need to
	 * signal it now */
	if (info->writable)
		call_appsrc_writable_locked (info);
	g_mutex_unlock (&manager->priv->appdata_mutex);
}

static GstElement *
create_send_appsrc(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	PurpleMediaManager *manager = purple_media_manager_get ();
	PurpleMediaAppDataInfo * info = ensure_app_data_info_and_lock (manager,
		media, session_id, participant);
	GstElement *appsrc = (GstElement *)info->appsrc;

	if (appsrc == NULL) {
		GstAppSrcCallbacks callbacks = {appsrc_need_data, appsrc_enough_data,
										appsrc_seek_data, {NULL}};
		GstCaps *caps = gst_caps_new_empty_simple ("application/octet-stream");

		appsrc = gst_element_factory_make("appsrc", NULL);

		info->appsrc = (GstAppSrc *)appsrc;

		gst_app_src_set_caps (info->appsrc, caps);
		gst_app_src_set_callbacks (info->appsrc,
			&callbacks, info, (GDestroyNotify) appsrc_destroyed);
		g_signal_connect (media, "candidate-pair-established",
			(GCallback) media_established_cb, info);
		gst_caps_unref (caps);
	}

	g_mutex_unlock (&manager->priv->appdata_mutex);
	return appsrc;
}

static void
appsink_eos (GstAppSink *appsink, gpointer user_data)
{
}

static GstFlowReturn
appsink_new_preroll (GstAppSink *appsink, gpointer user_data)
{
	return GST_FLOW_OK;
}

static gboolean
appsink_readable (gpointer user_data)
{
	PurpleMediaManager *manager = purple_media_manager_get ();
	PurpleMediaAppDataInfo *info = user_data;
	void (*readable_cb) (PurpleMediaManager *manager, PurpleMedia *media,
		const gchar *session_id, const gchar *participant, gpointer user_data);
	PurpleMedia *media;
	gchar *session_id;
	gchar *participant;
	gpointer cb_data;
	guint *cb_token_ptr = &info->readable_cb_token;
	guint cb_token = *cb_token_ptr;
	gboolean run_again = FALSE;

	g_mutex_lock (&manager->priv->appdata_mutex);
	if (cb_token == 0 || cb_token != *cb_token_ptr) {
		/* Avoided a race condition (see writable callback) */
		g_mutex_unlock (&manager->priv->appdata_mutex);
		return FALSE;
	}

	if (info->callbacks.readable &&
		(info->num_samples > 0 || info->current_sample != NULL)) {
		readable_cb = info->callbacks.readable;
		media = g_weak_ref_get (&info->media_ref);
		session_id = g_strdup (info->session_id);
		participant = g_strdup (info->participant);
		cb_data = info->user_data;
		g_mutex_unlock (&manager->priv->appdata_mutex);

		if (readable_cb)
			readable_cb (manager, media, session_id, participant, cb_data);

		g_mutex_lock (&manager->priv->appdata_mutex);
		g_object_unref (media);
		g_free (session_id);
		g_free (participant);
		if (cb_token == 0 || cb_token != *cb_token_ptr) {
			/* We got cancelled */
			g_mutex_unlock (&manager->priv->appdata_mutex);
			return FALSE;
		}
	}

	/* Do we still have samples? Schedule appsink_readable again. We break here
	 * so that other events get a chance to be processed too. */
	if (info->num_samples > 0 || info->current_sample != NULL) {
		run_again = TRUE;
	} else {
		info->readable_cb_token = 0;
	}

	g_mutex_unlock (&manager->priv->appdata_mutex);
	return run_again;
}

static void
call_appsink_readable_locked (PurpleMediaAppDataInfo *info)
{
	PurpleMediaManager *manager = purple_media_manager_get ();

	/* We must signal that a new sample has arrived to release blocking reads */
	g_cond_broadcast (&info->readable_cond);

	/* We already have a writable callback scheduled, don't create another one */
	if (info->readable_cb_token || info->callbacks.readable == NULL)
		return;

	info->readable_cb_token = ++manager->priv->appdata_cb_token;
	info->readable_timer_id = purple_timeout_add (0, appsink_readable, info);
}

static GstFlowReturn
appsink_new_sample (GstAppSink *appsink, gpointer user_data)
{
	PurpleMediaManager *manager = purple_media_manager_get ();
	PurpleMediaAppDataInfo *info = user_data;

	g_mutex_lock (&manager->priv->appdata_mutex);
	info->num_samples++;
	call_appsink_readable_locked (info);
	g_mutex_unlock (&manager->priv->appdata_mutex);

	return GST_FLOW_OK;
}

static void
appsink_destroyed (PurpleMediaAppDataInfo *info)
{
	PurpleMediaManager *manager = purple_media_manager_get ();

	g_mutex_lock (&manager->priv->appdata_mutex);
	info->appsink = NULL;
	info->num_samples = 0;
	g_mutex_unlock (&manager->priv->appdata_mutex);
}

static GstElement *
create_recv_appsink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	PurpleMediaManager *manager = purple_media_manager_get ();
	PurpleMediaAppDataInfo * info = ensure_app_data_info_and_lock (manager,
		media, session_id, participant);
	GstElement *appsink = (GstElement *)info->appsink;

	if (appsink == NULL) {
		GstAppSinkCallbacks callbacks = {appsink_eos, appsink_new_preroll,
										 appsink_new_sample, {NULL}};
		GstCaps *caps = gst_caps_new_empty_simple ("application/octet-stream");

		appsink = gst_element_factory_make("appsink", NULL);

		info->appsink = (GstAppSink *)appsink;

		gst_app_sink_set_caps (info->appsink, caps);
		gst_app_sink_set_callbacks (info->appsink,
			&callbacks, info, (GDestroyNotify) appsink_destroyed);
		gst_caps_unref (caps);

	}

	g_mutex_unlock (&manager->priv->appdata_mutex);
	return appsink;
}
#endif

static PurpleMediaElementInfo *
get_send_application_element_info ()
{
	static PurpleMediaElementInfo *info = NULL;

#ifdef HAVE_MEDIA_APPLICATION
	if (info == NULL) {
		info = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidginappsrc",
			"name", "Pidgin Application Source",
			"type", PURPLE_MEDIA_ELEMENT_APPLICATION
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC,
			"create-cb", create_send_appsrc, NULL);
	}
#endif

	return info;
}


static PurpleMediaElementInfo *
get_recv_application_element_info ()
{
	static PurpleMediaElementInfo *info = NULL;

#ifdef HAVE_MEDIA_APPLICATION
	if (info == NULL) {
		info = g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidginappsink",
			"name", "Pidgin Application Sink",
			"type", PURPLE_MEDIA_ELEMENT_APPLICATION
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_recv_appsink, NULL);
	}
#endif

	return info;
}

GstElement *
purple_media_manager_get_element(PurpleMediaManager *manager,
		PurpleMediaSessionType type, PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
#ifdef USE_VV
	GstElement *ret = NULL;
	PurpleMediaElementInfo *info = NULL;
	PurpleMediaElementType element_type;

	if (type & PURPLE_MEDIA_SEND_AUDIO)
		info = manager->priv->audio_src;
	else if (type & PURPLE_MEDIA_RECV_AUDIO)
		info = manager->priv->audio_sink;
	else if (type & PURPLE_MEDIA_SEND_VIDEO)
		info = manager->priv->video_src;
	else if (type & PURPLE_MEDIA_RECV_VIDEO)
		info = manager->priv->video_sink;
	else if (type & PURPLE_MEDIA_SEND_APPLICATION)
		info = get_send_application_element_info ();
	else if (type & PURPLE_MEDIA_RECV_APPLICATION)
		info = get_recv_application_element_info ();

	if (info == NULL)
		return NULL;

	element_type = purple_media_element_info_get_element_type(info);

	if (element_type & PURPLE_MEDIA_ELEMENT_UNIQUE &&
			element_type & PURPLE_MEDIA_ELEMENT_SRC) {
		GstElement *tee;
		GstPad *pad;
		GstPad *ghost;
		gchar *id = purple_media_element_info_get_id(info);

		ret = gst_bin_get_by_name(GST_BIN(
				purple_media_manager_get_pipeline(
				manager)), id);

		if (ret == NULL) {
			GstElement *bin, *fakesink;
			ret = purple_media_element_info_call_create(info,
					media, session_id, participant);
			bin = gst_bin_new(id);
			tee = gst_element_factory_make("tee", "tee");
			gst_bin_add_many(GST_BIN(bin), ret, tee, NULL);

			if (type & PURPLE_MEDIA_SEND_VIDEO) {
				GstElement *videoscale;
				GstElement *capsfilter;

				videoscale = gst_element_factory_make("videoscale", NULL);
				capsfilter = gst_element_factory_make("capsfilter", "prpl_video_caps");

				g_object_set(G_OBJECT(capsfilter),
					"caps", purple_media_manager_get_video_caps(manager), NULL);

				gst_bin_add_many(GST_BIN(bin), videoscale, capsfilter, NULL);
				gst_element_link_many(ret, videoscale, capsfilter, tee, NULL);
			} else
				gst_element_link(ret, tee);

			/*
			 * This shouldn't be necessary, but it stops it from
			 * giving a not-linked error upon destruction
			 */
			fakesink = gst_element_factory_make("fakesink", NULL);
			g_object_set(fakesink, "sync", FALSE, NULL);
			gst_bin_add(GST_BIN(bin), fakesink);
			gst_element_link(tee, fakesink);

			ret = bin;
			gst_object_ref(ret);
			gst_bin_add(GST_BIN(purple_media_manager_get_pipeline(
					manager)), ret);
		}
		g_free(id);

		tee = gst_bin_get_by_name(GST_BIN(ret), "tee");
#if GST_CHECK_VERSION(1,0,0)
		pad = gst_element_get_request_pad(tee, "src_%u");
#else
		pad = gst_element_get_request_pad(tee, "src%d");
#endif
		gst_object_unref(tee);
		ghost = gst_ghost_pad_new(NULL, pad);
		gst_object_unref(pad);
		g_signal_connect(GST_PAD(ghost), "unlinked",
				G_CALLBACK(request_pad_unlinked_cb), NULL);
		gst_pad_set_active(ghost, TRUE);
		gst_element_add_pad(ret, ghost);
	} else {
		ret = purple_media_element_info_call_create(info,
				media, session_id, participant);
		if (element_type & PURPLE_MEDIA_ELEMENT_SRC) {
			GstPad *pad = gst_element_get_static_pad(ret, "src");
			g_signal_connect(pad, "unlinked",
					G_CALLBACK(nonunique_src_unlinked_cb), NULL);
			gst_object_unref(pad);
			gst_object_ref(ret);
			gst_bin_add(GST_BIN(purple_media_manager_get_pipeline(manager)),
				ret);
		}
	}

	if (ret == NULL)
		purple_debug_error("media", "Error creating source or sink\n");

	return ret;
#else
	return NULL;
#endif
}

PurpleMediaElementInfo *
purple_media_manager_get_element_info(PurpleMediaManager *manager,
		const gchar *id)
{
#ifdef USE_VV
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	iter = manager->priv->elements;

	for (; iter; iter = g_list_next(iter)) {
		gchar *element_id =
				purple_media_element_info_get_id(iter->data);
		if (!strcmp(element_id, id)) {
			g_free(element_id);
			g_object_ref(iter->data);
			return iter->data;
		}
		g_free(element_id);
	}
#endif

	return NULL;
}

gboolean
purple_media_manager_register_element(PurpleMediaManager *manager,
		PurpleMediaElementInfo *info)
{
#ifdef USE_VV
	PurpleMediaElementInfo *info2;
	gchar *id;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(info != NULL, FALSE);

	id = purple_media_element_info_get_id(info);
	info2 = purple_media_manager_get_element_info(manager, id);
	g_free(id);

	if (info2 != NULL) {
		g_object_unref(info2);
		return FALSE;
	}

	manager->priv->elements =
			g_list_prepend(manager->priv->elements, info);
	return TRUE;
#else
	return FALSE;
#endif
}

gboolean
purple_media_manager_unregister_element(PurpleMediaManager *manager,
		const gchar *id)
{
#ifdef USE_VV
	PurpleMediaElementInfo *info;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);

	info = purple_media_manager_get_element_info(manager, id);

	if (info == NULL) {
		g_object_unref(info);
		return FALSE;
	}

	if (manager->priv->audio_src == info)
		manager->priv->audio_src = NULL;
	if (manager->priv->audio_sink == info)
		manager->priv->audio_sink = NULL;
	if (manager->priv->video_src == info)
		manager->priv->video_src = NULL;
	if (manager->priv->video_sink == info)
		manager->priv->video_sink = NULL;

	manager->priv->elements = g_list_remove(
			manager->priv->elements, info);
	g_object_unref(info);
	return TRUE;
#else
	return FALSE;
#endif
}

gboolean
purple_media_manager_set_active_element(PurpleMediaManager *manager,
		PurpleMediaElementInfo *info)
{
#ifdef USE_VV
	PurpleMediaElementInfo *info2;
	PurpleMediaElementType type;
	gboolean ret = FALSE;
	gchar *id;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(info != NULL, FALSE);

	id = purple_media_element_info_get_id(info);
	info2 = purple_media_manager_get_element_info(manager, id);
	g_free(id);

	if (info2 == NULL)
		purple_media_manager_register_element(manager, info);
	else
		g_object_unref(info2);

	type = purple_media_element_info_get_element_type(info);

	if (type & PURPLE_MEDIA_ELEMENT_SRC) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO) {
			manager->priv->audio_src = info;
			ret = TRUE;
		}
		if (type & PURPLE_MEDIA_ELEMENT_VIDEO) {
			manager->priv->video_src = info;
			ret = TRUE;
		}
	}
	if (type & PURPLE_MEDIA_ELEMENT_SINK) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO) {
			manager->priv->audio_sink = info;
			ret = TRUE;
		}
		if (type & PURPLE_MEDIA_ELEMENT_VIDEO) {
			manager->priv->video_sink = info;
			ret = TRUE;
		}
	}

	return ret;
#else
	return FALSE;
#endif
}

PurpleMediaElementInfo *
purple_media_manager_get_active_element(PurpleMediaManager *manager,
		PurpleMediaElementType type)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	if (type & PURPLE_MEDIA_ELEMENT_SRC) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO)
			return manager->priv->audio_src;
		else if (type & PURPLE_MEDIA_ELEMENT_VIDEO)
			return manager->priv->video_src;
		else if (type & PURPLE_MEDIA_ELEMENT_APPLICATION)
			return get_send_application_element_info ();
	} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO)
			return manager->priv->audio_sink;
		else if (type & PURPLE_MEDIA_ELEMENT_VIDEO)
			return manager->priv->video_sink;
		else if (type & PURPLE_MEDIA_ELEMENT_APPLICATION)
			return get_recv_application_element_info ();

	}
#endif

	return NULL;
}
#endif /* USE_GSTREAMER */

#ifdef USE_VV
static void
window_id_cb(GstBus *bus, GstMessage *msg, PurpleMediaOutputWindow *ow)
{
	GstElement *sink;

	if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT
#if GST_CHECK_VERSION(1,0,0)
	 || !gst_is_video_overlay_prepare_window_handle_message(msg))
#else
	 || !gst_structure_has_name(msg->structure, "prepare-xwindow-id"))
#endif
		return;

	sink = GST_ELEMENT(GST_MESSAGE_SRC(msg));
	while (sink != ow->sink) {
		if (sink == NULL)
			return;
		sink = GST_ELEMENT_PARENT(sink);
	}

	g_signal_handlers_disconnect_matched(bus, G_SIGNAL_MATCH_FUNC
			| G_SIGNAL_MATCH_DATA, 0, 0, NULL,
			window_id_cb, ow);

#if GST_CHECK_VERSION(1,0,0)
	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg)),
	                                    ow->window_id);
#elif GST_CHECK_VERSION(0,10,31)
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)),
	                                ow->window_id);
#else
	gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)),
	                             ow->window_id);
#endif
}
#endif

gboolean
purple_media_manager_create_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant)
{
#ifdef USE_VV
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	iter = manager->priv->output_windows;
	for(; iter; iter = g_list_next(iter)) {
		PurpleMediaOutputWindow *ow = iter->data;

		if (ow->sink == NULL && ow->media == media &&
				((participant != NULL &&
				ow->participant != NULL &&
				!strcmp(participant, ow->participant)) ||
				(participant == ow->participant)) &&
				!strcmp(session_id, ow->session_id)) {
			GstBus *bus;
			GstElement *queue, *convert;
			GstElement *tee = purple_media_get_tee(media,
					session_id, participant);

			if (tee == NULL)
				continue;

			queue = gst_element_factory_make("queue", NULL);
#if GST_CHECK_VERSION(1,0,0)
			convert = gst_element_factory_make("videoconvert", NULL);
#else
			convert = gst_element_factory_make("ffmpegcolorspace", NULL);
#endif
			ow->sink = purple_media_manager_get_element(
					manager, PURPLE_MEDIA_RECV_VIDEO,
					ow->media, ow->session_id,
					ow->participant);

			if (participant == NULL) {
				/* aka this is a preview sink */
				GObjectClass *klass =
						G_OBJECT_GET_CLASS(ow->sink);
				if (g_object_class_find_property(klass,
						"sync"))
					g_object_set(G_OBJECT(ow->sink),
							"sync", FALSE, NULL);
				if (g_object_class_find_property(klass,
						"async"))
					g_object_set(G_OBJECT(ow->sink),
							"async", FALSE, NULL);
			}

			gst_bin_add_many(GST_BIN(GST_ELEMENT_PARENT(tee)),
					queue, convert, ow->sink, NULL);

			bus = gst_pipeline_get_bus(GST_PIPELINE(
					manager->priv->pipeline));
			g_signal_connect(bus, "sync-message::element",
					G_CALLBACK(window_id_cb), ow);
			gst_object_unref(bus);

			gst_element_set_state(ow->sink, GST_STATE_PLAYING);
			gst_element_set_state(convert, GST_STATE_PLAYING);
			gst_element_set_state(queue, GST_STATE_PLAYING);
			gst_element_link(convert, ow->sink);
			gst_element_link(queue, convert);
			gst_element_link(tee, queue);
		}
	}
	return TRUE;
#else
	return FALSE;
#endif
}

gulong
purple_media_manager_set_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id)
{
#ifdef USE_VV
	PurpleMediaOutputWindow *output_window;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	output_window = g_new0(PurpleMediaOutputWindow, 1);
	output_window->id = manager->priv->next_output_window_id++;
	output_window->media = media;
	output_window->session_id = g_strdup(session_id);
	output_window->participant = g_strdup(participant);
	output_window->window_id = window_id;

	manager->priv->output_windows = g_list_prepend(
			manager->priv->output_windows, output_window);

	if (purple_media_get_tee(media, session_id, participant) != NULL)
		purple_media_manager_create_output_window(manager,
				media, session_id, participant);

	return output_window->id;
#else
	return 0;
#endif
}

gboolean
purple_media_manager_remove_output_window(PurpleMediaManager *manager,
		gulong output_window_id)
{
#ifdef USE_VV
	PurpleMediaOutputWindow *output_window = NULL;
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);

	iter = manager->priv->output_windows;
	for (; iter; iter = g_list_next(iter)) {
		PurpleMediaOutputWindow *ow = iter->data;
		if (ow->id == output_window_id) {
			manager->priv->output_windows = g_list_delete_link(
					manager->priv->output_windows, iter);
			output_window = ow;
			break;
		}
	}

	if (output_window == NULL)
		return FALSE;

	if (output_window->sink != NULL) {
		GstPad *pad = gst_element_get_static_pad(
				output_window->sink, "sink");
		GstPad *peer = gst_pad_get_peer(pad);
		GstElement *colorspace = GST_ELEMENT_PARENT(peer), *queue;
		gst_object_unref(pad);
		gst_object_unref(peer);
		pad = gst_element_get_static_pad(colorspace, "sink");
		peer = gst_pad_get_peer(pad);
		queue = GST_ELEMENT_PARENT(peer);
		gst_object_unref(pad);
		gst_object_unref(peer);
		pad = gst_element_get_static_pad(queue, "sink");
		peer = gst_pad_get_peer(pad);
		gst_object_unref(pad);
		if (peer != NULL)
			gst_element_release_request_pad(GST_ELEMENT_PARENT(peer), peer);
		gst_element_set_locked_state(queue, TRUE);
		gst_element_set_state(queue, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(queue)), queue);
		gst_element_set_locked_state(colorspace, TRUE);
		gst_element_set_state(colorspace, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(colorspace)), colorspace);
		gst_element_set_locked_state(output_window->sink, TRUE);
		gst_element_set_state(output_window->sink, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(output_window->sink)),
				output_window->sink);
	}

	g_free(output_window->session_id);
	g_free(output_window->participant);
	g_free(output_window);

	return TRUE;
#else
	return FALSE;
#endif
}

void
purple_media_manager_remove_output_windows(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant)
{
#ifdef USE_VV
	GList *iter;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	iter = manager->priv->output_windows;

	for (; iter;) {
		PurpleMediaOutputWindow *ow = iter->data;
		iter = g_list_next(iter);

	if (media == ow->media &&
			((session_id != NULL && ow->session_id != NULL &&
			!strcmp(session_id, ow->session_id)) ||
			(session_id == ow->session_id)) &&
			((participant != NULL && ow->participant != NULL &&
			!strcmp(participant, ow->participant)) ||
			(participant == ow->participant)))
		purple_media_manager_remove_output_window(
				manager, ow->id);
	}
#endif
}

void
purple_media_manager_set_ui_caps(PurpleMediaManager *manager,
		PurpleMediaCaps caps)
{
#ifdef USE_VV
	PurpleMediaCaps oldcaps;

	g_return_if_fail(PURPLE_IS_MEDIA_MANAGER(manager));

	oldcaps = manager->priv->ui_caps;
	manager->priv->ui_caps = caps;

	if (caps != oldcaps)
		g_signal_emit(manager,
				purple_media_manager_signals[UI_CAPS_CHANGED],
				0, caps, oldcaps);
#endif
}

PurpleMediaCaps
purple_media_manager_get_ui_caps(PurpleMediaManager *manager)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager),
			PURPLE_MEDIA_CAPS_NONE);
	return manager->priv->ui_caps;
#else
	return PURPLE_MEDIA_CAPS_NONE;
#endif
}

void
purple_media_manager_set_backend_type(PurpleMediaManager *manager,
		GType backend_type)
{
#ifdef USE_VV
	g_return_if_fail(PURPLE_IS_MEDIA_MANAGER(manager));

	manager->priv->backend_type = backend_type;
#endif
}

GType
purple_media_manager_get_backend_type(PurpleMediaManager *manager)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager),
			PURPLE_MEDIA_CAPS_NONE);

	return manager->priv->backend_type;
#else
	return G_TYPE_NONE;
#endif
}

void
purple_media_manager_set_application_data_callbacks(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant, PurpleMediaAppDataCallbacks *callbacks,
		gpointer user_data, GDestroyNotify notify)
{
#ifdef HAVE_MEDIA_APPLICATION
	PurpleMediaAppDataInfo * info = ensure_app_data_info_and_lock (manager,
		media, session_id, participant);

	if (info->notify)
		info->notify (info->user_data);

	if (info->readable_cb_token) {
		purple_timeout_remove (info->readable_timer_id);
		info->readable_cb_token = 0;
	}

	if (info->writable_cb_token) {
		purple_timeout_remove (info->writable_timer_id);
		info->writable_cb_token = 0;
	}

	if (callbacks) {
		info->callbacks = *callbacks;
	} else {
		info->callbacks.writable = NULL;
		info->callbacks.readable = NULL;
	}
	info->user_data = user_data;
	info->notify = notify;

	call_appsrc_writable_locked (info);
	if (info->num_samples > 0 || info->current_sample != NULL)
		call_appsink_readable_locked (info);

	g_mutex_unlock (&manager->priv->appdata_mutex);
#endif
}

gint
purple_media_manager_send_application_data (
	PurpleMediaManager *manager, PurpleMedia *media, const gchar *session_id,
	const gchar *participant, gpointer buffer, guint size, gboolean blocking)
{
#ifdef HAVE_MEDIA_APPLICATION
	PurpleMediaAppDataInfo * info = get_app_data_info_and_lock (manager,
		media, session_id, participant);

	if (info && info->appsrc && info->connected) {
		GstBuffer *gstbuffer = gst_buffer_new_wrapped (g_memdup (buffer, size),
			size);
		GstAppSrc *appsrc = gst_object_ref (info->appsrc);

		g_mutex_unlock (&manager->priv->appdata_mutex);
		if (gst_app_src_push_buffer (appsrc, gstbuffer) == GST_FLOW_OK) {
			if (blocking) {
				GstPad *srcpad;

				srcpad = gst_element_get_static_pad (GST_ELEMENT (appsrc),
					"src");
				if (srcpad) {
					gst_pad_peer_query (srcpad, gst_query_new_drain ());
					gst_object_unref (srcpad);
				}
			}
			gst_object_unref (appsrc);
			return size;
		} else {
			gst_object_unref (appsrc);
			return -1;
		}
	}
	g_mutex_unlock (&manager->priv->appdata_mutex);
	return -1;
#else
	return -1;
#endif
}

gint
purple_media_manager_receive_application_data (
	PurpleMediaManager *manager, PurpleMedia *media, const gchar *session_id,
	const gchar *participant, gpointer buffer, guint max_size,
	gboolean blocking)
{
#ifdef HAVE_MEDIA_APPLICATION
	PurpleMediaAppDataInfo * info = get_app_data_info_and_lock (manager,
		media, session_id, participant);
	guint bytes_read = 0;

	if (info) {
		/* If we are in a blocking read, we need to loop until max_size data
		 * is read into the buffer, if we're not, then we need to read as much
		 * data as possible
		 */
		do {
			if (!info->current_sample && info->appsink && info->num_samples > 0) {
				info->current_sample = gst_app_sink_pull_sample (info->appsink);
				info->sample_offset = 0;
				if (info->current_sample)
					info->num_samples--;
			}

			if (info->current_sample) {
				GstBuffer *gstbuffer = gst_sample_get_buffer (
					info->current_sample);

				if (gstbuffer) {
					GstMapInfo mapinfo;
					guint bytes_to_copy;

					gst_buffer_map (gstbuffer, &mapinfo, GST_MAP_READ);
					/* We must copy only the data remaining in the buffer without
					 * overflowing the buffer */
					bytes_to_copy = max_size - bytes_read;
					if (bytes_to_copy > mapinfo.size - info->sample_offset)
						bytes_to_copy = mapinfo.size - info->sample_offset;
					memcpy ((guint8 *)buffer + bytes_read,
						mapinfo.data + info->sample_offset,	bytes_to_copy);

					gst_buffer_unmap (gstbuffer, &mapinfo);
					info->sample_offset += bytes_to_copy;
					bytes_read += bytes_to_copy;
					if (info->sample_offset == mapinfo.size) {
						gst_sample_unref (info->current_sample);
						info->current_sample = NULL;
						info->sample_offset = 0;
					}
				} else {
					/* In case there's no buffer in the sample (should never
					 * happen), we need to at least unref it */
					gst_sample_unref (info->current_sample);
					info->current_sample = NULL;
					info->sample_offset = 0;
				}
			}

			/* If blocking, wait until there's an available sample */
			while (bytes_read < max_size && blocking &&
				info->current_sample == NULL && info->num_samples == 0) {
				g_cond_wait (&info->readable_cond, &manager->priv->appdata_mutex);

				/* We've been signaled, we need to unlock and regrab the info
				 * struct to make sure nothing changed */
				g_mutex_unlock (&manager->priv->appdata_mutex);
				info = get_app_data_info_and_lock (manager,
					media, session_id, participant);
				if (info == NULL || info->appsink == NULL) {
					/* The session was destroyed while we were waiting, we
					 * should return here */
					g_mutex_unlock (&manager->priv->appdata_mutex);
					return bytes_read;
				}
			}
		} while (bytes_read < max_size &&
			(blocking || info->num_samples > 0));

		g_mutex_unlock (&manager->priv->appdata_mutex);
		return bytes_read;
	}
	g_mutex_unlock (&manager->priv->appdata_mutex);
	return -1;
#else
	return -1;
#endif
}

#ifdef USE_GSTREAMER

/*
 * PurpleMediaElementType
 */

GType
purple_media_element_type_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GFlagsValue values[] = {
			{ PURPLE_MEDIA_ELEMENT_NONE,
				"PURPLE_MEDIA_ELEMENT_NONE", "none" },
			{ PURPLE_MEDIA_ELEMENT_AUDIO,
				"PURPLE_MEDIA_ELEMENT_AUDIO", "audio" },
			{ PURPLE_MEDIA_ELEMENT_VIDEO,
				"PURPLE_MEDIA_ELEMENT_VIDEO", "video" },
			{ PURPLE_MEDIA_ELEMENT_AUDIO_VIDEO,
				"PURPLE_MEDIA_ELEMENT_AUDIO_VIDEO",
				"audio-video" },
			{ PURPLE_MEDIA_ELEMENT_NO_SRCS,
				"PURPLE_MEDIA_ELEMENT_NO_SRCS", "no-srcs" },
			{ PURPLE_MEDIA_ELEMENT_ONE_SRC,
				"PURPLE_MEDIA_ELEMENT_ONE_SRC", "one-src" },
			{ PURPLE_MEDIA_ELEMENT_MULTI_SRC,
				"PURPLE_MEDIA_ELEMENT_MULTI_SRC",
				"multi-src" },
			{ PURPLE_MEDIA_ELEMENT_REQUEST_SRC,
				"PURPLE_MEDIA_ELEMENT_REQUEST_SRC",
				"request-src" },
			{ PURPLE_MEDIA_ELEMENT_NO_SINKS,
				"PURPLE_MEDIA_ELEMENT_NO_SINKS", "no-sinks" },
			{ PURPLE_MEDIA_ELEMENT_ONE_SINK,
				"PURPLE_MEDIA_ELEMENT_ONE_SINK", "one-sink" },
			{ PURPLE_MEDIA_ELEMENT_MULTI_SINK,
				"PURPLE_MEDIA_ELEMENT_MULTI_SINK",
				"multi-sink" },
			{ PURPLE_MEDIA_ELEMENT_REQUEST_SINK,
				"PURPLE_MEDIA_ELEMENT_REQUEST_SINK",
				"request-sink" },
			{ PURPLE_MEDIA_ELEMENT_UNIQUE,
				"PURPLE_MEDIA_ELEMENT_UNIQUE", "unique" },
			{ PURPLE_MEDIA_ELEMENT_SRC,
				"PURPLE_MEDIA_ELEMENT_SRC", "src" },
			{ PURPLE_MEDIA_ELEMENT_SINK,
				"PURPLE_MEDIA_ELEMENT_SINK", "sink" },
			{ PURPLE_MEDIA_ELEMENT_APPLICATION,
				"PURPLE_MEDIA_ELEMENT_APPLICATION", "application" },
			{ 0, NULL, NULL }
		};
		type = g_flags_register_static(
				"PurpleMediaElementType", values);
	}
	return type;
}

/*
 * PurpleMediaElementInfo
 */

struct _PurpleMediaElementInfoClass
{
	GObjectClass parent_class;
};

struct _PurpleMediaElementInfo
{
	GObject parent;
};

#ifdef USE_VV
struct _PurpleMediaElementInfoPrivate
{
	gchar *id;
	gchar *name;
	PurpleMediaElementType type;
	PurpleMediaElementCreateCallback create;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_TYPE,
	PROP_CREATE_CB,
};

static void
purple_media_element_info_init(PurpleMediaElementInfo *info)
{
	PurpleMediaElementInfoPrivate *priv =
			PURPLE_MEDIA_ELEMENT_INFO_GET_PRIVATE(info);
	priv->id = NULL;
	priv->name = NULL;
	priv->type = PURPLE_MEDIA_ELEMENT_NONE;
	priv->create = NULL;
}

static void
purple_media_element_info_finalize(GObject *info)
{
	PurpleMediaElementInfoPrivate *priv =
			PURPLE_MEDIA_ELEMENT_INFO_GET_PRIVATE(info);
	g_free(priv->id);
	g_free(priv->name);
}

static void
purple_media_element_info_set_property (GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	PurpleMediaElementInfoPrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(object));

	priv = PURPLE_MEDIA_ELEMENT_INFO_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_ID:
			g_free(priv->id);
			priv->id = g_value_dup_string(value);
			break;
		case PROP_NAME:
			g_free(priv->name);
			priv->name = g_value_dup_string(value);
			break;
		case PROP_TYPE: {
			priv->type = g_value_get_flags(value);
			break;
		}
		case PROP_CREATE_CB:
			priv->create = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_element_info_get_property (GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	PurpleMediaElementInfoPrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(object));

	priv = PURPLE_MEDIA_ELEMENT_INFO_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_ID:
			g_value_set_string(value, priv->id);
			break;
		case PROP_NAME:
			g_value_set_string(value, priv->name);
			break;
		case PROP_TYPE:
			g_value_set_flags(value, priv->type);
			break;
		case PROP_CREATE_CB:
			g_value_set_pointer(value, priv->create);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_element_info_class_init(PurpleMediaElementInfoClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;

	gobject_class->finalize = purple_media_element_info_finalize;
	gobject_class->set_property = purple_media_element_info_set_property;
	gobject_class->get_property = purple_media_element_info_get_property;

	g_object_class_install_property(gobject_class, PROP_ID,
			g_param_spec_string("id",
			"ID",
			"The unique identifier of the element.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_NAME,
			g_param_spec_string("name",
			"Name",
			"The friendly/display name of this element.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_TYPE,
			g_param_spec_flags("type",
			"Element Type",
			"The type of element this is.",
			PURPLE_TYPE_MEDIA_ELEMENT_TYPE,
			PURPLE_MEDIA_ELEMENT_NONE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CREATE_CB,
			g_param_spec_pointer("create-cb",
			"Create Callback",
			"The function called to create this element.",
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(PurpleMediaElementInfoPrivate));
}

G_DEFINE_TYPE(PurpleMediaElementInfo,
		purple_media_element_info, G_TYPE_OBJECT);
#else
GType
purple_media_element_info_get_type()
{
	return G_TYPE_NONE;
}
#endif

gchar *
purple_media_element_info_get_id(PurpleMediaElementInfo *info)
{
#ifdef USE_VV
	gchar *id;
	g_return_val_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(info), NULL);
	g_object_get(info, "id", &id, NULL);
	return id;
#else
	return NULL;
#endif
}

gchar *
purple_media_element_info_get_name(PurpleMediaElementInfo *info)
{
#ifdef USE_VV
	gchar *name;
	g_return_val_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(info), NULL);
	g_object_get(info, "name", &name, NULL);
	return name;
#else
	return NULL;
#endif
}

PurpleMediaElementType
purple_media_element_info_get_element_type(PurpleMediaElementInfo *info)
{
#ifdef USE_VV
	PurpleMediaElementType type;
	g_return_val_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(info),
			PURPLE_MEDIA_ELEMENT_NONE);
	g_object_get(info, "type", &type, NULL);
	return type;
#else
	return PURPLE_MEDIA_ELEMENT_NONE;
#endif
}

GstElement *
purple_media_element_info_call_create(PurpleMediaElementInfo *info,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant)
{
#ifdef USE_VV
	PurpleMediaElementCreateCallback create;
	g_return_val_if_fail(PURPLE_IS_MEDIA_ELEMENT_INFO(info), NULL);
	g_object_get(info, "create-cb", &create, NULL);
	if (create)
		return create(media, session_id, participant);
#endif
	return NULL;
}


#endif /* USE_GSTREAMER */

