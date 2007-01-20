/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "oscar.h"

static aim_tlv_t *
createtlv(guint16 type, guint16 length, guint8 *value)
{
	aim_tlv_t *ret;

	ret = g_new(aim_tlv_t, 1);
	ret->type = type;
	ret->length = length;
	ret->value = value;

	return ret;
}

static void
freetlv(aim_tlv_t **oldtlv)
{

	if (!oldtlv || !*oldtlv)
		return;

	free((*oldtlv)->value);
	free(*oldtlv);
	*oldtlv = NULL;

	return;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, aim_tlvlist_free() should
 * be called to free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each
 * element doesn't need to be malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @return Return the TLV chain read
 */
aim_tlvlist_t *aim_tlvlist_read(ByteStream *bs)
{
	aim_tlvlist_t *list = NULL, *cur;

	while (byte_stream_empty(bs) > 0) {
		guint16 type, length;

		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);

#if 0 /* temporarily disabled until I know if they're still doing it or not */
		/*
		 * Okay, so now AOL has decided that any TLV of
		 * type 0x0013 can only be two bytes, despite
		 * what the actual given length is.  So here
		 * we dump any invalid TLVs of that sort.  Hopefully
		 * there's no special cases to this special case.
		 *   - mid (30jun2000)
		 */
		if ((type == 0x0013) && (length != 0x0002))
			length = 0x0002;
#else
		if (0)
			;
#endif
		else {

			if (length > byte_stream_empty(bs)) {
				aim_tlvlist_free(&list);
				return NULL;
			}

			cur = g_new0(aim_tlvlist_t, 1);
			cur->tlv = createtlv(type, length, NULL);
			if (cur->tlv->length > 0) {
				cur->tlv->value = byte_stream_getraw(bs, length);
				if (!cur->tlv->value) {
					freetlv(&cur->tlv);
					free(cur);
					aim_tlvlist_free(&list);
					return NULL;
				}
			}

			cur->next = list;
			list = cur;
		}
	}

	return list;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, aim_tlvlist_free() should
 * be called to free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each
 * element doesn't need to be malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @param num The max number of TLVs that will be read, or -1 if unlimited.
 *        There are a number of places where you want to read in a tlvchain,
 *        but the chain is not at the end of the SNAC, and the chain is
 *        preceded by the number of TLVs.  So you can limit that with this.
 * @return Return the TLV chain read
 */
aim_tlvlist_t *aim_tlvlist_readnum(ByteStream *bs, guint16 num)
{
	aim_tlvlist_t *list = NULL, *cur;

	while ((byte_stream_empty(bs) > 0) && (num != 0)) {
		guint16 type, length;

		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);

		if (length > byte_stream_empty(bs)) {
			aim_tlvlist_free(&list);
			return NULL;
		}

		cur = g_new0(aim_tlvlist_t, 1);
		cur->tlv = createtlv(type, length, NULL);
		if (cur->tlv->length > 0) {
			cur->tlv->value = byte_stream_getraw(bs, length);
			if (!cur->tlv->value) {
				freetlv(&cur->tlv);
				free(cur);
				aim_tlvlist_free(&list);
				return NULL;
			}
		}

		if (num > 0)
			num--;
		cur->next = list;
		list = cur;
	}

	return list;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, aim_tlvlist_free() should
 * be called to free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each
 * element doesn't need to be malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @param len The max length in bytes that will be read.
 *        There are a number of places where you want to read in a tlvchain,
 *        but the chain is not at the end of the SNAC, and the chain is
 *        preceded by the length of the TLVs.  So you can limit that with this.
 * @return Return the TLV chain read
 */
aim_tlvlist_t *aim_tlvlist_readlen(ByteStream *bs, guint16 len)
{
	aim_tlvlist_t *list = NULL, *cur;

	while ((byte_stream_empty(bs) > 0) && (len > 0)) {
		guint16 type, length;

		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);

		if (length > byte_stream_empty(bs)) {
			aim_tlvlist_free(&list);
			return NULL;
		}

		cur = g_new0(aim_tlvlist_t, 1);
		cur->tlv = createtlv(type, length, NULL);
		if (cur->tlv->length > 0) {
			cur->tlv->value = byte_stream_getraw(bs, length);
			if (!cur->tlv->value) {
				freetlv(&cur->tlv);
				free(cur);
				aim_tlvlist_free(&list);
				return NULL;
			}
		}

		len -= aim_tlvlist_size(&cur);
		cur->next = list;
		list = cur;
	}

	return list;
}

/**
 * Duplicate a TLV chain.
 * This is pretty self explanatory.
 *
 * @param orig The TLV chain you want to make a copy of.
 * @return A newly allocated TLV chain.
 */
aim_tlvlist_t *aim_tlvlist_copy(aim_tlvlist_t *orig)
{
	aim_tlvlist_t *new = NULL;

	while (orig) {
		aim_tlvlist_add_raw(&new, orig->tlv->type, orig->tlv->length, orig->tlv->value);
		orig = orig->next;
	}

	return new;
}

/*
 * Compare two TLV lists for equality.  This probably is not the most
 * efficient way to do this.
 *
 * @param one One of the TLV chains to compare.
 * @param two The other TLV chain to compare.
 * @return Return 0 if the lists are the same, return 1 if they are different.
 */
int aim_tlvlist_cmp(aim_tlvlist_t *one, aim_tlvlist_t *two)
{
	ByteStream bs1, bs2;

	if (aim_tlvlist_size(&one) != aim_tlvlist_size(&two))
		return 1;

	byte_stream_new(&bs1, aim_tlvlist_size(&one));
	byte_stream_new(&bs2, aim_tlvlist_size(&two));

	aim_tlvlist_write(&bs1, &one);
	aim_tlvlist_write(&bs2, &two);

	if (memcmp(bs1.data, bs2.data, bs1.len)) {
		free(bs1.data);
		free(bs2.data);
		return 1;
	}

	g_free(bs1.data);
	g_free(bs2.data);

	return 0;
}

/**
 * Free a TLV chain structure
 *
 * Walks the list of TLVs in the passed TLV chain and
 * frees each one. Note that any references to this data
 * should be removed before calling this.
 *
 * @param list Chain to be freed
 */
void aim_tlvlist_free(aim_tlvlist_t **list)
{
	aim_tlvlist_t *cur;

	if (!list || !*list)
		return;

	for (cur = *list; cur; ) {
		aim_tlvlist_t *tmp;

		freetlv(&cur->tlv);

		tmp = cur->next;
		free(cur);
		cur = tmp;
	}

	list = NULL;

	return;
}

/**
 * Count the number of TLVs in a chain.
 *
 * @param list Chain to be counted.
 * @return The number of TLVs stored in the passed chain.
 */
int aim_tlvlist_count(aim_tlvlist_t **list)
{
	aim_tlvlist_t *cur;
	int count;

	if (!list || !*list)
		return 0;

	for (cur = *list, count = 0; cur; cur = cur->next)
		count++;

	return count;
}

/**
 * Count the number of bytes in a TLV chain.
 *
 * @param list Chain to be sized
 * @return The number of bytes that would be needed to
 *         write the passed TLV chain to a data buffer.
 */
int aim_tlvlist_size(aim_tlvlist_t **list)
{
	aim_tlvlist_t *cur;
	int size;

	if (!list || !*list)
		return 0;

	for (cur = *list, size = 0; cur; cur = cur->next)
		size += (4 + cur->tlv->length);

	return size;
}

/**
 * Adds the passed string as a TLV element of the passed type
 * to the TLV chain.
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param length Length of string to add (not including %NULL).
 * @param value String to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_raw(aim_tlvlist_t **list, const guint16 type, const guint16 length, const guint8 *value)
{
	aim_tlvlist_t *newtlv, *cur;

	if (list == NULL)
		return 0;

	newtlv = g_new0(aim_tlvlist_t, 1);
	newtlv->tlv = createtlv(type, length, NULL);
	if (newtlv->tlv->length > 0)
		newtlv->tlv->value = g_memdup(value, length);

	if (!*list)
		*list = newtlv;
	else {
		for(cur = *list; cur->next; cur = cur->next)
			;
		cur->next = newtlv;
	}

	return newtlv->tlv->length;
}

/**
 * Add a one byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_8(aim_tlvlist_t **list, const guint16 type, const guint8 value)
{
	guint8 v8[1];

	aimutil_put8(v8, value);

	return aim_tlvlist_add_raw(list, type, 1, v8);
}

/**
 * Add a two byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_16(aim_tlvlist_t **list, const guint16 type, const guint16 value)
{
	guint8 v16[2];

	aimutil_put16(v16, value);

	return aim_tlvlist_add_raw(list, type, 2, v16);
}

/**
 * Add a four byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_32(aim_tlvlist_t **list, const guint16 type, const guint32 value)
{
	guint8 v32[4];

	aimutil_put32(v32, value);

	return aim_tlvlist_add_raw(list, type, 4, v32);
}

/**
 * Add a string to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_str(aim_tlvlist_t **list, const guint16 type, const char *value)
{
	return aim_tlvlist_add_raw(list, type, strlen(value), (guint8 *)value);
}

/**
 * Adds a block of capability blocks to a TLV chain. The bitfield
 * passed in should be a bitwise %OR of any of the %AIM_CAPS constants:
 *
 *     %OSCAR_CAPABILITY_BUDDYICON   Supports Buddy Icons
 *     %OSCAR_CAPABILITY_TALK        Supports Voice Chat
 *     %OSCAR_CAPABILITY_IMIMAGE     Supports DirectIM/IMImage
 *     %OSCAR_CAPABILITY_CHAT        Supports Chat
 *     %OSCAR_CAPABILITY_GETFILE     Supports Get File functions
 *     %OSCAR_CAPABILITY_SENDFILE    Supports Send File functions
 *
 * @param list Destination chain
 * @param type TLV type to add
 * @param caps Bitfield of capability flags to send
 * @return The size of the value added.
 */
int aim_tlvlist_add_caps(aim_tlvlist_t **list, const guint16 type, const guint32 caps)
{
	guint8 buf[16*16]; /* XXX icky fixed length buffer */
	ByteStream bs;

	if (!caps)
		return 0; /* nothing there anyway */

	byte_stream_init(&bs, buf, sizeof(buf));

	byte_stream_putcaps(&bs, caps);

	return aim_tlvlist_add_raw(list, type, byte_stream_curpos(&bs), buf);
}

/**
 * Adds the given userinfo struct to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_userinfo(aim_tlvlist_t **list, guint16 type, aim_userinfo_t *userinfo)
{
	guint8 buf[1024]; /* bleh */
	ByteStream bs;

	byte_stream_init(&bs, buf, sizeof(buf));

	aim_putuserinfo(&bs, userinfo);

	return aim_tlvlist_add_raw(list, type, byte_stream_curpos(&bs), buf);
}

/**
 * Adds the given chatroom info to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param roomname The name of the chat.
 * @param instance The instance.
 * @return The size of the value added.
 */
int aim_tlvlist_add_chatroom(aim_tlvlist_t **list, guint16 type, guint16 exchange, const char *roomname, guint16 instance)
{
	int len;
	ByteStream bs;

	byte_stream_new(&bs, 2 + 1 + strlen(roomname) + 2);

	byte_stream_put16(&bs, exchange);
	byte_stream_put8(&bs, strlen(roomname));
	byte_stream_putstr(&bs, roomname);
	byte_stream_put16(&bs, instance);

	len = aim_tlvlist_add_raw(list, type, byte_stream_curpos(&bs), bs.data);

	g_free(bs.data);

	return len;
}

/**
 * Adds a TLV with a zero length to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @return The size of the value added.
 */
int aim_tlvlist_add_noval(aim_tlvlist_t **list, const guint16 type)
{
	return aim_tlvlist_add_raw(list, type, 0, NULL);
}

/*
 * Note that the inner TLV chain will not be modifiable as a tlvchain once
 * it is written using this.  Or rather, it can be, but updates won't be
 * made to this.
 *
 * XXX should probably support sublists for real.
 *
 * This is so neat.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param t1 The TLV chain you want to write.
 * @return The number of bytes written to the destination TLV chain.
 *         0 is returned if there was an error or if the destination
 *         TLV chain has length 0.
 */
int aim_tlvlist_add_frozentlvlist(aim_tlvlist_t **list, guint16 type, aim_tlvlist_t **tl)
{
	int buflen;
	ByteStream bs;

	buflen = aim_tlvlist_size(tl);

	if (buflen <= 0)
		return 0;

	byte_stream_new(&bs, buflen);

	aim_tlvlist_write(&bs, tl);

	aim_tlvlist_add_raw(list, type, byte_stream_curpos(&bs), bs.data);

	g_free(bs.data);

	return buflen;
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If
 * you attempt to replace a TLV that does not exist, this function will
 * just add a new TLV as if you called aim_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param length Length of string to add (not including %NULL).
 * @param value String to add.
 * @return The length of the TLV.
 */
int aim_tlvlist_replace_raw(aim_tlvlist_t **list, const guint16 type, const guint16 length, const guint8 *value)
{
	aim_tlvlist_t *cur;

	if (list == NULL)
		return 0;

	for (cur = *list; ((cur != NULL) && (cur->tlv->type != type)); cur = cur->next);
	if (cur == NULL)
		return aim_tlvlist_add_raw(list, type, length, value);

	free(cur->tlv->value);
	cur->tlv->length = length;
	if (cur->tlv->length > 0) {
		cur->tlv->value = g_memdup(value, length);
	} else
		cur->tlv->value = NULL;

	return cur->tlv->length;
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If
 * you attempt to replace a TLV that does not exist, this function will
 * just add a new TLV as if you called aim_tlvlist_add_str().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param str String to add.
 * @return The length of the TLV.
 */
int aim_tlvlist_replace_str(aim_tlvlist_t **list, const guint16 type, const char *str)
{
	return aim_tlvlist_replace_raw(list, type, strlen(str), (const guchar *)str);
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If
 * you attempt to replace a TLV that does not exist, this function will
 * just add a new TLV as if you called aim_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @return The length of the TLV.
 */
int aim_tlvlist_replace_noval(aim_tlvlist_t **list, const guint16 type)
{
	return aim_tlvlist_replace_raw(list, type, 0, NULL);
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If
 * you attempt to replace a TLV that does not exist, this function will
 * just add a new TLV as if you called aim_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param value 8 bit value to add.
 * @return The length of the TLV.
 */
int aim_tlvlist_replace_8(aim_tlvlist_t **list, const guint16 type, const guint8 value)
{
	guint8 v8[1];

	aimutil_put8(v8, value);

	return aim_tlvlist_replace_raw(list, type, 1, v8);
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If
 * you attempt to replace a TLV that does not exist, this function will
 * just add a new TLV as if you called aim_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param value 32 bit value to add.
 * @return The length of the TLV.
 */
int aim_tlvlist_replace_32(aim_tlvlist_t **list, const guint16 type, const guint32 value)
{
	guint8 v32[4];

	aimutil_put32(v32, value);

	return aim_tlvlist_replace_raw(list, type, 4, v32);
}

/**
 * Remove a TLV of a given type.  If you attempt to remove a TLV that
 * does not exist, nothing happens.
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 */
void aim_tlvlist_remove(aim_tlvlist_t **list, const guint16 type)
{
	aim_tlvlist_t *del;

	if (!list || !(*list))
		return;

	/* Remove the item from the list */
	if ((*list)->tlv->type == type) {
		del = *list;
		*list = (*list)->next;
	} else {
		aim_tlvlist_t *cur;
		for (cur=*list; (cur->next && (cur->next->tlv->type!=type)); cur=cur->next);
		if (!cur->next)
			return;
		del = cur->next;
		cur->next = del->next;
	}

	/* Free the removed item */
	free(del->tlv->value);
	free(del->tlv);
	free(del);
}

/**
 * Write a TLV chain into a data buffer.
 *
 * Copies a TLV chain into a raw data buffer, writing only the number
 * of bytes specified. This operation does not free the chain;
 * aim_tlvlist_free() must still be called to free up the memory used
 * by the chain structures.
 *
 * XXX clean this up, make better use of bstreams
 *
 * @param bs Input bstream
 * @param list Source TLV chain
 * @return Return 0 if the destination bstream is too small.
 */
int aim_tlvlist_write(ByteStream *bs, aim_tlvlist_t **list)
{
	int goodbuflen;
	aim_tlvlist_t *cur;

	/* do an initial run to test total length */
	goodbuflen = aim_tlvlist_size(list);

	if (goodbuflen > byte_stream_empty(bs))
		return 0; /* not enough buffer */

	/* do the real write-out */
	for (cur = *list; cur; cur = cur->next) {
		byte_stream_put16(bs, cur->tlv->type);
		byte_stream_put16(bs, cur->tlv->length);
		if (cur->tlv->length)
			byte_stream_putraw(bs, cur->tlv->value, cur->tlv->length);
	}

	return 1; /* XXX this is a nonsensical return */
}


/**
 * Grab the Nth TLV of type type in the TLV list list.
 *
 * Returns a pointer to an aim_tlv_t of the specified type;
 * %NULL on error.  The @nth parameter is specified starting at %1.
 * In most cases, there will be no more than one TLV of any type
 * in a chain.
 *
 * @param list Source chain.
 * @param type Requested TLV type.
 * @param nth Index of TLV of type to get.
 * @return The TLV you were looking for, or NULL if one could not be found.
 */
aim_tlv_t *aim_tlv_gettlv(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlvlist_t *cur;
	int i;

	for (cur = list, i = 0; cur; cur = cur->next) {
		if (cur && cur->tlv) {
			if (cur->tlv->type == type)
				i++;
			if (i >= nth)
				return cur->tlv;
		}
	}

	return NULL;
}

/**
 * Get the length of the data of the nth TLV in the given TLV chain.
 *
 * @param list Source chain.
 * @param type Requested TLV type.
 * @param nth Index of TLV of type to get.
 * @return The length of the data in this TLV, or -1 if the TLV could not be
 *         found.  Unless -1 is returned, this value will be 2 bytes.
 */
int aim_tlv_getlength(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlvlist_t *cur;
	int i;

	for (cur = list, i = 0; cur; cur = cur->next) {
		if (cur && cur->tlv) {
			if (cur->tlv->type == type)
				i++;
			if (i >= nth)
				return cur->tlv->length;
		}
	}

	return -1;
}

char *
aim_tlv_getvalue_as_string(aim_tlv_t *tlv)
{
	char *ret;

	ret = malloc(tlv->length + 1);
	memcpy(ret, tlv->value, tlv->length);
	ret[tlv->length] = '\0';

	return ret;
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as a string.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value of the TLV you were looking for, or NULL if one could
 *         not be found.  This is a dynamic buffer and must be freed by the
 *         caller.
 */
char *aim_tlv_getstr(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlv_t *tlv;

	if (!(tlv = aim_tlv_gettlv(list, type, nth)))
		return NULL;

	return aim_tlv_getvalue_as_string(tlv);
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as an 8bit
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could
 *         not be found.
 */
guint8 aim_tlv_get8(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlv_t *tlv;

	if (!(tlv = aim_tlv_gettlv(list, type, nth)))
		return 0; /* erm */
	return aimutil_get8(tlv->value);
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as a 16bit
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could
 *         not be found.
 */
guint16 aim_tlv_get16(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlv_t *tlv;

	if (!(tlv = aim_tlv_gettlv(list, type, nth)))
		return 0; /* erm */
	return aimutil_get16(tlv->value);
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as a 32bit
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could
 *         not be found.
 */
guint32 aim_tlv_get32(aim_tlvlist_t *list, const guint16 type, const int nth)
{
	aim_tlv_t *tlv;

	if (!(tlv = aim_tlv_gettlv(list, type, nth)))
		return 0; /* erm */
	return aimutil_get32(tlv->value);
}
