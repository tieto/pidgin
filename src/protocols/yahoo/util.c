/*
 * gaim
 *
 * Some code copyright 2003 Tim Ringenbach <omarvo@hotmail.com>
 * (marv on irc.freenode.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "prpl.h"
#include "debug.h"

#include <string.h>


/*
 * I found these on some website but i don't know that they actually
 * work (or are supposed to work). I didn't impliment them yet.
 * 
     * [0;30m ---black
     * [1;37m ---white
     * [0;37m ---tan
     * [0;38m ---light black
     * [1;39m ---dark blue
     * [0;32m ---green
     * [0;33m ---yellow
     * [0;35m ---pink
     * [1;35m ---purple
     * [1;30m ---light blue
     * [0;31m ---red
     * [0;34m ---blue
     * [0;36m ---aqua
         * (shift+comma)lyellow(shift+period) ---light yellow
     * (shift+comma)lgreen(shift+period) ---light green
[2;30m <--white out

*/


static GHashTable *ht = NULL;

void yahoo_init_colorht()
{
	ht = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(ht, "30", "<FONT COLOR=\"#000000\">"); /* black */
	g_hash_table_insert(ht, "31", "<FONT COLOR=\"#0000FF\">"); /* blue */
	g_hash_table_insert(ht, "32", "<FONT COLOR=\"#008080\">"); /* cyan */      /* 00b2b2 */
	g_hash_table_insert(ht, "33", "<FONT COLOR=\"#808080\">"); /* gray */      /* 808080 */
	g_hash_table_insert(ht, "34", "<FONT COLOR=\"#008000\">"); /* green */     /* 00c200 */
	g_hash_table_insert(ht, "35", "<FONT COLOR=\"#FF0080\">"); /* pink */      /* ffafaf */
	g_hash_table_insert(ht, "36", "<FONT COLOR=\"#800080\">"); /* purple */    /* b200b2 */
	g_hash_table_insert(ht, "37", "<FONT COLOR=\"#FF8000\">"); /* orange */    /* ffff00 */
	g_hash_table_insert(ht, "38", "<FONT COLOR=\"#FF0000\">"); /* red */
	g_hash_table_insert(ht, "39", "<FONT COLOR=\"#808000\">"); /* olive */     /* 546b50 */

	g_hash_table_insert(ht,  "1",  "<B>");
	g_hash_table_insert(ht, "x1", "</B>");
	g_hash_table_insert(ht,  "2",  "<I>");
	g_hash_table_insert(ht, "x2", "</I>");
	g_hash_table_insert(ht,  "4",  "<U>");
	g_hash_table_insert(ht, "x4", "</U>");

	g_hash_table_insert(ht, "<black>",  "<FONT COLOR=\"#000000\">");
	g_hash_table_insert(ht, "<blue>",   "<FONT COLOR=\"#0000FF\">");
	g_hash_table_insert(ht, "<cyan>",   "<FONT COLOR=\"#008284\">");
	g_hash_table_insert(ht, "<gray>",   "<FONT COLOR=\"#848284\">");
	g_hash_table_insert(ht, "<green>",  "<FONT COLOR=\"#008200\">");
	g_hash_table_insert(ht, "<pink>",   "<FONT COLOR=\"#FF0084\">");
	g_hash_table_insert(ht, "<purple>", "<FONT COLOR=\"#840084\">");
	g_hash_table_insert(ht, "<orange>", "<FONT COLOR=\"#FF8000\">");
	g_hash_table_insert(ht, "<red>",    "<FONT COLOR=\"#FF0000\">");
	g_hash_table_insert(ht, "<yellow>", "<FONT COLOR=\"#848200\">");

	g_hash_table_insert(ht, "</black>",  "</FONT>");
	g_hash_table_insert(ht, "</blue>",   "</FONT>");
	g_hash_table_insert(ht, "</cyan>",   "</FONT>");
	g_hash_table_insert(ht, "</gray>",   "</FONT>");
	g_hash_table_insert(ht, "</green>",  "</FONT>");
	g_hash_table_insert(ht, "</pink>",   "</FONT>");
	g_hash_table_insert(ht, "</purple>", "</FONT>");
	g_hash_table_insert(ht, "</orange>", "</FONT>");
	g_hash_table_insert(ht, "</red>",    "</FONT>");
	g_hash_table_insert(ht, "</yellow>", "</FONT>");

	/* remove these once we have proper support for <FADE> and <ALT> */
	g_hash_table_insert(ht, "</fade>", "");
	g_hash_table_insert(ht, "</alt>", "");
}

void yahoo_dest_colorht()
{
	g_hash_table_destroy(ht);
}

char *yahoo_codes_to_html(char *x)
{
	GString *s, *tmp;
	int i, j, xs, nomoreendtags = 0;
	char *match, *ret;


	s = g_string_sized_new(strlen(x));

	for (i = 0, xs = strlen(x); i < xs; i++) {
		if ((x[i] == 0x1b) && (x[i+1] == '[')) {
			j = i + 1;

			while (j++ < xs) {
				if (x[j] != 'm')
					continue;
				else {
					tmp = g_string_new_len(x + i + 2, j - i - 2);
					if (tmp->str[0] == '#')
						g_string_append_printf(s, "<FONT COLOR=\"%s\">", tmp->str);
					else if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else {
						gaim_debug(GAIM_DEBUG_ERROR, "yahoo",
							"Unknown ansi code 'ESC[%sm'.\n", tmp->str);
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}
			}


		} else if (!nomoreendtags && (x[i] == '<')) {
			j = i + 1;

			while (j++ < xs) {
				if (x[j] != '>')
					if (j == xs) {
						g_string_append_c(s, '<');
						nomoreendtags = 1;
					}
					else
						continue;
				else {
					tmp = g_string_new_len(x + i, j - i + 1);
					g_string_ascii_down(tmp);

					if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else if (!g_ascii_strncasecmp(tmp->str, "<FADE ", 6) ||
						!g_ascii_strncasecmp(tmp->str, "<ALT ", 5) ||
						!g_ascii_strncasecmp(tmp->str, "<SND ", 5)) {

						/* remove this if gtkhtml ever supports any of these */
						i = j;
						g_string_free(tmp, TRUE);
						break;

					} else {
						g_string_append_c(s, '<');
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}

			}



		} else {
			g_string_append_c(s, x[i]);
		}
	}

	ret = s->str;
	g_string_free(s, FALSE);
	gaim_debug(GAIM_DEBUG_MISC, "yahoo", "Returning string: '%s'.\n", ret);
	return ret;
}
