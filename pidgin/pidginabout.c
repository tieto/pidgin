#include <gdk-pixbuf/gdk-pixbuf.h>
#include <json-glib/json-glib.h>

#include "package_revision.h"
#include "pidginabout.h"
#include "pidginresources.h"
#include "internal.h"
#include "gtkutils.h"
#include "gtkwebview.h"

#include <stdio.h>

#include "config.h"
#ifdef HAVE_MESON_CONFIG
#include "meson-config.h"
#else
#error HAVE_MESON_CONFIG is not defined
#endif

struct _PidginAboutDialogPrivate {
	GtkWidget *application_name;
	GtkWidget *stack;

	GtkWidget *main_scrolled_window;

	GtkWidget *developers_button;
	GtkWidget *developers_page;
	GtkWidget *developers_treeview;
	GtkTreeStore *developers_store;

	GtkWidget *translators_button;
	GtkWidget *translators_page;
	GtkWidget *translators_treeview;
	GtkTreeStore *translators_store;

	GtkWidget *build_info_button;
	GtkWidget *build_info_page;
	GtkWidget *build_info_treeview;
	GtkTreeStore *build_info_store;

	gboolean switching_pages;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
_pidgin_about_dialog_switch_page(PidginAboutDialog *about, const gchar *name) {
	about->priv->switching_pages = TRUE;

	gtk_stack_set_visible_child_name(GTK_STACK(about->priv->stack), name);

	/* now figure out if developers button is active */
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(about->priv->developers_button),
		g_str_equal("developers", name)
	);

	/* is the translators button active? */
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(about->priv->translators_button),
		g_str_equal("translators", name)
	);

	/* is the build info button active? */
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(about->priv->build_info_button),
		g_str_equal("build-info", name)
	);

	about->priv->switching_pages = FALSE;
}

static void
_pidgin_about_dialog_load_application_name(PidginAboutDialog *about) {
	gchar *label = g_strdup_printf(
		"%s %s",
		PIDGIN_NAME,
		VERSION
	);

	gtk_label_set_text(GTK_LABEL(about->priv->application_name), label);

	g_free(label);
}

static void
_pidgin_about_dialog_load_main_page(PidginAboutDialog *about) {
	GtkWidget *webview = NULL;
	GInputStream *istream = NULL;
	GString *str = NULL;
	gchar buffer[8192];
	gssize read = 0;

	/* create our webview */
	webview = pidgin_webview_new(FALSE);
	pidgin_setup_webview(webview);
	pidgin_webview_set_format_functions(PIDGIN_WEBVIEW(webview), PIDGIN_WEBVIEW_ALL ^ PIDGIN_WEBVIEW_SMILEY);

	gtk_container_add(GTK_CONTAINER(about->priv->main_scrolled_window), webview);

	/* now load the html */
	istream = g_resource_open_stream(
		pidgin_get_resource(),
		"/im/pidgin/Pidgin/About/about.html",
		G_RESOURCE_LOOKUP_FLAGS_NONE,
		NULL
	);

	str = g_string_new("");

	while((read = g_input_stream_read(istream, buffer, sizeof(buffer), NULL, NULL)) > 0) {
		g_string_append_len(str, (gchar *)buffer, read);
	}

	pidgin_webview_append_html(PIDGIN_WEBVIEW(webview), str->str);

	g_string_free(str, TRUE);

	g_input_stream_close(istream, NULL, NULL);
}

static void
_pidgin_about_dialog_load_developers(PidginAboutDialog *about) {
	GInputStream *istream = NULL;
	GList *l = NULL, *sections = NULL;
	GError *error = NULL;
	JsonParser *parser = NULL;
	JsonNode *root_node = NULL;
	JsonObject *root_object = NULL;
	JsonArray *developers = NULL;

	/* get a stream to the developers resource */
	istream = g_resource_open_stream(
		pidgin_get_resource(),
		"/im/pidgin/Pidgin/About/credits.json",
		G_RESOURCE_LOOKUP_FLAGS_NONE,
		NULL
	);

	/* create our parser */
	parser = json_parser_new();

	if(!json_parser_load_from_stream(parser, istream, NULL, &error)) {
		g_critical("%s", error->message);
	}

	root_node = json_parser_get_root(parser);
	root_object = json_node_get_object(root_node);

	developers = json_object_get_array_member(root_object, "developers");
	sections = json_array_get_elements(developers);

	for(l = sections; l; l = l->next) {
		GtkTreeIter section_iter;
		GList *ll = NULL, *people = NULL;
		JsonObject *section = json_node_get_object(l->data);
		JsonArray *people_array = NULL;
		gchar *markup = NULL;

		markup = g_strdup_printf(
			"<span font_weight=\"bold\" font_size=\"large\">%s</span>",
			json_object_get_string_member(section, "title")
		);

		gtk_tree_store_append(about->priv->developers_store, &section_iter, NULL);
		gtk_tree_store_set(
			about->priv->developers_store,
			&section_iter,
			0, markup,
			1, 0.5f,
			-1
		);

		g_free(markup);

		people_array = json_object_get_array_member(section, "people");
		people = json_array_get_elements(people_array);

		for(ll = people; ll; ll = ll->next) {
			GtkTreeIter person_iter;
			gchar *markup = g_strdup(json_node_get_string(ll->data));

			gtk_tree_store_append(about->priv->developers_store, &person_iter, &section_iter);
			gtk_tree_store_set(
				about->priv->developers_store,
				&person_iter,
				0, markup,
				1, 0.5f,
				-1
			);

			g_free(markup);
		}

		g_list_free(people);
	}

	g_list_free(sections);

	/* clean up */
	g_object_unref(G_OBJECT(parser));

	g_input_stream_close(istream, NULL, NULL);
}

static void
_pidgin_about_dialog_load_translators(PidginAboutDialog *about) {
	GInputStream *istream = NULL;
	GList *l = NULL, *sections = NULL;
	GError *error = NULL;
	JsonParser *parser = NULL;
	JsonNode *root_node = NULL;
	JsonObject *root_object = NULL;
	JsonArray *languages = NULL;

	/* get a stream to the developers resource */
	istream = g_resource_open_stream(
		pidgin_get_resource(),
		"/im/pidgin/Pidgin/About/credits.json",
		G_RESOURCE_LOOKUP_FLAGS_NONE,
		NULL
	);

	/* create our parser */
	parser = json_parser_new();

	if(!json_parser_load_from_stream(parser, istream, NULL, &error)) {
		g_critical("%s", error->message);
	}

	root_node = json_parser_get_root(parser);
	root_object = json_node_get_object(root_node);

	languages = json_object_get_array_member(root_object, "languages");
	sections = json_array_get_elements(languages);

	for(l = sections; l; l = l->next) {
		GtkTreeIter section_iter;
		GList *ll = NULL, *people = NULL;
		JsonObject *section = json_node_get_object(l->data);
		JsonArray *people_array = NULL;
		gchar *markup = NULL;

		markup = g_strdup_printf(
			"<span font_weight=\"bold\" font_size=\"large\">%s</span>",
			json_object_get_string_member(section, "title")
		);

		gtk_tree_store_append(about->priv->translators_store, &section_iter, NULL);
		gtk_tree_store_set(
			about->priv->translators_store,
			&section_iter,
			0, markup,
			1, 0.5f,
			-1
		);

		g_free(markup);

		people_array = json_object_get_array_member(section, "people");
		people = json_array_get_elements(people_array);

		for(ll = people; ll; ll = ll->next) {
			GtkTreeIter person_iter;
			gchar *markup = g_strdup(json_node_get_string(ll->data));

			gtk_tree_store_append(about->priv->translators_store, &person_iter, &section_iter);
			gtk_tree_store_set(
				about->priv->translators_store,
				&person_iter,
				0, markup,
				1, 0.5f,
				-1
			);

			g_free(markup);
		}

		g_list_free(people);
	}

	g_list_free(sections);

	/* clean up */
	g_object_unref(G_OBJECT(parser));

	g_input_stream_close(istream, NULL, NULL);
}

static void
_pidgin_about_dialog_add_build_args(
	PidginAboutDialog *about,
	const gchar *title,
	const gchar *build_args
) {
	GtkTreeIter section, value;
	gchar **splits = NULL;
	gchar *markup = NULL;
	gint idx = 0;

	markup = g_strdup_printf("<span font-weight=\"bold\">%s</span>", title);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* now walk through the arguments and add them */
	splits = g_strsplit(build_args, " ", -1);
	for(idx = 0; splits[idx]; idx++) {
		gchar **value_split = g_strsplit(splits[idx], "=", 2);

		gtk_tree_store_append(about->priv->build_info_store, &value, &section);
		gtk_tree_store_set(
			about->priv->build_info_store,
			&value,
			0, value_split[0],
			1, value_split[1],
			-1
		);

		g_strfreev(value_split);
	}

	g_strfreev(splits);
}

static void
_pidgin_about_dialog_build_info_add_version(
	GtkTreeStore *store,
	GtkTreeIter *section,
	const gchar *title,
	guint major,
	guint minor,
	guint micro
) {
	GtkTreeIter item;
	gchar *version = g_strdup_printf("%u.%u.%u", major, minor, micro);

	gtk_tree_store_append(store, &item, section);
	gtk_tree_store_set(
		store, &item,
		0, title,
		1, version,
		-1
	);
	g_free(version);
}

static void
_pidgin_about_dialog_load_build_info(PidginAboutDialog *about) {
	GtkTreeIter section, item;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf(
		"<span font-weight=\"bold\">%s</span>",
		_("Build Information")
	);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* add the commit hash */
	gtk_tree_store_append(about->priv->build_info_store, &item, &section);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&item,
		0, "Commit Hash",
		1, REVISION,
		-1
	);

	/* add the purple version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Purple Version"),
		PURPLE_MAJOR_VERSION,
		PURPLE_MINOR_VERSION,
		PURPLE_MICRO_VERSION
	);

	/* add the glib version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("GLib Version"),
		GLIB_MAJOR_VERSION,
		GLIB_MINOR_VERSION,
		GLIB_MICRO_VERSION
	);

	/* add the gtk version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Gtk+ Version"),
		GTK_MAJOR_VERSION,
		GTK_MINOR_VERSION,
		GTK_MICRO_VERSION
	);
}

static void
_pidgin_about_dialog_load_runtime_info(PidginAboutDialog *about) {
	GtkTreeIter section;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf(
		"<span font-weight=\"bold\">%s</span>",
		_("Runtime Information")
	);
	gtk_tree_store_append(about->priv->build_info_store, &section, NULL);
	gtk_tree_store_set(
		about->priv->build_info_store,
		&section,
		0, markup,
		-1
	);
	g_free(markup);

	/* add the purple version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Purple Version"),
		purple_major_version,
		purple_minor_version,
		purple_micro_version
	);

	/* add the glib version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("GLib Version"),
		glib_major_version,
		glib_minor_version,
		glib_micro_version
	);

	/* add the gtk version */
	_pidgin_about_dialog_build_info_add_version(
		about->priv->build_info_store,
		&section,
		_("Gtk+ Version"),
		gtk_major_version,
		gtk_minor_version,
		gtk_micro_version
	);
}

static void
_pidgin_about_dialog_load_build_configuration(PidginAboutDialog *about) {
#ifdef MESON_ARGS
	_pidgin_about_dialog_add_build_args(about, "Meson Arguments", MESON_ARGS);
#endif /* MESON_ARGS */
#ifdef CONFIG_ARGS
	_pidgin_about_dialog_add_build_args(about, "Configure Arguments", CONFIG_ARGS);
#endif /* CONFIG_ARGS */

	_pidgin_about_dialog_load_build_info(about);
	_pidgin_about_dialog_load_runtime_info(about);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
_pidgin_about_dialog_toggle_developers(GtkToggleButton *b, gpointer d) {
	PidginAboutDialog *about = d;
	gboolean show = FALSE;

	if(about->priv->switching_pages)
		return;

	show = gtk_toggle_button_get_active(b);

	_pidgin_about_dialog_switch_page(d, show ? "developers" : "main");
}

static void
_pidgin_about_dialog_toggle_translators(GtkToggleButton *b, gpointer d) {
	PidginAboutDialog *about = d;
	gboolean show = FALSE;

	if(about->priv->switching_pages)
		return;

	show = gtk_toggle_button_get_active(b);

	_pidgin_about_dialog_switch_page(d, show ? "translators" : "main");
}

static void
_pidgin_about_dialog_toggle_build_info(GtkToggleButton *b, gpointer d) {
	PidginAboutDialog *about = d;
	gboolean show = FALSE;

	if(about->priv->switching_pages)
		return;

	show = gtk_toggle_button_get_active(b);

	_pidgin_about_dialog_switch_page(d, show ? "build-info" : "main");
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
G_DEFINE_TYPE_WITH_PRIVATE(PidginAboutDialog, pidgin_about_dialog, GTK_TYPE_DIALOG);

static void
pidgin_about_dialog_class_init(PidginAboutDialogClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/About/about.ui"
	);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, application_name);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, stack);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, main_scrolled_window);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, developers_button);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, developers_page);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, developers_store);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, developers_treeview);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, translators_button);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, translators_page);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, translators_store);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, translators_treeview);

	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_button);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_page);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_store);
	gtk_widget_class_bind_template_child_private(widget_class, PidginAboutDialog, build_info_treeview);
}

static void
pidgin_about_dialog_init(PidginAboutDialog *about) {
	about->priv = pidgin_about_dialog_get_instance_private(about);

	about->priv->switching_pages = FALSE;

	gtk_widget_init_template(GTK_WIDGET(about));

	/* setup the application name label */
	_pidgin_about_dialog_load_application_name(about);

	/* setup the main page */
	_pidgin_about_dialog_load_main_page(about);

	/* setup the developers stuff */
	g_signal_connect(
		about->priv->developers_button,
		"toggled",
		G_CALLBACK(_pidgin_about_dialog_toggle_developers),
		about
	);

	_pidgin_about_dialog_load_developers(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->priv->developers_treeview));

	/* setup the translators stuff */
	g_signal_connect(
		about->priv->translators_button,
		"toggled",
		G_CALLBACK(_pidgin_about_dialog_toggle_translators),
		about
	);

	_pidgin_about_dialog_load_translators(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->priv->translators_treeview));

	/* setup the build info page */
	g_signal_connect(
		about->priv->build_info_button,
		"toggled",
		G_CALLBACK(_pidgin_about_dialog_toggle_build_info),
		about
	);

	_pidgin_about_dialog_load_build_configuration(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->priv->build_info_treeview));
}

GtkWidget *
pidgin_about_dialog_new(void) {
	GtkWidget *about = NULL;

	about = g_object_new(
		PIDGIN_TYPE_ABOUT_DIALOG,
		"title", "About Pidgin",
		NULL
	);

	return about;
}

