/* Standard system headers */
#include <stdlib.h>
#include <string.h>

/*
 * memtok differs from strtok in a few ways:
 * The pointer to the buffer to be scanned AND the pointer to the delimiters are NOT NULL terminated
 * strings but are each a pair of a pointer and byte count (so that NIL characters can be contained
 * in either of these buffers!
 *
 * Also memtok does not replace the "found" delimiter with a NIL character, but places the number
 * of bytes delimited by that delimiter into the location of the size_t pointer to by found.
 *
 * The whole **real** point of this function was that strtok skips any repeating delimiters, but we
 * need a function that retuns "empty strings" should there be two delimiters in a row.
 *
 * For some sense of consistency, the byte count of the buffer to be searched through is ALSO ignored
 * by memtok iff the buffer to be scanned is NULL
 *
 * Here's an example:
 *
 * size_t found = 0;
 * char *tok = 0, *buffer = malloc (COUNT);
 * fill_buffer_with_some_data (buffer, COUNT);
 * tok = memtok (buffer, COUNT, "\000\002", 2, &found);
 *
 * if tok != NULL then the bytes from tok to (tok + found) are the token
 * You can then look for more tokens with:
 *
 * tok = memtok (NULL, 0, "\000\002", 2, &found);
 *
 * If tmp == NULL noone of the delimiters were found, however tmp can != NULL and found CAN == 0
 *
 * This means that although a delimiter was found it was immediately preceded by another delimiter and
 * thus delimited an empty token.
 *
 * ( As it happens, if one of the delimiters you want to search for is a NIL character, you can put the
 * other delimiter characters in a string literal and "lie" about how many delimiter characters there are
 * because all string literals are NIL terminated!
 *
 * Therefor the above example could have been written:
 * tok = memtok (buffer, COUNT, "\002", 2, &found);
 *
 * There are also two supplimentary functions that make using these tokens easier
 *
 * memdup is akin to strdup except that instead of it looking for a NIL termination character
 * it simply mallocs copies the specified number of bytes
 *
 * memdupasstr does as memdup except that it mallocs 1 more byte and makes it a NIL char so that you
 * can treat it as a string (as long as you're sure that the memory being described by the pointer and
 * byte count don't already contain any NIL characters)
 *
 */

/**********************************************************************************************************************************/
/* Interface (global) functions */
/**********************************************************************************************************************************/
char *memtok(char *m, size_t bytes, const char *delims, size_t delim_count,
	size_t * found)
{
	static char *mem = 0, *c = 0;
	static size_t offset = 0, offset_now = 0, limit = 0;

	if (0 != m)
	{
		mem = m;
		offset = 0;
		limit = bytes;
	}

	offset_now = offset;

	for (c = mem; offset < limit; ++offset, ++c)
	{
		if (0 != memchr(delims, *c, delim_count))
		{
			static char *ret = 0;

			ret = mem;
			mem = c + 1;
			*found = offset - offset_now;
			offset_now = offset + 1;
			return ret;
		}
	}

	return 0;
}

char *memdup(const char *mem, size_t bytes)
{
	char *dup = 0;

	if (0 < bytes && 0 != mem)
	{
		dup = malloc(bytes);
		memcpy(dup, mem, bytes);
	}

	return dup;
}

char *memdupasstr(const char *mem, size_t bytes)
{
	char *string = 0;

	if (0 < bytes && 0 != mem)
	{
		string = memdup(mem, bytes + 1);
		string[bytes] = '\0';
	}

	return string;
}
