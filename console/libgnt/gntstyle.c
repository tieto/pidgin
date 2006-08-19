#include "gntstyle.h"
#include "gntcolors.h"

#include <string.h>

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
	              {NULL, 0}};

	if (error)
	{
		/* XXX: some error happened. */
		g_error_free(error);
	}
	else
	{
		for (i = 0; styles[i].style; i++)
		{
			error = NULL;
			str_styles[styles[i].en] =
					g_key_file_get_string(kfile, "general", styles[i].style, &error);
		}
	}
}
#endif

void gnt_style_read_configure_file(const char *filename)
{
#if GLIB_CHECK_VERSION(2,6,0)
	GKeyFile *kfile = g_key_file_new();
	GError *error = NULL;

	if (!g_key_file_load_from_file(kfile, filename, G_KEY_FILE_NONE, &error))
	{
		/* XXX: Print the error or something */
		g_error_free(error);
		return;
	}
	gnt_colors_parse(kfile);
	read_general_style(kfile);

	g_key_file_free(kfile);
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
}

