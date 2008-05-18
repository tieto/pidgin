#include "module.h"

static void
purple_perl_util_url_cb(PurpleUtilFetchUrlData *url_data, void *user_data,
                        const gchar *url_text, size_t size,
                        const gchar *error_message)
{
	SV *sv = (SV *)user_data;
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpvn(url_text, size)));
	PUTBACK;

	call_sv(sv, G_EVAL | G_SCALAR);
	SPAGAIN;

	/* XXX Make sure this destroys it correctly and that we don't want
	 * something like sv_2mortal(sv) or something else here instead. */
	SvREFCNT_dec(sv);

	PUTBACK;
	FREETMPS;
	LEAVE;
}

MODULE = Purple::Util  PACKAGE = Purple::Util  PREFIX = purple_
PROTOTYPES: ENABLE

int
purple_build_dir(path, mode)
	const char *path
	int mode

gboolean
purple_email_is_valid(address)
	const char *address

const char *
purple_escape_filename(str)
	const char *str

gchar_own *
purple_fd_get_ip(fd)
	int fd

const gchar *
purple_home_dir()

gboolean
purple_message_meify(message, len)
	char *message
	size_t len

FILE *
purple_mkstemp(path, binary)
	char **path
	gboolean binary

const char *
purple_normalize(account, str)
	Purple::Account account
	const char *str

gboolean
purple_program_is_valid(program)
	const char *program

gchar_own *
purple_strcasereplace(string, delimiter, replacement)
	const char *string
	const char *delimiter
	const char *replacement

const char *
purple_strcasestr(haystack, needle)
	const char *haystack
	const char *needle

gchar_own *
purple_strdup_withhtml(src)
	const gchar *src

gchar_own *
purple_strreplace(string, delimiter, replacement)
	const char *string
	const char *delimiter
	const char *replacement

gchar_own *
purple_text_strip_mnemonic(in)
	const char *in

time_t
purple_time_build(year, month, day, hour, min, sec)
	int year
	int month
	int day
	int hour
	int min
	int sec

const char *
purple_time_format(tm)
	const struct tm *tm

const char *
purple_unescape_filename(str)
	const char *str

gchar_own *
purple_unescape_html(html)
	const char *html

const char *
purple_url_decode(str)
	const char *str

const char *
purple_url_encode(str)
	const char *str

gboolean
purple_url_parse(url, ret_host, ret_port, ret_path, ret_user, ret_passwd)
	const char *url
	char **ret_host
	int *ret_port
	char **ret_path
	char **ret_user
	char **ret_passwd

const char *
purple_user_dir()

const char *
purple_utf8_strftime(const char *format, const struct tm *tm);

MODULE = Purple::Util  PACKAGE = Purple::Util::Str  PREFIX = purple_str_
PROTOTYPES: ENABLE

gchar_own *
purple_str_add_cr(str)
	const char *str

gchar_own *
purple_str_binary_to_ascii(binary, len)
	const unsigned char *binary
	guint len

gboolean
purple_str_has_prefix(s, p)
	const char *s
	const char *p

gboolean
purple_str_has_suffix(s, x)
	const char *s
	const char *x

gchar_own *
purple_str_seconds_to_string(sec)
	guint sec

gchar_own *
purple_str_size_to_units(size)
	size_t size

void
purple_str_strip_char(str, thechar)
	char *str
	char thechar

time_t
purple_str_to_time(timestamp, utc = FALSE, tm = NULL, tz_off = NULL, rest = NULL)
	const char *timestamp
	gboolean utc
	struct tm *tm
	long *tz_off
	const char **rest

MODULE = Purple::Util  PACKAGE = Purple::Util::Date  PREFIX = purple_date_
PROTOTYPES: ENABLE

const char *
purple_date_format_full(tm)
	const struct tm *tm

const char *
purple_date_format_long(tm)
	const struct tm *tm

const char *
purple_date_format_short(tm)
	const struct tm *tm

MODULE = Purple::Util  PACKAGE = Purple::Util::Markup  PREFIX = purple_markup_
PROTOTYPES: ENABLE

gboolean
purple_markup_extract_info_field(str, len, user_info, start_token, skip, end_token, check_value, no_value_token, display_name, is_link, link_prefix, format_cb)
	const char *str
	int len
	Purple::NotifyUserInfo user_info
	const char *start_token
	int skip
	const char *end_token
	char check_value
	const char *no_value_token
	const char *display_name
	gboolean is_link
	const char *link_prefix
	Purple::Util::InfoFieldFormatCallback format_cb

gboolean
purple_markup_find_tag(needle, haystack, start, end, attributes)
	const char *needle
	const char *haystack
	const char **start
	const char **end
	GData **attributes

gchar_own *
purple_markup_get_tag_name(tag)
	const char *tag

void
purple_markup_html_to_xhtml(html, dest_xhtml, dest_plain)
	const char *html
	char **dest_xhtml
	char **dest_plain

gchar_own *
purple_markup_linkify(str)
	const char *str

gchar_own *
purple_markup_slice(str, x, y)
	const char *str
	guint x
	guint y

gchar_own *
purple_markup_strip_html(str)
	const char *str

MODULE = Purple::Util  PACKAGE = Purple::Util  PREFIX = purple_util_
PROTOTYPES: ENABLE

void
purple_util_fetch_url(plugin, url, full, user_agent, http11, cb)
	Purple::Plugin plugin
	const char *url
	gboolean full
	const char *user_agent
	gboolean http11
	SV * cb
CODE:
	SV *sv = purple_perl_sv_from_fun(plugin, cb);

	if (sv != NULL) {
		purple_util_fetch_url(url, full, user_agent, http11,
		                      purple_perl_util_url_cb, sv);
	} else {
		purple_debug_warning("perl", "Callback not a valid type, only strings and coderefs allowed in purple_util_fetch_url.\n");
	}

void
purple_util_set_user_dir(dir)
	const char *dir

gboolean
purple_util_write_data_to_file(filename, data, size)
	const char *filename
	const char *data
	size_t size
