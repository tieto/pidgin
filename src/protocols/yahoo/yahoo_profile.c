/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "util.h"

#include "yahoo.h"

typedef struct {
	GaimConnection *gc;
	char *name;
} YahooGetInfoData;


typedef enum profile_lang_id {
	XX, DA, DE, EL, 
	EN, EN_GB, 
	ES_AR, ES_ES, ES_MX, ES_US,
	FR_CA, FR_FR, 
	IT, JA, KO, NO, PT, SV, 
	ZH_CN, ZH_HK, ZH_TW, ZH_US
} profile_lang_id_t;

typedef struct profile_lang_node {
	profile_lang_id_t lang;
	char *last_updated_string;
	char *det;
} profile_lang_node_t;

typedef struct profile_strings_node {
	profile_lang_id_t lang;
	char *lang_string;                   /* Only to make debugging output saner */
	char *charset;
	char *yahoo_id_string;
	char *private_string;
	char *no_answer_string;
	char *my_email_string;
	char *realname_string;
	char *location_string;
	char *age_string;
	char *maritalstatus_string;
	char *gender_string;
	char *occupation_string;
	char *hobbies_string;
	char *latest_news_string;
	char *favorite_quote_string;
	char *links_string;
	char *no_home_page_specified_string;
	char *home_page_string;
	char *no_cool_link_specified_string;
	char *cool_link_1_string;
	char *cool_link_2_string;
	char *cool_link_3_string;
	char *dummy;
} profile_strings_node_t;

/* Strings to determine the profile "language" (more accurately "locale").
 * Strings in this list must be in the original charset in the profile.
 * The "Last Updated" string is used, but sometimes is not sufficient to
 * distinguish 2 locales with this (e.g., ES_ES from ES_US, or FR_CA from
 * FR_FR, or EL from EN_GB), in which case a second string is added and
 * such special cases must be placed before the more general case.
 */
static const profile_lang_node_t profile_langs[] = {
	{ DA,    "Opdateret sidste gang&nbsp;",                          NULL },
	{ DE,    "Letzter Update&nbsp;",                                 NULL },
	{ EL,    "Last Updated:",              "http://gr.profiles.yahoo.com" },
	{ EN_GB, "Last Updated&nbsp;",                      "Favourite Quote" },
	{ EN,    "Last Updated:",                                        NULL },
	{ EN,    "Last Updated&nbsp;",                                   NULL },
	{ ES_AR, "\332ltima actualizaci\363n&nbsp;",                     NULL },
	{ ES_ES, "Actualizada el&nbsp;",       "http://es.profiles.yahoo.com" },
	{ ES_MX, "Actualizada el &nbsp;",      "http://mx.profiles.yahoo.com" },
	{ ES_US, "Actualizada el &nbsp;",                                NULL },
	{ FR_CA, "Derni\xe8re mise \xe0 jour", "http://cf.profiles.yahoo.com" },
	{ FR_FR, "Derni\xe8re mise \xe0 jour",                           NULL },
	{ IT,    "Ultimo aggiornamento&nbsp;",                           NULL },
	{ JA,    "\xba\xc7\xbd\xaa\xb9\xb9\xbf\xb7\xc6\xfc\xa1\xa7",     NULL },
	{ KO,    "\xb0\xbb\xbd\xc5\x20\xb3\xaf\xc2\xa5&nbsp;",           NULL },
	{ NO,    "Sist oppdatert&nbsp;",                                 NULL },
	{ PT,    "\332ltima atualiza\347\343o&nbsp;",                    NULL },
	{ SV,    "Senast uppdaterad&nbsp;",                              NULL },
	{ ZH_CN, "\xd7\xee\xba\xf3\xd0\xde\xb8\xc4\xc8\xd5\xc6\xda",     NULL },
	{ ZH_HK, "\xb3\xcc\xaa\xf1\xa7\xf3\xb7\x73\xae\xc9\xb6\xa1",     NULL },
	{ ZH_US, "\xb3\xcc\xab\xe1\xad\xd7\xa7\xef\xa4\xe9\xb4\xc1", "http://chinese.profiles.yahoo.com" },
	{ ZH_TW, "\xb3\xcc\xab\xe1\xad\xd7\xa7\xef\xa4\xe9\xb4\xc1",     NULL },
	{ XX,     NULL,                                                  NULL }
};

/* Strings in this list must be in UTF-8; &nbsp;'s should be specified as spaces. */
static const profile_strings_node_t profile_strings[] = {
	{ DA, "da", "ISO-8859-1",
		"Yahoo! ID:",
		"Privat",
		"Intet svar",
		"Min Email",
		"Rigtige navn:",
		"Opholdssted:",
		"Alder:",
		"Ægteskabelig status:",
		"Køn:",
		"Erhverv:",
		"Hobbyer:",
		"Sidste nyt:",
		"Favoritcitat",
		"Links",
		"Ingen hjemmeside specificeret",
		"Forside:",
		"Intet cool link specificeret",
		"Cool link 1:",
		"Cool link 2:",
		"Cool link 3:",
		NULL
	},
	{ DE, "de", "ISO-8859-1",
		"Yahoo!-ID:",
		"Privat",
		"Keine Antwort",
		"Meine E-Mail",
		"Realer Name:",
		"Ort:",
		"Alter:",
		"Familienstand:",
		"Geschlecht:",
		"Beruf:",
		"Hobbys:",
		"Neuste Nachrichten:",
		"Mein Lieblingsspruch",
		"Links",
		"Keine Homepage angegeben",
		"Homepage:",
		"Keinen coolen Link angegeben",
		"Cooler Link 1:",
		"Cooler Link 2:",
		"Cooler Link 3:",
		NULL
	},
	{ EL, "el", "ISO-8859-7", /* EL is identical to EN, except no_answer_string */
		"Yahoo! ID:",
		"Private",
		"Καμία απάντηση",
		"My Email",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies:",
		"Latest News",
		"Favorite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1:",
		"Cool Link 2:",
		"Cool Link 3:",
		NULL
	},
	{ EN, "en", "ISO-8859-1",
		"Yahoo! ID:",
		"Private",
		"No Answer",
		"My Email",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies:",
		"Latest News",
		"Favorite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1:",
		"Cool Link 2:",
		"Cool Link 3:",
		NULL
	},
	{ EN_GB, "en_GB", "ISO-8859-1", /* Same as EN except spelling of "Favourite" */
		"Yahoo! ID:",
		"Private",
		"No Answer",
		"My Email",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies:",
		"Latest News",
		"Favourite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1:",
		"Cool Link 2:",
		"Cool Link 3:",
		NULL
	},
	{ ES_AR, "es_AR", "ISO-8859-1",
		"Usuario de Yahoo!:",
		"Privado",
		"No introdujiste una respuesta",
		"Mi dirección de correo electrónico",
		"Nombre real:",
		"Ubicación:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Últimas noticias:",
		"Tu cita favorita",
		"Enlaces",
		"Ninguna página de inicio especificada",
		"Página de inicio:",
		"Ningún enlace preferido",
		"Enlace genial 1:",
		"Enlace genial 2:",
		"Enlace genial 3:",
		NULL
	},
	{ ES_ES, "es_ES", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"Sin respuesta",
		"Mi correo-e",
		"Nombre verdadero:",
		"Lugar:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Aficiones:",
		"Ultimas Noticias:",
		"Tu cita Favorita",
		"Enlace",
		"Ninguna página personal especificada",
		"Página de Inicio:",
		"Ningún enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ ES_MX, "es_MX", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"Sin responder",
		"Mi Dirección de correo-e",
		"Nombre real:",
		"Ubicación:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Ultimas Noticias:",
		"Su cita favorita",
		"Enlaces",
		"Ninguna Página predefinida",
		"Página web:",
		"Ningún Enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ ES_US, "es_US", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"No introdujo una respuesta",
		"Mi Dirección de correo-e",
		"Nombre real:",
		"Localidad:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Ultimas Noticias:",
		"Su cita Favorita",
		"Enlaces",
		"Ninguna Página de inicio predefinida",
		"Página de inicio:",
		"Ningún Enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ FR_CA, "fr_CA", "ISO-8859-1",
		"Compte Yahoo!:",
		"Privé",
		"Sans réponse",
		"Mon courriel",
		"Nom réel:",
		"Lieu:",
		"Âge:",
		"État civil:",
		"Sexe:",
		"Profession:",
		"Passe-temps:",
		"Actualités:",
		"Citation préférée",
		"Liens",
		"Pas de mention d'une page personnelle",
		"Page personnelle:",
		"Pas de mention d'un lien favori",
		"Lien préféré 1:",
		"Lien préféré 2:",
		"Lien préféré 3:",
		NULL
	},
	{ FR_FR, "fr_FR", "ISO-8859-1",
		"Compte Yahoo!:",
		"Privé",
		"Sans réponse",
		"Mon E-mail",
		"Nom réel:",
		"Lieu:",
		"Âge:",
		"Situation de famille:",
		"Sexe:",
		"Profession:",
		"Centres d'intérêts:",
		"Actualités:",
		"Citation préférée",
		"Liens",
		"Pas de mention d'une page perso",
		"Page perso:",
		"Pas de mention d'un lien favori",
		"Lien préféré 1:",
		"Lien préféré 2:",
		"Lien préféré 3:",
		NULL
	},
	{ IT, "it", "ISO-8859-1",
		"ID Yahoo!:",
		"Non pubblica",
		"Nessuna risposta",
		"La mia e-mail",
		"Nome vero:",
		"Località:",
		"Eta':",
		"Stato civile:",
		"Sesso:",
		"Occupazione:",
		"Hobby:",
		"Ultime notizie:",
		"Citazione preferita",
		"Link",
		"Nessuna home page specificata",
		"Inizio:",
		"Nessun link specificato",
		"Link Preferiti 1:",
		"Link Preferiti 2:",
		"Link Preferiti 3:",
		NULL
	},
	{ JA, "ja", "EUC-JP",
		"Yahoo! JAPAN ID：",
		"非公開",
		"無回答",
		"メール：",
		"名前：",
		"住所：",
		"年齢：",
		"未婚/既婚：",
		"性別：",
		"職業：",
		"趣味：",
		"最近の出来事：",
		NULL,
#if 0
		"おすすめサイト",
#else
		"自己PR", /* "Self description" comes before "Links" for yahoo.co.jp */
#endif
		NULL,
		NULL,
		NULL,
		"おすすめサイト1：",
		"おすすめサイト2：",
		"おすすめサイト3：",
		NULL
	},
	{ KO, "ko", "EUC-KR",
		"야후! ID:",
		"비공개",
		"비공개",
		"My Email",
		"실명:",
		"거주지:",
		"나이:",
		"결혼 여부:",
		"성별:",
		"직업:",
		"취미:",
		"자기 소개:",
		"좋아하는 명언",
		"링크",
		"홈페이지를 지정하지 않았습니다.",
		"홈페이지:",
		"추천 사이트가 없습니다.",
		"추천 사이트 1:",
		"추천 사이트 2:",
		"추천 사이트 3:",
		NULL
	},
	{ NO, "no", "ISO-8859-1",
		"Yahoo! ID:",
		"Privat",
		"Ikke noe svar",
		"Min e-post",
		"Virkelig navn:",
		"Sted:",
		"Alder:",
		"Sivilstatus:",
		"Kjønn:",
		"Yrke:",
		"Hobbyer:",
		"Siste nytt:",
		"Yndlingssitat",
		"Lenker",
		"Ingen hjemmeside angitt",
		"Hjemmeside:",
		"No cool link specified",
		"Bra lenke 1:",
		"Bra lenke 2:",
		"Bra lenke 3:",
		NULL
	},
	{ PT, "pt", "ISO-8859-1",
		"ID Yahoo!:",
		"Particular",
		"Sem resposta",
		"Meu e-mail",
		"Nome verdadeiro:",
		"Local:",
		"Idade:",
		"Estado civil:",
		"Sexo:",
		"Ocupação:",
		"Hobbies:",
		"Últimas notícias:",
		"Frase favorita",
		"Links",
		"Nenhuma página pessoal especificada",
		"Página pessoal:",
		"Nenhum site legal especificado",
		"Site legal 1:",
		"Site legal 2:",
		"Site legal 3:",
		NULL
	},
	{ SV, "sv", "ISO-8859-1",
		"Yahoo!-ID:",
		"Privat",
		"Inget svar",
		"Min mail",
		"Riktigt namn:",
		"Plats:",
		"Ålder:",
		"Civilstånd:",
		"Kön:",
		"Yrke:",
		"Hobby:",
		"Senaste nytt:",
		"Favoritcitat",
		"Länkar",
		"Ingen hemsida specificerad",
		"Hemsida:",
		"Ingen cool länk specificerad",
		"Coola länkar 1:",
		"Coola länkar 2:",
		"Coola länkar 3:",
		NULL
	},
	{ ZH_CN, "zh_CN", "GB2312",
		"Yahoo! ID:",
		"没有提供",
		"没有回答",
		"个人电邮地址",
		"真实姓名:",
		"所在地点:",
		"年龄:",
		"婚姻状况:",
		"性别:",
		"职业:",
		"业余爱好:",
		"个人近况:",
		"喜欢的引言",
		"链接",
		"没有个人主页",
		"个人主页:",
		"没有推荐网站链接",
		"推荐网站链接 1:",
		"推荐网站链接 2:",
		"推荐网站链接 3:",
		NULL
	},
	{ ZH_HK, "zh_HK", "Big5",
		"Yahoo! ID:",
		"私人的",
		"沒有回答",
		"電子信箱",
		"真實姓名:",
		"地點:",
		"年齡:",
		"婚姻狀況:",
		"性別:",
		"職業:",
		"嗜好:",
		"最新消息:",
		"最喜愛的股票叫價", /* [sic] Yahoo!'s translators don't check context */
		"連結",
		"沒有注明個人網頁", /* [sic] */
		"個人網頁:",
		"沒有注明 Cool 連結", /* [sic] */
		"Cool 連結 1:", /* TODO */
		"Cool 連結 2:", /* TODO */
		"Cool 連結 3:", /* TODO */
		NULL
	},
	{ ZH_TW, "zh_TW", "Big5",
		"帳 號:",
		"沒有提供",
		"沒有回應",
		"電子信箱",
		"姓名:",
		"地點:",
		"年齡:",
		"婚姻狀態:",
		"性別:",
		"職業:",
		"興趣:",
		"個人近況:",
		"喜歡的名句",
		"連結",
		"沒有個人網頁",
		"個人網頁:",
		"沒有推薦網站連結",
		"推薦網站連結 1:",
		"推薦網站連結 2:",
		"推薦網站連結 3:",
		NULL
	},
	{ ZH_US, "zh_US", "Big5", /* ZH_US is like ZH_TW, but also a bit like ZH_HK */
		"Yahoo! ID:",
		"沒有提供",
		"沒有回答",
		"個人Email地址",
		"真實姓名:",
		"地點:",
		"年齡:",
		"婚姻狀態:",
		"性別:",
		"職業:",
		"嗜好:",
		"個人近況:",
		"喜歡的名句",
		"連結",
		"沒有個人網頁",
		"個人網頁:",
		"沒有推薦網站連結",
		"推薦網站連結 1:", /* TODO */
		"推薦網站連結 2:", /* TODO */
		"推薦網站連結 3:", /* TODO */
		NULL
	},
	{ XX, NULL, NULL, NULL, NULL, NULL, NULL },
};

static char *yahoo_remove_nonbreaking_spaces(char *str)
{
	char *p;
	while ((p = strstr(str, "&nbsp;")) != NULL) {
		*p = ' '; /* Turn &nbsp;'s into ordinary blanks */
		p += 1;
		memmove(p, p + 5, strlen(p + 5));
		str[strlen(str) - 5] = '\0';
	}
	return str;
}

static void yahoo_got_info(void *data, const char *url_text, size_t len)
{
	YahooGetInfoData *info_data = (YahooGetInfoData *)data;
	char *stripped, *p;
	char buf[1024];
	gboolean found = FALSE;
	char *url_buffer;
	GString *s;
	int stripped_len;
	const char *last_updated_string = NULL;
	char *last_updated_utf8_string;
	int lang, strid;
	GaimBuddy *b;
	struct yahoo_data *yd;

	if (!GAIM_CONNECTION_IS_VALID(info_data->gc)) {
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	gaim_debug_info("yahoo", "In yahoo_got_info\n");

	yd = info_data->gc->proto_data;
	
	/* we failed to grab the profile URL. this should never happen */
	if (url_text == NULL || strcmp(url_text, "") == 0) {
		gaim_notify_formatted(info_data->gc, NULL, _("Buddy Information"), NULL,
			_("<html><body><b>Error retrieving profile</b></body></html>"),
			  NULL, NULL);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	/* we don't yet support the multiple link level of the warning page for
	 * 'adult' profiles, not to mention the fact that yahoo wants you to be
	 * logged in (on the website) to be able to view an 'adult' profile.  for
	 * now, just tell them that we can't help them, and provide a link to the
	 * profile if they want to do the web browser thing.
	 */
	p = strstr(url_text, "Adult Profiles Warning Message");
	if (p) {
		g_snprintf(buf, 1024, "<html><body>%s%s<a href=\"%s%s\">%s%s</a></body></html>",
				_("<b>Sorry, profiles marked as containing adult content are not supported at this time.</b><br><br>\n"),
				_("If you wish to view this profile, you will need to visit this link in your web browser<br>"),
				YAHOO_PROFILE_URL, info_data->name, YAHOO_PROFILE_URL, info_data->name);

		gaim_notify_formatted(info_data->gc, NULL, _("Buddy Information"), NULL,
				buf, NULL, NULL);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	/* Check whether the profile is written in a supported language */
	for (lang = 0;; lang += 1) {
		last_updated_string = profile_langs[lang].last_updated_string;
	if (!last_updated_string) break;
		p = strstr(url_text, last_updated_string);
		if (p && profile_langs[lang].det && !strstr(url_text, profile_langs[lang].det)) {
			p = NULL;
		}
	if (p) break;
	}
	if (p) {
		for (strid = 0; profile_strings[strid].lang != XX; strid += 1) {
			if (profile_strings[strid].lang == profile_langs[lang].lang) break;
		}
		gaim_debug_info("yahoo", "detected profile lang = %s (%d)\n", profile_strings[strid].lang_string, lang);
	}

	/* Every user may choose his/her own profile language, and this language
	 * has nothing to do with the preferences of the user which looks at the
	 * profile. We try to support all languages, but nothing is guaranteed.
	 */
	if (!p || profile_strings[strid].lang == XX) {
		if (strstr(url_text, "was not found on this server.") == NULL && strstr(url_text, "Yahoo! Member Directory - User not found") == NULL) {
			g_snprintf(buf, 1024, "<html><body>%s%s<a href=\"%s%s\">%s%s</a></body></html>",
					_("<b>Sorry, this profile seems to be in a language "
					  "that is not supported at this time.</b><br><br>\n"),
					_("If you wish to view this profile, you will need to visit this link in your web browser<br>"),
					YAHOO_PROFILE_URL, info_data->name, YAHOO_PROFILE_URL, info_data->name);
		} else {
			g_snprintf(buf, 1024, "<html><body><b>Error retrieving profile</b></body></html>");
		}

		gaim_notify_formatted(info_data->gc, NULL, _("Buddy Information"), NULL,
				buf, NULL, NULL);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	url_buffer = g_strdup(url_text);

	/*
	 * gaim_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	*/
	yahoo_remove_nonbreaking_spaces(url_buffer);
#if 1
	while ((p = strstr(url_buffer, "&#183;")) != NULL) {
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}
#endif

	/* nuke the nasty \r's */
	while ((p = strchr(url_buffer, '\r')) != NULL) {
		memmove(p, p + 1, strlen(p + 1));
		url_buffer[strlen(url_buffer) - 1] = '\0';
	}

	/* nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = gaim_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	gaim_debug_misc("yahoo", "stripped = %p\n", stripped);
	gaim_debug_misc("yahoo", "url_buffer = %p\n", url_buffer);

	/* convert to utf8 */
	p = g_convert(stripped, -1, "utf-8", profile_strings[strid].charset, NULL, NULL, NULL);
	if (!p) {
		p = g_locale_to_utf8(stripped, -1, NULL, NULL, NULL);
		if (!p) {
			p = g_convert(stripped, -1, "utf-8", "windows-1252", NULL, NULL, NULL);
		}
	}
	if (p) {
		g_free(stripped);
		stripped = gaim_utf8_ncr_decode(p);
		stripped_len = strlen(stripped);
		g_free(p);
		p = stripped;
	}
	/* FIXME need error dialog here */

	/* "Last updated" should also be converted to utf8 and with &nbsp; killed */
	last_updated_utf8_string = g_convert(last_updated_string, -1, "utf-8", profile_strings[strid].charset, NULL, NULL, NULL);
	yahoo_remove_nonbreaking_spaces(last_updated_utf8_string);

	gaim_debug_misc("yahoo", "after utf8 conversion: stripped = (%s)\n", stripped);

	/* gonna re-use the memory we've already got for url_buffer */
	/* no we're not */
	s = g_string_sized_new(strlen(url_buffer));
	g_string_append(s, "<html><body>\n");

	/* extract their Yahoo! ID and put it in. Don't bother marking has_info as
	 * true, since the Yahoo! ID will always be there */
	if (!gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].yahoo_id_string, 2, "\n", 0,
			NULL, _("Yahoo! ID"), 0, NULL))
		g_string_append_printf(s, _("<b>%s:</b> %s<br>"), _("Yahoo! ID"), info_data->name);

	/* Display the alias, idle time, and status message below the Yahoo! ID */
	b = gaim_find_buddy(gaim_connection_get_account(info_data->gc), info_data->name);
	if (b) {
		char *statustext = yahoo_tooltip_text(b);
		if(b->alias && b->alias[0]) {
			char *aliastext = g_markup_escape_text(b->alias, -1);
			g_string_append_printf(s, _("<b>Alias:</b> %s<br>"), aliastext);
			g_free(aliastext);
		}
		if (b->idle > 0) {
			char *idletime = gaim_str_seconds_to_string(time(NULL) - b->idle);
			g_string_append_printf(s, _("<b>%s:</b> %s<br>"), _("Idle"), idletime);
			g_free(idletime);
		}
		if (statustext) {
			g_string_append_printf(s, "%s<br>", statustext);
			g_free(statustext);
		}
	}

	/* extract their Email address and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].my_email_string, 5, "\n", 0,
			profile_strings[strid].private_string, _("Email"), 0, NULL);

	/* extract the Nickname if it exists */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, "Nickname:", 1, "\n", '\n',
			NULL, _("Nickname"), 0, NULL);

	/* extract their RealName and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].realname_string, 1, "\n", '\n',
			NULL, _("Realname"), 0, NULL);

	/* extract their Location and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].location_string, 2, "\n", '\n',
			NULL, _("Location"), 0, NULL);

	/* extract their Age and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].age_string, 3, "\n", '\n',
			NULL, _("Age"), 0, NULL);

	/* extract their MaritalStatus and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].maritalstatus_string, 3, "\n", '\n',
			profile_strings[strid].no_answer_string, _("Marital Status"), 0, NULL);

	/* extract their Gender and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].gender_string, 3, "\n", '\n',
			profile_strings[strid].no_answer_string, _("Gender"), 0, NULL);

	/* extract their Occupation and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].occupation_string, 2, "\n", '\n',
			NULL, _("Occupation"), 0, NULL);

	/* Hobbies, Latest News, and Favorite Quote are a bit different, since the
	 * values can contain embedded newlines... but any or all of them can also
	 * not appear.  The way we delimit them is to successively look for the next
	 * one that _could_ appear, and if all else fails, we end the section by
	 * looking for the 'Links' heading, which is the next thing to follow this
	 * bunch.
	 */

	if (!gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].hobbies_string, 1, profile_strings[strid].latest_news_string,
			'\n', NULL, _("Hobbies"), 0, NULL))
	{
		if (!gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].hobbies_string, 1, profile_strings[strid].favorite_quote_string,
				'\n', NULL, _("Hobbies"), 0, NULL))
		{
			found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].hobbies_string, 1, profile_strings[strid].links_string,
					'\n', NULL, _("Hobbies"), 0, NULL);
		}
		else
			found = TRUE;
	}
	else
		found = TRUE;

	if (!gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].latest_news_string, 1, profile_strings[strid].favorite_quote_string,
			'\n', NULL, _("Latest News"), 0, NULL))
	{
		found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].latest_news_string, 1, profile_strings[strid].links_string,
				'\n', NULL, _("Latest News"), 0, NULL);
	}
	else
		found = TRUE;

	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].favorite_quote_string, 1, profile_strings[strid].links_string,
			'\n', NULL, _("Favorite Quote"), 0, NULL);

	/* Home Page will either be "No home page specified",
	 * or "Home Page: " and a link.
	 * For non-English profiles, there might be no "Home Page:" string at all,
	 * in which case we probably can do nothing about it.
	 */
	if (profile_strings[strid].home_page_string) {
		p = !profile_strings[strid].no_home_page_specified_string? NULL:
			strstr(stripped, profile_strings[strid].no_home_page_specified_string);
		if(!p)
		{
			found |= gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].home_page_string, 1, "\n", 0, NULL,
					_("Home Page"), 1, NULL);
		}
	}

	/* Cool Link {1,2,3} is also different.  If "No cool link specified" exists,
	 * then we have none.  If we have one however, we'll need to check and see
	 * if we have a second one.  If we have a second one, we have to check to
	 * see if we have a third one.
	 */
	p = !profile_strings[strid].no_cool_link_specified_string? NULL:
		strstr(stripped,profile_strings[strid].no_cool_link_specified_string);
	if (!p)
	{
		if (gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].cool_link_1_string, 1, "\n", 0, NULL,
				_("Cool Link 1"), 1, NULL))
		{
			found = TRUE;
			if (gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].cool_link_2_string, 1, "\n", 0, NULL,
					_("Cool Link 2"), 1, NULL))
							gaim_markup_extract_info_field(stripped, stripped_len, s, profile_strings[strid].cool_link_3_string, 1, "\n", 0, NULL,
						_("Cool Link 3"), 1, NULL);
		}
	}

	/* see if Member Since is there, and if so, extract it. */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, "Member Since:", 1, last_updated_utf8_string,
			'\n', NULL, _("Member Since"), 0, NULL);

	/* extract the Last Updated date and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s, last_updated_utf8_string, 0, "\n", '\n', NULL,
			_("Last Updated"), 0, NULL);

	/* put a link to the actual profile URL */
	g_string_append_printf(s, _("<b>%s:</b> "), _("Profile URL"));
	g_string_append_printf(s, "<a href=\"%s%s\">%s%s</a><br>",
			(yd->jp? YAHOOJP_PROFILE_URL: YAHOO_PROFILE_URL), info_data->name,
			(yd->jp? YAHOOJP_PROFILE_URL: YAHOO_PROFILE_URL), info_data->name);

	/* finish off the html */
	g_string_append(s, "</body></html>\n");
	g_free(stripped);

	if(found)
	{
		/* show it to the user */
		gaim_notify_formatted(info_data->gc, NULL, _("Buddy Information"), NULL,
							  s->str, NULL, NULL);
	}
	else
	{
		char *primary;
		primary = g_strdup_printf(_("User information for %s unavailable"), info_data->name);
		gaim_notify_error(info_data->gc, NULL, primary,
				_("The user's profile is empty."));
		g_free(primary);
	}

	g_free(last_updated_utf8_string);
	g_free(url_buffer);
	g_string_free(s, TRUE);
	g_free(info_data->name);
	g_free(info_data);
}

void yahoo_get_info(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd = gc->proto_data;
	YahooGetInfoData *data;
	char *url;

	data       = g_new0(YahooGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s",
			(yd->jp? YAHOOJP_PROFILE_URL: YAHOO_PROFILE_URL), name);

	gaim_url_fetch(url, FALSE, NULL, FALSE, yahoo_got_info, data);

	g_free(url);
}
