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

#include <glob.h>

enum
{
	SIG_FILE_SELECTED,
	SIGS
};

static GntWindowClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };
static void (*orig_map)(GntWidget *widget);

static void
gnt_file_sel_destroy(GntWidget *widget)
{
	GntFileSel *sel = GNT_FILE_SEL(widget);
	g_free(sel->current);
}

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
	tmp = (const char*)gnt_tree_get_selection_data(sel->dirsonly ? GNT_TREE(sel->dirs) : GNT_TREE(sel->files));
	old = g_strdup_printf("%s%s%s", sel->current, sel->current[1] ? G_DIR_SEPARATOR_S : "", tmp ? tmp : "");
	gnt_entry_set_text(GNT_ENTRY(sel->location), old);
	g_free(old);
}

static gboolean
location_changed(GntFileSel *sel, GError **err)
{
	GDir *dir;
	const char *str;

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
	
	dir = g_dir_open(sel->current, 0, err);
	if (dir == NULL || *err) {
		g_printerr("GntFileSel: error opening location %s (%s)\n",
			sel->current, *err ? (*err)->message : "reason unknown");
		return FALSE;
	}

	if (*sel->current != '\0' && strcmp(sel->current, G_DIR_SEPARATOR_S))
		gnt_tree_add_row_after(GNT_TREE(sel->dirs), g_strdup(".."),
				gnt_tree_create_row(GNT_TREE(sel->dirs), ".."), NULL, NULL);

	while ((str = g_dir_read_name(dir)) != NULL) {
		char *fp = g_build_filename(sel->current, str, NULL);
		struct stat st;

		if (stat(fp, &st)) {
			g_printerr("Error stating location %s\n", fp);
		} else {
			if (S_ISDIR(st.st_mode))
				gnt_tree_add_row_after(GNT_TREE(sel->dirs), g_strdup(str),
						gnt_tree_create_row(GNT_TREE(sel->dirs), str), NULL, NULL);
			else if (!sel->dirsonly) {
				char size[128];
				snprintf(size, sizeof(size), "%ld", (long)st.st_size);

				gnt_tree_add_row_after(GNT_TREE(sel->files), g_strdup(str),
						gnt_tree_create_row(GNT_TREE(sel->files), str, size, ""), NULL, NULL);
			}
		}
		g_free(fp);
	}
	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(sel), GNT_WIDGET_MAPPED))
		gnt_widget_draw(GNT_WIDGET(sel));
	return TRUE;
}

static gboolean
dir_key_pressed(GntTree *tree, const char *key, GntFileSel *sel)
{
	if (strcmp(key, "\r") == 0) {
		/* XXX: if we are moving up the tree, make sure the current node is selected after the redraw */
		char *str = g_strdup(gnt_tree_get_selection_data(tree));
		char *path = g_build_filename(sel->current, str, NULL);
		char *dir = g_path_get_basename(sel->current);
		if (!gnt_file_sel_set_current_location(sel, path)) {
			gnt_tree_set_selected(tree, str);
		} else if (strcmp(str, "..") == 0) {
			gnt_tree_set_selected(tree, dir);
		}
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
	if (strcmp(key, "\r") == 0) {
		int count;
		glob_t gl;
		char *path;
		char *str;
		struct stat st;
		int glob_ret;

		str = (char*)gnt_entry_get_text(GNT_ENTRY(sel->location));
		if (*str == G_DIR_SEPARATOR)
			path = g_strdup(str);
		else
			path = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", sel->current, str);
		str = process_path(path);
		g_free(path);
		path = str;

		if (!stat(path, &st)) {
			if (S_ISDIR(st.st_mode)) {
				gnt_file_sel_set_current_location(sel, path);
				goto success;
			}
		}

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
success:
		g_free(path);
		return TRUE;
	}
	return FALSE;
}

static void
file_sel_changed(GntWidget *widget, gpointer old, gpointer current, GntFileSel *sel)
{
	update_location(sel);
}

static void
gnt_file_sel_map(GntWidget *widget)
{
	GntFileSel *sel = GNT_FILE_SEL(widget);
	GntWidget *hbox, *vbox;

	vbox = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(vbox), 0);
	gnt_box_set_alignment(GNT_BOX(vbox), GNT_ALIGN_LEFT);

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

static void
gnt_file_sel_class_init(GntFileSelClass *klass)
{
	GntWidgetClass *kl = GNT_WIDGET_CLASS(klass);
	parent_class = GNT_WINDOW_CLASS(klass);
	kl->destroy = gnt_file_sel_destroy;
	orig_map = kl->map;
	kl->map = gnt_file_sel_map;

	signals[SIG_FILE_SELECTED] = 
		g_signal_new("file_selected",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntFileSelClass, file_selected),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 0);
	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));

	GNTDEBUG;
}

static void
gnt_file_sel_init(GTypeInstance *instance, gpointer class)
{
	GNTDEBUG;
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

GntWidget *gnt_file_sel_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_FILE_SEL, NULL);
	GntFileSel *sel = GNT_FILE_SEL(widget);

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
	g_signal_connect(G_OBJECT(sel->files), "selection_changed", G_CALLBACK(file_sel_changed), sel);

	/* The location entry */
	sel->location = gnt_entry_new(NULL);
	g_signal_connect(G_OBJECT(sel->location), "key_pressed", G_CALLBACK(location_key_pressed), sel);

	sel->cancel = gnt_button_new("Cancel");
	sel->select = gnt_button_new("Select");

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

char *gnt_file_sel_get_selected_file(GntFileSel *sel)
{
	char *ret;
	const char *tmp;
	tmp = (const char*)gnt_tree_get_selection_data(sel->dirsonly ? GNT_TREE(sel->dirs) : GNT_TREE(sel->files));
	ret = g_strdup_printf("%s%s%s", sel->current, sel->current[1] ? G_DIR_SEPARATOR_S : "", tmp ? tmp : "");
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

