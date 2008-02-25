/**
 * @file smiley.c Simley API
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
#include "xmlnode.h"
#include "debug.h"
#include "imgstore.h"
#include "smiley.h"
#include "util.h"


/**************************************************************************/
/* Main structures, members and constants                                 */
/**************************************************************************/

struct _PurpleSmiley
{
        PurpleStoredImage *img;        /**< The id of the stored image with the
                                        the smiley data.        */
        char *shortcut;                /**< Shortcut associated with the custom
                                        smiley. This field will work as a
                                        unique key by this API. */
        char *checksum;                /**< The smiley checksum.        */
};

static GHashTable *smiley_data_index = NULL;
static GHashTable *smiley_shortcut_index = NULL;
static GHashTable *smiley_checksum_index = NULL;

static guint save_timer = 0;
static gboolean smileys_loaded = FALSE;
static char *smileys_dir = NULL;

#define SMILEYS_DEFAULT_FOLDER			"custom_smiley"
#define SMILEYS_LOG_ID				"smileys"

#define XML_FILE_NAME				"smileys.xml"

#define XML_ROOT_TAG				"smileys"
#define XML_PROFILE_TAG			"profile"
#define XML_PROFILE_NAME_ATTRIB_TAG		"name"
#define XML_ACCOUNT_TAG			"account"
#define XML_ACCOUNT_USERID_ATTRIB_TAG		"userid"
#define XML_SMILEY_SET_TAG			"smiley_set"
#define XML_SMILEY_TAG				"smiley"
#define XML_SHORTCUT_ATTRIB_TAG		"shortcut"
#define XML_CHECKSUM_ATRIB_TAG			"checksum"
#define XML_FILENAME_ATRIB_TAG			"filename"


/******************************************************************************
 * XML descriptor file layout                                                 *
 ******************************************************************************
 *
 * Althought we are creating the profile XML structure here, now we
 * won't handle it.
 * So, we just add one profile named "default" that has no associated
 * account elements, and have only the smiley_set that will contain
 * all existent custom smiley.
 *
 * It's our "Highlander Profile" :-)
 *
 ******************************************************************************
 *
 * <smileys>
 *   <profile name="john.doe">
 *     <account userid="john.doe@jabber.org">
 *     <account userid="john.doe@gmail.com">
 *     <smiley_set>
 *       <smiley shortcut="aaa" checksum="xxxxxxxx" filename="file_name1.gif"/>
 *       <smiley shortcut="bbb" checksum="yyyyyyy" filename="file_name2.gif"/>
 *     </smiley_set>
 *   </profile>
 * </smiley>
 *
 *****************************************************************************/


/*********************************************************************
 * Forward declarations                                              *
 *********************************************************************/

static gboolean read_smiley_file(const char *path, guchar **data, size_t *len);

static char *get_file_full_path(const char *filename);

static PurpleSmiley *purple_smiley_create(const char *shortcut);

static PurpleSmiley *purple_smiley_load_file(const char *shortcut, const char *checksum,
		const char *filename);

static void
purple_smiley_set_data_impl(PurpleSmiley *smiley, guchar *smiley_data,
		size_t smiley_data_len, const char *filename);


/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static xmlnode *
smiley_to_xmlnode(PurpleSmiley *smiley)
{
	xmlnode *smiley_node = NULL;

	smiley_node = xmlnode_new(XML_SMILEY_TAG);

	if (!smiley_node)
		return NULL;

	xmlnode_set_attrib(smiley_node, XML_SHORTCUT_ATTRIB_TAG,
			smiley->shortcut);

	xmlnode_set_attrib(smiley_node, XML_CHECKSUM_ATRIB_TAG,
			smiley->checksum);

	xmlnode_set_attrib(smiley_node, XML_FILENAME_ATRIB_TAG,
			purple_imgstore_get_filename(smiley->img));

	return smiley_node;
}

static void
add_smiley_to_main_node(gpointer key, gpointer value, gpointer user_data)
{
	xmlnode *child_node;

	child_node = smiley_to_xmlnode(value);
	xmlnode_insert_child((xmlnode*)user_data, child_node);
}

static xmlnode *
smileys_to_xmlnode()
{
	xmlnode *root_node, *profile_node, *smileyset_node;

	root_node = xmlnode_new(XML_ROOT_TAG);
	xmlnode_set_attrib(root_node, "version", "1.0");

	/* See the top comment's above to understand why initial tag elements
	 * are not being considered by now. */
	profile_node = xmlnode_new(XML_PROFILE_TAG);
	if (profile_node) {
		xmlnode_set_attrib(profile_node, XML_PROFILE_NAME_ATTRIB_TAG, "Default");
		xmlnode_insert_child(root_node, profile_node);

		smileyset_node = xmlnode_new(XML_SMILEY_SET_TAG);
		if (smileyset_node) {
			xmlnode_insert_child(profile_node, smileyset_node);
			g_hash_table_foreach(smiley_data_index, add_smiley_to_main_node, smileyset_node);
		}
	}

	return root_node;
}

static void
sync_smileys()
{
	xmlnode *root_node;
	char *data;

	if (!smileys_loaded) {
		purple_debug_error(SMILEYS_LOG_ID, "Attempted to save smileys before it "
						 "was read!\n");
		return;
	}

	root_node = smileys_to_xmlnode();
	data = xmlnode_to_formatted_str(root_node, NULL);
	purple_util_write_data_to_file(XML_FILE_NAME, data, -1);

	g_free(data);
	xmlnode_free(root_node);
}

static gboolean
save_smileys_cb(gpointer data)
{
	sync_smileys();
	save_timer = 0;
	return FALSE;
}

static void
purple_smileys_save()
{
	if (save_timer == 0)
		save_timer = purple_timeout_add_seconds(5, save_smileys_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static PurpleSmiley *
parse_smiley(xmlnode *smiley_node)
{
	PurpleSmiley *smiley;
	const char *shortcut = NULL;
	const char *checksum = NULL;
	const char *filename = NULL;

	shortcut = xmlnode_get_attrib(smiley_node, XML_SHORTCUT_ATTRIB_TAG);
	checksum = xmlnode_get_attrib(smiley_node, XML_CHECKSUM_ATRIB_TAG);
	filename = xmlnode_get_attrib(smiley_node, XML_FILENAME_ATRIB_TAG);

	if ((shortcut == NULL) || (checksum == NULL) || (filename == NULL))
		return NULL;

	smiley = purple_smiley_load_file(shortcut, checksum, filename);

	return smiley;
}

static void
purple_smileys_load()
{
	xmlnode *root_node, *profile_node;
	xmlnode *smileyset_node = NULL;
	xmlnode *smiley_node;

	smileys_loaded = TRUE;

	root_node = purple_util_read_xml_from_file(XML_FILE_NAME,
			_(SMILEYS_LOG_ID));

	if (root_node == NULL)
		return;

	/* See the top comment's above to understand why initial tag elements
	 * are not being considered by now. */
	profile_node = xmlnode_get_child(root_node, XML_PROFILE_TAG);
	if (profile_node)
		smileyset_node = xmlnode_get_child(profile_node, XML_SMILEY_SET_TAG);

	if (smileyset_node) {
		smiley_node = xmlnode_get_child(smileyset_node, XML_SMILEY_TAG);
		for (; smiley_node != NULL;
				smiley_node = xmlnode_get_next_twin(smiley_node)) {
			PurpleSmiley *smiley;

			smiley = parse_smiley(smiley_node);

			purple_smileys_add(smiley);
		}
	}

	xmlnode_free(root_node);
}


/*********************************************************************
 * Stuff                                                             *
 *********************************************************************/

static char *get_file_full_path(const char *filename)
{
	char *path;

	path = g_build_filename(purple_smileys_get_storing_dir(), filename, NULL);

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_free(path);
		return NULL;
	}

	return path;
}

static PurpleSmiley *
purple_smiley_load_file(const char *shortcut, const char *checksum, const char *filename)
{
	PurpleSmiley *smiley = NULL;
	guchar *smiley_data;
	size_t smiley_data_len;
	char *fullpath = NULL;

	g_return_val_if_fail(shortcut  != NULL, NULL);
	g_return_val_if_fail(checksum  != NULL, NULL);
	g_return_val_if_fail(filename != NULL, NULL);

	fullpath = get_file_full_path(filename);
	if (!fullpath)
		return NULL;

	smiley = purple_smiley_create(shortcut);
	if (!smiley) {
		g_free(fullpath);
		return NULL;
	}

	smiley->checksum = g_strdup(checksum);

	if (read_smiley_file(fullpath, &smiley_data, &smiley_data_len))
		purple_smiley_set_data_impl(smiley, smiley_data,
				smiley_data_len, filename);
	else
		purple_smiley_delete(smiley);

	g_free(fullpath);

	return smiley;
}

static void
purple_smiley_data_store(PurpleStoredImage *stored_img)
{
	const char *dirname;
	char *path;
	FILE *file = NULL;

	g_return_if_fail(stored_img != NULL);

	if (!smileys_loaded)
		return;

	dirname  = purple_smileys_get_storing_dir();
	path = g_build_filename(dirname, purple_imgstore_get_filename(stored_img), NULL);

	if (!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
		purple_debug_info(SMILEYS_LOG_ID, "Creating smileys directory.\n");

		if (g_mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			purple_debug_error(SMILEYS_LOG_ID,
			                   "Unable to create directory %s: %s\n",
			                   dirname, g_strerror(errno));
		}
	}

	if ((file = g_fopen(path, "wb")) != NULL) {
		if (!fwrite(purple_imgstore_get_data(stored_img),
				purple_imgstore_get_size(stored_img), 1, file)) {
			purple_debug_error(SMILEYS_LOG_ID, "Error writing %s: %s\n",
			                   path, g_strerror(errno));
		} else {
			purple_debug_info(SMILEYS_LOG_ID, "Wrote cache file: %s\n", path);
		}

		fclose(file);
	} else {
		purple_debug_error(SMILEYS_LOG_ID, "Unable to create file %s: %s\n",
		                   path, g_strerror(errno));
		g_free(path);

		return;
	}

	g_free(path);
}

static void
purple_smiley_data_unstore(const char *filename)
{
	const char *dirname;
	char *path;

	g_return_if_fail(filename != NULL);

	dirname  = purple_smileys_get_storing_dir();
	path = g_build_filename(dirname, filename, NULL);

	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		if (g_unlink(path))
			purple_debug_error(SMILEYS_LOG_ID, "Failed to delete %s: %s\n",
			                   path, g_strerror(errno));
		else
			purple_debug_info(SMILEYS_LOG_ID, "Deleted cache file: %s\n", path);
	}

	g_free(path);
}

static gboolean
read_smiley_file(const char *path, guchar **data, size_t *len)
{
	GError *err = NULL;

	if (!g_file_get_contents(path, (gchar **)data, len, &err)) {
		purple_debug_error(SMILEYS_LOG_ID, "Error reading %s: %s\n",
				path, err->message);
		g_error_free(err);

		return FALSE;
	}

	return TRUE;
}

static PurpleStoredImage *
purple_smiley_data_new(guchar *smiley_data, size_t smiley_data_len)
{
	char *filename;
	PurpleStoredImage *stored_img;

	g_return_val_if_fail(smiley_data != NULL,   NULL);
	g_return_val_if_fail(smiley_data_len  > 0,  NULL);

	filename = purple_util_get_image_filename(smiley_data, smiley_data_len);

	if (filename == NULL) {
		g_free(smiley_data);
		return NULL;
	}

	stored_img = purple_imgstore_add(smiley_data, smiley_data_len, filename);

	g_free(filename);

	return stored_img;
}

static void
purple_smiley_set_data_impl(PurpleSmiley *smiley, guchar *smiley_data,
				size_t smiley_data_len, const char *filename)
{
	PurpleStoredImage *old_img;
	const char *old_filename = NULL;
	const char *new_filename = NULL;

	g_return_if_fail(smiley     != NULL);
	g_return_if_fail(smiley_data != NULL);
	g_return_if_fail(smiley_data_len > 0);

	old_img = smiley->img;
	smiley->img = NULL;

	if (filename)
		smiley->img = purple_imgstore_add(smiley_data,
				smiley_data_len, filename);
	else
		smiley->img = purple_smiley_data_new(smiley_data, smiley_data_len);

	g_free(smiley->checksum);
	smiley->checksum = purple_util_get_image_checksum(
			smiley_data, smiley_data_len);

	if (!old_img)
		return;

	/* If the old and new image files have different names we need
	 * to unstore old image file. */
	old_filename = purple_imgstore_get_filename(old_img);
	new_filename = purple_imgstore_get_filename(smiley->img);

	if (g_ascii_strcasecmp(old_filename, new_filename)) {
		purple_smiley_data_unstore(old_filename);
		purple_imgstore_unref(old_img);
	}
}


/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

static PurpleSmiley *
purple_smiley_create(const char *shortcut)
{
	PurpleSmiley *smiley;

	smiley = g_slice_new0(PurpleSmiley);
	if (!smiley)
		return NULL;

	smiley->shortcut = g_strdup(shortcut);

	return smiley;
}

static void
purple_smiley_destroy(PurpleSmiley *smiley)
{
	g_return_if_fail(smiley != NULL);

	g_free(smiley->shortcut);
	g_free(smiley->checksum);
	purple_imgstore_unref(smiley->img);

	g_slice_free(PurpleSmiley, smiley);
}

PurpleSmiley *
purple_smiley_new(PurpleStoredImage *img, const char *shortcut)
{
	PurpleSmiley *smiley = NULL;

	g_return_val_if_fail(shortcut  != NULL, NULL);
	g_return_val_if_fail(img       != NULL, NULL);

	smiley = purple_smileys_find_by_shortcut(shortcut);
	if (smiley)
		return smiley;

	smiley = purple_smiley_create(shortcut);
	if (!smiley)
		return NULL;

	smiley->checksum = purple_util_get_image_checksum(
				purple_imgstore_get_data(img),
				purple_imgstore_get_size(img));

	smiley->img = img;
	purple_smiley_data_store(img);

	return smiley;
}

PurpleSmiley *
purple_smiley_new_from_stream(const char *shortcut, guchar *smiley_data,
			size_t smiley_data_len, const char *filename)
{
	PurpleSmiley *smiley;

	g_return_val_if_fail(shortcut  != NULL,    NULL);
	g_return_val_if_fail(smiley_data != NULL,  NULL);
	g_return_val_if_fail(smiley_data_len  > 0, NULL);

	smiley = purple_smileys_find_by_shortcut(shortcut);
	if (smiley)
		return smiley;

	/* purple_smiley_create() sets shortcut */
	smiley = purple_smiley_create(shortcut);
	if (!smiley)
		return NULL;

	purple_smiley_set_data_impl(smiley, smiley_data, smiley_data_len, filename);

	purple_smiley_data_store(smiley->img);

	return smiley;
}

PurpleSmiley *
purple_smiley_new_from_file(const char *shortcut, const char *filepath, const char *filename)
{
	PurpleSmiley *smiley = NULL;
	guchar *smiley_data;
	size_t smiley_data_len;

	g_return_val_if_fail(shortcut  != NULL,  NULL);
	g_return_val_if_fail(filepath != NULL,  NULL);

	if (read_smiley_file(filepath, &smiley_data, &smiley_data_len))
		smiley = purple_smiley_new_from_stream(shortcut, smiley_data,
				smiley_data_len, filename);

	return smiley;
}

void
purple_smiley_delete(PurpleSmiley *smiley)
{
	guchar *filename = NULL;

	g_return_if_fail(smiley != NULL);

	filename = g_hash_table_lookup(smiley_shortcut_index, smiley->shortcut);

	if (filename != NULL)
		purple_smileys_remove(smiley);

	if (smiley->img)
		purple_smiley_data_unstore(purple_imgstore_get_filename(smiley->img));

	purple_smiley_destroy(smiley);
}

gboolean
purple_smiley_set_shortcut(PurpleSmiley *smiley, const char *shortcut)
{
	char *filename = NULL;

	g_return_val_if_fail(smiley  != NULL, FALSE);
	g_return_val_if_fail(shortcut != NULL, FALSE);

	/* Check out whether the shortcut is already being used. */
	filename = g_hash_table_lookup(smiley_shortcut_index, shortcut);

	if (filename)
		return FALSE;

	/* Save filename associated with the current shortcut. */
	filename = g_strdup(g_hash_table_lookup(
			smiley_shortcut_index, smiley->shortcut));

	/* If the updating smiley was already inserted to internal indexes, the
	 * shortcut index will need update.
	 * So we remove the old element first, and add the new one right after.
	 */
	if (filename) {
		g_hash_table_remove(smiley_shortcut_index, smiley->shortcut);
		g_hash_table_insert(smiley_shortcut_index,
			g_strdup(shortcut), filename);
	}

	g_free(smiley->shortcut);
	smiley->shortcut = g_strdup(shortcut);

	purple_smileys_save();

	return TRUE;
}

void
purple_smiley_set_data(PurpleSmiley *smiley, guchar *smiley_data,
			   size_t smiley_data_len, gboolean keepfilename)
{
	gboolean updateindex = FALSE;
	const char *filename = NULL;

	g_return_if_fail(smiley     != NULL);
	g_return_if_fail(smiley_data != NULL);
	g_return_if_fail(smiley_data_len > 0);

	/* If the updating smiley was already inserted to internal indexes, the
	 * checksum index will need update. And, considering that the filename
	 * could be regenerated using the file's checksum, the filename index
	 * will be updated too.
	 * So we remove it here, and add it again after the information is
	 * updated by "purple_smiley_set_data_impl" method. */
	filename = g_hash_table_lookup(smiley_checksum_index, smiley->checksum);
	if (filename) {
		g_hash_table_remove(smiley_checksum_index, smiley->checksum);
		g_hash_table_remove(smiley_data_index, filename);

		updateindex = TRUE;
	}

	/* Updates the file data. */
	if ((keepfilename) && (smiley->img) &&
			(purple_imgstore_get_filename(smiley->img)))
		purple_smiley_set_data_impl(smiley, smiley_data,
				smiley_data_len,
				purple_imgstore_get_filename(smiley->img));
	else
		purple_smiley_set_data_impl(smiley, smiley_data,
				smiley_data_len, NULL);

	/* Reinserts the index item. */
	filename = purple_imgstore_get_filename(smiley->img);
	if ((updateindex) && (filename)) {
		g_hash_table_insert(smiley_checksum_index,
				g_strdup(smiley->checksum), g_strdup(filename));
		g_hash_table_insert(smiley_data_index, g_strdup(filename), smiley);
	}

	purple_smileys_save();
}

PurpleStoredImage *
purple_smiley_get_stored_image(const PurpleSmiley *smiley)
{
	return purple_imgstore_ref(smiley->img);
}

const char *purple_smiley_get_shortcut(const PurpleSmiley *smiley)
{
	g_return_val_if_fail(smiley != NULL, NULL);

	return smiley->shortcut;
}

const char *
purple_smiley_get_checksum(const PurpleSmiley *smiley)
{
	g_return_val_if_fail(smiley != NULL, NULL);

	return smiley->checksum;
}

gconstpointer
purple_smiley_get_data(const PurpleSmiley *smiley, size_t *len)
{
	g_return_val_if_fail(smiley != NULL, NULL);

	if (smiley->img) {
		if (len != NULL)
			*len = purple_imgstore_get_size(smiley->img);

		return purple_imgstore_get_data(smiley->img);
	}

	return NULL;
}

const char *
purple_smiley_get_extension(const PurpleSmiley *smiley)
{
	if (smiley->img != NULL)
		return purple_imgstore_get_extension(smiley->img);

	return NULL;
}

char *purple_smiley_get_full_path(PurpleSmiley *smiley)
{
	g_return_val_if_fail(smiley != NULL, NULL);

	if (smiley->img == NULL)
		return NULL;

	return get_file_full_path(purple_imgstore_get_filename(smiley->img));
}

static void add_smiley_to_list(gpointer key, gpointer value, gpointer user_data)
{
	GList** preturninglist = (GList**)user_data;
	GList *returninglist = *preturninglist;

	returninglist = g_list_append(returninglist, value);

	*preturninglist = returninglist;
}

GList *
purple_smileys_get_all(void)
{
	GList *returninglist = NULL;

	g_hash_table_foreach(smiley_data_index, add_smiley_to_list, &returninglist);

	return returninglist;
}

void
purple_smileys_add(PurpleSmiley *smiley)
{
	const char *filename = NULL;

	g_return_if_fail(smiley != NULL);

	if (!g_hash_table_lookup(smiley_shortcut_index, smiley->shortcut)) {
		filename = purple_imgstore_get_filename(smiley->img);

		g_hash_table_insert(smiley_data_index, g_strdup(filename), smiley);
		g_hash_table_insert(smiley_shortcut_index, g_strdup(smiley->shortcut), g_strdup(filename));
		g_hash_table_insert(smiley_checksum_index, g_strdup(smiley->checksum), g_strdup(filename));

		purple_smileys_save();
	}
}

void
purple_smileys_remove(PurpleSmiley *smiley)
{
	const char *filename = NULL;

	g_return_if_fail(smiley != NULL);

	filename = purple_imgstore_get_filename(smiley->img);

	if (g_hash_table_lookup(smiley_data_index, filename)) {
		g_hash_table_remove(smiley_data_index, filename);
		g_hash_table_remove(smiley_shortcut_index, smiley->shortcut);
		g_hash_table_remove(smiley_checksum_index, smiley->checksum);

		purple_smileys_save();
	}
}

PurpleSmiley *
purple_smileys_find_by_shortcut(const char *shortcut)
{
	PurpleSmiley *smiley = NULL;
	guchar *filename = NULL;

	g_return_val_if_fail(shortcut != NULL, NULL);

	filename = g_hash_table_lookup(smiley_shortcut_index, shortcut);

	if (filename == NULL)
		return NULL;

	smiley = g_hash_table_lookup(smiley_data_index, filename);

	if (!smiley)
		return NULL;

	return smiley;
}

PurpleSmiley *
purple_smileys_find_by_checksum(const char *checksum)
{
	PurpleSmiley *smiley = NULL;
	guchar *filename = NULL;

	g_return_val_if_fail(checksum != NULL, NULL);

	filename = g_hash_table_lookup(smiley_checksum_index, checksum);

	if (filename == NULL)
		return NULL;

	smiley = g_hash_table_lookup(smiley_data_index, filename);

	if (!smiley)
		return NULL;

	return smiley;
}

const char *
purple_smileys_get_storing_dir(void)
{
	return smileys_dir;
}

void
purple_smileys_init()
{
	smiley_data_index = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, NULL);
	smiley_shortcut_index = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	smiley_checksum_index = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);

	smileys_dir = g_build_filename(purple_user_dir(), SMILEYS_DEFAULT_FOLDER, NULL);

	purple_smileys_load();
}

static gboolean
free_smiley_data(gpointer key, gpointer value, gpointer user_data)
{
	purple_smiley_destroy(value);

	return TRUE;
}

void
purple_smileys_uninit()
{
	if (save_timer != 0) {
		purple_timeout_remove(save_timer);
		save_timer = 0;
		sync_smileys();
	}

	g_hash_table_foreach_remove(smiley_data_index, free_smiley_data, NULL);
	g_hash_table_destroy(smiley_shortcut_index);
	g_hash_table_destroy(smiley_checksum_index);
	g_free(smileys_dir);
}

