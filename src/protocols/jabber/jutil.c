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

JabberID*
jabber_id_new(const char *str)
{
	char *at;
	char *slash;

	JabberID *jid;

	if(!str)
		return NULL;

	jid = g_new0(JabberID, 1);

	at = strchr(str, '@');
	slash = strchr(str, '/');

	if(at) {
		jid->node = g_strndup(str, at-str);
		if(slash) {
			jid->domain = g_strndup(at+1, slash-(at+1));
			jid->resource = g_strdup(slash+1);
		} else {
			jid->domain = g_strdup(at+1);
		}
	} else {
		if(slash) {
			jid->domain = g_strndup(str, slash-str);
			jid->resource = g_strdup(slash+1);
		} else {
			jid->domain = g_strdup(str);
		}
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


const char *jabber_get_resource(const char *jid)
{
	char *slash;

	slash = strrchr(jid, '/');
	if(slash)
		return slash+1;
	else
		return NULL;
}

char *jabber_get_bare_jid(const char *jid)
{
	char *slash;
	slash = strrchr(jid, '/');

	if(slash)
		return g_strndup(jid, slash - jid);
	else
		return g_strdup(jid);
}

const char *jabber_normalize(const GaimAccount *account, const char *in)
{
	static char buf[2048]; /* maximum legal length of a jabber jid */
	char *tmp;

	tmp = jabber_get_bare_jid(in);
	g_snprintf(buf, sizeof(buf), "%s", tmp);
	g_free(tmp);
	return buf;
}
