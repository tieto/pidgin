/*
 *
 * Various SNAC-related dodads... 
 *
 * outstanding_snacs is a list of aim_snac_t structs.  A SNAC should be added
 * whenever a new SNAC is sent and it should remain in the list until the
 * response for it has been receieved.  
 *
 * cleansnacs() should be called periodically by the client in order
 * to facilitate the aging out of unreplied-to SNACs. This can and does
 * happen, so it should be handled.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * Called from aim_session_init() to initialize the hash.
 */
faim_internal void aim_initsnachash(struct aim_session_t *sess)
{
  int i;

  for (i = 0; i < FAIM_SNAC_HASH_SIZE; i++) {
    sess->snac_hash[i] = NULL;
    faim_mutex_init(&sess->snac_hash_locks[i]);
  }

  return;
}

faim_internal unsigned long aim_cachesnac(struct aim_session_t *sess,
					  const unsigned short family,
					  const unsigned short type,
					  const unsigned short flags,
					  const void *data, const int datalen)
{
  struct aim_snac_t snac;

  snac.id = sess->snac_nextid++;
  snac.family = family;
  snac.type = type;
  snac.flags = flags;

  if (datalen) {
    if (!(snac.data = malloc(datalen)))
      return 0; /* er... */
    memcpy(snac.data, data, datalen);
  } else
    snac.data = NULL;

  return aim_newsnac(sess, &snac);
}

/*
 * Clones the passed snac structure and caches it in the
 * list/hash.
 */
faim_internal unsigned long aim_newsnac(struct aim_session_t *sess,
					struct aim_snac_t *newsnac) 
{
  struct aim_snac_t *snac = NULL;
  int index;

  if (!newsnac)
    return 0;

  if (!(snac = calloc(1, sizeof(struct aim_snac_t))))
    return 0;
  memcpy(snac, newsnac, sizeof(struct aim_snac_t));
  snac->issuetime = time(&snac->issuetime);
  snac->next = NULL;

  index = snac->id % FAIM_SNAC_HASH_SIZE;

  faim_mutex_lock(&sess->snac_hash_locks[index]);
  snac->next = sess->snac_hash[index];
  sess->snac_hash[index] = snac;
  faim_mutex_unlock(&sess->snac_hash_locks[index]);

  return(snac->id);
}

/*
 * Finds a snac structure with the passed SNAC ID, 
 * removes it from the list/hash, and returns a pointer to it.
 *
 * The returned structure must be freed by the caller.
 *
 */
faim_internal struct aim_snac_t *aim_remsnac(struct aim_session_t *sess, 
					     u_long id) 
{
  struct aim_snac_t *cur = NULL;
  int index;

  index = id % FAIM_SNAC_HASH_SIZE;

  faim_mutex_lock(&sess->snac_hash_locks[index]);
  if (!sess->snac_hash[index])
    ;
  else if (sess->snac_hash[index]->id == id) {
    cur = sess->snac_hash[index];
    sess->snac_hash[index] = cur->next;
  } else {
    cur = sess->snac_hash[index];
    while (cur->next) {
      if (cur->next->id == id) {
	struct aim_snac_t *tmp;
	
	tmp = cur->next;
	cur->next = cur->next->next;
	cur = tmp;
	break;
      }
      cur = cur->next;
    }
  }
  faim_mutex_unlock(&sess->snac_hash_locks[index]);

  return cur;
}

/*
 * This is for cleaning up old SNACs that either don't get replies or
 * a reply was never received for.  Garabage collection. Plain and simple.
 *
 * maxage is the _minimum_ age in seconds to keep SNACs.
 *
 */
faim_internal int aim_cleansnacs(struct aim_session_t *sess,
				 int maxage)
{
  int i;

  for (i = 0; i < FAIM_SNAC_HASH_SIZE; i++) {
    struct aim_snac_t *cur, **prev;
    time_t curtime;

    faim_mutex_lock(&sess->snac_hash_locks[i]);
    if (!sess->snac_hash[i]) {
      faim_mutex_unlock(&sess->snac_hash_locks[i]);
      continue;
    }

    curtime = time(NULL); /* done here in case we waited for the lock */

    for (prev = &sess->snac_hash[i]; (cur = *prev); ) {
      if ((curtime - cur->issuetime) > maxage) {

	*prev = cur->next;

	/* XXX should we have destructors here? */
	if (cur->data)
	  free(cur->data);
	free(cur);

      } else
	prev = &cur->next;
    }

    faim_mutex_unlock(&sess->snac_hash_locks[i]);
  }

  return 0;
}

faim_internal int aim_putsnac(u_char *buf, int family, int subtype, int flags, u_long snacid)
{
  int curbyte = 0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(family&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(subtype&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(flags&0xffff));
  curbyte += aimutil_put32(buf+curbyte, snacid);
  return curbyte;
}
