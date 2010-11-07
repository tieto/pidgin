/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include "gntbutton.h"
#include "gntentry.h"
#include "gntfilesel.h"
#include "gntlabel.h"
#include "gntmarshal.h"
#include "gntstyle.h"
#include "gnttree.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if 0
#include <glob.h>
#endif

enum
{
	SIG_FILE_SELECTED,
	SIGS
};

static GntWindowClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };
static void (*orig_map)(GntWidget *widget);
static void (*orig_size_request)(GntWidget *widget);

static void select_activated_cb(GntWidget *button, GntFileSel *sel);

static void
gnt_file_sel_destroy(GntWidget *widget)
{
	GntFileSel *sel = GNT_FILE_SEL(widget);
	g_free(sel->current);
	g_free(sel->suggest);
	if (sel->tags) {
		g_list_foreach(sel->tags, (GFunc)g_free, NULL);
		g_list_free(sel->tags);
	}
}

#if !GLIB_CHECK_VERSION(2,8,0)
/* ripped from glib/gfileutils.c */
static gchar *
g_build_path_va (const gchar  *separator,
		gchar       **str_array)
{
	GString *result;
	gint separator_len = strlen (separator);
	gboolean is_first = TRUE;
	gboolean have_leading = FALSE;
	const gchar *single_element = NULL;
	const gchar *next_element;
	const gchar *last_trailing = NULL;
	gint i = 0;

	result = g_string_new (NULL);

	next_element = str_array[i++];

	while (TRUE) {
		const gchar *element;
		const gchar *start;
		const gchar *end;

		if (next_element) {
			element = next_element;
			next_element = str_array[i++];
		} else
			break;

		/* Ignore empty elements */
		if (!*element)
			continue;

		start = element;

		if (separator_len) {
			while (start &&
					strncmp (start, separator, separator_len) == 0)
				start += separator_len;
		}

		end = start + strlen (start);

		if (separator_len) {
			while (end >= start + separator_len &&
					strncmp (end - separator_len, separator, separator_len) == 0)
				end -= separator_len;

			last_trailing = end;
			while (last_trailing >= element + separator_len &&
					strncmp (last_trailing - separator_len, separator, separator_len) == 0)
				last_trailing -= separator_len;

			if (!have_leading) {
				/* If the leading and trailing separator strings are in the
				 * same element and overlap, the result is exactly that element
				 */
				if (last_trailing <= start)
					single_element = element;

				g_string_append_len (result, element, start - element);
				have_leading = TRUE;
			} else
				single_element = NULL;
		}

		if (end == start)
			continue;

		if (!is_first)
			g_string_append (result, separator);

		g_string_append_len (result, start, end - start);
		is_first = FALSE;
	}

	if (single_element) {
		g_string_free (result, TRUE);
		return g_strdup (single_element);
	} else {
		if (last_trailing)
			g_string_append (result, last_trailing);

		return g_string_free (result, FALSE);
	}
}

static gchar *
g_build_pathv (const gchar  *separator,
		gchar       **args)
{
	if (!args)
		return NULL;

	return g_build_path_va (separator, args);
}

#endif

static char *
process_path(const char *path)
{
	char **splits = NULL;
	int i, j;
	char *str, *ret;

	splits = g_strsplit(path, G_DIR_SEPARATOR_S, -1);
	for (i = 0, j = 0; splits[i]; i++) {
		if (strcmp(splits[i], ".") == 0) {
		} else if (strcmp(splits[i], "..") == 0) {
			if (j)
				j--;
		} else {
			if (i != j) {
				g_free(splits[j]);
				splits[j] = splits[i];
				splits[i] = NULL;
			}
			j++;
		}
	}
	g_free(splits[j]);
	splits[j] = NULL;
	str = g_build_pathv(G_DIR_SEPARATOR_S, splits);
	ret = g_strdup_printf(G_DIR_SEPARATOR_S "%s", str);
	g_free(str);
	g_strfreev(splits);
	return ret;
}

static void
update_location(GntFileSel *sel)
{
	char *old;
	const char *tmp;
	tmp = sel->suggest ? sel->suggest :
		(const char*)gnt_tree_get_selection_data(sel->dirsonly ? GNT_TREE(sel->dirs) : GNT_TREE(sel->files));
	old = g_strdup_printf("%s%s%s", SAFE(sel->current), SAFE(sel->current)[1] ? G_DIR_SEPARATOR_S : "", tmp ? tmp : "");
	gnt_entry_set_text(GNT_ENTRY(sel->location), old);
	g_free(old);
}

static gboolean
is_tagged(GntFileSel *sel, const char *f)
{
	char *ret = g_strdup_printf("%s%s%s", sel->current, sel->current[1] ? G_DIR_SEPARATOR_S : "", f);
	gboolean find = g_list_find_custom(sel->tags, ret, (GCompareFunc)g_utf8_collate) != NULL;
	g_free(ret);
	return find;
}

GntFile* gnt_file_new_dir(const char *name)
{
	GntFile *file = g_new0(GntFile, 1);
	file->basename = g_strdup(name);
	file->type = GNT_FILE_DIR;
	return file;
}

GntFile* gnt_file_new(const char *name, unsigned long size)
{
	GntFile *file = g_new0(GntFile, 1);
	file->basename = g_strdup(name);
	file->type = GNT_FILE_REGULAR;
	file->size = size;
	return file;
}

static gboolean
local_read_fn(const char *path, GList **files, GError **error)
{
	GDir *dir;
	GntFile *file;
	const char *str;
	
	dir = g_dir_open(path, 0, error);
	if (dir == NULL || (error && *error)) {
		return FALSE;
	}

	*files = NULL;
	if (*path != '\0' && strcmp(path, G_DIR_SEPARATOR_S)) {
		file = gnt_file_new_dir("..");
		*files = g_list_prepend(*files, file);
	}

	while ((str = g_dir_read_name(dir)) != NULL) {
		char *fp = g_build_filename(path, str, NULL);
		struct stat st;

		if (stat(fp, &st)) {
			g_printerr("Error stating location %s\n", fp);
		} else {
			if (S_ISDIR(st.st_mode)) {
				file = gnt_file_new_dir(str);
			} else {
				file = gnt_file_new(str, (long)st.st_size);
			}
			*files = g_list_prepend(*files, file);
		}
		g_free(fp);
	}
	g_dir_close(dir);

	*files = g_list_reverse(*files);
	return TRUE;
}

static void
gnt_file_free(GntFile *file)
{
	g_free(file->fullpath);
	g_free(file->basename);
	g_free(file);
}

static gboolean
location_changed(GntFileSel *sel, GError **err)
{
	GList *files, *iter;
	gboolean success;

	if (!sel->dirs)
		return TRUE;

	gnt_tree_remove_all(GNT_TREE(sel->dirs));
	if (sel->files)
		gnt_tree_remove_all(GNT_TREE(sel->files));
	gnt_entry_set_text(GNT_ENTRY(sel->location), NULL);
	if (sel->current == NULL) {
		if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(sel), GNT_WIDGET_MAPPED))
			gnt_widget_draw(GNT_WIDGET(sel));
		return TRUE;
	}

	/* XXX:\
	 * XXX: This is blocking.
	 * XXX:/
	 */
	files = NULL;
	if (sel->read_fn)
		success = sel->read_fn(sel->current, &files, err);
	else
		success = local_read_fn(sel->current, &files, err);
	
	if (!success || *err) {
		g_printerr("GntFileSel: error opening location %s (%s)\n",
			sel->current, *err ? (*err)->message : "reason unknown");
		return FALSE;
	}

	for (iter = files; iter; iter = iter->next) {
		GntFile *file = iter->data;
		char *str = file->basename;
		if (file->type == GNT_FILE_DIR) {
			gnt_tree_add_row_after(GNT_TREE(sel->dirs), g_strdup(str),
					gnt_tree_create_row(GNT_TREE(sel->dirs), str), NULL, NULL);
			if (sel->multiselect && sel->dirsonly && is_tagged(sel, str))
				gnt_tree_set_row_flags(GNT_TREE(sel->dirs), (gpointer)str, GNT_TEXT_FLAG_BOLD);
		} else if (!sel->dirsonly) {
			char size[128];
			snprintf(size, sizeof(size), "%ld", file->size);

			gnt_tree_add_row_after(GNT_TREE(sel->files), g_strdup(str),
					gnt_tree_create_row(GNT_TREE(sel->files), str, size, ""), NULL, NULL);
			if (sel->multiselect && is_tagged(sel, str))
				gnt_tree_set_row_flags(GNT_TREE(sel->files), (gpointer)str, GNT_TEXT_FLAG_BOLD);
		}
	}
	g_list_foreach(files, (GFunc)gnt_file_free, NULL);
	g_list_free(files);
	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(sel), GNT_WIDGET_MAPPED))
		gnt_widget_draw(GNT_WIDGET(sel));
	return TRUE;
}

static gboolean
dir_key_pressed(GntTree *tree, const char *key, GntFileSel *sel)
{
	if (strcmp(key, "\r") == 0 || strcmp(key, "\n") == 0) {
		char *str = g_strdup(gnt_tree_get_selection_data(tree));
		char *path, *dir;

		if (!str)
			return TRUE;
		
		path = g_build_filename(sel->current, str, NULL);
		dir = g_path_get_basename(sel->current);
		if (!gnt_file_sel_set_current_location(sel, path)) {
			gnt_tree_set_selected(tree, str);
		} else if (strcmp(str, "..") == 0) {
			gnt_tree_set_selected(tree, dir);
		}
		gnt_bindable_perform_action_named(GNT_BINDABLE(tree), "end-search", NULL);
		g_free(dir);
		g_free(str);
		g_free(path);
		return TRUE;
	}
	return FALSE;
}

static gboolean
location_key_pressed(GntTree *tree, const char *key, GntFileSel *sel)
{
	char *path;
	char *str;
#if 0
	int count;
	glob_t gl;
	struct stat st;
	int glob_ret;
#endif
	if (strcmp(key, "\r") && strcmp(key, "\n"))
		return FALSE;

	str = (char*)gnt_entry_get_text(GNT_ENTRY(sel->location));
	if (*str == G_DIR_SEPARATOR)
		path = g_strdup(str);
	else
		path = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", sel->current, str);
	str = process_path(path);
	g_free(path);
	path = str;

	if (gnt_file_sel_set_current_location(sel, path))
		goto success;

	path = g_path_get_dirname(str);
	g_free(str);

	if (!gnt_file_sel_set_current_location(sel, path)) {
		g_free(path);
		return FALSE;
	}
#if 0
	/* XXX: there needs to be a way to allow other methods for globbing,
	 * like the read_fn stuff. */
	glob_ret = glob(path, GLOB_MARK, NULL, &gl);
	if (!glob_ret) {  /* XXX: do something with the return value */
		char *loc = g_path_get_dirname(gl.gl_pathv[0]);

		stat(gl.gl_pathv[0], &st);
		gnt_file_sel_set_current_location(sel, loc);  /* XXX: check the return value */
		g_free(loc);
		if (!S_ISDIR(st.st_mode) && !sel->dirsonly) {
			gnt_tree_remove_all(GNT_TREE(sel->files));
			for (count = 0; count < gl.gl_pathc; count++) {
				char *tmp = process_path(gl.gl_pathv[count]);
				loc = g_path_get_dirname(tmp);
				if (g_utf8_collate(sel->current, loc) == 0) {
					char *base = g_path_get_basename(tmp);
					char size[128];
					snprintf(size, sizeof(size), "%ld", (long)st.st_size);
					gnt_tree_add_row_after(GNT_TREE(sel->files), base,
							gnt_tree_create_row(GNT_TREE(sel->files), base, size, ""), NULL, NULL);
				}
				g_free(loc);
				g_free(tmp);
			}
			gnt_widget_draw(sel->files);
		}
	} else if (sel->files) {
		gnt_tree_remove_all(GNT_TREE(sel->files));
		gnt_widget_draw(sel->files);
	}
	globfree(&gl);
#endif
success:
	g_free(path);
	return TRUE;
}

static void
file_sel_changed(GntWidget *widget, gpointer old, gpointer current, GntFileSel *sel)
{
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_HAS_FOCUS)) {
		g_free(sel->suggest);
		sel->suggest = NULL;
		update_location(sel);
	}
}

static void
gnt_file_sel_map(GntWidget *widget)
{
	GntFileSel *sel = GNT_FILE_SEL(widget);
	GntWidget *hbox, *vbox;

	if (sel->current == NULL)
		gnt_file_sel_set_current_location(sel, g_get_home_dir());

	vbox = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(vbox), 0);
	gnt_box_set_alignment(GNT_BOX(vbox), GNT_ALIGN_MID);

	/* The dir. and files list */
	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);

	gnt_box_add_widget(GNT_BOX(hbox), sel->dirs);

	if (!sel->dirsonly) {
		gnt_box_add_widget(GNT_BOX(hbox), sel->files);
	} else {
		g_signal_connect(G_OBJECT(sel->dirs), "selection_changed", G_CALLBACK(file_sel_changed), sel);
	}

	gnt_box_add_widget(GNT_BOX(vbox), hbox);
	gnt_box_add_widget(GNT_BOX(vbox), sel->location);

	/* The buttons */
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), sel->cancel);
	gnt_box_add_widget(GNT_BOX(hbox), sel->select);
	gnt_box_add_widget(GNT_BOX(vbox), hbox);

	gnt_box_add_widget(GNT_BOX(sel), vbox);
	orig_map(widget);
	update_location(sel);
}

static gboolean
toggle_tag_selection(GntBindable *bind, GList *null)
{
	GntFileSel *sel = GNT_FILE_SEL(bind);
	char *str;
	GList *find;
	char *file;
	GntWidget *tree;

	if (!sel->multiselect)
		return FALSE;
	tree = sel->dirsonly ? sel->dirs : sel->files;
	if (!gnt_widget_has_focus(tree) ||
			gnt_tree_is_searching(GNT_TREE(tree)))
		return FALSE;

	file = gnt_tree_get_selection_data(GNT_TREE(tree));

	str = gnt_file_sel_get_selected_file(sel);
	if ((find = g_list_find_custom(sel->tags, str, (GCompareFunc)g_utf8_collate)) != NULL) {
		g_free(find->data);
		sel->tags = g_list_delete_link(sel->tags, find);
		gnt_tree_set_row_flags(GNT_TREE(tree), file, GNT_TEXT_FLAG_NORMAL);
		g_free(str);
	} else {
		sel->tags = g_list_prepend(sel->tags, str);
		gnt_tree_set_row_flags(GNT_TREE(tree), file, GNT_TEXT_FLAG_BOLD);
	}

	gnt_bindable_perform_action_named(GNT_BINDABLE(tree), "move-down", NULL);

	return TRUE;
}

static gboolean
clear_tags(GntBindable *bind, GList *null)
{
	GntFileSel *sel = GNT_FILE_SEL(bind);
	GntWidget *tree;
	GList *iter;

	if (!sel->multiselect)
		return FALSE;
	tree = sel->dirsonly ? sel->dirs : sel->files;
	if (!gnt_widget_has_focus(tree) ||
			gnt_tree_is_searching(GNT_TREE(tree)))
		return FALSE;

	g_list_foreach(sel->tags, (GFunc)g_free, NULL);
	g_list_free(sel->tags);
	sel->tags = NULL;

	for (iter = GNT_TREE(tree)->list; iter; iter = iter->next)
		gnt_tree_set_row_flags(GNT_TREE(tree), iter->data, GNT_TEXT_FLAG_NORMAL);

	return TRUE;
}

static gboolean
up_directory(GntBindable *bind, GList *null)
{
	char *path, *dir;
	GntFileSel *sel = GNT_FILE_SEL(bind);
	if (!gnt_widget_has_focus(sel->dirs) &&
			!gnt_widget_has_focus(sel->files))
		return FALSE;
	if (gnt_tree_is_searching(GNT_TREE(sel->dirs)) ||
			gnt_tree_is_searching(GNT_TREE(sel->files)))
		return FALSE;

	path = g_build_filename(sel->current, "..", NULL);
	dir = g_path_get_basename(sel->current);
	if (gnt_file_sel_set_current_location(sel, path))
		gnt_tree_set_selected(GNT_TREE(sel->dirs), dir);
	g_free(dir);
	g_free(path);
	return TRUE;
}

static void
gnt_file_sel_size_request(GntWidget *widget)
{
	GntFileSel *sel;
	if (widget->priv.height > 0)
		return;

	sel = GNT_FILE_SEL(widget);
	sel->dirs->priv.height = 16;
	sel->files->priv.height = 16;
	orig_size_request(widget);
}

static void
gnt_file_sel_class_init(GntFileSelClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	GntWidgetClass *kl = GNT_WIDGET_CLASS(klass);
	parent_class = GNT_WINDOW_CLASS(klass);
	kl->destroy = gnt_file_sel_destroy;
	orig_map = kl->map;
	kl->map = gnt_file_sel_map;
	orig_size_request = kl->size_request;
	kl->size_request = gnt_file_sel_size_request;

	signals[SIG_FILE_SELECTED] = 
		g_signal_new("file_selected",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntFileSelClass, file_selected),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

	gnt_bindable_class_register_action(bindable, "toggle-tag", toggle_tag_selection, "t", NULL);
	gnt_bindable_class_register_action(bindable, "clear-tags", clear_tags, "c", NULL);
	gnt_bindable_class_register_action(bindable, "up-directory", up_directory, GNT_KEY_BACKSPACE, NULL);
	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));

	GNTDEBUG;
}

static void
gnt_file_sel_init(GTypeInstance *instance, gpointer class)
{
	GntFileSel *sel = GNT_FILE_SEL(instance);

	sel->dirs = gnt_tree_new();
	gnt_tree_set_compare_func(GNT_TREE(sel->dirs), (GCompareFunc)g_utf8_collate);
	gnt_tree_set_hash_fns(GNT_TREE(sel->dirs), g_str_hash, g_str_equal, g_free);
	gnt_tree_set_column_titles(GNT_TREE(sel->dirs), "Directories");
	gnt_tree_set_show_title(GNT_TREE(sel->dirs), TRUE);
	gnt_tree_set_col_width(GNT_TREE(sel->dirs), 0, 20);
	g_signal_connect(G_OBJECT(sel->dirs), "key_pressed", G_CALLBACK(dir_key_pressed), sel);

	sel->files = gnt_tree_new_with_columns(2);  /* Name, Size */
	gnt_tree_set_compare_func(GNT_TREE(sel->files), (GCompareFunc)g_utf8_collate);
	gnt_tree_set_column_titles(GNT_TREE(sel->files), "Filename", "Size");
	gnt_tree_set_show_title(GNT_TREE(sel->files), TRUE);
	gnt_tree_set_col_width(GNT_TREE(sel->files), 0, 25);
	gnt_tree_set_col_width(GNT_TREE(sel->files), 1, 10);
	gnt_tree_set_column_is_right_aligned(GNT_TREE(sel->files), 1, TRUE);
	g_signal_connect(G_OBJECT(sel->files), "selection_changed", G_CALLBACK(file_sel_changed), sel);

	/* The location entry */
	sel->location = gnt_entry_new(NULL);
	g_signal_connect(G_OBJECT(sel->location), "key_pressed", G_CALLBACK(location_key_pressed), sel);

	sel->cancel = gnt_button_new("Cancel");
	sel->select = gnt_button_new("Select");

	g_signal_connect_swapped(G_OBJECT(sel->files), "activate", G_CALLBACK(gnt_widget_activate), sel->select);
	g_signal_connect(G_OBJECT(sel->select), "activate", G_CALLBACK(select_activated_cb), sel);
}

/******************************************************************************
 * GntFileSel API
 *****************************************************************************/
GType
gnt_file_sel_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntFileSelClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_file_sel_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntFileSel),
			0,						/* n_preallocs		*/
			gnt_file_sel_init,			/* instance_init	*/
			NULL
		};

		type = g_type_register_static(GNT_TYPE_WINDOW,
									  "GntFileSel",
									  &info, 0);
	}

	return type;
}

static void
select_activated_cb(GntWidget *button, GntFileSel *sel)
{
	char *path = gnt_file_sel_get_selected_file(sel);
	char *file = g_path_get_basename(path);
	g_signal_emit(sel, signals[SIG_FILE_SELECTED], 0, path, file);
	g_free(file);
	g_free(path);
}

GntWidget *gnt_file_sel_new(void)
{
	GntWidget *widget = g_object_new(GNT_TYPE_FILE_SEL, NULL);
	return widget;
}

gboolean gnt_file_sel_set_current_location(GntFileSel *sel, const char *path)
{
	char *old;
	GError *error = NULL;
	gboolean ret = TRUE;

	old = sel->current;
	sel->current = process_path(path);
	if (!location_changed(sel, &error)) {
		g_error_free(error);
		error = NULL;
		g_free(sel->current);
		sel->current = old;
		location_changed(sel, &error);
		ret = FALSE;
	} else
		g_free(old);

	update_location(sel);
	return ret;
}

void gnt_file_sel_set_dirs_only(GntFileSel *sel, gboolean dirs)
{
	sel->dirsonly = dirs;
}

gboolean gnt_file_sel_get_dirs_only(GntFileSel *sel)
{
	return sel->dirsonly;
}

void gnt_file_sel_set_suggested_filename(GntFileSel *sel, const char *suggest)
{
	g_free(sel->suggest);
	sel->suggest = g_strdup(suggest);
}

char *gnt_file_sel_get_selected_file(GntFileSel *sel)
{
	char *ret;
	if (sel->dirsonly) {
		ret = g_path_get_dirname(gnt_entry_get_text(GNT_ENTRY(sel->location)));
	} else {
		ret = g_strdup(gnt_entry_get_text(GNT_ENTRY(sel->location)));
	}
	return ret;
}

void gnt_file_sel_set_must_exist(GntFileSel *sel, gboolean must)
{
	/*XXX: What do I do with this? */
	sel->must_exist = must;
}

gboolean gnt_file_sel_get_must_exist(GntFileSel *sel)
{
	return sel->must_exist;
}

void gnt_file_sel_set_multi_select(GntFileSel *sel, gboolean set)
{
	sel->multiselect = set;
}

GList *gnt_file_sel_get_selected_multi_files(GntFileSel *sel)
{
	GList *list = NULL, *iter;
	char *str = gnt_file_sel_get_selected_file(sel);

	for (iter = sel->tags; iter; iter = iter->next) {
		list = g_list_prepend(list, g_strdup(iter->data));
		if (g_utf8_collate(str, iter->data)) {
			g_free(str);
			str = NULL;
		}
	}
	if (str)
		list = g_list_prepend(list, str);
	list = g_list_reverse(list);
	return list;
}

void gnt_file_sel_set_read_fn(GntFileSel *sel, gboolean (*read_fn)(const char *path, GList **files, GError **error))
{
	sel->read_fn = read_fn;
}


