#include <faim/aim.h>

struct aim_tlvlist_t *aim_readtlvchain(u_char *buf, int maxlen)
{
  int pos;
  struct aim_tlvlist_t *list;
  struct aim_tlvlist_t *cur;
  
  u_short type;
  u_short length;

  if (!buf)
    return NULL;

  list = NULL;
  
  pos = 0;

  while (pos < maxlen)
    {
      type = aimutil_get16(buf+pos);
      pos += 2;

      if (pos < maxlen)
	{
	  length = aimutil_get16(buf+pos);
	  pos += 2;
	  
	  if ((pos+length) <= maxlen)
	    {
	      /*
	       * Okay, so now AOL has decided that any TLV of
	       * type 0x0013 can only be two bytes, despite
	       * what the actual given length is.  So here 
	       * we dump any invalid TLVs of that sort.  Hopefully
	       * theres no special cases to this special case.
	       *   - mid (30jun2000)
	       */
	      if ((type == 0x0013) && (length != 0x0002)) {
		printf("faim: skipping TLV t(0013) with invalid length (0x%04x)\n", length);
		length = 0x0002;
	      } else {
		cur = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
		memset(cur, 0x00, sizeof(struct aim_tlvlist_t));

		cur->tlv = aim_createtlv();	
		cur->tlv->type = type;
		cur->tlv->length = length; 
		if (length) {
		  cur->tlv->value = (unsigned char *)malloc(length);
		  memcpy(cur->tlv->value, buf+pos, length);
		} 

		cur->next = list;
		list = cur;
	      }
	      pos += length;
	    }
	}
    }

  return list;
}

void aim_freetlvchain(struct aim_tlvlist_t **list)
{
  struct aim_tlvlist_t *cur, *cur2;

  if (!list || !(*list))
    return;

  cur = *list;
  while (cur)
    {
      aim_freetlv(&cur->tlv);
      cur2 = cur->next;
      free(cur);
      cur = cur2;
    }
  list = NULL;
  return;
}

int aim_counttlvchain(struct aim_tlvlist_t **list)
{
  struct aim_tlvlist_t *cur;
  int count = 0;

  if (!list || !(*list))
    return 0;

  for (cur = *list; cur; cur = cur->next)
    count++;
 
  return count;
}

int aim_addtlvtochain_str(struct aim_tlvlist_t **list, unsigned short type, char *str, int len)
{
  struct aim_tlvlist_t *new;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  new = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(new, 0x00, sizeof(struct aim_tlvlist_t));

  new->tlv = aim_createtlv();	
  new->tlv->type = type;
  new->tlv->length = len;
  new->tlv->value = (u_char *)malloc(new->tlv->length*sizeof(u_char));
  memcpy(new->tlv->value, str, new->tlv->length);

  new->next = NULL;

  if (*list == NULL) {
    *list = new;
  } else if ((*list)->next == NULL) {
    (*list)->next = new;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = new;
  }
  return new->tlv->length;
}

int aim_addtlvtochain16(struct aim_tlvlist_t **list, unsigned short type, unsigned short val)
{
  struct aim_tlvlist_t *new;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  new = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(new, 0x00, sizeof(struct aim_tlvlist_t));

  new->tlv = aim_createtlv();	
  new->tlv->type = type;
  new->tlv->length = 2;
  new->tlv->value = (u_char *)malloc(new->tlv->length*sizeof(u_char));
  aimutil_put16(new->tlv->value, val);

  new->next = NULL;

  if (*list == NULL) {
    *list = new;
  } else if ((*list)->next == NULL) {
    (*list)->next = new;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = new;
  }
  return 2;
}

int aim_addtlvtochain32(struct aim_tlvlist_t **list, unsigned short type, unsigned long val)
{
  struct aim_tlvlist_t *new;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  new = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(new, 0x00, sizeof(struct aim_tlvlist_t));

  new->tlv = aim_createtlv();	
  new->tlv->type = type;
  new->tlv->length = 4;
  new->tlv->value = (u_char *)malloc(new->tlv->length*sizeof(u_char));
  aimutil_put32(new->tlv->value, val);

  new->next = NULL;

  if (*list == NULL) {
    *list = new;
  } else if ((*list)->next == NULL) {
    (*list)->next = new;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = new;
  }
  return 4;
}

int aim_writetlvchain(u_char *buf, int buflen, struct aim_tlvlist_t **list)
{
  int goodbuflen = 0;
  int i = 0;
  struct aim_tlvlist_t *cur;

  if (!list || !buf || !buflen)
    return 0;

  /* do an initial run to test total length */
  for (cur = *list; cur; cur = cur->next) {
    goodbuflen += 2 + 2; /* type + len */
    goodbuflen += cur->tlv->length;
  }

  if (goodbuflen > buflen)
    return 0; /* not enough buffer */

  /* do the real write-out */
  for (cur = *list; cur; cur = cur->next) {
    i += aimutil_put16(buf+i, cur->tlv->type);
    i += aimutil_put16(buf+i, cur->tlv->length);
    memcpy(buf+i, cur->tlv->value, cur->tlv->length);
    i += cur->tlv->length;
  }

  return i;
}


/*
 * Grab the Nth TLV of type type in the TLV list list.
 */
struct aim_tlv_t *aim_gettlv(struct aim_tlvlist_t *list, u_short type, int nth)
{
  int i;
  struct aim_tlvlist_t *cur;
  
  i = 0;
  for (cur = list; cur != NULL; cur = cur->next)
    {
      if (cur && cur->tlv)
	{
	  if (cur->tlv->type == type)
	    i++;
	  if (i >= nth)
	    return cur->tlv;
	}
    }
  return NULL;
}

char *aim_gettlv_str(struct aim_tlvlist_t *list, u_short type, int nth)
{
  struct aim_tlv_t *tlv;
  char *newstr;

  if (!(tlv = aim_gettlv(list, type, nth)))
    return NULL;
  
  newstr = (char *) malloc(tlv->length + 1);
  memcpy(newstr, tlv->value, tlv->length);
  *(newstr + tlv->length) = '\0';

  return newstr;
}

struct aim_tlv_t *aim_grabtlv(u_char *src)
{
  struct aim_tlv_t *dest = NULL;

  dest = aim_createtlv();

  dest->type = src[0] << 8;
  dest->type += src[1];

  dest->length = src[2] << 8;
  dest->length += src[3];

  dest->value = (u_char *) malloc(dest->length*sizeof(u_char));
  memset(dest->value, 0, dest->length*sizeof(u_char));

  memcpy(dest->value, &(src[4]), dest->length*sizeof(u_char));
  
  return dest;
}

struct aim_tlv_t *aim_grabtlvstr(u_char *src)
{
  struct aim_tlv_t *dest = NULL;

  dest = aim_createtlv();

  dest->type = src[0] << 8;
  dest->type += src[1];

  dest->length = src[2] << 8;
  dest->length += src[3];

  dest->value = (u_char *) malloc((dest->length+1)*sizeof(u_char));
  memset(dest->value, 0, (dest->length+1)*sizeof(u_char));

  memcpy(dest->value, &(src[4]), dest->length*sizeof(u_char));
  dest->value[dest->length] = '\0';

  return dest;
}

int aim_puttlv (u_char *dest, struct aim_tlv_t *newtlv)
{
  int i=0;

  dest[i++] = newtlv->type >> 8;
  dest[i++] = newtlv->type & 0x00FF;
  dest[i++] = newtlv->length >> 8;
  dest[i++] = newtlv->length & 0x00FF;
  memcpy(&(dest[i]), newtlv->value, newtlv->length);
  i+=newtlv->length;
  return i;
}

struct aim_tlv_t *aim_createtlv(void)
{
  struct aim_tlv_t *newtlv = NULL;
  newtlv = (struct aim_tlv_t *)malloc(sizeof(struct aim_tlv_t));
  memset(newtlv, 0, sizeof(struct aim_tlv_t));
  return newtlv;
}

int aim_freetlv(struct aim_tlv_t **oldtlv)
{
  if (!oldtlv)
    return -1;
  if (!*oldtlv)
    return -1;
  if ((*oldtlv)->value)
    free((*oldtlv)->value);
  free(*(oldtlv));
  (*oldtlv) = NULL;

  return 0;
}

int aim_puttlv_16(u_char *buf, u_short t, u_short v)
{
  int curbyte=0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)0x0002);
  curbyte += aimutil_put16(buf+curbyte, (u_short)(v&0xffff));
  return curbyte;
}

int aim_puttlv_32(u_char *buf, u_short t, u_long v)
{
  int curbyte=0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)0x0004);
  curbyte += aimutil_put32(buf+curbyte, (u_long)(v&0xffffffff));
  return curbyte;
}

int aim_puttlv_str(u_char *buf, u_short t, u_short l, u_char *v)
{
  int curbyte;
  
  curbyte  = 0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(l&0xffff));
  if (v)
    memcpy(buf+curbyte, v, l);
  curbyte += l;
  return curbyte;
}
