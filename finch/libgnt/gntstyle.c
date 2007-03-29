#include "gntstyle.h"
#include "gntcolors.h"

#include <ctype.h>
#include <string.h>

#if GLIB_CHECK_VERSION(2,6,0)
static GKeyFile *gkfile;
#endif

static char * str_styles[GNT_STYLES];
static int int_styles[GNT_STYLES];
static int bool_styles[GNT_STYLES];

const char *gnt_style_get(GntStyle style)
{
	return str_styles[style];
}

gboolean gnt_style_get_bool(GntStyle style, gboolean def)
{
	int i;
	const char * str;

	if (bool_styles[style] != -1)
		return bool_styles[style];
	
	str = gnt_style_get(style);

	if (str)
	{
		if (strcmp(str, "false") == 0)
			def = FALSE;
		else if (strcmp(str, "true") == 0)
			def = TRUE;
		else if (sscanf(str, "%d", &i) == 1)
		{
			if (i)
				def = TRUE;
			else
				def = FALSE;
		}
	}

	bool_styles[style] = def;
	return bool_styles[style];
}

static void
refine(char *text)
{
	char *s = text, *t = text;

	while (*s)
	{
		if (*s == '^' && *(s + 1) == '[')
		{
			*t = '\033';  /* escape */
			s++;
		}
		else if (*s == '\\')
		{
			if (*(s + 1) == '\0')
				*t = ' ';
			else
			{
				s++;
				if (*s == 'r' || *s == 'n')
					*t = '\r';
				else if (*s == 't')
					*t = '\t';
				else
					*t = *s;
			}
		}
		else
			*t = *s;
		t++;
		s++;
	}
	*t = '\0';
}

static char *
parse_key(const char *key)
{
	return (char *)gnt_key_translate(key);
}

void gnt_style_read_actions(GType type, GntBindableClass *klass)
{
#if GLIB_CHECK_VERSION(2,6,0)
	char *name;
	GError *error = NULL;

	name = g_strdup_printf("%s::binding", g_type_name(type));

	if (g_key_file_has_group(gkfile, name))
	{
		gsize len = 0;
		char **keys;
		
		keys = g_key_file_get_keys(gkfile, name, &len, &error);
		if (error)
		{
			g_printerr("GntStyle: %s\n", error->message);
			g_error_free(error);
			g_free(name);
			return;
		}

		while (len--)
		{
			char *key, *action;

			key = g_strdup(keys[len]);
			action = g_key_file_get_string(gkfile, name, keys[len], &error);

			if (error)
			{
				g_printerr("GntStyle: %s\n", error->message);
				g_error_free(error);
				error = NULL;
			}
			else
			{
				const char *keycode = parse_key(key);
				if (keycode == NULL) {
					g_printerr("GntStyle: Invalid key-binding %s\n", key);
				} else {
					gnt_bindable_register_binding(klass, action, keycode, NULL);
				}
			}
			g_free(key);
			g_free(action);
		}
		g_strfreev(keys);
	}
	g_free(name);
#endif
}

void gnt_styles_get_keyremaps(GType type, GHashTable *hash)
{
#if GLIB_CHECK_VERSION(2,6,0)
	char *name;
	GError *error = NULL;
	
	name = g_strdup_printf("%s::remap", g_type_name(type));

	if (g_key_file_has_group(gkfile, name))
	{
		gsize len = 0;
		char **keys;
		
		keys = g_key_file_get_keys(gkfile, name, &len, &error);
		if (error)
		{
			g_printerr("GntStyle: %s\n", error->message);
			g_error_free(error);
			g_free(name);
			return;
		}

		while (len--)
		{
			char *key, *replace;

			key = g_strdup(keys[len]);
			replace = g_key_file_get_string(gkfile, name, keys[len], &error);

			if (error)
			{
				g_printerr("GntStyle: %s\n", error->message);
				g_error_free(error);
				error = NULL;
				g_free(key);
			}
			else
			{
				refine(key);
				refine(replace);
				g_hash_table_insert(hash, key, replace);
			}
		}
		g_strfreev(keys);
	}

	g_free(name);
#endif
}

#if GLIB_CHECK_VERSION(2,6,0)
static void
read_general_style(GKeyFile *kfile)
{
	GError *error = NULL;
	gsize nkeys;
	char **keys = g_key_file_get_keys(kfile, "general", &nkeys, &error);
	int i;
	struct
	{
		const char *style;
		GntStyle en;
	} styles[] = {{"shadow", GNT_STYLE_SHADOW},
	              {"customcolor", GNT_STYLE_COLOR},
	              {"mouse", GNT_STYLE_MOUSE},
	              {"wm", GNT_STYLE_WM},
	              {"remember_position", GNT_STYLE_REMPOS},
	              {NULL, 0}};

	if (error)
	{
		g_printerr("GntStyle: %s\n", error->message);
		g_error_free(error);
	}
	else
	{
		for (i = 0; styles[i].style; i++)
		{
			str_styles[styles[i].en] =
					g_key_file_get_string(kfile, "general", styles[i].style, NULL);
		}
	}
	g_strfreev(keys);
}
#endif

void gnt_style_read_configure_file(const char *filename)
{
#if GLIB_CHECK_VERSION(2,6,0)
	GError *error = NULL;
	gkfile = g_key_file_new();

	if (!g_key_file_load_from_file(gkfile, filename, G_KEY_FILE_NONE, &error))
	{
		g_printerr("GntStyle: %s\n", error->message);
		g_error_free(error);
		return;
	}
	gnt_colors_parse(gkfile);
	read_general_style(gkfile);
#endif
}

void gnt_init_styles()
{
	int i;
	for (i = 0; i < GNT_STYLES; i++)
	{
		str_styles[i] = NULL;
		int_styles[i] = -1;
		bool_styles[i] = -1;
	}
}

void gnt_uninit_styles()
{
	int i;
	for (i = 0; i < GNT_STYLES; i++)
		g_free(str_styles[i]);

#if GLIB_CHECK_VERSION(2,6,0)
	g_key_file_free(gkfile);
#endif
}

