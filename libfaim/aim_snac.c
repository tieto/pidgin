
/*
 *
 * Various SNAC-related dodads... 
 *
 * outstanding_snacs is a list of aim_snac_t structs.  A SNAC should be added
 * whenever a new SNAC is sent and it should remain in the list until the
 * response for it has been receieved.
 *
 * First edition badly written by Adam Fritzler (afritz@delphid.ml.org)
 * Current edition nicely rewritten (it even works) by n (n@ml.org)
 *
 */

#include <faim/aim.h>

u_long aim_newsnac(struct aim_session_t *sess,
		   struct aim_snac_t *newsnac) 
{
  struct aim_snac_t *snac = NULL, *cur = NULL;
  
  if (!newsnac)
    return 0;

  cur = sess->outstanding_snacs;

  snac = calloc(1, sizeof(struct aim_snac_t));
  if (!snac)
    return 0;
  memcpy(snac, newsnac, sizeof(struct aim_snac_t));
  snac->issuetime = time(&snac->issuetime);
  snac->next = NULL;
  
  if (cur == NULL) {
    sess->outstanding_snacs = snac;
    return(snac->id);
  }
  while (cur->next != NULL)
    cur = cur->next;
  cur->next = snac;

  return(snac->id);
}

struct aim_snac_t *aim_remsnac(struct aim_session_t *sess, 
			       u_long id) 
{
  struct aim_snac_t *cur;

  cur = sess->outstanding_snacs;

  if (cur == NULL)
    return(NULL);

  if (cur->id == id) {
    sess->outstanding_snacs = cur->next;
    return(cur);
  }
  while (cur->next != NULL) {
    if (cur->next->id == id) {
      struct aim_snac_t	*tmp = NULL;
      
      tmp = cur->next;
      cur->next = cur->next->next;
      return(tmp);
    }
    cur = cur->next;
  }
  return(NULL);
}

/*
 * This is for cleaning up old SNACs that either don't get replies or
 * a reply was never received for.  Garabage collection. Plain and simple.
 *
 * maxage is the _minimum_ age in seconds to keep SNACs (though I don't know
 * why its called _max_age).
 *
 */
int aim_cleansnacs(struct aim_session_t *sess,
		   int maxage)
{
  struct aim_snac_t *cur;
  struct aim_snac_t *remed = NULL;
  time_t curtime;
 
  cur = sess->outstanding_snacs;
  
  curtime = time(&curtime);
 
  while (cur)
    {
      if ( (cur) && (((cur->issuetime) + maxage) < curtime))
	{
#if DEBUG > 1
	  printf("aimsnac: WARNING purged obsolete snac %08lx\n", cur->id);
#endif
	  remed = aim_remsnac(sess, cur->id);
	  if (remed)
	    {
	      if (remed->data)
		free(remed->data);
	      free(remed);
	    }
	}
      cur = cur->next;
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
