/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: cyrillic.c 1442 2001-01-28 01:52:27Z warmenhoven $
$Log$
Revision 1.3  2001/01/28 01:52:27  warmenhoven
icqlib 1.1.5

Revision 1.7  2000/05/21 17:41:14  denis
Applied patch for russian letters IO and io by
Vladimir Eltchinov <elik@design.ru>

Revision 1.6  2000/02/07 02:31:37  bills
added icq_RusConv_n

Revision 1.5  1999/09/29 16:45:26  denis
Cleanups

Revision 1.4  1999/07/16 12:08:20  denis
Cleaned up.

Revision 1.3  1999/07/12 15:13:29  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.2  1999/04/14 14:44:30  denis
Switched from icq_Log callback to icq_FmtLog function.

Revision 1.1  1999/03/24 11:37:35  denis
Underscored files with TCP stuff renamed.
TCP stuff cleaned up
Function names changed to corresponding names.
icqlib.c splitted to many small files by subject.
C++ comments changed to ANSI C comments.

*/

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"
#include "util.h"

BYTE kw[] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
             144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
             160,161,162,184,164,165,166,167,168,169,170,171,172,173,174,175,
             176,177,178,168,180,181,182,183,184,185,186,187,188,189,190,191,
             254,224,225,246,228,229,244,227,245,232,233,234,235,236,237,238,
             239,255,240,241,242,243,230,226,252,251,231,248,253,249,247,250,
             222,192,193,214,196,197,212,195,213,200,201,202,203,204,205,206,
             207,223,208,209,210,211,198,194,220,219,199,216,221,217,215,218};

BYTE wk[] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
             144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
             160,161,162,163,164,165,166,167,179,169,170,171,172,173,174,175,
             176,177,178,179,180,181,182,183,163,185,186,187,188,189,190,191,
             225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
             242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
             193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
             210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,209};

/********************************************************
Russian language ICQ fix.
Usual Windows ICQ users do use Windows 1251 encoding but
unix users do use koi8 encoding, so we need to convert it.
This function will convert string from windows 1251 to koi8
or from koi8 to windows 1251.
Andrew Frolov dron@ilm.net
*********************************************************/

extern int icq_Russian;

void icq_RusConv(const char to[4], char *t_in)
{
  BYTE *table;
  int i;

/* 6-17-1998 by Linux_Dude
 * Moved initialization of table out front of 'if' block to prevent compiler
 * warning. Improved error message, and now return without performing string
 * conversion to prevent addressing memory out of range (table pointer would
 * previously have remained uninitialized (= bad)).
 */

  table = wk;
  if(strcmp(to, "kw") == 0)
    table = kw;
  else if(strcmp(to, "wk") != 0)
  {
    icq_FmtLog(NULL, ICQ_LOG_ERROR, "Unknown option in call to Russian Convert\n");
    return;
  }
      
/* End Linux_Dude's changes ;) */

  if(icq_Russian)
  {
    for(i=0;t_in[i]!=0;i++)
    {
      t_in[i] &= 0377;
      if(t_in[i] & 0200)
        t_in[i] = table[t_in[i] & 0177];
    }
  }
}

void icq_RusConv_n(const char to[4], char *t_in, int len)
{
  BYTE *table;
  int i;

/* 6-17-1998 by Linux_Dude
 * Moved initialization of table out front of 'if' block to prevent compiler
 * warning. Improved error message, and now return without performing string
 * conversion to prevent addressing memory out of range (table pointer would
 * previously have remained uninitialized (= bad)).
 */

  table = wk;
  if(strcmp(to, "kw") == 0)
    table = kw;
  else if(strcmp(to, "wk") != 0)
  {
    icq_FmtLog(NULL, ICQ_LOG_ERROR, "Unknown option in call to Russian Convert\n");
    return;
  }
      
/* End Linux_Dude's changes ;) */

  if(icq_Russian)
  {
    for(i=0;i < len;i++)
    {
      t_in[i] &= 0377;
      if(t_in[i] & 0200)
        t_in[i] = table[t_in[i] & 0177];
    }
  }
}
