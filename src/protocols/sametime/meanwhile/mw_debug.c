
/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <glib/gstring.h>

#include "mw_debug.h"


#define FRM               "%02x"
#define FRMT              "%02x%02x "
#define BUF(n)            ((unsigned char) buf[n])
#define ADVANCE(b, n, c)  {b += c; n -= c;}


#ifdef DEBUG
/** writes hex pairs of buf to str */
static void t_pretty_print(GString *str, const char *buf, gsize len) {
  while(len) {
    if(len >= 16) {
      g_string_append_printf(str,
			     FRMT FRMT FRMT FRMT FRMT FRMT FRMT FRMT "\n",
			     BUF(0),  BUF(1),  BUF(2),  BUF(3),
			     BUF(4),  BUF(5),  BUF(6),  BUF(7),
			     BUF(8),  BUF(9),  BUF(10), BUF(11),
			     BUF(12), BUF(13), BUF(14), BUF(15));
      ADVANCE(buf, len, 16);

    } else if(len == 2) {
      g_string_append_printf(str, FRMT "\n", BUF(0), BUF(1));
      ADVANCE(buf, len, 2);
      
    } else if(len > 1) {
      g_string_append_printf(str, FRMT, BUF(0), BUF(1));
      ADVANCE(buf, len, 2);

    } else {
      g_string_append_printf(str, FRM "\n", BUF(0));
      ADVANCE(buf, len, 1);
    }
  }
}
#endif


void pretty_print(const char *buf, gsize len) {
#ifdef DEBUG
  GString *str;

  if(! len) return;

  g_return_if_fail(buf != NULL);

  str = g_string_new(NULL);
  t_pretty_print(str, buf, len);
  g_debug(str->str);
  g_string_free(str, TRUE);
#endif
  ;
}


void pretty_print_opaque(struct mwOpaque *o) {
  if(! o) return;
  pretty_print(o->data, o->len);
}


void mw_debug_mailme_v(struct mwOpaque *block,
		       const char *info, va_list args) {
  /*
    MW_MAILME_MESSAGE
    begin here
    info % args
    pretty_print
    end here
  */

#ifdef DEBUG
  GString *str;
  char *txt;

  str = g_string_new(MW_MAILME_MESSAGE "\n"
		     "  Please send mail to: " MW_MAILME_ADDRESS "\n"
		     MW_MAILME_CUT_START "\n");

  txt = g_strdup_vprintf(info, args);
  g_string_append(str, txt);
  g_free(txt);

  g_string_append(str, "\n");

  if(block) {
    t_pretty_print(str, block->data, block->len);
  }

  g_string_append(str, MW_MAILME_CUT_STOP);

  g_debug(str->str);
  g_string_free(str, TRUE);
#endif
  ;
}


void mw_debug_mailme(struct mwOpaque *block,
		     const char *info, ...) {
  va_list args;
  va_start(args, info);
  mw_debug_mailme_v(block, info, args);
  va_end(args);
}

