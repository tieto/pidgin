
#define FAIM_INTERNAL
#include <aim.h>

/**
 * aim_readtlvchain - Read a TLV chain from a buffer.
 * @buf: Input buffer
 * @maxlen: Length of input buffer
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, aim_freetlvchain() should
 * be called to free the dynamic substructures.
 *
 */
faim_export struct aim_tlvlist_t *aim_readtlvchain(u_char *buf, int maxlen)
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
	      if ((type == 0x0013) && (length != 0x0002))
		length = 0x0002;
	      else {
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

/**
 * aim_freetlvchain - Free a TLV chain structure
 * @list: Chain to be freed
 *
 * Walks the list of TLVs in the passed TLV chain and
 * frees each one. Note that any references to this data
 * should be removed before calling this.
 *
 */
faim_export void aim_freetlvchain(struct aim_tlvlist_t **list)
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

/**
 * aim_counttlvchain - Count the number of TLVs in a chain
 * @list: Chain to be counted
 *
 * Returns the number of TLVs stored in the passed chain.
 *
 */
faim_export int aim_counttlvchain(struct aim_tlvlist_t **list)
{
  struct aim_tlvlist_t *cur;
  int count = 0;

  if (!list || !(*list))
    return 0;

  for (cur = *list; cur; cur = cur->next)
    count++;
 
  return count;
}

/**
 * aim_sizetlvchain - Count the number of bytes in a TLV chain
 * @list: Chain to be sized
 *
 * Returns the number of bytes that would be needed to 
 * write the passed TLV chain to a data buffer.
 *
 */
faim_export int aim_sizetlvchain(struct aim_tlvlist_t **list)
{
  struct aim_tlvlist_t *cur;
  int size = 0;

  if (!list || !(*list))
    return 0;

  for (cur = *list; cur; cur = cur->next)
    size += (4 + cur->tlv->length);
 
  return size;
}

/**
 * aim_addtlvtochain_str - Add a string to a TLV chain
 * @list: Desination chain (%NULL pointer if empty)
 * @type: TLV type
 * @str: String to add
 * @len: Length of string to add (not including %NULL)
 *
 * Adds the passed string as a TLV element of the passed type
 * to the TLV chain.
 *
 */
faim_export int aim_addtlvtochain_str(struct aim_tlvlist_t **list, unsigned short type, char *str, int len)
{
  struct aim_tlvlist_t *newtlv;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  newtlv = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(newtlv, 0x00, sizeof(struct aim_tlvlist_t));

  newtlv->tlv = aim_createtlv();	
  newtlv->tlv->type = type;
  newtlv->tlv->length = len;
  newtlv->tlv->value = (unsigned char *)malloc(newtlv->tlv->length*sizeof(unsigned char));
  memcpy(newtlv->tlv->value, str, newtlv->tlv->length);

  newtlv->next = NULL;

  if (*list == NULL) {
    *list = newtlv;
  } else if ((*list)->next == NULL) {
    (*list)->next = newtlv;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = newtlv;
  }
  return newtlv->tlv->length;
}

/**
 * aim_addtlvtochain16 - Add a 16bit integer to a TLV chain
 * @list: Destination chain
 * @type: TLV type to add
 * @val: Value to add
 *
 * Adds a two-byte unsigned integer to a TLV chain.
 *
 */
faim_export int aim_addtlvtochain16(struct aim_tlvlist_t **list, unsigned short type, unsigned short val)
{
  struct aim_tlvlist_t *newtl;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  newtl = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(newtl, 0x00, sizeof(struct aim_tlvlist_t));

  newtl->tlv = aim_createtlv();	
  newtl->tlv->type = type;
  newtl->tlv->length = 2;
  newtl->tlv->value = (unsigned char *)malloc(newtl->tlv->length*sizeof(unsigned char));
  aimutil_put16(newtl->tlv->value, val);

  newtl->next = NULL;

  if (*list == NULL) {
    *list = newtl;
  } else if ((*list)->next == NULL) {
    (*list)->next = newtl;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = newtl;
  }
  return 2;
}

/**
 * aim_addtlvtochain32 - Add a 32bit integer to a TLV chain
 * @list: Destination chain
 * @type: TLV type to add
 * @val: Value to add
 *
 * Adds a four-byte unsigned integer to a TLV chain.
 *
 */
faim_export int aim_addtlvtochain32(struct aim_tlvlist_t **list, unsigned short type, unsigned long val)
{
  struct aim_tlvlist_t *newtl;
  struct aim_tlvlist_t *cur;

  if (!list)
    return 0;

  newtl = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(newtl, 0x00, sizeof(struct aim_tlvlist_t));

  newtl->tlv = aim_createtlv();	
  newtl->tlv->type = type;
  newtl->tlv->length = 4;
  newtl->tlv->value = (unsigned char *)malloc(newtl->tlv->length*sizeof(unsigned char));
  aimutil_put32(newtl->tlv->value, val);

  newtl->next = NULL;

  if (*list == NULL) {
    *list = newtl;
  } else if ((*list)->next == NULL) {
    (*list)->next = newtl;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = newtl;
  }
  return 4;
}

/**
 * aim_addtlvtochain_caps - Add a capability block to a TLV chain
 * @list: Destination chain
 * @type: TLV type to add
 * @caps: Bitfield of capability flags to send
 *
 * Adds a block of capability blocks to a TLV chain. The bitfield
 * passed in should be a bitwise %OR of any of the %AIM_CAPS constants:
 *
 *      %AIM_CAPS_BUDDYICON   Supports Buddy Icons
 *
 *      %AIM_CAPS_VOICE       Supports Voice Chat
 *
 *      %AIM_CAPS_IMIMAGE     Supports DirectIM/IMImage
 *
 *      %AIM_CAPS_CHAT        Supports Chat
 *
 *      %AIM_CAPS_GETFILE     Supports Get File functions
 *
 *      %AIM_CAPS_SENDFILE    Supports Send File functions
 *
 */
faim_export int aim_addtlvtochain_caps(struct aim_tlvlist_t **list, unsigned short type, unsigned short caps)
{
  unsigned char buf[128]; /* icky fixed length buffer */
  struct aim_tlvlist_t *newtl;
  struct aim_tlvlist_t *cur;

  if(!list)
    return 0;

  newtl = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(newtl, 0x00, sizeof(struct aim_tlvlist_t));

  newtl->tlv = aim_createtlv();	
  newtl->tlv->type = type;

  newtl->tlv->length = aim_putcap(buf, sizeof(buf), caps);
  newtl->tlv->value = (unsigned char *)calloc(1, newtl->tlv->length);
  memcpy(newtl->tlv->value, buf, newtl->tlv->length);

  newtl->next = NULL;

  if (*list == NULL) {
    *list = newtl;
  } else if ((*list)->next == NULL) {
    (*list)->next = newtl;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = newtl;
  }
  return newtl->tlv->length;
}

/**
 * aim_addtlvtochain_noval - Add a blank TLV to a TLV chain
 * @list: Destination chain
 * @type: TLV type to add
 *
 * Adds a TLV with a zero length to a TLV chain.
 *
 */
faim_internal int aim_addtlvtochain_noval(struct aim_tlvlist_t **list, unsigned short type)
{
  struct aim_tlvlist_t *newtlv;
  struct aim_tlvlist_t *cur;

  newtlv = (struct aim_tlvlist_t *)malloc(sizeof(struct aim_tlvlist_t));
  memset(newtlv, 0x00, sizeof(struct aim_tlvlist_t));

  newtlv->tlv = aim_createtlv();	
  newtlv->tlv->type = type;
  newtlv->tlv->length = 0;
  newtlv->tlv->value = NULL;

  newtlv->next = NULL;

  if (*list == NULL) {
    *list = newtlv;
  } else if ((*list)->next == NULL) {
    (*list)->next = newtlv;
  } else {
    for(cur = *list; cur->next; cur = cur->next)
      ;
    cur->next = newtlv;
  }
  return newtlv->tlv->length;
}

/**
 * aim_writetlvchain - Write a TLV chain into a data buffer.
 * @buf: Destination buffer
 * @buflen: Maximum number of bytes that will be written to buffer
 * @list: Source TLV chain
 *
 * Copies a TLV chain into a raw data buffer, writing only the number
 * of bytes specified. This operation does not free the chain; 
 * aim_freetlvchain() must still be called to free up the memory used
 * by the chain structures.
 *
 */
faim_export int aim_writetlvchain(u_char *buf, int buflen, struct aim_tlvlist_t **list)
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


/**
 * aim_gettlv - Grab the Nth TLV of type type in the TLV list list.
 * @list: Source chain
 * @type: Requested TLV type
 * @nth: Index of TLV of type to get
 *
 * Returns a pointer to an aim_tlv_t of the specified type; 
 * %NULL on error.  The @nth parameter is specified starting at %1.
 * In most cases, there will be no more than one TLV of any type
 * in a chain.
 *
 */
faim_export struct aim_tlv_t *aim_gettlv(struct aim_tlvlist_t *list, u_short type, int nth)
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

/**
 * aim_gettlv_str - Retrieve the Nth TLV in chain as a string.
 * @list: Source TLV chain
 * @type: TLV type to search for
 * @nth: Index of TLV to return
 *
 * Same as aim_gettlv(), except that the return value is a %NULL-
 * terminated string instead of an aim_tlv_t.  This is a 
 * dynamic buffer and must be freed by the caller.
 *
 */
faim_export char *aim_gettlv_str(struct aim_tlvlist_t *list, u_short type, int nth)
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

/**
 * aim_gettlv8 - Retrieve the Nth TLV in chain as a 8bit integer.
 * @list: Source TLV chain
 * @type: TLV type to search for
 * @nth: Index of TLV to return
 *
 * Same as aim_gettlv(), except that the return value is a 
 * 8bit integer instead of an aim_tlv_t. 
 *
 */
faim_internal unsigned char aim_gettlv8(struct aim_tlvlist_t *list, unsigned short type, int num)
{
  struct aim_tlv_t *tlv;

  if (!(tlv = aim_gettlv(list, type, num)) || !tlv->value)
    return 0; /* erm */
  return aimutil_get8(tlv->value);
}

/**
 * aim_gettlv16 - Retrieve the Nth TLV in chain as a 16bit integer.
 * @list: Source TLV chain
 * @type: TLV type to search for
 * @nth: Index of TLV to return
 *
 * Same as aim_gettlv(), except that the return value is a 
 * 16bit integer instead of an aim_tlv_t. 
 *
 */
faim_internal unsigned short aim_gettlv16(struct aim_tlvlist_t *list, unsigned short type, int num)
{
  struct aim_tlv_t *tlv;

  if (!(tlv = aim_gettlv(list, type, num)) || !tlv->value)
    return 0; /* erm */
  return aimutil_get16(tlv->value);
}

/**
 * aim_gettlv32 - Retrieve the Nth TLV in chain as a 32bit integer.
 * @list: Source TLV chain
 * @type: TLV type to search for
 * @nth: Index of TLV to return
 *
 * Same as aim_gettlv(), except that the return value is a 
 * 32bit integer instead of an aim_tlv_t. 
 *
 */
faim_internal unsigned long aim_gettlv32(struct aim_tlvlist_t *list, unsigned short type, int num)
{
  struct aim_tlv_t *tlv;

  if (!(tlv = aim_gettlv(list, type, num)) || !tlv->value)
    return 0; /* erm */
  return aimutil_get32(tlv->value);
}

/**
 * aim_grabtlv - Grab a single TLV from a data buffer
 * @src: Source data buffer (must be at least 4 bytes long)
 *
 * Creates a TLV structure aim_tlv_t and returns it
 * filled with values from a buffer, possibly including a 
 * dynamically allocated buffer for the value portion.
 *
 * Both the aim_tlv_t and the tlv->value pointer
 * must be freed by the caller if non-%NULL.
 *
 */
faim_export struct aim_tlv_t *aim_grabtlv(u_char *src)
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

/**
 * aim_grabtlvstr - Grab a single TLV from a data buffer as string
 * @src: Source data buffer (must be at least 4 bytes long)
 *
 * Creates a TLV structure aim_tlv_t and returns it
 * filled with values from a buffer, possibly including a 
 * dynamically allocated buffer for the value portion, which 
 * is %NULL-terminated as a string.
 *
 * Both the aim_tlv_t and the tlv->value pointer
 * must be freed by the caller if non-%NULL.
 *
 */
faim_export struct aim_tlv_t *aim_grabtlvstr(u_char *src)
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

/**
 * aim_puttlv - Write a aim_tlv_t into a data buffer
 * @dest: Destination data buffer
 * @newtlv: Source TLV structure
 *
 * Writes out the passed TLV structure into the buffer. No bounds
 * checking is done on the output buffer.
 *
 * The passed aim_tlv_t is not freed. aim_freetlv() should
 * still be called by the caller to free the structure.
 *
 */
faim_export int aim_puttlv(u_char *dest, struct aim_tlv_t *newtlv)
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

/**
 * aim_createtlv - Generate an aim_tlv_t structure.
 * 
 * Allocates an empty TLV structure and returns a pointer
 * to it; %NULL on error.
 *
 */
faim_export struct aim_tlv_t *aim_createtlv(void)
{
  struct aim_tlv_t *newtlv;

  if (!(newtlv = (struct aim_tlv_t *)malloc(sizeof(struct aim_tlv_t))))
    return NULL;
  memset(newtlv, 0, sizeof(struct aim_tlv_t));
  return newtlv;
}

/**
 * aim_freetlv - Free a aim_tlv_t structure
 * @oldtlv: TLV to be destroyed
 *
 * Frees both the TLV structure and the value portion.
 *
 */
faim_export int aim_freetlv(struct aim_tlv_t **oldtlv)
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

/**
 * aim_puttlv_8 - Write a one-byte TLV.
 * @buf: Destination buffer
 * @t: TLV type
 * @v: Value
 *
 * Writes a TLV with a one-byte integer value portion.
 *
 */
faim_export int aim_puttlv_8(unsigned char *buf, unsigned short t, unsigned char  v)
{
  int curbyte=0;

  curbyte += aimutil_put16(buf+curbyte, (unsigned short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (unsigned short)0x0001);
  curbyte += aimutil_put8(buf+curbyte, (unsigned char)(v&0xff));

  return curbyte;
}

/**
 * aim_puttlv_16 - Write a two-byte TLV.
 * @buf: Destination buffer
 * @t: TLV type
 * @v: Value
 *
 * Writes a TLV with a two-byte integer value portion.
 *
 */
faim_export int aim_puttlv_16(u_char *buf, u_short t, u_short v)
{
  int curbyte=0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)0x0002);
  curbyte += aimutil_put16(buf+curbyte, (u_short)(v&0xffff));
  return curbyte;
}

/**
 * aim_puttlv_32 - Write a four-byte TLV.
 * @buf: Destination buffer
 * @t: TLV type
 * @v: Value
 *
 * Writes a TLV with a four-byte integer value portion.
 *
 */
faim_export int aim_puttlv_32(u_char *buf, u_short t, u_long v)
{
  int curbyte=0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)0x0004);
  curbyte += aimutil_put32(buf+curbyte, (u_long)(v&0xffffffff));
  return curbyte;
}

/**
 * aim_puttlv_str - Write a string TLV.
 * @buf: Destination buffer
 * @t: TLV type
 * @l: Length of string
 * @v: String to write
 *
 * Writes a TLV with a string value portion.  (Only the first @l
 * bytes of the passed string will be written, which should not
 * include the terminating NULL.)
 *
 */
faim_export int aim_puttlv_str(u_char *buf, u_short t, int l, char *v)
{
  int curbyte;
  
  curbyte  = 0;
  curbyte += aimutil_put16(buf+curbyte, (u_short)(t&0xffff));
  curbyte += aimutil_put16(buf+curbyte, (u_short)(l&0xffff));
  if (v)
    memcpy(buf+curbyte, (unsigned char *)v, l);
  curbyte += l;
  return curbyte;
}
