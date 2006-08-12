#ifndef GNT_KEYS_H
#define GNT_KEYS_H

#define GNT_KEY_POPUP   "[29~"

/* Arrow keys */
#define GNT_KEY_LEFT   "[D"
#define GNT_KEY_RIGHT  "[C"
#define GNT_KEY_UP     "[A"
#define GNT_KEY_DOWN   "[B"

#define GNT_KEY_CTRL_UP     "[1;5A"
#define GNT_KEY_CTRL_DOWN   "[1;5B"
#define GNT_KEY_CTRL_RIGHT  "[1;5C"
#define GNT_KEY_CTRL_LEFT   "[1;5D"

#define GNT_KEY_PGUP   "[5~"
#define GNT_KEY_PGDOWN "[6~"

#define GNT_KEY_ENTER  "\r"

#define GNT_KEY_BACKSPACE "\177"
#define GNT_KEY_DEL    "[3~"

/**
 * This will do stuff with the terminal settings and stuff.
 */
void gnt_keys_refine(char *text);

#endif
