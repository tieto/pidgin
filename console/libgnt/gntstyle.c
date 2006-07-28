#include "gntstyle.h"
#include "gntcolors.h"

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

	g_key_file_free(kfile);
#endif
}

