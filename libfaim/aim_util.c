/*
 *
 *
 *
 */

#include <aim.h>
#include <ctype.h>

#define AIMUTIL_USEMACROS

#ifdef AIMUTIL_USEMACROS
/* macros in aim.h */
#else
inline int aimutil_put8(u_char *buf, u_char data)
{
  buf[0] = (u_char)data&0xff;
  return 1;
}

inline u_char aimutil_get8(u_char *buf)
{
  return buf[0];
}

/*
 * Endian-ness issues here?
 */
inline int aimutil_put16(u_char *buf, u_short data)
{
  buf[0] = (u_char)(data>>8)&0xff;
  buf[1] = (u_char)(data)&0xff;
  return 2;
}

inline u_short aimutil_get16(u_char *buf)
{
  u_short val;
  val = (buf[0] << 8) & 0xff00;
  val+= (buf[1]) & 0xff;
  return val;
}

inline int aimutil_put32(u_char *buf, u_long data)
{
  buf[0] = (u_char)(data>>24)&0xff;
  buf[1] = (u_char)(data>>16)&0xff;
  buf[2] = (u_char)(data>>8)&0xff;
  buf[3] = (u_char)(data)&0xff;
  return 4;
}

inline u_long aimutil_get32(u_char *buf)
{
  u_long val;
  val = (buf[0] << 24) & 0xff000000;
  val+= (buf[1] << 16) & 0x00ff0000;
  val+= (buf[2] <<  8) & 0x0000ff00;
  val+= (buf[3]      ) & 0x000000ff;
  return val;
}
#endif /* AIMUTIL_USEMACROS */

inline int aimutil_putstr(u_char *dest, const u_char *src, int len)
{
  memcpy(dest, src, len);
  return len;
}

/*
 * Tokenizing functions.  Used to portably replace strtok/sep.
 *   -- DMP.
 *
 */
int aimutil_tokslen(char *toSearch, int index, char dl)
{
  int curCount = 1;
  char *next;
  char *last;
  int toReturn;

  last = toSearch;
  next = strchr(toSearch, dl);
  
  while(curCount < index && next != NULL)
    {
      curCount++;
      last = next + 1;
      next = strchr(last, dl);
    }
  
  if ((curCount < index) || (next == NULL))
    toReturn = strlen(toSearch) - (curCount - 1);
  else
    toReturn = next - toSearch - (curCount - 1);

  return toReturn;
}

int aimutil_itemcnt(char *toSearch, char dl)
{
  int curCount;
  char *next;
  
  curCount = 1;
  
  next = strchr(toSearch, dl);
  
  while(next != NULL)
    {
      curCount++;
      next = strchr(next + 1, dl);
    }
  
  return curCount;
}

char *aimutil_itemidx(char *toSearch, int index, char dl)
{
  int curCount;
  char *next;
  char *last;
  char *toReturn;
  
  curCount = 0;
  
  last = toSearch;
  next = strchr(toSearch, dl);
  
  while(curCount < index && next != NULL)
    {
      curCount++;
      last = next + 1;
      next = strchr(last, dl);
    }
  
  if (curCount < index)
    {
      toReturn = malloc(sizeof(char));
      *toReturn = '\0';
    }
  next = strchr(last, dl);
  
  if (curCount < index)
    {
      toReturn = malloc(sizeof(char));
      *toReturn = '\0';
    }
  else
    {
      if (next == NULL)
	{
	  toReturn = malloc((strlen(last) + 1) * sizeof(char));
	  strcpy(toReturn, last);
	}
      else
	{
	  toReturn = malloc((next - last + 1) * sizeof(char));
	  memcpy(toReturn, last, (next - last));
	  toReturn[next - last] = '\0';
	}
    }
  return toReturn;
}

/*
 * int snlen(const char *)
 * 
 * This takes a screen name and returns its length without
 * spaces.  If there are no spaces in the SN, then the 
 * return is equal to that of strlen().
 *
 */
int aim_snlen(const char *sn)
{
  int i = 0;
  const char *curPtr = NULL;

  if (!sn)
    return 0;

  curPtr = sn;
  while ( (*curPtr) != (char) NULL) {
      if ((*curPtr) != ' ')
	i++;
      curPtr++;
    }

  return i;
}

/*
 * int sncmp(const char *, const char *)
 *
 * This takes two screen names and compares them using the rules
 * on screen names for AIM/AOL.  Mainly, this means case and space
 * insensitivity (all case differences and spacing differences are
 * ignored).
 *
 * Return: 0 if equal
 *     non-0 if different
 *
 */

int aim_sncmp(const char *sn1, const char *sn2)
{
  const char *curPtr1 = NULL, *curPtr2 = NULL;

  if (aim_snlen(sn1) != aim_snlen(sn2))
    return 1;

  curPtr1 = sn1;
  curPtr2 = sn2;
  while ( (*curPtr1 != (char) NULL) && (*curPtr2 != (char) NULL) ) {
    if ( (*curPtr1 == ' ') || (*curPtr2 == ' ') ) {
      if (*curPtr1 == ' ')
	curPtr1++;
      if (*curPtr2 == ' ')
	curPtr2++;
    } else {
      if ( toupper(*curPtr1) != toupper(*curPtr2))
	return 1;
      curPtr1++;
      curPtr2++;
    }
  }

  return 0;
}
