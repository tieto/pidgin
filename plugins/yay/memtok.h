#ifndef MEMTOK_H
#define MEMTOK_H

char *memtok (char *m, size_t bytes, const char *delims, size_t delim_count, size_t *found);
char *memdup (const char *mem, size_t bytes);
char *memdupasstr (const char *mem, size_t bytes);

#endif /* ndef MEMTOK_H */
