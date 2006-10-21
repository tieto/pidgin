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
#define GNT_KEY_HOME   "[7~"
#define GNT_KEY_END    "[8~"

#define GNT_KEY_ENTER  "\r"

#define GNT_KEY_BACKSPACE "\177"
#define GNT_KEY_DEL    "[3~"
#define GNT_KEY_INS    "[2~"

#define GNT_KEY_CTRL_A     "\001"
#define GNT_KEY_CTRL_B     "\002"
#define GNT_KEY_CTRL_D     "\004"
#define GNT_KEY_CTRL_E     "\005"
#define GNT_KEY_CTRL_F     "\006"
#define GNT_KEY_CTRL_G     "\007"
#define GNT_KEY_CTRL_H     "\010"
#define GNT_KEY_CTRL_I     "\011"
#define GNT_KEY_CTRL_J     "\012"
#define GNT_KEY_CTRL_K     "\013"
#define GNT_KEY_CTRL_L     "\014"
#define GNT_KEY_CTRL_M     "\012"
#define GNT_KEY_CTRL_N     "\016"
#define GNT_KEY_CTRL_O     "\017"
#define GNT_KEY_CTRL_P     "\020"
#define GNT_KEY_CTRL_R     "\022"
#define GNT_KEY_CTRL_T     "\024"
#define GNT_KEY_CTRL_U     "\025"
#define GNT_KEY_CTRL_V     "\026"
#define GNT_KEY_CTRL_W     "\027"
#define GNT_KEY_CTRL_X     "\030"
#define GNT_KEY_CTRL_Y     "\031"

#define GNT_KEY_F1         "[[A"
#define GNT_KEY_F2         "[[B"
#define GNT_KEY_F3         "[[C"
#define GNT_KEY_F4         "[[D"
#define GNT_KEY_F5         "[[E"
#define GNT_KEY_F6         "[17~"
#define GNT_KEY_F7         "[18~"
#define GNT_KEY_F8         "[19~"
#define GNT_KEY_F9         "[20~"
#define GNT_KEY_F10        "[21~"
#define GNT_KEY_F11        "[23~"
#define GNT_KEY_F12        "[24~"

/**
 * This will do stuff with the terminal settings and stuff.
 */
void gnt_keys_refine(char *text);

#endif
