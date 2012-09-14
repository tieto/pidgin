#ifndef _GGP_HTML_H
#define _GGP_HTML_H

#include <internal.h>

typedef enum
{
	GGP_HTML_TAG_UNKNOWN,
	GGP_HTML_TAG_IGNORED,
	GGP_HTML_TAG_EOM,
	GGP_HTML_TAG_B,
	GGP_HTML_TAG_I,
	GGP_HTML_TAG_U,
	GGP_HTML_TAG_S,
	GGP_HTML_TAG_FONT,
	GGP_HTML_TAG_SPAN,
	GGP_HTML_TAG_DIV,
	GGP_HTML_TAG_BR,
	GGP_HTML_TAG_HR,
} ggp_html_tag;

void ggp_html_setup(void);
void ggp_html_cleanup(void);

GHashTable * ggp_html_tag_attribs(const gchar *attribs_str);
GHashTable * ggp_html_css_attribs(const gchar *attribs_str);
int ggp_html_decode_color(const gchar *str);
ggp_html_tag ggp_html_parse_tag(const gchar *tag_str);


#endif /* _GGP_HTML_H */
