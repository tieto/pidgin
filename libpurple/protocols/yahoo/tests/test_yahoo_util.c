#include <glib.h>

#include "../ymsg.h"

typedef struct {
	gchar *input;
	gchar *output;
} YahooStringTestData;

static void
test_codes_to_html(void) {
	YahooStringTestData data[] = {
		{
			"",
			"",
		}, {
			"\x1B[12345m",
			"",
		}, {
			"plain",
			"plain",
		}, {
			"unknown \x1B[12345m ansi code",
			"unknown  ansi code",
		}, {
			"plain <peanut>",
			"plain &lt;peanut&gt;",
		}, {
			"plain <peanut",
			"plain &lt;peanut",
		}, {
			"plain> peanut",
			"plain&gt; peanut",
		}, {
			"<font face='inva>lid'>test",
			"<font face='inva&gt;lid'>test</font>",
		}, {
			"<font face='inva>lid",
			"&lt;font face=&apos;inva&gt;lid",
		}, {
			"\x1B[1mbold",
			"<b>bold</b>",
		}, {
			"\x1B[2mitalic",
			"<i>italic</i>",
		}, {
			"\x1B[4munderline",
			"<u>underline</u>",
		}, {
			"no\x1B[x4m markup",
			"no markup",
		}, {
			/* bold italic underline */
			"\x1B[1mbold\x1B[x1m \x1B[2mitalic\x1B[x2m \x1B[4munderline",
			"<b>bold</b> <i>italic</i> <u>underline</u>",
		}, {
			"\x1B[1mbold \x1B[2mbolditalic\x1B[x1m italic",
			"<b>bold <i>bolditalic</i></b><i> italic</i>",
		}, {
			"\x1B[1mbold \x1B[2mbolditalic\x1B[x1m \x1B[4mitalicunderline",
			"<b>bold <i>bolditalic</i></b><i> <u>italicunderline</u></i>",
		}, {
			"\x1B[1mbold \x1B[2mbolditalic \x1B[4mbolditalicunderline\x1B[x2m boldunderline",
			"<b>bold <i>bolditalic <u>bolditalicunderline</u></i><u> boldunderline</u></b>",
		}, {
			"\x1B[1mbold \x1B[2mbolditalic \x1B[4mbolditalicunderline\x1B[x1m italicunderline",
			"<b>bold <i>bolditalic <u>bolditalicunderline</u></i></b><i><u> italicunderline</u></i>",
		}, {
			/* links */
			"\x1B[lmhttps://pidgin.im/\x1B[xlm",
			"https://pidgin.im/",
		}, {
			/* font color */
			"\x1B[31mblue",
			"<font color='#0000FF'>blue</font>",
		}, {
			"\x1B[#70ea15mcustom color",
			"<font color='#70ea15'>custom color</font>",
		}, {
			"<ALT #ff0000,#00ff00,#0000ff>test</ALT>",
			"test",
		}, {
			/* font face */
			"<font face='Georgia'>test",
			"<font face='Georgia'>test</font>",
		}, {
			/* font size */
			"<font size='15'>test",
			"<font size='4' absz='15'>test</font>",
		}, {
			"<font size='32'>size 32",
			"<font size='6' absz='32'>size 32</font>",
		}, {
			/* combinations */
			"<font face='Georgia' size='32'>test",
			"<font face='Georgia' size='6' absz='32'>test</font>",
		}, {
			"\x1B[35m<font size='15'>test",
			"<font color='#FF0080'><font size='4' absz='15'>test</font></font>",
		}, {
			"<FADE #ff0000,#00ff00,#0000ff>:<</FADE>",
			":&lt;",
		}, {
			NULL,
			NULL,
		}
	};
	gint i;

	yahoo_init_colorht();

	for(i = 0; data[i].input; i++) {
		gchar *result = yahoo_codes_to_html(data[i].input);

		g_assert_cmpstr(result, ==, data[i].output);

		g_free(result);
	}

	yahoo_dest_colorht();
}

static void
test_html_to_codes(void) {
	YahooStringTestData data[] = {
		{
			"plain",
			"plain",
		}, {
			"plain &lt;peanut&gt;",
			"plain <peanut>",
		}, {
			"plain &lt;peanut",
			"plain <peanut",
		}, {
			"plain&gt; peanut",
			"plain> peanut",
		}, {
			"plain &gt;",
			"plain >",
		}, {
			"plain &gt; ",
			"plain > ",
		}, {
			"plain &lt;",
			"plain <",
		}, {
			"plain &lt; ",
			"plain < ",
		}, {
			"plain &lt",
			"plain &lt",
		}, {
			"plain &amp;",
			"plain &",
		}, {
			/* bold/italic/underline */
			"<b>bold</b>",
			"\x1B[1mbold\x1B[x1m",
		}, {
			"<i>italic</i>",
			"\x1B[2mitalic\x1B[x2m",
		}, {
			"<u>underline</u>",
			"\x1B[4munderline\x1B[x4m",
		}, {
			"no</u> markup",
			"no markup",
		}, {
			"<b>bold</b> <i>italic</i> <u>underline</u>",
			"\x1B[1mbold\x1B[x1m \x1B[2mitalic\x1B[x2m \x1B[4munderline\x1B[x4m",
		}, {
			"<b>bold <i>bolditalic</i></b><i> italic</i>",
			"\x1B[1mbold \x1B[2mbolditalic\x1B[x2m\x1B[x1m\x1B[2m italic\x1B[x2m",
		}, {
			"<b>bold <i>bolditalic</i></b><i> <u>italicunderline</u></i>",
			"\x1B[1mbold \x1B[2mbolditalic\x1B[x2m\x1B[x1m\x1B[2m \x1B[4mitalicunderline\x1B[x4m\x1B[x2m",
		}, {
			/* link */
			"<A HREF=\"https://pidgin.im/\">https://pidgin.im/</A>",
			"https://pidgin.im/",
		}, {
			"<A HREF=\"mailto:mark@example.com\">mark@example.com</A>",
			"mark@example.com",
		}, {
			/* font nothing */
			"<font>nothing</font>",
			"nothing",
		}, {
			/* font color */
			"<font color=\"#E71414\">red</font>",
			"\x1B[#E71414mred\x1B[#000000m",
		}, {
			"<font color=\"#FF0000\">red</font> <font color=\"#0000FF\">blue</font> black",
			"\x1B[#FF0000mred\x1B[#000000m \x1B[#0000FFmblue\x1B[#000000m black",
		}, {
			/* font size */
			"<font size=\"2\">test</font>",
			"<font size=\"10\">test</font>",
		}, {
			"<font size=\"6\">test</font>",
			"<font size=\"30\">test</font>",
		}, {
			/* combinations */
			"<font color=\"#FF0000\"><font size=\"1\">redsmall</font> rednormal</font>",
			"\x1B[#FF0000m<font size=\"8\">redsmall</font> rednormal\x1B[#000000m",
		}, {
			"<font color=\"#FF0000\"><font size=\"1\">redsmall</font> <font color=\"#00FF00\">greennormal</font> rednormal</font>",
			"\x1B[#FF0000m<font size=\"8\">redsmall</font> \x1B[#00FF00mgreennormal\x1B[#FF0000m rednormal\x1B[#000000m",
		}, {
			"<b>bold <font color=\"#FF0000\">red <font face=\"Comic Sans MS\" size=\"5\">larger <font color=\"#000000\">backtoblack <font size=\"3\">normalsize</font></font></font></font></b>",
			"\x1B[1mbold \x1B[#FF0000mred <font face=\"Comic Sans MS\" size=\"20\">larger \x1B[#000000mbacktoblack <font size=\"12\">normalsize</font>\x1B[#FF0000m</font>\x1B[#000000m\x1B[x1m",
		}, {
			/* buzz/unknown tags */
			"<ding>",
			"<ding>",
		}, {
			"Unknown <tags>",
			"Unknown <tags>",
		}, {
			NULL,
			NULL,
		}
	};
	gint i;

	for(i = 0; data[i].input; i++) {
		gchar *result = yahoo_html_to_codes(data[i].input);

		g_assert_cmpstr(result, ==, data[i].output);

		g_free(result);
	}
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/yahoo/format/network to html",
	                test_codes_to_html);
	g_test_add_func("/yahoo/format/html to network",
	                test_html_to_codes);

	return g_test_run();
}
