/*
 * Adium Webkit views
 * Copyright (C) 2007
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* This plugins is basically Sadrul's x-chat plugin, but with Webkit
 * instead of xtext.
 */

#define PLUGIN_ID		"gtk-webview-adium-ims"
#define PLUGIN_NAME		"webview-adium-ims"
#define PLUGIN_AUTHOR		"Sean Egan <seanegan@gmail.com>"
#define PURPLE_PLUGINS          "Hell yeah"

/* System headers */
#include <string.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <webkit/webkit.h>

/* Purple headers */
#include <conversation.h>
#include <notify.h>
#include <util.h>
#include <version.h>

/* Pidgin headers */
#include <gtkconv.h>
#include <gtkplugin.h>
#include <gtkwebview.h>
#include <smileyparser.h>

static PurpleConversationUiOps *uiops = NULL;


/* Cache the contents of the HTML files */
char *template_html = NULL;                 /* This is the skeleton: some basic javascript mostly */
char *header_html = NULL;                   /* This is the first thing to be appended to any conversation */
char *footer_html = NULL;		    /* This is the last thing appended to the conversation */
char *incoming_content_html = NULL;         /* This is a received message */
char *outgoing_content_html = NULL;         /* And a sent one */
char *incoming_next_content_html = NULL;    /* The same things, but used when someone sends multiple subsequent */
char *outgoing_next_content_html = NULL;    /* messages in a row */
char *status_html = NULL;                   /* Non-IM status messages */
char *basestyle_css = NULL;		    /* Shared CSS attributes */

/* Cache their lenghts too, to pass into g_string_new_len, avoiding crazy allocation */
gsize template_html_len = 0;
gsize header_html_len = 0;
gsize footer_html_len = 0;
gsize incoming_content_html_len = 0;
gsize outgoing_content_html_len = 0;
gsize incoming_next_content_html_len = 0;
gsize outgoing_next_content_html_len = 0;
gsize status_html_len = 0;
gsize basestyle_css_len = 0;

/* And their paths */
char *style_dir = NULL;
char *template_path = NULL;
char *css_path = NULL;

static void *handle = NULL;
static void* webkit_plugin_get_handle ()
{
	if (handle) return handle;
	else return (handle = g_malloc (1));
}

static void webkit_plugin_free_handle ()
{
	purple_signals_disconnect_by_handle (handle);
	g_free (handle);
}

static char *
replace_message_tokens(char *text, gsize len, PurpleConversation *conv, const char *name, const char *alias, 
			     const char *message, PurpleMessageFlags flags, time_t mtime)
{
	GString *str = g_string_new_len(NULL, len);
	char *cur = text;
	char *prev = cur;

	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		char *fin = NULL;
			
		if (!strncmp(cur, "%message%", strlen("%message%"))) {
			replace = message;
		} else if (!strncmp(cur, "%messageClasses%", strlen("%messageClasses%"))) {
			replace = flags & PURPLE_MESSAGE_SEND ? "outgoing" :
				  flags & PURPLE_MESSAGE_RECV ? "incoming" : "event";
		} else if (!strncmp(cur, "%time", strlen("%time"))) {
			char *format = NULL;
			if (*(cur + strlen("%time")) == '{') {
				char *start = cur + strlen("%time") + 1;
				char *end = strstr(start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup(start, end - start);
				fin = end + 1;
			} 
			replace = purple_utf8_strftime(format ? format : "%X", NULL);
			g_free(format);
		} else if (!strncmp(cur, "%userIconPath%", strlen("%userIconPath%"))) {
			if (flags & PURPLE_MESSAGE_SEND) {
				if (purple_account_get_bool(conv->account, "use-global-buddyicon", TRUE)) {
					replace = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon");
				} else {
					PurpleStoredImage *img = purple_buddy_icons_find_account_icon(conv->account);
					replace = purple_imgstore_get_filename(img);
				}
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = g_build_filename("Outgoing", "buddy_icon.png", NULL);
				}
			} else if (flags & PURPLE_MESSAGE_RECV) {
				PurpleBuddyIcon *icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
				replace = purple_buddy_icon_get_full_path(icon);
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = g_build_filename("Incoming", "buddy_icon.png", NULL);
				}
			}
			
		} else if (!strncmp(cur, "%senderScreenName%", strlen("%senderScreenName%"))) {
			replace = name;
		} else if (!strncmp(cur, "%sender%", strlen("%sender%"))) {
			replace = alias;
		} else if (!strncmp(cur, "%service%", strlen("%service%"))) {
			replace = purple_account_get_protocol_name(conv->account);
		} else {
			cur++;
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		g_string_append(str, replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;	
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}

	}
	
	/* And wrap it up */
	g_string_append(str, prev);
	return g_string_free(str, FALSE);
}

static char *
replace_header_tokens(char *text, gsize len, PurpleConversation *conv)
{
	GString *str = g_string_new_len(NULL, len);
	char *cur = text;
	char *prev = cur;

	if (text == NULL)
		return NULL;

	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		char *fin = NULL;

  		if (!strncmp(cur, "%chatName%", strlen("%chatName%"))) {
			replace = conv->name;
  		} else if (!strncmp(cur, "%sourceName%", strlen("%sourceName%"))) {
			replace = purple_account_get_alias(conv->account);
			if (replace == NULL)
				replace = purple_account_get_username(conv->account);
  		} else if (!strncmp(cur, "%destinationName%", strlen("%destinationName%"))) {
			PurpleBuddy *buddy = purple_find_buddy(conv->account, conv->name);
			if (buddy) {
				replace = purple_buddy_get_alias(buddy);
			} else {
				replace = conv->name;
			}
  		} else if (!strncmp(cur, "%incomingIconPath%", strlen("%incomingIconPath%"))) {
			PurpleBuddyIcon *icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
			replace = purple_buddy_icon_get_full_path(icon);
  		} else if (!strncmp(cur, "%outgoingIconPath%", strlen("%outgoingIconPath%"))) {
  		} else if (!strncmp(cur, "%timeOpened", strlen("%timeOpened"))) {
			char *format = NULL;
			if (*(cur + strlen("%timeOpened")) == '{') {
				char *start = cur + strlen("%timeOpened") + 1;
				char *end = strstr(start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup(start, end - start);
				fin = end + 1;
			} 
			replace = purple_utf8_strftime(format ? format : "%X", NULL);
			g_free(format);
		} else {
		//	cur++;
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		g_string_append(str, replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;	
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}
	}
	
	/* And wrap it up */
	g_string_append(str, prev);
	return g_string_free(str, FALSE);
}

static char *
replace_template_tokens(char *text, int len, char *header, char *footer) {
	GString *str = g_string_new_len(NULL, len);

	char **ms = g_strsplit(text, "%@", 6);
	char *base = NULL;

	if (ms[0] == NULL || ms[1] == NULL || ms[2] == NULL || ms[3] == NULL || ms[4] == NULL || ms[5] == NULL) {
		g_strfreev(ms);
		g_string_free(str, TRUE);
		return NULL;
	}

	g_string_append(str, ms[0]);
	g_string_append(str, "file://");
	base = g_build_filename (style_dir, "Contents", "Resources", "Template.html", NULL);
	g_string_append(str, base);
	g_free (base);

	g_string_append(str, ms[1]);
	if (basestyle_css)
		g_string_append(str, basestyle_css);
	g_string_append(str, ms[2]);
	if (css_path) {
		g_string_append(str, "file://");
		g_string_append(str, css_path);
	}

	g_string_append(str, ms[3]);
	if (header)
		g_string_append(str, header);
	g_string_append(str, ms[4]);
	if (footer)
		g_string_append(str, footer);
	g_string_append(str, ms[5]);
	
	g_strfreev(ms);
	return g_string_free (str, FALSE);
}

static GtkWidget *
get_webkit(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return NULL;
	else 
		return gtkconv->webview;
}

/**
 * Called when either a new PurpleConversation is created
 * or when a PidginConversation changes its active PurpleConversation
 *
 * FIXME: it's not at all clear to me as to how
 * Adium themes handle the case when the PurpleConversation
 * changes.
 */
static void
init_theme_for_webkit (PurpleConversation *conv)
{
	GtkWidget *webkit = PIDGIN_CONVERSATION(conv)->webview;
	char *header, *footer;
	char *template;

	char* basedir = g_build_filename (style_dir, "Contents", "Resources", "Template.html", NULL);
	char* baseuri = g_strdup_printf ("file://%s", basedir);

	header = replace_header_tokens(header_html, header_html_len, conv);
	footer = replace_header_tokens(footer_html, footer_html_len, conv);
	template = replace_template_tokens(template_html, template_html_len + header_html_len, header, footer);

	if (!g_object_get_data (G_OBJECT(webkit), "adium-themed"))
		webkit_web_view_load_string(WEBKIT_WEB_VIEW(webkit), template, "text/html", "UTF-8", baseuri);

	g_object_set_data (G_OBJECT(webkit), "adium-themed", GINT_TO_POINTER(1));

	g_free (basedir);
	g_free (baseuri);
	g_free (header);
	g_free (footer);
	g_free (template);
}


/* restore the non theme version of the conversation window */
static void
finalize_theme_for_webkit (PurpleConversation *conv)
{
	GtkWidget *webview = PIDGIN_CONVERSATION(conv)->webview;
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), "", "text/html", "UTF-8", "");
	g_object_set_data (G_OBJECT(webview), "adium-themed", NULL);
}

struct webkit_script {
	GtkWidget *webkit;
	char *script;
};

static gboolean purple_webkit_execute_script(gpointer _script)
{
	struct webkit_script *script = (struct webkit_script*) _script;
	printf ("%s\n", script->script);
	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(script->webkit), script->script);
	g_free(script->script);
	g_free(script);
	return FALSE;
}

static gboolean purple_webkit_displaying_im_msg (PurpleAccount *account,
						 const char* name,
						 char **pmessage,
						 PurpleConversation *conv,
						 PurpleMessageFlags flags,
						 gpointer data)
{
	GtkWidget *webkit;
	char *message = *pmessage;
	const char *alias = name; /* FIXME: signal doesn't give me alias */
	char *stripped;
	char *message_html;
	char *msg;
	char *escape;
	char *script;
	char *func = "appendMessage";
	char *smileyed;
	time_t mtime = time (NULL); /* FIXME: this should come from the write_conv calback, but the signal doesn't pass this to me */

	struct webkit_script *wk_script;
	PurpleMessageFlags old_flags = GPOINTER_TO_INT(purple_conversation_get_data(conv, "webkit-lastflags")); 

	fprintf (stderr, "hmm.. here %s %s\n", name, message);
	webkit = get_webkit(conv);
	stripped = g_strdup(message);

	if (flags & PURPLE_MESSAGE_SEND && old_flags & PURPLE_MESSAGE_SEND) {
		message_html = outgoing_next_content_html;
		func = "appendNextMessage";
	} else if (flags & PURPLE_MESSAGE_SEND) {
		message_html = outgoing_content_html;
	} else if (flags & PURPLE_MESSAGE_RECV && old_flags & PURPLE_MESSAGE_RECV) {
		message_html = incoming_next_content_html;
		func = "appendNextMessage";
	} else if (flags & PURPLE_MESSAGE_RECV) {
		message_html = incoming_content_html;
	} else {
		message_html = status_html;
	}
	purple_conversation_set_data(conv, "webkit-lastflags", GINT_TO_POINTER(flags));

	smileyed = smiley_parse_markup(stripped, conv->account->protocol_id);
	msg = replace_message_tokens(message_html, 0, conv, name, alias, smileyed, flags, mtime);
	escape = gtk_webview_quote_js_string (msg);
	script = g_strdup_printf("%s(%s)", func, escape);

	wk_script = g_new0(struct webkit_script, 1);
	wk_script->script = script;
	wk_script->webkit = webkit;

	g_idle_add(purple_webkit_execute_script, wk_script);

	g_free(smileyed);
	g_free(msg);
	g_free(stripped);
	g_free(escape);

	return TRUE; /* GtkConv should not handle this guy */
}


static void
webkit_on_converstation_displayed (PidginConversation *gtkconv, gpointer data)
{
	init_theme_for_webkit (gtkconv->active_conv);
}

static void
webkit_on_conversation_switched (PurpleConversation *conv, gpointer data)
{
	init_theme_for_webkit (conv);
}

static void
webkit_on_conversation_hiding (PidginConversation *gtkconv, gpointer data)
{
	/* 
	 * I'm not sure if I need to do anything here, but let's keep
	 * this anyway
	 */
}

static GList*
get_theme_files() 
{
	GList *ret = NULL;
        GDir *variants;
	char *globe = g_build_filename(DATADIR, "pidgin", "webkit", "styles", NULL);
	const char *css_file;
	char *css;

	if (style_dir != NULL) {
		char *variant_dir = g_build_filename(style_dir, "Contents", "Resources", "Variants", NULL);
		variants = g_dir_open(variant_dir, 0, NULL);
		while ((css_file = g_dir_read_name(variants)) != NULL) {
			if (!strstr(css_file, ".css")) {
				continue;
			}
			css = g_build_filename(variant_dir, css_file, NULL);
			ret = g_list_append(ret,css);
		}
		g_dir_close(variants);
		g_free(variant_dir);
	}
	g_free(globe);

	ret = g_list_sort (ret, (GCompareFunc)g_strcmp0);
	return ret;	
}

static void
variant_set_default ()
{

	GList* all;
	GList* copy;
	css_path = g_strdup (purple_prefs_get_string ("/plugins/gtk/adiumthemes/csspath"));

	if (g_file_test (css_path, G_FILE_TEST_EXISTS))
		return;
	else {
		g_free (css_path);
		css_path = NULL;
	}
	
	all = get_theme_files ();
	copy = all;
	if (css_path) {
		g_free (css_path);
		css_path = NULL;
	}
	if (all) {
		css_path = g_strdup (all->data);
		purple_prefs_set_string ("/plugins/gtk/adiumthemes/csspath", css_path);
	}
	
	while (all) {
		g_free (all->data);
		all = g_list_next(all);
	}
	g_list_free (copy);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	char *file;

	if (g_path_is_absolute (purple_user_dir())) 
		style_dir = g_build_filename(purple_user_dir(), "style", NULL);
	else {
		char* cur = g_get_current_dir ();
		style_dir = g_build_filename (cur, purple_user_dir(), "style", NULL);
		g_free (cur);
	}

	variant_set_default ();
	if (!css_path)
		return FALSE;


	template_path = g_build_filename(style_dir, "Contents", "Resources", "Template.html", NULL);
	if (!g_file_test(template_path, G_FILE_TEST_EXISTS)) {
		g_free(template_path);
		template_path = g_build_filename(DATADIR, "pidgin", "webkit", "Template.html", NULL);
	}
	if (!g_file_get_contents(template_path, &template_html, &template_html_len, NULL))
		return FALSE;
	
	file = g_build_filename(style_dir, "Contents", "Resources", "Header.html", NULL);
	g_file_get_contents(file, &header_html, &header_html_len, NULL);

	file = g_build_filename(style_dir, "Contents", "Resources", "Footer.html", NULL);
	g_file_get_contents(file, &footer_html, &footer_html_len, NULL);

	file = g_build_filename(style_dir, "Contents", "Resources", "Incoming", "Content.html", NULL);
	if (!g_file_get_contents(file, &incoming_content_html, &incoming_content_html_len, NULL))
		return FALSE;

	file = g_build_filename(style_dir, "Contents", "Resources", "Incoming", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &incoming_next_content_html, &incoming_next_content_html_len, NULL)) {
		incoming_next_content_html = incoming_content_html;
		incoming_next_content_html_len = incoming_content_html_len;
	}

	file = g_build_filename(style_dir, "Contents", "Resources", "Outgoing", "Content.html", NULL);
	if (!g_file_get_contents(file, &outgoing_content_html, &outgoing_content_html_len, NULL)) {
		outgoing_content_html = incoming_content_html;
		outgoing_content_html_len = incoming_content_html_len;
	}

	file = g_build_filename(style_dir, "Contents", "Resources", "Outgoing", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &outgoing_next_content_html, &outgoing_next_content_html_len, NULL)) {
		outgoing_next_content_html = outgoing_content_html;
		outgoing_next_content_html_len = outgoing_content_html_len;
	}


	file = g_build_filename(style_dir, "Contents", "Resources", "Status.html", NULL);
	if (!g_file_get_contents(file, &status_html, &status_html_len, NULL))
		return FALSE;

	file = g_build_filename(style_dir, "Contents", "Resources", "main.css", NULL);
	g_file_get_contents(file, &basestyle_css, &basestyle_css_len, NULL);

	uiops = pidgin_conversations_get_conv_ui_ops();

	if (uiops == NULL)
		return FALSE;

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "displaying-im-msg",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(purple_webkit_displaying_im_msg),
			       NULL);
			    
	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-displayed",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_converstation_displayed),
			       NULL);

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-switched",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_conversation_switched),
			       NULL);

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-hiding",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_conversation_hiding),
			       NULL);

	/* finally update each of the existing conversation windows */
	{
		GList* list = purple_get_conversations ();
		for (;list; list = g_list_next(list))
			init_theme_for_webkit (list->data);
			
	}
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *list;

	webkit_plugin_free_handle ();

	list = purple_get_conversations ();
	while (list) {
		finalize_theme_for_webkit(list->data);
		list = g_list_next(list);
	}

	return TRUE;
}


static void
variant_update_conversation (PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION (conv);
	WebKitWebView *webview = WEBKIT_WEB_VIEW (gtkconv->webview);
	char* script = g_strdup_printf ("setStylesheet(\"mainStyle\",\"%s\")", css_path);
	printf ("css_path: %s\n", css_path);
	webkit_web_view_execute_script (webview, script);
	g_free (script);
}

static void
variant_changed (GtkWidget* combobox, gpointer null)
{
	char *name, *name_with_ext;
	GList *list;

	g_free (css_path);
	name = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combobox));
	name_with_ext = g_strdup_printf ("%s.css", name);
	g_free (name);
	
	css_path = g_build_filename (style_dir, "Contents", "Resources", "Variants", name_with_ext, NULL);
	purple_prefs_set_string ("/plugins/gtk/adiumthemes/csspath", css_path);
	g_free (name_with_ext);

	/* update each conversation */
	list = purple_get_conversations ();
	while (list) {
		variant_update_conversation (list->data);
		list = g_list_next(list);
	}
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin) {
	GList *themes = get_theme_files();
	GList *theme = themes;
	char *curdir = NULL;
	GtkWidget *combobox = gtk_combo_box_new_text();	
	int def = -1, index = 0;

	while (theme) {
		char *basename = g_path_get_basename(theme->data);
		char *dirname = g_path_get_dirname(theme->data);
		if (!curdir || strcmp(curdir, dirname)) {
			char *plist, *plist_xml;
			gsize plist_len;
			xmlnode *node;
			g_free(curdir);
			curdir = strdup(dirname);
			plist = g_build_filename(curdir, "..", "..", "Info.plist", NULL);
		        if (!g_file_get_contents(plist, &plist_xml, &plist_len, NULL)) {
				continue;
			}
	                node = xmlnode_from_str(plist_xml, plist_len);
			if (!node) continue;
			node = xmlnode_get_child(node, "dict");
			if (!node) continue;
			node = xmlnode_get_child(node, "key");
			while (node && strcmp(xmlnode_get_data(node), "CFBundleName")) {
				node = xmlnode_get_next_twin(node);
			}
			if (!node) continue;
			node = node->next;
			while (node && node->type != XMLNODE_TYPE_TAG) {
				node = node->next;
			}

		}
		
		{
			char *temp = g_strndup (basename, strlen(basename)-4);
			gtk_combo_box_append_text (GTK_COMBO_BOX(combobox), temp);
			g_free (temp);
		}
		if (g_str_has_suffix (css_path, basename))
			def = index;
		index ++;
		theme = theme->next;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX(combobox), def);
	g_signal_connect (G_OBJECT(combobox), "changed", G_CALLBACK(variant_changed), NULL);

	return combobox;
}

PidginPluginUiInfo ui_info =
{
        get_config_frame,
        0, /* page_num (Reserved) */

        /* padding */
        NULL,
        NULL,
        NULL,
        NULL
};


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,		/* Magic				*/
	PURPLE_MAJOR_VERSION,		/* Purple Major Version	*/
	PURPLE_MINOR_VERSION,		/* Purple Minor Version	*/
	PURPLE_PLUGIN_STANDARD,		/* plugin type			*/
PIDGIN_PLUGIN_TYPE,			/* ui requirement		*/
	0,							/* flags				*/
	NULL,						/* dependencies			*/
	PURPLE_PRIORITY_DEFAULT,	/* priority				*/

	PLUGIN_ID,					/* plugin id			*/
	NULL,						/* name					*/
	"0.1",					/* version				*/
	NULL,						/* summary				*/
	NULL,						/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	"http://pidgin.im",					/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,						/* destroy				*/

	&ui_info,						/* ui_info				*/
	NULL,						/* extra_info			*/
	NULL,						/* prefs_info			*/
	NULL,						/* actions				*/
	NULL,						/* reserved 1			*/
	NULL,						/* reserved 2			*/
	NULL,						/* reserved 3			*/
	NULL						/* reserved 4			*/
};

static void
init_plugin(PurplePlugin *plugin) {
	info.name = "Adium IMs";
	info.summary = "Adium-like IMs with Pidgin";
	info.description = "You can chat in Pidgin using Adium's WebKit view.";

	purple_prefs_add_none ("/plugins");
	purple_prefs_add_none ("/plugins/gtk");
	purple_prefs_add_none ("/plugins/gtk/adiumthemes");
	purple_prefs_add_string ("/plugins/gtk/adiumthemes/csspath", "");
}

PURPLE_INIT_PLUGIN(webkit, init_plugin, info)
