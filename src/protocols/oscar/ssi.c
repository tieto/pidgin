/*
 * Server-Side/Stored Information.
 *
 * Relatively new facility that allows storing of certain types of information,
 * such as a users buddy list, permit/deny list, and permit/deny preferences, 
 * to be stored on the server, so that they can be accessed from any client.
 *
 * This is entirely too complicated.
 * You don't know the half of it.
 *
 * XXX - Test for memory leaks
 * XXX - Better parsing of rights, and use the rights info to limit adds
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * Checks if the given screen name is anywhere in our buddy list.
 */
faim_export int aim_ssi_inlist(aim_session_t *sess, aim_conn_t *conn, char *name, fu16_t type)
{
	struct aim_ssi_item *cur;
	if (!sess && !conn && !name)
		return -EINVAL;
	for (cur=sess->ssi.items; cur; cur=cur->next)
		if ((cur->type==type) && (cur->name) && (!aim_sncmp(cur->name, name)))
			return 1;
	return 0;
}

/*
 * Return the parent group of a given buddy.
 */
faim_export char *aim_ssi_getparentgroup(aim_session_t *sess, aim_conn_t *conn, char *name)
{
	fu16_t gid;
	struct aim_ssi_item *cur;
	if (!sess && !conn && !name)
		return NULL;
	for (cur=sess->ssi.items; cur && (cur->type!=AIM_SSI_TYPE_BUDDY || !cur->name || aim_sncmp(cur->name, name)); cur=cur->next)
	if (!cur)
		return NULL;
	gid = cur->gid;
	for (cur=sess->ssi.items; cur; cur=cur->next)
		if ((cur->type == AIM_SSI_TYPE_GROUP) && (cur->gid == gid) && (cur->name))
			return cur->name;
	return 0;
}

/*
 * Returns a pointer to an item with the given name and type, or NULL if one does not exist.
 */
static struct aim_ssi_item *get_ssi_item(struct aim_ssi_item *items, char *name, fu16_t type)
{
	struct aim_ssi_item *cur;
	if (name) {
		for (cur=items; cur; cur=cur->next)
			if ((cur->type == type) && (cur->name) && !(aim_sncmp(cur->name, name)))
				return cur;
	 } else { /* return the given type with gid 0 */
		for (cur=items; cur; cur=cur->next)
			if ((cur->type == type) && (cur->gid == 0x0000))
				return cur;
	}
	return NULL;
}

/*
 * Returns the permit/deny byte
 */
faim_export int aim_ssi_getpermdeny(aim_session_t *sess, aim_conn_t *conn)
{
	struct aim_ssi_item *cur = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_PDINFO);
	if (cur) {
		aim_tlvlist_t *tlvlist = cur->data;
		if (tlvlist) {
			aim_tlv_t *tlv = aim_gettlv(tlvlist, 0x00ca, 1);
			if (tlv && tlv->value)
				return aimutil_get8(tlv->value);
		}
	}

	return 0;
}

/*
 * Returns the presence flag
 * I'm pretty sure this is a bitmask, but really have no evidence for that.
 * 0x00000400 - Show up as visible to others
 */
faim_export fu32_t aim_ssi_getpresence(aim_session_t *sess, aim_conn_t *conn)
{
	struct aim_ssi_item *cur = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_PRESENCEPREFS);
	if (cur) {
		aim_tlvlist_t *tlvlist = cur->data;
		if (tlvlist) {
			aim_tlv_t *tlv = aim_gettlv(tlvlist, 0x00c9, 1);
			if (tlv && tlv->length)
				return aimutil_get32(tlv->value);
		}
	}

	return 0xFFFFFFFF;
}

/*
 * Add the given packet to the holding queue.
 */
static int aim_ssi_enqueue(aim_session_t *sess, aim_conn_t *conn, aim_frame_t *fr)
{
	aim_frame_t *cur;

	if (!sess || !conn || !fr)
		return -EINVAL;

	fr->next = NULL;
	if (sess->ssi.holding_queue == NULL) {
		sess->ssi.holding_queue = fr;
		if (!sess->ssi.waiting_for_ack)
			aim_ssi_modbegin(sess, conn);
	} else {
		for (cur = sess->ssi.holding_queue; cur->next; cur = cur->next) ;
		cur->next = fr;
	}

	return 0;
}

/*
 * Send the next SNAC from the holding queue.
 */
static int aim_ssi_dispatch(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *cur;

	if (!sess || !conn)
		return -EINVAL;

	if (!sess->ssi.waiting_for_ack) {
		if (sess->ssi.holding_queue) {
			sess->ssi.waiting_for_ack = 1;
			cur = sess->ssi.holding_queue->next;
			sess->ssi.holding_queue->next = NULL;
			aim_tx_enqueue(sess, sess->ssi.holding_queue);
			sess->ssi.holding_queue = cur;
		} else
			aim_ssi_modend(sess, conn);
	}

	return 0;
}

/*
 * Rebuild the additional data for the given group(s).
 */
static int aim_ssi_rebuildgroup(aim_session_t *sess, aim_conn_t *conn, struct aim_ssi_item *parentgroup)
{
	int newlen;
	struct aim_ssi_item *cur;

	if (!sess || !conn || !parentgroup)
		return -EINVAL;

	/* Free the old additional data */
	if (parentgroup->data) {
		aim_freetlvchain((aim_tlvlist_t **)&parentgroup->data);
		parentgroup->data = NULL;
	}

	/* Find the length for the new additional data */
	newlen = 0;
	if (parentgroup->gid == 0x0000) {
		for (cur=sess->ssi.items; cur; cur=cur->next)
			if ((cur->gid != 0x0000) && (cur->type == AIM_SSI_TYPE_GROUP))
				newlen += 2;
	} else {
		for (cur=sess->ssi.items; cur; cur=cur->next)
			if ((cur->gid == parentgroup->gid) && (cur->type == AIM_SSI_TYPE_BUDDY))
				newlen += 2;
	}

	/* Rebuild the additional data */
	if (newlen>0) {
		aim_bstream_t tbs;
		tbs.len = newlen+4;
		tbs.offset = 0;
		if (!(tbs.data = (fu8_t *)malloc((tbs.len)*sizeof(fu8_t))))
			return -ENOMEM;
		aimbs_put16(&tbs, 0x00c8);
		aimbs_put16(&tbs, tbs.len-4);
		if (parentgroup->gid == 0x0000) {
			for (cur=sess->ssi.items; cur; cur=cur->next)
				if ((cur->gid != 0x0000) && (cur->type == AIM_SSI_TYPE_GROUP))
						aimbs_put16(&tbs, cur->gid);
		} else {
			for (cur=sess->ssi.items; cur; cur=cur->next)
				if ((cur->gid == parentgroup->gid) && (cur->type == AIM_SSI_TYPE_BUDDY))
					aimbs_put16(&tbs, cur->bid);
		}
		tbs.offset = 0;
		parentgroup->data = (void *)aim_readtlvchain(&tbs);
		free(tbs.data);

/* XXX - WHYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY does this crash?
		fu8_t *newdata;
		if (!(newdata = (fu8_t *)malloc((newlen)*sizeof(fu8_t))))
			return -ENOMEM;
		newlen = 0;
		if (parentgroup->gid == 0x0000) {
			for (cur=sess->ssi.items; cur; cur=cur->next)
				if ((cur->gid != 0x0000) && (cur->type == AIM_SSI_TYPE_GROUP)) {
						memcpy(&newdata[newlen*2], &cur->gid, 2);
						newlen += 2;
				}
		} else {
			for (cur=sess->ssi.items; cur; cur=cur->next)
				if ((cur->gid == parentgroup->gid) && (cur->type == AIM_SSI_TYPE_BUDDY)) {
						memcpy(&newdata[newlen*2], &cur->bid, 2);
						newlen += 2;
				}
		}
		aim_addtlvtochain_raw((aim_tlvlist_t **)&parentgroup->data, 0x00c8, newlen, newdata);
		free(newdata); */
	}

	return 0;
}

/*
 * Clears all of our locally stored buddy list information.
 */
static int aim_ssi_freelist(aim_session_t *sess)
{
	struct aim_ssi_item *cur, *delitem;

	cur = sess->ssi.items;
	while (cur) {
		if (cur->name)  free(cur->name);
		if (cur->data)  aim_freetlvchain((aim_tlvlist_t **)&cur->data);
		delitem = cur;
		cur = cur->next;
		free(delitem);
	}

	sess->ssi.revision = 0;
	sess->ssi.items = NULL;
	sess->ssi.timestamp = (time_t)0;

	return 0;
}

/*
 * This removes all ssi data from server and local copy.
 */
faim_export int aim_ssi_deletelist(aim_session_t *sess, aim_conn_t *conn)
{
	int num;
	struct aim_ssi_item *cur, **items;

	for (cur=sess->ssi.items, num=0; cur; cur=cur->next)
		num++;

	if (!(items = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *))))
		return -ENOMEM;
	memset(items, 0, num*sizeof(struct aim_ssi_item *));
	for (cur=sess->ssi.items, num=0; cur; cur=cur->next) {
		items[num] = cur;
		num++;
	}

	aim_ssi_addmoddel(sess, conn, items, num, AIM_CB_SSI_DEL);
	free(items);
	aim_ssi_dispatch(sess, conn);
	aim_ssi_freelist(sess);

	return 0;
}

/*
 * This "cleans" the ssi list.  It makes sure all buddies are in a group, and 
 * all groups have additional data for the buddies that are in them.  It does 
 * this by rebuilding the additional data for every group, sending mod SNACs
 * as necessary.
 */
faim_export int aim_ssi_cleanlist(aim_session_t *sess, aim_conn_t *conn)
{
	unsigned int num_to_be_fixed;
	struct aim_ssi_item *cur, *parentgroup;

	/* Make sure we actually need to clean out the list */
	for (cur=sess->ssi.items, num_to_be_fixed=0; cur; cur=cur->next) {
		/* Any buddies directly in the master group */
		if ((cur->type == AIM_SSI_TYPE_BUDDY) && (cur->gid == 0x0000))
			num_to_be_fixed++;
	}
	if (!num_to_be_fixed)
		return 0;

	/* Remove all the additional data from all groups and buddies */
	for (cur=sess->ssi.items; cur; cur=cur->next)
		if (cur->data && ((cur->type == AIM_SSI_TYPE_BUDDY) || (cur->type == AIM_SSI_TYPE_GROUP))) {
			aim_freetlvchain((aim_tlvlist_t **)&cur->data);
			cur->data = NULL;
		}

	/* If there are buddies directly in the master group, make sure  */
	/* there is a group to put them in.  Any group, any group at all. */
	for (cur=sess->ssi.items; ((cur) && ((cur->type != AIM_SSI_TYPE_BUDDY) || (cur->gid != 0x0000))); cur=cur->next);
	if (!cur) {
		for (parentgroup=sess->ssi.items; ((parentgroup) && ((parentgroup->type!=AIM_SSI_TYPE_GROUP) || (parentgroup->gid!=0x0000))); parentgroup=parentgroup->next);
		if (!parentgroup) {
			char *newgroup;
			newgroup = (char*)malloc(strlen("Unknown")*sizeof(char));
			strcpy(newgroup, "Unknown");
			aim_ssi_addgroups(sess, conn, &newgroup, 1);
		}
	}

	/* Set parentgroup equal to any arbitray group */
	for (parentgroup=sess->ssi.items; parentgroup->gid==0x0000 || parentgroup->type!=AIM_SSI_TYPE_GROUP; parentgroup=parentgroup->next);

	/* If there are any buddies directly in the master group, put them in a real group */
	for (cur=sess->ssi.items; cur; cur=cur->next)
		if ((cur->type == AIM_SSI_TYPE_BUDDY) && (cur->gid == 0x0000)) {
			aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_DEL);
			cur->gid = parentgroup->gid;
			aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_ADD);
		}

	/* Rebuild additional data for all groups */
	for (parentgroup=sess->ssi.items; parentgroup; parentgroup=parentgroup->next)
		if (parentgroup->type == AIM_SSI_TYPE_GROUP)
			aim_ssi_rebuildgroup(sess, conn, parentgroup);

	/* Send a mod snac for all groups */
	for (cur=sess->ssi.items; cur; cur=cur->next)
		if (cur->type == AIM_SSI_TYPE_GROUP)
			aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_MOD);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * The next few functions take data as screen names and groups, 
 * modifies the ssi item list, and calls the functions to 
 * add, mod, or del an item or items.
 *
 * These are what the client should call.  The client should 
 * also make sure it's not adding a buddy that's already in 
 * the list (using aim_ssi_inlist).
 */
faim_export int aim_ssi_addbuddies(aim_session_t *sess, aim_conn_t *conn, char *gn, char **sn, unsigned int num)
{
	struct aim_ssi_item *cur, *parentgroup, **newitems;
	fu16_t i, j;

	if (!sess || !conn || !gn || !sn || !num)
		return -EINVAL;

	/* Look up the parent group */
	if (!(parentgroup = get_ssi_item(sess->ssi.items, gn, AIM_SSI_TYPE_GROUP))) {
		aim_ssi_addgroups(sess, conn, &gn, 1);
		if (!(parentgroup = get_ssi_item(sess->ssi.items, gn, AIM_SSI_TYPE_GROUP)))
			return -ENOMEM;
	}

	/* Allocate an array of pointers to each of the new items */
	if (!(newitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *))))
		return -ENOMEM;
	memset(newitems, 0, num*sizeof(struct aim_ssi_item *));

	/* The following for loop iterates once per item that needs to be added. */
	/* For each i, create an item and tack it onto the newitems array */
	for (i=0; i<num; i++) {
		if (!(newitems[i] = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems);
			return -ENOMEM;
		}
		if (!(newitems[i]->name = (char *)malloc((strlen(sn[i])+1)*sizeof(char)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems[i]);
			free(newitems);
			return -ENOMEM;
		}
		strcpy(newitems[i]->name, sn[i]);
		newitems[i]->gid = parentgroup->gid;
		newitems[i]->bid = i ? newitems[i-1]->bid : 0;
		do {
			newitems[i]->bid += 0x0001;
			for (cur=sess->ssi.items, j=0; ((cur) && (!j)); cur=cur->next)
				if ((cur->bid == newitems[i]->bid) && (cur->gid == newitems[i]->gid) && (cur->type == AIM_SSI_TYPE_BUDDY))
					j=1;
		} while (j);
		newitems[i]->type = AIM_SSI_TYPE_BUDDY;
		newitems[i]->data = NULL;
		newitems[i]->next = i ? newitems[i-1] : NULL;
	}

	/* Add the items to our list */
	newitems[0]->next = sess->ssi.items;
	sess->ssi.items = newitems[num-1];

	/* Send the add item SNAC */
	aim_ssi_addmoddel(sess, conn, newitems, num, AIM_CB_SSI_ADD);

	/* Free the array of pointers to each of the new items */
	free(newitems);

	/* Rebuild the additional data in the parent group */
	aim_ssi_rebuildgroup(sess, conn, parentgroup);

	/* Send the mod item SNAC */
	aim_ssi_addmoddel(sess, conn, &parentgroup, 1, AIM_CB_SSI_MOD);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * This adds the master group (the group containing all groups) if it doesn't exist.
 * It's called by aim_ssi_addgroups, if your list is empty.
 */
faim_export int aim_ssi_addmastergroup(aim_session_t *sess, aim_conn_t *conn) {
	struct aim_ssi_item *newitem;

	if (!sess || !conn)
		return -EINVAL;

	/* Allocate an array of pointers to each of the new items */
	if (!(newitem = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item))))
		return -ENOMEM;
	memset(newitem, 0, sizeof(struct aim_ssi_item));

	/* memset to 0 sets most of the vars to what they should be */
	newitem->type = AIM_SSI_TYPE_GROUP;

	/* Add the item to our list */
	newitem->next = sess->ssi.items;
	sess->ssi.items = newitem;

	/* If there are any existing groups (technically there shouldn't be, but */
	/* just in case) then add their group ID#'s to the additional data */
	if (sess->ssi.items)
		aim_ssi_rebuildgroup(sess, conn, newitem);

	/* Send the add item SNAC */
	aim_ssi_addmoddel(sess, conn, &newitem, 1, AIM_CB_SSI_ADD);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_addgroups(aim_session_t *sess, aim_conn_t *conn, char **gn, unsigned int num)
{
	struct aim_ssi_item *cur, *parentgroup, **newitems;
	fu16_t i, j;

	if (!sess || !conn || !gn || !num)
		return -EINVAL;

	/* Look up the parent group */
	if (!(parentgroup = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_GROUP))) {
		aim_ssi_addmastergroup(sess, conn);
		if (!(parentgroup = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_GROUP)))
			return -ENOMEM;
	}

	/* Allocate an array of pointers to each of the new items */
	if (!(newitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *))))
		return -ENOMEM;
	memset(newitems, 0, num*sizeof(struct aim_ssi_item *));

	/* The following for loop iterates once per item that needs to be added. */
	/* For each i, create an item and tack it onto the newitems array */
	for (i=0; i<num; i++) {
		if (!(newitems[i] = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems);
			return -ENOMEM;
		}
		if (!(newitems[i]->name = (char *)malloc((strlen(gn[i])+1)*sizeof(char)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems[i]);
			free(newitems);
			return -ENOMEM;
		}
		strcpy(newitems[i]->name, gn[i]);
		newitems[i]->gid = i ? newitems[i-1]->gid : 0;
		do {
			newitems[i]->gid += 0x0001;
			for (cur=sess->ssi.items, j=0; ((cur) && (!j)); cur=cur->next)
				if ((cur->gid == newitems[i]->gid) && (cur->type == AIM_SSI_TYPE_GROUP))
					j=1;
		} while (j);
		newitems[i]->bid = 0x0000;
		newitems[i]->type = AIM_SSI_TYPE_GROUP;
		newitems[i]->data = NULL;
		newitems[i]->next = i ? newitems[i-1] : NULL;
	}

	/* Add the items to our list */
	newitems[0]->next = sess->ssi.items;
	sess->ssi.items = newitems[num-1];

	/* Send the add item SNAC */
	aim_ssi_addmoddel(sess, conn, newitems, num, AIM_CB_SSI_ADD);

	/* Free the array of pointers to each of the new items */
	free(newitems);

	/* Rebuild the additional data in the parent group */
	aim_ssi_rebuildgroup(sess, conn, parentgroup);

	/* Send the mod item SNAC */
	aim_ssi_addmoddel(sess, conn, &parentgroup, 1, AIM_CB_SSI_MOD);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * Add permit or deny buddies to the permit or deny list.
 * The buddies are passed as an array of pointers to char strings.
 */
faim_export int aim_ssi_addpord(aim_session_t *sess, aim_conn_t *conn, char **sn, unsigned int num, fu16_t type)
{
	struct aim_ssi_item *cur, **newitems;
	fu16_t i, j;

	if (!sess || !conn || !sn || !num || (type!=AIM_SSI_TYPE_PERMIT && type!=AIM_SSI_TYPE_DENY))
		return -EINVAL;

	/* Allocate an array of pointers to each of the new items */
	if (!(newitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *))))
		return -ENOMEM;
	memset(newitems, 0, num*sizeof(struct aim_ssi_item *));

	/* The following for loop iterates once per item that needs to be added. */
	/* For each i, create an item and tack it onto the newitems array */
	for (i=0; i<num; i++) {
		if (!(newitems[i] = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems);
			return -ENOMEM;
		}
		if (!(newitems[i]->name = (char *)malloc((strlen(sn[i])+1)*sizeof(char)))) {
			for (j=0; j<(i-1); j++) {
				free(newitems[j]->name);
				free(newitems[j]);
			}
			free(newitems[i]);
			free(newitems);
			return -ENOMEM;
		}
		strcpy(newitems[i]->name, sn[i]);
		newitems[i]->gid = 0x0000;
		newitems[i]->bid = i ? newitems[i-1]->bid : 0x0000;
		do {
			newitems[i]->bid += 0x0001;
			for (cur=sess->ssi.items, j=0; ((cur) && (!j)); cur=cur->next)
				if ((cur->bid == newitems[i]->bid) && ((cur->type == AIM_SSI_TYPE_PERMIT) || (cur->type == AIM_SSI_TYPE_DENY)))
					j=1;
		} while (j);
		newitems[i]->type = type;
		newitems[i]->data = NULL;
		newitems[i]->next = i ? newitems[i-1] : NULL;
	}

	/* Add the items to our list */
	newitems[0]->next = sess->ssi.items;
	sess->ssi.items = newitems[num-1];

	/* Send the add item SNAC */
	aim_ssi_addmoddel(sess, conn, newitems, num, AIM_CB_SSI_ADD);

	/* Free the array of pointers to each of the new items */
	free(newitems);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_movebuddy(aim_session_t *sess, aim_conn_t *conn, char *oldgn, char *newgn, char *sn)
{
	struct aim_ssi_item **groups, *buddy, *cur;
	fu16_t i;

	if (!sess || !conn || !oldgn || !newgn || !sn)
		return -EINVAL;

	/* Look up the buddy */
	if (!(buddy = get_ssi_item(sess->ssi.items, sn, AIM_SSI_TYPE_BUDDY)))
		return -ENOMEM;

	/* Allocate an array of pointers to the two groups */
	if (!(groups = (struct aim_ssi_item **)malloc(2*sizeof(struct aim_ssi_item *))))
		return -ENOMEM;

	/* Look up the old parent group */
	if (!(groups[0] = get_ssi_item(sess->ssi.items, oldgn, AIM_SSI_TYPE_GROUP))) {
		free(groups);
		return -ENOMEM;
	}

	/* Look up the new parent group */
	if (!(groups[1] = get_ssi_item(sess->ssi.items, newgn, AIM_SSI_TYPE_GROUP))) {
		free(groups);
		return -ENOMEM;
	}

	/* Send the delete item SNAC */
	aim_ssi_addmoddel(sess, conn, &buddy, 1, AIM_CB_SSI_DEL);

	/* Put the buddy in the new group */
	buddy->gid = groups[1]->gid;

	/* Assign a new buddy ID#, because the new group might already have a buddy with this ID# */
	buddy->bid = 0;
	do {
		buddy->bid += 0x0001;
		for (cur=sess->ssi.items, i=0; ((cur) && (!i)); cur=cur->next)
			if ((cur->bid == buddy->bid) && (cur->gid == buddy->gid) && (cur->type == AIM_SSI_TYPE_BUDDY) && (cur->name) && aim_sncmp(cur->name, buddy->name))
				i=1;
	} while (i);

	/* Rebuild the additional data in the two parent groups */
	aim_ssi_rebuildgroup(sess, conn, groups[0]);
	aim_ssi_rebuildgroup(sess, conn, groups[1]);

	/* Send the add item SNAC */
	aim_ssi_addmoddel(sess, conn, &buddy, 1, AIM_CB_SSI_ADD);

	/* Send the mod item SNAC */
	aim_ssi_addmoddel(sess, conn, groups, 2, AIM_CB_SSI_MOD);

	/* Free the temporary array */
	free(groups);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_delbuddies(aim_session_t *sess, aim_conn_t *conn, char *gn, char **sn, unsigned int num)
{
	struct aim_ssi_item *cur, *parentgroup, **delitems;
	int i;

	if (!sess || !conn || !gn || !sn || !num)
		return -EINVAL;

	/* Look up the parent group */
	if (!(parentgroup = get_ssi_item(sess->ssi.items, gn, AIM_SSI_TYPE_GROUP)))
		return -EINVAL;

	/* Allocate an array of pointers to each of the items to be deleted */
	delitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *));
	memset(delitems, 0, num*sizeof(struct aim_ssi_item *));

	/* Make the delitems array a pointer to the aim_ssi_item structs to be deleted */
	for (i=0; i<num; i++) {
		if (!(delitems[i] = get_ssi_item(sess->ssi.items, sn[i], AIM_SSI_TYPE_BUDDY))) {
			free(delitems);
			return -EINVAL;
		}

		/* Remove the delitems from the item list */
		if (sess->ssi.items == delitems[i]) {
			sess->ssi.items = sess->ssi.items->next;
		} else {
			for (cur=sess->ssi.items; (cur->next && (cur->next!=delitems[i])); cur=cur->next);
			if (cur->next)
				cur->next = cur->next->next;
		}
	}

	/* Send the del item SNAC */
	aim_ssi_addmoddel(sess, conn, delitems, num, AIM_CB_SSI_DEL);

	/* Free the items */
	for (i=0; i<num; i++) {
		if (delitems[i]->name)
			free(delitems[i]->name);
		if (delitems[i]->data)
			aim_freetlvchain((aim_tlvlist_t **)&delitems[i]->data);
		free(delitems[i]);
	}
	free(delitems);

	/* Rebuild the additional data in the parent group */
	aim_ssi_rebuildgroup(sess, conn, parentgroup);

	/* Send the mod item SNAC */
	aim_ssi_addmoddel(sess, conn, &parentgroup, 1, AIM_CB_SSI_MOD);

	/* Delete the group, but only if it's empty */
	if (!parentgroup->data)
		aim_ssi_delgroups(sess, conn, &parentgroup->name, 1);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_delmastergroup(aim_session_t *sess, aim_conn_t *conn) {
	struct aim_ssi_item *cur, *delitem;

	if (!sess || !conn)
		return -EINVAL;

	/* Make delitem a pointer to the aim_ssi_item to be deleted */
	if (!(delitem = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_GROUP)))
		return -EINVAL;

	/* Remove delitem from the item list */
	if (sess->ssi.items == delitem) {
		sess->ssi.items = sess->ssi.items->next;
	} else {
		for (cur=sess->ssi.items; (cur->next && (cur->next!=delitem)); cur=cur->next);
		if (cur->next)
			cur->next = cur->next->next;
	}

	/* Send the del item SNAC */
	aim_ssi_addmoddel(sess, conn, &delitem, 1, AIM_CB_SSI_DEL);

	/* Free the item */
	if (delitem->name)
		free(delitem->name);
	if (delitem->data)
		aim_freetlvchain((aim_tlvlist_t **)&delitem->data);
	free(delitem);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_delgroups(aim_session_t *sess, aim_conn_t *conn, char **gn, unsigned int num) {
	struct aim_ssi_item *cur, *parentgroup, **delitems;
	int i;

	if (!sess || !conn || !gn || !num)
		return -EINVAL;

	/* Look up the parent group */
	if (!(parentgroup = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_GROUP)))
		return -EINVAL;

	/* Allocate an array of pointers to each of the items to be deleted */
	delitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *));
	memset(delitems, 0, num*sizeof(struct aim_ssi_item *));

	/* Make the delitems array a pointer to the aim_ssi_item structs to be deleted */
	for (i=0; i<num; i++) {
		if (!(delitems[i] = get_ssi_item(sess->ssi.items, gn[i], AIM_SSI_TYPE_GROUP))) {
			free(delitems);
			return -EINVAL;
		}

		/* Remove the delitems from the item list */
		if (sess->ssi.items == delitems[i]) {
			sess->ssi.items = sess->ssi.items->next;
		} else {
			for (cur=sess->ssi.items; (cur->next && (cur->next!=delitems[i])); cur=cur->next);
			if (cur->next)
				cur->next = cur->next->next;
		}
	}

	/* Send the del item SNAC */
	aim_ssi_addmoddel(sess, conn, delitems, num, AIM_CB_SSI_DEL);

	/* Free the items */
	for (i=0; i<num; i++) {
		if (delitems[i]->name)
			free(delitems[i]->name);
		if (delitems[i]->data)
			aim_freetlvchain((aim_tlvlist_t **)&delitems[i]->data);
		free(delitems[i]);
	}
	free(delitems);

	/* Rebuild the additional data in the parent group */
	aim_ssi_rebuildgroup(sess, conn, parentgroup);

	/* Send the mod item SNAC */
	aim_ssi_addmoddel(sess, conn, &parentgroup, 1, AIM_CB_SSI_MOD);

	/* Delete the group, but only if it's empty */
	if (!parentgroup->data)
		aim_ssi_delmastergroup(sess, conn);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

faim_export int aim_ssi_delpord(aim_session_t *sess, aim_conn_t *conn, char **sn, unsigned int num, fu16_t type) {
	struct aim_ssi_item *cur, **delitems;
	int i;

	if (!sess || !conn || !sn || !num || (type!=AIM_SSI_TYPE_PERMIT && type!=AIM_SSI_TYPE_DENY))
		return -EINVAL;

	/* Allocate an array of pointers to each of the items to be deleted */
	delitems = (struct aim_ssi_item **)malloc(num*sizeof(struct aim_ssi_item *));
	memset(delitems, 0, num*sizeof(struct aim_ssi_item *));

	/* Make the delitems array a pointer to the aim_ssi_item structs to be deleted */
	for (i=0; i<num; i++) {
		if (!(delitems[i] = get_ssi_item(sess->ssi.items, sn[i], type))) {
			free(delitems);
			return -EINVAL;
		}

		/* Remove the delitems from the item list */
		if (sess->ssi.items == delitems[i]) {
			sess->ssi.items = sess->ssi.items->next;
		} else {
			for (cur=sess->ssi.items; (cur->next && (cur->next!=delitems[i])); cur=cur->next);
			if (cur->next)
				cur->next = cur->next->next;
		}
	}

	/* Send the del item SNAC */
	aim_ssi_addmoddel(sess, conn, delitems, num, AIM_CB_SSI_DEL);

	/* Free the items */
	for (i=0; i<num; i++) {
		if (delitems[i]->name)
			free(delitems[i]->name);
		if (delitems[i]->data)
			aim_freetlvchain((aim_tlvlist_t **)&delitems[i]->data);
		free(delitems[i]);
	}
	free(delitems);

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * Stores your permit/deny setting on the server, and starts using it.
 * The permit/deny byte should be:
 * 	1 - Allow all users
 * 	2 - Block all users
 * 	3 - Allow only the users below
 * 	4 - Block the users below
 * 	5 - Allow only users on my buddy list
 * To block AIM users, change the x00cb tlv from xffff to x0004
 */
faim_export int aim_ssi_setpermdeny(aim_session_t *sess, aim_conn_t *conn, int permdeny) {
	struct aim_ssi_item *cur, *tmp;
	fu16_t j;
	aim_tlv_t *tlv;

	if (!sess || !conn)
		return -EINVAL;

	/* Look up the permit/deny settings item */
	cur = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_PDINFO);

	if (cur) {
		/* The permit/deny item exists */
		if (cur->data && (tlv = aim_gettlv(cur->data, 0x00ca, 1))) {
			/* Just change the value of the x00ca TLV */
			if (tlv->length != 1) {
				tlv->length = 1;
				free(tlv->value);
				tlv->value = (fu8_t *)malloc(sizeof(fu8_t));
			}
			tlv->value[0] = permdeny;
		} else {
			/* Need to add the x00ca TLV to the TLV chain */
			aim_addtlvtochain8((aim_tlvlist_t**)&cur->data, 0x00ca, permdeny);
		}

		/* Send the mod item SNAC */
		aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_MOD);
	} else {
		/* Need to add the permit/deny item */
		if (!(cur = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item))))
			return -ENOMEM;
		cur->name = NULL;
		cur->gid = 0x0000;
		cur->bid = 0x007a; /* XXX - Is this number significant? */
		do {
			cur->bid += 0x0001;
			for (tmp=sess->ssi.items, j=0; ((tmp) && (!j)); tmp=tmp->next)
				if (tmp->bid == cur->bid)
					j=1;
		} while (j);
		cur->type = AIM_SSI_TYPE_PDINFO;
		cur->data = NULL;
		aim_addtlvtochain8((aim_tlvlist_t**)&cur->data, 0x00ca, permdeny);
		aim_addtlvtochain32((aim_tlvlist_t**)&cur->data, 0x00cb, 0xffffffff);

		/* Add the item to our list */
		cur->next = sess->ssi.items;
		sess->ssi.items = cur;

		/* Send the add item SNAC */
		aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_ADD);
	}

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * Stores your setting for whether you should show up as idle or not.
 * presence is a bitmask (at least, I think so...)
 * 0x00000400 if you want others to see your idle time
 */
faim_export int aim_ssi_setpresence(aim_session_t *sess, aim_conn_t *conn, fu32_t presence) {
	struct aim_ssi_item *cur, *tmp;
	fu16_t j;
	aim_tlv_t *tlv;

	if (!sess || !conn)
		return -EINVAL;

	/* Look up the item */
	cur = get_ssi_item(sess->ssi.items, NULL, AIM_SSI_TYPE_PRESENCEPREFS);

	if (cur) {
		/* The item exists */
		if (cur->data && (tlv = aim_gettlv(cur->data, 0x00c9, 1))) {
			/* Just change the value of the x00c9 TLV */
			if (tlv->length != 4) {
				tlv->length = 4;
				free(tlv->value);
				tlv->value = (fu8_t *)malloc(4*sizeof(fu8_t));
			}
			aimutil_put32(tlv->value, presence);
		} else {
			/* Need to add the x00c9 TLV to the TLV chain */
			aim_addtlvtochain32((aim_tlvlist_t**)&cur->data, 0x00c9, presence);
		}

		/* Send the mod item SNAC */
		aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_MOD);
	} else {
		/* Need to add the item */
		if (!(cur = (struct aim_ssi_item *)malloc(sizeof(struct aim_ssi_item))))
			return -ENOMEM;
		cur->name = NULL;
		cur->gid = 0x0000;
		cur->bid = 0x007a; /* XXX - Is this number significant? */
		do {
			cur->bid += 0x0001;
			for (tmp=sess->ssi.items, j=0; ((tmp) && (!j)); tmp=tmp->next)
				if (tmp->bid == cur->bid)
					j=1;
		} while (j);
		cur->type = AIM_SSI_TYPE_PRESENCEPREFS;
		cur->data = NULL;
		aim_addtlvtochain32((aim_tlvlist_t**)&cur->data, 0x00c9, presence);

		/* Add the item to our list */
		cur->next = sess->ssi.items;
		sess->ssi.items = cur;

		/* Send the add item SNAC */
		aim_ssi_addmoddel(sess, conn, &cur, 1, AIM_CB_SSI_ADD);
	}

	/* Begin sending SSI SNACs */
	aim_ssi_dispatch(sess, conn);

	return 0;
}

/*
 * Request SSI Rights.
 */
faim_export int aim_ssi_reqrights(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, AIM_CB_FAM_SSI, AIM_CB_SSI_REQRIGHTS);
}

/*
 * SSI Rights Information.
 */
static int parserights(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx);

	return ret;
}

/*
 * Request SSI Data.
 *
 * The data will only be sent if it is newer than the posted local
 * timestamp and revision.
 * 
 * Note that the client should never increment the revision, only the server.
 * 
 */
faim_export int aim_ssi_reqdata(aim_session_t *sess, aim_conn_t *conn, time_t localstamp, fu16_t localrev)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+4+2)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, AIM_CB_FAM_SSI, AIM_CB_SSI_REQLIST, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, AIM_CB_FAM_SSI, AIM_CB_SSI_REQLIST, 0x0000, snacid);
	aimbs_put32(&fr->data, localstamp);
	aimbs_put16(&fr->data, localrev);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * SSI Data.
 */
static int parsedata(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	struct aim_ssi_item *cur = NULL;
	fu8_t fmtver; /* guess */
	fu16_t revision;
	fu32_t timestamp;

	fmtver = aimbs_get8(bs); /* Version of ssi data.  Should be 0x00 */
	revision = aimbs_get16(bs); /* # of times ssi data has been modified */
	if (revision != 0)
		sess->ssi.revision = revision;

	for (cur = sess->ssi.items; cur && cur->next; cur=cur->next) ;

	while (aim_bstream_empty(bs) > 4) { /* last four bytes are stamp */
		fu16_t namelen, tbslen;

		if (!sess->ssi.items) {
			if (!(sess->ssi.items = malloc(sizeof(struct aim_ssi_item))))
				return -ENOMEM;
			cur = sess->ssi.items;
		} else {
			if (!(cur->next = malloc(sizeof(struct aim_ssi_item))))
				return -ENOMEM;
			cur = cur->next;
		}
		memset(cur, 0, sizeof(struct aim_ssi_item));

		if ((namelen = aimbs_get16(bs)))
			cur->name = aimbs_getstr(bs, namelen);
		cur->gid = aimbs_get16(bs);
		cur->bid = aimbs_get16(bs);
		cur->type = aimbs_get16(bs);

		if ((tbslen = aimbs_get16(bs))) {
			aim_bstream_t tbs;

			aim_bstream_init(&tbs, bs->data + bs->offset /* XXX */, tbslen);
			cur->data = (void *)aim_readtlvchain(&tbs);
			aim_bstream_advance(bs, tbslen);
		}
	}

	timestamp = aimbs_get32(bs);
	if (timestamp != 0)
		sess->ssi.timestamp = timestamp;
	sess->ssi.received_data = 1;

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, fmtver, sess->ssi.revision, sess->ssi.timestamp, sess->ssi.items);

	return ret;
}

/*
 * SSI Data Enable Presence.
 *
 * Should be sent after receiving 13/6 or 13/f to tell the server you
 * are ready to begin using the list.  It will promptly give you the
 * presence information for everyone in your list and put your permit/deny
 * settings into effect.
 * 
 */
faim_export int aim_ssi_enable(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, AIM_CB_FAM_SSI, 0x0007);
}

/*
 * SSI Add/Mod/Del Item(s).
 *
 * Sends the SNAC to add, modify, or delete an item from the server-stored
 * information.  These 3 SNACs all have an identical structure.  The only
 * difference is the subtype that is set for the SNAC.
 * 
 */
faim_export int aim_ssi_addmoddel(aim_session_t *sess, aim_conn_t *conn, struct aim_ssi_item **items, unsigned int num, fu16_t subtype)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int i, snaclen;

	if (!sess || !conn || !items || !num)
		return -EINVAL;

	snaclen = 10; /* For family, subtype, flags, and SNAC ID */
	for (i=0; i<num; i++) {
		snaclen += 10; /* For length, GID, BID, type, and length */
		if (items[i]->name)
			snaclen += strlen(items[i]->name);
		if (items[i]->data)
			snaclen += aim_sizetlvchain((aim_tlvlist_t **)&items[i]->data);
	}

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, snaclen)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, AIM_CB_FAM_SSI, subtype, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, AIM_CB_FAM_SSI, subtype, 0x0000, snacid);

	for (i=0; i<num; i++) {
		aimbs_put16(&fr->data, items[i]->name ? strlen(items[i]->name) : 0);
		if (items[i]->name)
			aimbs_putraw(&fr->data, items[i]->name, strlen(items[i]->name));
		aimbs_put16(&fr->data, items[i]->gid);
		aimbs_put16(&fr->data, items[i]->bid);
		aimbs_put16(&fr->data, items[i]->type);
		aimbs_put16(&fr->data, items[i]->data ? aim_sizetlvchain((aim_tlvlist_t **)&items[i]->data) : 0);
		if (items[i]->data)
			aim_writetlvchain(&fr->data, (aim_tlvlist_t **)&items[i]->data);
	}

	aim_ssi_enqueue(sess, conn, fr);

	return 0;
}

/*
 * SSI Add/Mod/Del Ack.
 *
 * Response to add, modify, or delete SNAC (sent with aim_ssi_addmoddel).
 *
 */
static int parseack(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;

	sess->ssi.waiting_for_ack = 0;
	aim_ssi_dispatch(sess, rx->conn);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx);

	return ret;
}

/*
 * SSI Begin Data Modification.
 *
 * Tells the server you're going to start modifying data.
 * 
 */
faim_export int aim_ssi_modbegin(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, AIM_CB_FAM_SSI, AIM_CB_SSI_EDITSTART);
}

/*
 * SSI End Data Modification.
 *
 * Tells the server you're done modifying data.
 *
 */
faim_export int aim_ssi_modend(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, AIM_CB_FAM_SSI, AIM_CB_SSI_EDITSTOP);
}

/*
 * SSI Data Unchanged.
 *
 * Response to aim_ssi_reqdata() if the server-side data is not newer than
 * posted local stamp/revision.
 *
 */
static int parsedataunchanged(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;

	sess->ssi.received_data = 1;

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx);

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == AIM_CB_SSI_RIGHTSINFO)
		return parserights(sess, mod, rx, snac, bs);
	else if (snac->subtype == AIM_CB_SSI_LIST)
		return parsedata(sess, mod, rx, snac, bs);
	else if (snac->subtype == AIM_CB_SSI_SRVACK)
		return parseack(sess, mod, rx, snac, bs);
	else if (snac->subtype == AIM_CB_SSI_NOLIST)
		return parsedataunchanged(sess, mod, rx, snac, bs);

	return 0;
}

static void ssi_shutdown(aim_session_t *sess, aim_module_t *mod)
{
	aim_ssi_freelist(sess);

	return;
}

faim_internal int ssi_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = AIM_CB_FAM_SSI;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x047b;
	mod->flags = 0;
	strncpy(mod->name, "ssi", sizeof(mod->name));
	mod->snachandler = snachandler;
	mod->shutdown = ssi_shutdown;

	return 0;
}
