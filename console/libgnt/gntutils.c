#include "gntutils.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"

#ifndef HAVE_WCWIDTH
#define wcwidth(X) 1
#else
#define __USE_XOPEN
#endif

#include <wchar.h>

void gnt_util_get_text_bound(const char *text, int *width, int *height)
{
	const char *s = text, *last;
	int count = 1, max = 0;
	int len;

	/* XXX: ew ... everyone look away */
	last = s;
	if (s)
	{
		while (*s)
		{
			if (*s == '\n' || *s == '\r')
			{
				count++;
				len = g_utf8_pointer_to_offset(last, s);
				if (max < len)
					max = len;
				last = s + 1;
			}
			s++;
		}

		len = g_utf8_pointer_to_offset(last, s);
		if (max < len)
			max = len;
	}

	if (height)
		*height = count;
	if (width)
		*width = max + (count > 1);
}

int gnt_util_onscreen_width(const char *start, const char *end)
{
	wchar_t wch;
	int size;
	int width = 0;

	while (start < end) {
		if ((size = mbtowc(&wch, start, end - start)) > 0) {
			start += size;
			width += wcwidth(wch);
		} else {
			++width;
			++start;
		}
	}
	return width;
}

char *gnt_util_onscreen_width_to_pointer(const char *string, int len, int *w)
{
	wchar_t wch;
	int size;
	int width = 0;
	char *str = (char*)string;
	int slen = strlen(string);  /* Yeah, no. of bytes */

	while (width < len && *str) {
		if ((size = mbtowc(&wch, str, slen)) > 0) {
			if (width + wcwidth(wch) > len)
				break;
			str += size;
			width += wcwidth(wch);
			slen -= size;
		} else {
			++str;
			++width;
			--slen;
		}
	}

	if (w)
		*w = width;

	return str;
}

