/*
 * gaim - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
#include "server.h"
#include "util.h"

#include "chat.h"
#include "presence.h"
#include "jutil.h"

time_t str_to_time(const char *timestamp)
{
	struct tm t;
	time_t retval = 0;
	char buf[32];
	char *c;
	int tzoff = 0;

	time(&retval);
	localtime_r(&retval, &t);

	snprintf(buf, sizeof(buf), "%s", timestamp);
	c = buf;

	/* 4 digit year */
	if(!sscanf(c, "%04d", &t.tm_year)) return 0;
	c+=4;
	if(*c == '-')
        c++;

    t.tm_year -= 1900;

    /* 2 digit month */
    if(!sscanf(c, "%02d", &t.tm_mon)) return 0;
    c+=2;
    if(*c == '-')
        c++;

    t.tm_mon -= 1;

    /* 2 digit day */
    if(!sscanf(c, "%02d", &t.tm_mday)) return 0;
    c+=2;
    if(*c == 'T') { /* we have more than a date, keep going */
        c++; /* skip the "T" */

        /* 2 digit hour */
        if(sscanf(c, "%02d:%02d:%02d", &t.tm_hour, &t.tm_min, &t.tm_sec)) {
            int tzhrs, tzmins;
            c+=8;
            if(*c == '.') /* dealing with precision we don't care about */
                c += 4;

            if((*c == '+' || *c == '-') &&
                    sscanf(c+1, "%02d:%02d", &tzhrs, &tzmins)) {
                tzoff = tzhrs*60*60 + tzmins*60;
                if(*c == '+')
                    tzoff *= -1;
            }

#ifdef HAVE_TM_GMTOFF
                tzoff += t.tm_gmtoff;
#else
#   ifdef HAVE_TIMEZONE
                tzset();    /* making sure */
                tzoff -= timezone;
#   endif
#endif
        }
    }
    retval = mktime(&t);

    retval += tzoff;

    return retval;
}

const char *jabber_get_state_string(int s) {
	switch(s) {
		case JABBER_STATE_AWAY:
			return _("Away");
			break;
		case JABBER_STATE_CHAT:
			return _("Chatty");
			break;
		case JABBER_STATE_XA:
			return _("Extended Away");
			break;
		case JABBER_STATE_DND:
			return _("Do Not Disturb");
			break;
		default:
			return _("Available");
			break;
	}
}

gboolean jabber_nodeprep_validate(const char *str)
{
	const char *c;

	if(!str)
		return TRUE;

	if(strlen(str) > 1023)
		return FALSE;

	c = str;
	while(c && *c) {
		gunichar ch = g_utf8_get_char(c);
		if(ch == '\"' || ch == '&' || ch == '\'' || ch == '/' || ch == ':' ||
				ch == '<' || ch == '>' || ch == '@' || !g_unichar_isgraph(ch)) {
			return FALSE;
		}
		c = g_utf8_next_char(c);
	}

	return TRUE;
}

gboolean jabber_nameprep_validate(const char *str)
{
	const char *c;

	if(!str)
		return TRUE;

	if(strlen(str) > 1023)
		return FALSE;

	c = str;
	while(c && *c) {
		gunichar ch = g_utf8_get_char(c);
		if(!g_unichar_isgraph(ch))
			return FALSE;

		c = g_utf8_next_char(c);
	}


	return TRUE;
}

gboolean jabber_resourceprep_validate(const char *str)
{
	const char *c;

	if(!str)
		return TRUE;

	if(strlen(str) > 1023)
		return FALSE;

	c = str;
	while(c && *c) {
		gunichar ch = g_utf8_get_char(c);
		if(!g_unichar_isgraph(ch) && ch != ' ')
			return FALSE;

		c = g_utf8_next_char(c);
	}

	return TRUE;
}


JabberID*
jabber_id_new(const char *str)
{
	char *at;
	char *slash;
	JabberID *jid;

	if(!str || !g_utf8_validate(str, -1, NULL))
		return NULL;

	jid = g_new0(JabberID, 1);

	at = g_utf8_strchr(str, -1, '@');
	slash = g_utf8_strchr(str, -1, '/');

	if(at) {
		jid->node = g_utf8_normalize(str, at-str, G_NORMALIZE_NFKC);
		if(slash) {
			jid->domain = g_utf8_normalize(at+1, slash-(at+1), G_NORMALIZE_NFKC);
			jid->resource = g_utf8_normalize(slash+1, -1, G_NORMALIZE_NFKC);
		} else {
			jid->domain = g_utf8_normalize(at+1, -1, G_NORMALIZE_NFKC);
		}
	} else {
		if(slash) {
			jid->domain = g_utf8_normalize(str, slash-str, G_NORMALIZE_NFKC);
			jid->resource = g_utf8_normalize(slash+1, -1, G_NORMALIZE_NFKC);
		} else {
			jid->domain = g_utf8_normalize(str, -1, G_NORMALIZE_NFKC);
		}
	}


	if(!jabber_nodeprep_validate(jid->node) ||
			!jabber_nameprep_validate(jid->domain) ||
			!jabber_resourceprep_validate(jid->resource)) {
		jabber_id_free(jid);
		return NULL;
	}

	return jid;
}

void
jabber_id_free(JabberID *jid)
{
	if(jid) {
		if(jid->node)
			g_free(jid->node);
		if(jid->domain)
			g_free(jid->domain);
		if(jid->resource)
			g_free(jid->resource);
		g_free(jid);
	}
}


char *jabber_get_resource(const char *in)
{
	JabberID *jid = jabber_id_new(in);
	char *out;

	if(!jid)
		return NULL;

	out = g_strdup(jid->resource);
	jabber_id_free(jid);

	return out;
}

char *jabber_get_bare_jid(const char *in)
{
	JabberID *jid = jabber_id_new(in);
	char *out;

	if(!jid)
		return NULL;

	out = g_strdup_printf("%s%s%s", jid->node ? jid->node : "",
			jid->node ? "@" : "", jid->domain);
	jabber_id_free(jid);

	return out;
}

const char *jabber_normalize(const GaimAccount *account, const char *in)
{
	GaimConnection *gc = account ? account->gc : NULL;
	JabberStream *js = gc ? gc->proto_data : NULL;
	static char buf[3072]; /* maximum legal length of a jabber jid */
	JabberID *jid;
	char *node, *domain;

	jid = jabber_id_new(in);

	if(!jid)
		return NULL;

	node = jid->node ? g_utf8_strdown(jid->node, -1) : NULL;
	domain = g_utf8_strdown(jid->domain, -1);


	if(js && node && jid->resource &&
			jabber_chat_find(js, node, domain))
		g_snprintf(buf, sizeof(buf), "%s@%s/%s", node, domain,
				jid->resource);
	else
		g_snprintf(buf, sizeof(buf), "%s%s%s", node ? node : "",
				node ? "@" : "", domain);

	jabber_id_free(jid);
	g_free(node);
	g_free(domain);

	return buf;
}

GaimConversation *
jabber_find_unnormalized_conv(const char *name, GaimAccount *account)
{
	GaimConversation *c = NULL;
	GList *cnv;

	g_return_val_if_fail(name != NULL, NULL);

	for(cnv = gaim_get_conversations(); cnv; cnv = cnv->next) {
		c = (GaimConversation*)cnv->data;
		if(gaim_conversation_get_type(c) == GAIM_CONV_IM &&
				!gaim_utf8_strcasecmp(name, gaim_conversation_get_name(c)) &&
				account == gaim_conversation_get_account(c))
			return c;
	}

	return NULL;
}

