/*
 * Cookie Caching stuff. Adam wrote this, apparently just some
 * derivatives of n's SNAC work. I cleaned it up, added comments.
 * 
 * I'm going to rewrite this stuff eventually, honest. -jbm
 * 
 */

/*
 * I'm assuming that cookies are type-specific. that is, we can have
 * "1234578" for type 1 and type 2 concurrently. if i'm wrong, then we
 * lose some error checking. if we assume cookies are not type-specific and are
 * wrong, we get quirky behavior when cookies step on each others' toes.
 */

#include <faim/aim.h>

/*
 * aim_cachecookie: 
 * appends a cookie to the cookie list for sess.
 * - if cookie->cookie for type cookie->type is found, addtime is updated.
 * - copies cookie struct; you need to free() it afterwards;
 * - cookie->data is not copied, but passed along. don't free it.
 * - newcook->addtime is updated accordingly;
 * - cookie->type is just passed across.
 * 
 * returns -1 on error, 0 on success.  
 */
faim_internal int aim_cachecookie(struct aim_session_t *sess,
				  struct aim_msgcookie_t *cookie)
{
  struct aim_msgcookie_t *newcook;

  if (!cookie)
    return -1;

  if( (newcook = aim_checkcookie(sess, cookie->cookie, cookie->type)) ) {
    newcook->addtime = time(NULL);
    if(cookie->data != newcook->data) {
      
      printf("faim: cachecookie: matching cookie/type pair "
	     "%x%x%x%x%x%x%x%x/%x has different *data. free()ing cookie copy..\n",
	     cookie->cookie[0], cookie->cookie[1], cookie->cookie[2],
	     cookie->cookie[3], cookie->cookie[4], cookie->cookie[5],
	     cookie->cookie[6], cookie->cookie[7], cookie->type);
      
      free(cookie->data);
    }

    return 0;
  }
  
  if (!(newcook = malloc(sizeof(struct aim_msgcookie_t))))
    return -1;
  memcpy(newcook, cookie, sizeof(struct aim_msgcookie_t));
  newcook->addtime = time(NULL);
  
  newcook->next = sess->msgcookies;
  sess->msgcookies = newcook;

  return 0;
}

/*
 * aim_uncachecookie:
 * takes a cookie string and grabs the cookie struct associated with
 * it. removes struct from chain.  returns the struct if found, or
 * NULL on not found.
 */
faim_internal struct aim_msgcookie_t *aim_uncachecookie(struct aim_session_t *sess, unsigned char *cookie, int type)
{
  struct aim_msgcookie_t *cur;

  if (!cookie || !sess->msgcookies)
    return NULL;

  if ((sess->msgcookies->type == type) && 
      (memcmp(sess->msgcookies->cookie, cookie, 8) == 0)) {
    struct aim_msgcookie_t *tmp;

    tmp = sess->msgcookies;
    sess->msgcookies = sess->msgcookies->next;

    return tmp;
  } 

  for (cur = sess->msgcookies; cur->next; cur = cur->next) {
    if ((cur->next->type == type) &&
	(memcmp(cur->next->cookie, cookie, 8) == 0)) {
      struct aim_msgcookie_t *tmp;
      
      tmp = cur->next;
      cur->next = cur->next->next;

      return tmp;
    }
  }

  return NULL;
}

/*
 * aim_purgecookies:
 * purge out old cookies
 *
 * finds old cookies, calls uncache on them.  
 *
 * this is highly inefficient, but It Works. and i don't feel like
 * totally rewriting this. it might have some concurrency issues as
 * well, if i rewrite it.
 *
 * i'll avoid the puns.  
 */

faim_export int aim_purgecookies(struct aim_session_t *sess, int maxage)
{
  struct aim_msgcookie_t *cur;
  time_t curtime;
 
  curtime = time(NULL);

  for (cur = sess->msgcookies; cur; cur = cur->next) {
    if (cur->addtime > (time(NULL) - maxage)) {
      struct aim_msgcookie_t *remed = NULL;

#if 1
      printf("aimmsgcookie: WARNING purged obsolete message cookie %x%x%x%x %x%x%x%x\n",
	     cur->cookie[0], cur->cookie[1], cur->cookie[2], cur->cookie[3],
	     cur->cookie[4], cur->cookie[5], cur->cookie[6], cur->cookie[7]);
#endif

      remed = aim_uncachecookie(sess, cur->cookie, cur->type);
      if (remed) {
	if (remed->data)
	  free(remed->data);
	free(remed);
      }
    }
  }
  
  return 0;
}

faim_internal struct aim_msgcookie_t *aim_mkcookie(unsigned char *c, int type, void *data) 
{
  struct aim_msgcookie_t *cookie;

  if (!c)
    return NULL;

  if (!(cookie = calloc(1, sizeof(struct aim_msgcookie_t))))
    return NULL;
  
  cookie->data = data;
  cookie->type = type;
  memcpy(cookie->cookie, c, 8);
  
  return cookie;
}
  
faim_internal struct aim_msgcookie_t *aim_checkcookie(struct aim_session_t *sess, unsigned char *cookie, int type)
{
  struct aim_msgcookie_t *cur;
  
  for (cur = sess->msgcookies; cur; cur = cur->next) {
    if ((cur->type == type) && 
	(memcmp(cur->cookie, cookie, 8) == 0))
      return cur;
  }

  return NULL;
}

static int aim_freecookie(struct aim_msgcookie_t *cookie) {
  return 0;
} 

faim_internal int aim_msgcookie_gettype(int reqclass) {
  /* XXX: hokey-assed. needs fixed. */
  switch(reqclass) {
  case AIM_CAPS_BUDDYICON:
    return AIM_COOKIETYPE_OFTICON;
    break;
  case AIM_CAPS_VOICE:
    return AIM_COOKIETYPE_OFTVOICE;
    break;
  case AIM_CAPS_IMIMAGE:
    return AIM_COOKIETYPE_OFTIMAGE;
    break;
  case AIM_CAPS_CHAT:
    return AIM_COOKIETYPE_CHAT;
    break;
  case AIM_CAPS_GETFILE:
    return AIM_COOKIETYPE_OFTGET;
    break;
  case AIM_CAPS_SENDFILE:
    return AIM_COOKIETYPE_OFTSEND;
    break;
  default:
    return AIM_COOKIETYPE_UNKNOWN;
    break;
  }           
}
