
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

#include <faim/aim.h>

/*
 * Called from aim_session_init() to initialize the hash.
 */
void aim_initsnachash(struct aim_session_t *sess)
{
  int i;

  for (i = 0; i < FAIM_SNAC_HASH_SIZE; i++) {
    sess->snac_hash[i] = NULL;
    faim_mutex_init(&sess->snac_hash_locks[i]);
  }

  return;
}

/*
 * Clones the passed snac structure and caches it in the
 * list/hash.
 */
u_long aim_newsnac(struct aim_session_t *sess,
		   struct aim_snac_t *newsnac) 
{
  struct aim_snac_t *snac = NULL, *cur = NULL;
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
struct aim_snac_t *aim_remsnac(struct aim_session_t *sess, 
			       u_long id) 
{
  struct aim_snac_t *cur = NULL;
  int index;

  index = id % FAIM_SNAC_HASH_SIZE;

  faim_mutex_lock(&sess->snac_hash_locks[index]);
  if (!sess->snac_hash[index])
    ;
  else if (!sess->snac_hash[index]->next) {
    if (sess->snac_hash[index]->id == id) {
      cur = sess->snac_hash[index];
      sess->snac_hash[index] = NULL;
    }
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
int aim_cleansnacs(struct aim_session_t *sess,
		   int maxage)
{
  struct aim_snac_t *cur;
  struct aim_snac_t *remed = NULL;
  time_t curtime;
  int i;

  for (i = 0; i < FAIM_SNAC_HASH_SIZE; i++) {
    faim_mutex_lock(&sess->snac_hash_locks[i]);
    if (!sess->snac_hash[i])
      ;
    else if (!sess->snac_hash[i]->next) {
      if ((sess->snac_hash[i]->issuetime + maxage) >= curtime) {
	remed = sess->snac_hash[i];
	if(remed->data)
	  free(remed->data);
	free(remed);
	sess->snac_hash[i] = NULL;
      }
    } else {
      cur = sess->snac_hash[i];	
      while(cur && cur->next) {
	if ((cur->next->issuetime + maxage) >= curtime) {
	  remed = cur->next;
	  cur->next = cur->next->next;
	  if (remed->data)
	    free(remed->data);	
	  free(remed);
	}
	cur = cur->next;
      }
    }
    faim_mutex_unlock(&sess->snac_hash_locks[i]);
  }

  return 0;
}

int aim_putsnac(u_char *buf, int family, int subtype, int flags, u_long snacid)
{
  int curbyte = 0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(family&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(subtype&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(flags&0xffff));
  curbyte += aimutil_put32(buf+curbyte, snacid);
  return curbyte;
}
