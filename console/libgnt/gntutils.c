#include "gntutils.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"

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
	int width = 0;

	while (start < end) {
		width += g_unichar_iswide(g_utf8_get_char(start)) ? 2 : 1;
		start = g_utf8_next_char(start);
	}
	return width;
}

char *gnt_util_onscreen_width_to_pointer(const char *string, int len, int *w)
{
	int size;
	int width = 0;
	char *str = (char*)string;

	while (width < len && *str) {
		size = g_unichar_iswide(g_utf8_get_char(str)) ? 2 : 1;
		if (width + size > len)
			break;
		str = g_utf8_next_char(str);
		width += size;
	}
	if (w)
		*w = width;
	return str;
}

