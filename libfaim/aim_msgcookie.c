
/*
 *
 *
 */

#include <faim/aim.h>

int aim_cachecookie(struct aim_session_t *sess,
		    struct aim_msgcookie_t *cookie)
{
  struct aim_msgcookie_t *newcook = NULL, *cur = NULL;
  
  if (!cookie)
    return -1;

  if (!(newcook = malloc(sizeof(struct aim_msgcookie_t))))
    return -1;
  memcpy(newcook, cookie, sizeof(struct aim_msgcookie_t));
  newcook->addtime = time(NULL);
  newcook->next = NULL;

  cur = sess->msgcookies;
  
  if (cur == NULL) {
    sess->msgcookies = newcook;
    return 0;
  }
  while (cur->next != NULL)
    cur = cur->next;
  cur->next = newcook;

  return 0;
}

struct aim_msgcookie_t *aim_uncachecookie(struct aim_session_t *sess, 
					  char *cookie)
{
  struct aim_msgcookie_t *cur;

  if (!cookie)
    return NULL;

  if (!sess->msgcookies)
    return NULL;

  if (memcmp(sess->msgcookies->cookie, cookie, 8) == 0) {
    cur = sess->msgcookies;
    sess->msgcookies = cur->next;
    return cur;
  } 

  cur = sess->msgcookies;
  while (cur->next) {
    if (memcmp(cur->next->cookie, cookie, 8) == 0) {
      struct aim_msgcookie_t *tmp;
      
      tmp = cur->next;
      cur->next = cur->next->next;
      return tmp;
    }
    cur = cur->next;
  }
  return NULL;
}

/*
 */
int aim_purgecookies(struct aim_session_t *sess)
{
  int maxage = 5*60;
  struct aim_msgcookie_t *cur;
  struct aim_msgcookie_t *remed = NULL;
  time_t curtime;
 
  cur = sess->msgcookies;
  
  curtime = time(&curtime);
 
  while (cur) {
    if ( (cur) && (((cur->addtime) + maxage) < curtime)) {
#if DEBUG > 1
      printf("aimmsgcookie: WARNING purged obsolete message cookie %x%x%x%x %x%x%x%x\n",
	     cur->cookie[0], cur->cookie[1], cur->cookie[2], cur->cookie[3],
	     cur->cookie[4], cur->cookie[5], cur->cookie[6], cur->cookie[7]);
#endif
      remed = aim_uncachecookie(sess, cur->cookie);
      if (remed) {
	if (remed->data)
	  free(remed->data);
	free(remed);
      }
    }
    cur = cur->next;
  }
  
  return 0;
}

