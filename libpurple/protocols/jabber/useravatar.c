/*
 * purple - Jabber Protocol Plugin
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	 02111-1307	 USA
 *
 */

#include "internal.h"

#include "useravatar.h"
#include "pep.h"
#include "debug.h"

#define MAX_HTTP_BUDDYICON_BYTES (200 * 1024)

static void update_buddy_metadata(JabberStream *js, const char *from, xmlnode *items);

void jabber_avatar_init(void)
{
	jabber_add_feature("avatarmeta", NS_AVATAR_0_12_METADATA,
	                   jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_add_feature("avatardata", NS_AVATAR_0_12_DATA,
	                   jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_pep_register_handler("avatar", NS_AVATAR_0_12_METADATA,
	                            update_buddy_metadata);

	jabber_add_feature("urn_avatarmeta", NS_AVATAR_1_1_METADATA,
	                   jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_add_feature("urn_avatardata", NS_AVATAR_1_1_DATA,
	                   jabber_pep_namespace_only_when_pep_enabled_cb);

	jabber_pep_register_handler("urn_avatar", NS_AVATAR_1_1_METADATA,
	                            update_buddy_metadata);
}

void jabber_avatar_set(JabberStream *js, PurpleStoredImage *img, const char *ns)
{
	xmlnode *publish, *metadata, *item;

	if (!js->pep)
		return;

	if (!img) {
		if (ns == NULL || !strcmp(ns, NS_AVATAR_0_12_METADATA)) {
			/* remove the metadata */
			publish = xmlnode_new("publish");
			xmlnode_set_attrib(publish, "node", NS_AVATAR_0_12_METADATA);

			item = xmlnode_new_child(publish, "item");
			metadata = xmlnode_new_child(item, "metadata");
			xmlnode_set_namespace(metadata, NS_AVATAR_0_12_METADATA);

			xmlnode_new_child(metadata, "stop");
			/* publish */
			jabber_pep_publish(js, publish);
		}

		if (ns == NULL || !strcmp(ns, NS_AVATAR_1_1_METADATA)) {
			/* Now for the XEP-0084 v1.1 namespace, where we publish an empty
			 * metadata node instead of a <stop/> element */
			publish = xmlnode_new("publish");
			xmlnode_set_attrib(publish, "node", NS_AVATAR_1_1_METADATA);

			item = xmlnode_new_child(publish, "item");
			metadata = xmlnode_new_child(item, "metadata");
			xmlnode_set_namespace(metadata, NS_AVATAR_1_1_METADATA);

			/* publish */
			jabber_pep_publish(js, publish);
		}
	} else {
		/*
		 * TODO: This is pretty gross.  The Jabber PRPL really shouldn't
		 *       do voodoo to try to determine the image type, height
		 *       and width.
		 */
		/* A PNG header, including the IHDR, but nothing else */
		const struct {
			guchar signature[8]; /* must be hex 89 50 4E 47 0D 0A 1A 0A */
			struct {
				guint32 length; /* must be 0x0d */
				guchar type[4]; /* must be 'I' 'H' 'D' 'R' */
				guint32 width;
				guint32 height;
				guchar bitdepth;
				guchar colortype;
				guchar compression;
				guchar filter;
				guchar interlace;
			} ihdr;
		} *png = purple_imgstore_get_data(img); /* ATTN: this is in network byte order! */

		/* check if the data is a valid png file (well, at least to some extend) */
		if(png->signature[0] == 0x89 &&
		   png->signature[1] == 0x50 &&
		   png->signature[2] == 0x4e &&
		   png->signature[3] == 0x47 &&
		   png->signature[4] == 0x0d &&
		   png->signature[5] == 0x0a &&
		   png->signature[6] == 0x1a &&
		   png->signature[7] == 0x0a &&
		   ntohl(png->ihdr.length) == 0x0d &&
		   png->ihdr.type[0] == 'I' &&
		   png->ihdr.type[1] == 'H' &&
		   png->ihdr.type[2] == 'D' &&
		   png->ihdr.type[3] == 'R') {
			/* parse PNG header to get the size of the image (yes, this is required) */
			guint32 width = ntohl(png->ihdr.width);
			guint32 height = ntohl(png->ihdr.height);
			xmlnode *data, *info;
			char *lengthstring, *widthstring, *heightstring;

			/* compute the sha1 hash */
			char *hash = jabber_calculate_data_sha1sum(purple_imgstore_get_data(img),
			                                           purple_imgstore_get_size(img));
			char *base64avatar = purple_base64_encode(purple_imgstore_get_data(img),
			                                          purple_imgstore_get_size(img));

			if (ns == NULL || !strcmp(ns, NS_AVATAR_0_12_METADATA)) {
				publish = xmlnode_new("publish");
				xmlnode_set_attrib(publish, "node", NS_AVATAR_0_12_DATA);

				item = xmlnode_new_child(publish, "item");
				xmlnode_set_attrib(item, "id", hash);

				data = xmlnode_new_child(item, "data");
				xmlnode_set_namespace(data, NS_AVATAR_0_12_DATA);

				xmlnode_insert_data(data, base64avatar, -1);
				/* publish the avatar itself */
				jabber_pep_publish(js, publish);
			}

			if (ns == NULL || !strcmp(ns, NS_AVATAR_1_1_METADATA)) {
				publish = xmlnode_new("publish");
				xmlnode_set_attrib(publish, "node", NS_AVATAR_1_1_DATA);

				item = xmlnode_new_child(publish, "item");
				xmlnode_set_attrib(item, "id", "hash");

				data = xmlnode_new_child(item, "data");
				xmlnode_set_namespace(data, NS_AVATAR_1_1_DATA);

				xmlnode_insert_data(data, base64avatar, -1);
				/* publish the avatar itself */
				jabber_pep_publish(js, publish);
			}

			g_free(base64avatar);

			lengthstring = g_strdup_printf("%" G_GSIZE_FORMAT,
			                               purple_imgstore_get_size(img));
			widthstring = g_strdup_printf("%u", width);
			heightstring = g_strdup_printf("%u", height);

			/* next step: publish the metadata to the old namespace */
			if (ns == NULL || !strcmp(ns, NS_AVATAR_0_12_METADATA)) {
				publish = xmlnode_new("publish");
				xmlnode_set_attrib(publish, "node", NS_AVATAR_0_12_METADATA);

				item = xmlnode_new_child(publish, "item");
				xmlnode_set_attrib(item, "id", hash);

				metadata = xmlnode_new_child(item, "metadata");
				xmlnode_set_namespace(metadata, NS_AVATAR_0_12_METADATA);

				info = xmlnode_new_child(metadata, "info");
				xmlnode_set_attrib(info, "id", hash);
				xmlnode_set_attrib(info, "type", "image/png");
				xmlnode_set_attrib(info, "bytes", lengthstring);
				xmlnode_set_attrib(info, "width", widthstring);
				xmlnode_set_attrib(info, "height", heightstring);
				/* publish the metadata */
				jabber_pep_publish(js, publish);
			}

			if (ns == NULL || !strcmp(ns, NS_AVATAR_1_1_METADATA)) {
				/* publish the metadata to the new namespace */
				publish = xmlnode_new("publish");
				xmlnode_set_attrib(publish, "node", NS_AVATAR_1_1_METADATA);

				item = xmlnode_new_child(publish, "item");
				xmlnode_set_attrib(item, "id", hash);

				metadata = xmlnode_new_child(item, "metdata");
				xmlnode_set_namespace(metadata, NS_AVATAR_1_1_METADATA);

				info = xmlnode_new_child(metadata, "info");
				xmlnode_set_attrib(info, "id", hash);
				xmlnode_set_attrib(info, "type", "image/png");
				xmlnode_set_attrib(info, "bytes", lengthstring);
				xmlnode_set_attrib(info, "width", widthstring);
				xmlnode_set_attrib(info, "height", heightstring);

				jabber_pep_publish(js, publish);
			}

			g_free(lengthstring);
			g_free(widthstring);
			g_free(heightstring);
			g_free(hash);
		} else {
			purple_debug_error("jabber", "Cannot set PEP avatar to non-PNG data\n");
		}
	}
}

static void
do_got_own_avatar_cb(JabberStream *js, const char *from, xmlnode *items)
{
	xmlnode *item = NULL, *metadata = NULL, *info = NULL;
	PurpleAccount *account = purple_connection_get_account(js->gc);
	const char *current_hash = purple_account_get_string(account, "prpl-jabber_icon_checksum", "");
	const char *server_hash = NULL;
	const char *ns;

	if ((item = xmlnode_get_child(items, "item")) &&
	     (metadata = xmlnode_get_child(item, "metadata")) &&
	     (info = xmlnode_get_child(metadata, "info"))) {
		server_hash = xmlnode_get_attrib(info, "id");
	}

	ns = xmlnode_get_namespace(metadata);
	if (!ns)
		return;

	/* Publish ours if it's different than the server's */
	if ((!server_hash && current_hash[0] != '\0') ||
		 (server_hash && strcmp(server_hash, current_hash))) {
		PurpleStoredImage *img = purple_buddy_icons_find_account_icon(account);
		jabber_avatar_set(js, img, ns);
		purple_imgstore_unref(img);
	}
}

void jabber_avatar_fetch_mine(JabberStream *js)
{
	char *jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	jabber_pep_request_item(js, jid, NS_AVATAR_0_12_METADATA, NULL,
	                        do_got_own_avatar_cb);
	jabber_pep_request_item(js, jid, NS_AVATAR_1_1_METADATA, NULL,
	                        do_got_own_avatar_cb);
	g_free(jid);
}

typedef struct _JabberBuddyAvatarUpdateURLInfo {
	JabberStream *js;
	char *from;
	char *id;
} JabberBuddyAvatarUpdateURLInfo;

static void
do_buddy_avatar_update_fromurl(PurpleUtilFetchUrlData *url_data,
                               gpointer user_data, const gchar *url_text,
                               gsize len, const gchar *error_message)
{
	JabberBuddyAvatarUpdateURLInfo *info = user_data;
	if(!url_text) {
		purple_debug(PURPLE_DEBUG_ERROR, "jabber",
		             "do_buddy_avatar_update_fromurl got error \"%s\"",
		             error_message);
		goto out;
	}

	purple_buddy_icons_set_for_user(purple_connection_get_account(info->js->gc), info->from, (void*)url_text, len, info->id);

out:
	g_free(info->from);
	g_free(info->id);
	g_free(info);
}

static void
do_buddy_avatar_update_data(JabberStream *js, const char *from, xmlnode *items)
{
	xmlnode *item, *data;
	const char *checksum, *ns;
	char *b64data;
	void *img;
	size_t size;
	if(!items)
		return;

	item = xmlnode_get_child(items, "item");
	if(!item)
		return;

	data = xmlnode_get_child(item, "data");
	if(!data)
		return;

	ns = xmlnode_get_namespace(data);
	/* Make sure the namespace is one of the two valid possibilities */
	if (!ns || (strcmp(ns, NS_AVATAR_0_12_DATA) &&
	            strcmp(ns, NS_AVATAR_1_1_DATA)))
		return;

	checksum = xmlnode_get_attrib(item,"id");
	if(!checksum)
		return;

	b64data = xmlnode_get_data(data);
	if(!b64data)
		return;

	img = purple_base64_decode(b64data, &size);
	if(!img) {
		g_free(b64data);
		return;
	}

	purple_buddy_icons_set_for_user(purple_connection_get_account(js->gc), from, img, size, checksum);
	g_free(b64data);
}

static void
update_buddy_metadata(JabberStream *js, const char *from, xmlnode *items)
{
	PurpleBuddy *buddy = purple_find_buddy(purple_connection_get_account(js->gc), from);
	const char *checksum, *ns;
	xmlnode *item, *metadata;
	if(!buddy)
		return;

	checksum = purple_buddy_icons_get_checksum_for_user(buddy);
	item = xmlnode_get_child(items,"item");
	metadata = xmlnode_get_child(item, "metadata");
	if(!metadata)
		return;

	ns = xmlnode_get_namespace(metadata);
	/* Make sure the namespace is one of the two valid possibilities */
	if (!ns || (strcmp(ns, NS_AVATAR_0_12_METADATA) &&
	            strcmp(ns, NS_AVATAR_1_1_METADATA)))
		return;

	/* check if we have received a stop */
	if(xmlnode_get_child(metadata, "stop")) {
		purple_buddy_icons_set_for_user(purple_connection_get_account(js->gc), from, NULL, 0, NULL);
	} else {
		xmlnode *info, *goodinfo = NULL;
		gboolean has_children = FALSE;

		/* iterate over all info nodes to get one we can use */
		for(info = metadata->child; info; info = info->next) {
			if(info->type == XMLNODE_TYPE_TAG)
				has_children = TRUE;
			if(info->type == XMLNODE_TYPE_TAG && !strcmp(info->name,"info")) {
				const char *type = xmlnode_get_attrib(info,"type");
				const char *id = xmlnode_get_attrib(info,"id");

				if(checksum && id && !strcmp(id, checksum)) {
					/* we already have that avatar, so we don't have to do anything */
					goodinfo = NULL;
					break;
				}
				/* We'll only pick the png one for now. It's a very nice image format anyways. */
				if(type && id && !goodinfo && !strcmp(type, "image/png"))
					goodinfo = info;
			}
		}
		if(has_children == FALSE) {
			purple_buddy_icons_set_for_user(purple_connection_get_account(js->gc), from, NULL, 0, NULL);
		} else if(goodinfo) {
			const char *url = xmlnode_get_attrib(goodinfo, "url");
			const char *id = xmlnode_get_attrib(goodinfo,"id");

			/* the avatar might either be stored in a pep node, or on a HTTP(S) URL */
			if(!url) {
				const char *data_ns;
				data_ns = (strcmp(ns, NS_AVATAR_0_12_METADATA) == 0 ?
				               NS_AVATAR_0_12_DATA : NS_AVATAR_1_1_DATA);
				jabber_pep_request_item(js, from, data_ns, id,
				                        do_buddy_avatar_update_data);
			} else {
				PurpleUtilFetchUrlData *url_data;
				JabberBuddyAvatarUpdateURLInfo *info = g_new0(JabberBuddyAvatarUpdateURLInfo, 1);
				info->js = js;

				url_data = purple_util_fetch_url_len(url, TRUE, NULL, TRUE,
										  MAX_HTTP_BUDDYICON_BYTES,
										  do_buddy_avatar_update_fromurl, info);
				if (url_data) {
					info->from = g_strdup(from);
					info->id = g_strdup(id);
					js->url_datas = g_slist_prepend(js->url_datas, url_data);
				} else
					g_free(info);

			}
		}
	}
}
