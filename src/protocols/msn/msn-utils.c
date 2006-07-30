/**
 * @file msn-utils.c Utility functions
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */
#include "msn.h"
#include "msn-utils.h"
//#include <openssl/md5.h>

void
msn_parse_format(const char *mime, char **pre_ret, char **post_ret)
{
	char *cur;
	GString *pre  = g_string_new(NULL);
	GString *post = g_string_new(NULL);
	unsigned int colors[3];

	if (pre_ret  != NULL) *pre_ret  = NULL;
	if (post_ret != NULL) *post_ret = NULL;

	cur = strstr(mime, "FN=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		pre = g_string_append(pre, "<FONT FACE=\"");

		while (*cur && *cur != ';')
		{
			pre = g_string_append_c(pre, *cur);
			cur++;
		}

		pre = g_string_append(pre, "\">");
		post = g_string_prepend(post, "</FONT>");
	}

	cur = strstr(mime, "EF=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		while (*cur && *cur != ';')
		{
			pre = g_string_append_c(pre, '<');
			pre = g_string_append_c(pre, *cur);
			pre = g_string_append_c(pre, '>');
			post = g_string_prepend_c(post, '>');
			post = g_string_prepend_c(post, *cur);
			post = g_string_prepend_c(post, '/');
			post = g_string_prepend_c(post, '<');
			cur++;
		}
	}

	cur = strstr(mime, "CO=");

	if (cur && (*(cur = cur + 3) != ';'))
	{
		int i;

		i = sscanf(cur, "%02x%02x%02x;", &colors[0], &colors[1], &colors[2]);

		if (i > 0)
		{
			char tag[64];

			if (i == 1)
			{
				colors[1] = 0;
				colors[2] = 0;
			}
			else if (i == 2)
			{
				unsigned int temp = colors[0];

				colors[0] = colors[1];
				colors[1] = temp;
				colors[2] = 0;
			}
			else if (i == 3)
			{
				unsigned int temp = colors[2];

				colors[2] = colors[0];
				colors[0] = temp;
			}

			g_snprintf(tag, sizeof(tag),
					   "<FONT COLOR=\"#%02hhx%02hhx%02hhx\">",
					   colors[0], colors[1], colors[2]);

			pre = g_string_append(pre, tag);
			post = g_string_prepend(post, "</FONT>");
		}
	}

	cur = g_strdup(gaim_url_decode(pre->str));
	g_string_free(pre, TRUE);

	if (pre_ret != NULL)
		*pre_ret = cur;
	else
		g_free(cur);

	cur = g_strdup(gaim_url_decode(post->str));
	g_string_free(post, TRUE);

	if (post_ret != NULL)
		*post_ret = cur;
	else
		g_free(cur);
}

/*
 * We need this because we're only supposed to encode spaces in the font
 * names. gaim_url_encode() isn't acceptable.
 */
static const char *
encode_spaces(const char *str)
{
	static char buf[BUF_LEN];
	const char *c;
	char *d;

	g_return_val_if_fail(str != NULL, NULL);

	for (c = str, d = buf; *c != '\0'; c++)
	{
		if (*c == ' ')
		{
			*d++ = '%';
			*d++ = '2';
			*d++ = '0';
		}
		else
			*d++ = *c;
	}

	return buf;
}

/*
 * Taken from the zephyr plugin.
 * This parses HTML formatting (put out by one of the gtkimhtml widgets
 * and converts it to msn formatting. It doesn't deal with the tag closing,
 * but gtkimhtml widgets give valid html.
 * It currently deals properly with <b>, <u>, <i>, <font face=...>,
 * <font color=...>.
 * It ignores <font back=...> and <font size=...>
 */
void
msn_import_html(const char *html, char **attributes, char **message)
{
	int len, retcount = 0;
	const char *c;
	char *msg;
	char *fontface = NULL;
	char fonteffect[4];
	char fontcolor[7];

	g_return_if_fail(html       != NULL);
	g_return_if_fail(attributes != NULL);
	g_return_if_fail(message    != NULL);

	len = strlen(html);
	msg = g_malloc0(len + 1);

	memset(fontcolor, 0, sizeof(fontcolor));
	strcat(fontcolor, "0");
	memset(fonteffect, 0, sizeof(fonteffect));

	for (c = html; *c != '\0';)
	{
		if (*c == '<')
		{
			if (!g_ascii_strncasecmp(c + 1, "br>", 3))
			{
				msg[retcount++] = '\r';
				msg[retcount++] = '\n';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c + 1, "i>", 2))
			{
				strcat(fonteffect, "I");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "b>", 2))
			{
				strcat(fonteffect, "B");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "u>", 2))
			{
				strcat(fonteffect, "U");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "s>", 2))
			{
				strcat(fonteffect, "S");
				c += 3;
			}
			else if (!g_ascii_strncasecmp(c + 1, "a href=\"", 8))
			{
				c += 9;

				if (!g_ascii_strncasecmp(c, "mailto:", 7))
					c += 7;

				while ((*c != '\0') && g_ascii_strncasecmp(c, "\">", 2))
					msg[retcount++] = *c++;

				if (*c != '\0')
					c += 2;

				/* ignore descriptive string */
				while ((*c != '\0') && g_ascii_strncasecmp(c, "</a>", 4))
					c++;

				if (*c != '\0')
					c += 4;
			}
			else if (!g_ascii_strncasecmp(c + 1, "font", 4))
			{
				c += 5;

				while ((*c != '\0') && !g_ascii_strncasecmp(c, " ", 1))
					c++;

				if (!g_ascii_strncasecmp(c, "color=\"#", 7))
				{
					c += 8;

					fontcolor[0] = *(c + 4);
					fontcolor[1] = *(c + 5);
					fontcolor[2] = *(c + 2);
					fontcolor[3] = *(c + 3);
					fontcolor[4] = *c;
					fontcolor[5] = *(c + 1);

					c += 8;
				}
				else if (!g_ascii_strncasecmp(c, "face=\"", 6))
				{
					const char *end = NULL;
					const char *comma = NULL;
					unsigned int namelen = 0;

					c += 6;
					end = strchr(c, '\"');
					comma = strchr(c, ',');

					if (comma == NULL || comma > end)
						namelen = (unsigned int)(end - c);
					else
						namelen = (unsigned int)(comma - c);

					fontface = g_strndup(c, namelen);
					c = end + 2;
				}
				else
				{
					/* Drop all unrecognized/misparsed font tags */
					while ((*c != '\0') && g_ascii_strncasecmp(c, "\">", 2))
						c++;

					if (*c != '\0')
						c += 2;
				}
			}
			else
			{
				while ((*c != '\0') && (*c != '>'))
					c++;
				if (*c != '\0')
					c++;
			}
		}
		else if (*c == '&')
		{
			if (!g_ascii_strncasecmp(c, "&lt;", 4))
			{
				msg[retcount++] = '<';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c, "&gt;", 4))
			{
				msg[retcount++] = '>';
				c += 4;
			}
			else if (!g_ascii_strncasecmp(c, "&nbsp;", 6))
			{
				msg[retcount++] = ' ';
				c += 6;
			}
			else if (!g_ascii_strncasecmp(c, "&quot;", 6))
			{
				msg[retcount++] = '"';
				c += 6;
			}
			else if (!g_ascii_strncasecmp(c, "&amp;", 5))
			{
				msg[retcount++] = '&';
				c += 5;
			}
			else if (!g_ascii_strncasecmp(c, "&apos;", 6))
			{
				msg[retcount++] = '\'';
				c += 6;
			}
			else
				msg[retcount++] = *c++;
		}
		else
			msg[retcount++] = *c++;
	}

	if (fontface == NULL)
		fontface = g_strdup("MS Sans Serif");

	*attributes = g_strdup_printf("FN=%s; EF=%s; CO=%s; PF=0",
								  encode_spaces(fontface),
								  fonteffect, fontcolor);
	*message = g_strdup(msg);

	g_free(fontface);
	g_free(msg);
}

void
msn_parse_socket(const char *str, char **ret_host, int *ret_port)
{
	char *host;
	char *c;
	int port;

	host = g_strdup(str);

	if ((c = strchr(host, ':')) != NULL){
		*c = '\0';
		port = atoi(c + 1);
	}else{
		port = 1863;
	}

	*ret_host = host;
	*ret_port = port;
}

/*check the edian of system*/
int 
isBigEndian(void)
{
		short int word = 0x0100;
		char *byte = (char *)&word;

		return(byte[0]);
}

/*swap utility*/
unsigned int 
swapInt(unsigned int dw)
{
		unsigned int tmp;
		tmp =  (dw & 0x000000FF);
		tmp = ((dw & 0x0000FF00) >> 0x08) | (tmp << 0x08);
		tmp = ((dw & 0x00FF0000) >> 0x10) | (tmp << 0x08);
		tmp = ((dw & 0xFF000000) >> 0x18) | (tmp << 0x08);
		return(tmp);
}

/*
 * Handle MSN Chanllege computation
 *This algorithm reference with http://msnpiki.msnfanatic.com/index.php/MSNP11:Challenges
 */
#define BUFSIZE	256
void 
msn_handle_chl(char *input, char *output)
{
		GaimCipher *cipher;
		GaimCipherContext *context;
		char *productKey = MSNP13_WLM_PRODUCT_KEY,
			 *productID  = MSNP13_WLM_PRODUCT_ID,
			 *hexChars   = "0123456789abcdef",
			 buf[BUFSIZE];
		unsigned char md5Hash[16], *newHash;
		unsigned int *md5Parts, *chlStringParts, newHashParts[5];

		long long nHigh=0, nLow=0;

		int i, bigEndian;

		/* Determine our endianess */
		bigEndian = isBigEndian();

		/* Create the MD5 hash by using Gaim MD5 algorithm*/
		cipher = gaim_ciphers_find_cipher("md5");
		context = gaim_cipher_context_new(cipher, NULL);

		gaim_cipher_context_append(context, (const guchar *)input,
						strlen(input));
		gaim_cipher_context_append(context, (const guchar *)productKey,
						strlen(productKey));
		gaim_cipher_context_digest(context, sizeof(md5Hash), md5Hash, NULL);
		gaim_cipher_context_destroy(context);

		/* Split it into four integers */
		md5Parts = (unsigned int *)md5Hash;
		for(i=0; i<4; i++){  
				/* check for endianess */
				if(bigEndian)
						md5Parts[i] = swapInt(md5Parts[i]);

				/* & each integer with 0x7FFFFFFF          */
				/* and save one unmodified array for later */
				newHashParts[i] = md5Parts[i];
				md5Parts[i] &= 0x7FFFFFFF;
		}

		/* make a new string and pad with '0' */
		snprintf(buf, BUFSIZE-5, "%s%s", input, productID);
		i = strlen(buf);
		memset(&buf[i], '0', 8 - (i % 8));
		buf[i + (8 - (i % 8))]='\0';

		/* split into integers */
		chlStringParts = (unsigned int *)buf;

		/* this is magic */
		for (i=0; i<(strlen(buf)/4)-1; i+=2){
				long long temp;

				if(bigEndian){
						chlStringParts[i]   = swapInt(chlStringParts[i]);
						chlStringParts[i+1] = swapInt(chlStringParts[i+1]);
				}

				temp=(md5Parts[0] * (((0x0E79A9C1 * (long long)chlStringParts[i]) % 0x7FFFFFFF)+nHigh) + md5Parts[1])%0x7FFFFFFF;
				nHigh=(md5Parts[2] * (((long long)chlStringParts[i+1]+temp) % 0x7FFFFFFF) + md5Parts[3]) % 0x7FFFFFFF;
				nLow=nLow + nHigh + temp;
		}
		nHigh=(nHigh+md5Parts[1]) % 0x7FFFFFFF;
		nLow=(nLow+md5Parts[3]) % 0x7FFFFFFF;

		newHashParts[0]^=nHigh;
		newHashParts[1]^=nLow;
		newHashParts[2]^=nHigh;
		newHashParts[3]^=nLow;

		/* swap more bytes if big endian */
		for(i=0; i<4 && bigEndian; i++)
				newHashParts[i] = swapInt(newHashParts[i]); 

		/* make a string of the parts */
		newHash = (unsigned char *)newHashParts;

		/* convert to hexadecimal */
		for (i=0; i<16; i++)
		{
				output[i*2]=hexChars[(newHash[i]>>4)&0xF];
				output[(i*2)+1]=hexChars[newHash[i]&0xF];
		}

		output[32]='\0';

//		gaim_debug_info("MaYuan","chl output{%s}\n",output);
}

