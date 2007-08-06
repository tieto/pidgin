/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "internal.h"
#include "debug.h"

#include "buddy.h"
#include "mdns_interface.h"
#include "dns_sd_proxy.h"
#include "dnsquery.h"


/* data structure for the resolve callback */
typedef struct _ResolveCallbackArgs {
	DNSServiceRef resolver;
	guint resolver_handler;

	PurpleDnsQueryData *query;

	BonjourBuddy* buddy;
} ResolveCallbackArgs;

/* data used by win32 bonjour implementation */
typedef struct _win32_session_impl_data {
	DNSServiceRef advertisement;
	DNSServiceRef browser;

	guint advertisement_handler; /* hack... windows bonjour is broken, so we have to have this */
} Win32SessionImplData;

typedef struct _win32_buddy_impl_data {
	DNSServiceRef txt_query;
	guint txt_query_handler;
	DNSServiceRef null_query;
	guint null_query_handler;
} Win32BuddyImplData;

static void
_mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition) {
	DNSServiceProcessResult((DNSServiceRef) data);
}

static void
_mdns_parse_text_record(BonjourBuddy* buddy, const char* record, uint16_t record_len)
{
	const char *txt_entry;
	uint8_t txt_len;
	int i;

	for (i = 0; buddy_TXT_records[i] != NULL; i++) {
		txt_entry = TXTRecordGetValuePtr(record_len, record, buddy_TXT_records[i], &txt_len);
		if (txt_entry != NULL)
			set_bonjour_buddy_value(buddy, buddy_TXT_records[i], txt_entry, txt_len);
	}
}

static void DNSSD_API
_mdns_text_record_query_callback(DNSServiceRef DNSServiceRef, DNSServiceFlags flags,
	uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullname,
	uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata,
	uint32_t ttl, void *context)
{

	if (kDNSServiceErr_NoError != errorCode)
		purple_debug_error("bonjour", "record query - callback error.\n");
	else if (flags & kDNSServiceFlagsAdd)
	{
		if (rrtype == kDNSServiceType_TXT) {
			/* New Buddy */
			BonjourBuddy *buddy = (BonjourBuddy*) context;
			_mdns_parse_text_record(buddy, rdata, rdlen);
			bonjour_buddy_add_to_purple(buddy);
		} else if (rrtype == kDNSServiceType_NULL) {
			/* Buddy Icon response */
			BonjourBuddy *buddy = (BonjourBuddy*) context;
			Win32BuddyImplData *idata = buddy->mdns_impl_data;

			g_return_if_fail(idata != NULL);

			purple_buddy_icons_set_for_user(buddy->account, buddy->name,
							g_memdup(rdata, rdlen), rdlen, buddy->phsh);

			/* We've got what we need; stop listening */
			purple_input_remove(idata->null_query_handler);
			idata->null_query_handler = -1;
			DNSServiceRefDeallocate(idata->null_query);
			idata->null_query = NULL;
		}
	}
}

static void
_mdns_resolve_host_callback(GSList *hosts, gpointer data, const char *error_message)
{
	ResolveCallbackArgs* args = (ResolveCallbackArgs*)data;

	if (!hosts || !hosts->data)
		purple_debug_error("bonjour", "host resolution - callback error.\n");
	else {
		struct sockaddr_in *addr = (struct sockaddr_in*)g_slist_nth_data(hosts, 1);
		BonjourBuddy* buddy = args->buddy;
		Win32BuddyImplData *idata = buddy->mdns_impl_data;

		g_return_if_fail(idata != NULL);

		buddy->ip = g_strdup(inet_ntoa(addr->sin_addr));

		/* finally, set up the continuous txt record watcher, and add the buddy to purple */

		if (kDNSServiceErr_NoError == DNSServiceQueryRecord(&idata->txt_query, 0, 0, buddy->full_service_name,
				kDNSServiceType_TXT, kDNSServiceClass_IN, _mdns_text_record_query_callback, buddy)) {
			int fd = DNSServiceRefSockFD(idata->txt_query);
			idata->txt_query_handler = purple_input_add(fd, PURPLE_INPUT_READ, _mdns_handle_event, idata->txt_query);

			bonjour_buddy_add_to_purple(buddy);

			purple_debug_info("bonjour", "Found buddy %s at %s:%d\n", buddy->name, buddy->ip, buddy->port_p2pj);
		} else
			bonjour_buddy_delete(buddy);

	}

	/* free the hosts list*/
	g_slist_free(hosts);

	/* free the remaining args memory */
	purple_dnsquery_destroy(args->query);
	g_free(args);
}

static void DNSSD_API
_mdns_service_resolve_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode,
    const char *fullname, const char *hosttarget, uint16_t port, uint16_t txtLen, const char *txtRecord, void *context)
{
	ResolveCallbackArgs *args = (ResolveCallbackArgs*)context;

	/* remove the input fd and destroy the service ref */
	purple_input_remove(args->resolver_handler);
	DNSServiceRefDeallocate(args->resolver);

	if (kDNSServiceErr_NoError != errorCode)
	{
		purple_debug_error("bonjour", "service resolver - callback error.\n");
		bonjour_buddy_delete(args->buddy);
		g_free(args);
	}
	else
	{
		args->buddy->port_p2pj = ntohs(port);

		/* parse the text record */
		_mdns_parse_text_record(args->buddy, txtRecord, txtLen);

		/* set more arguments, and start the host resolver */
		args->buddy->full_service_name = g_strdup(fullname);

		if (!(args->query =
			purple_dnsquery_a(hosttarget, port, _mdns_resolve_host_callback, args)))
		{
			purple_debug_error("bonjour", "service resolver - host resolution failed.\n");
			bonjour_buddy_delete(args->buddy);
			g_free(args);
		}
	}

}

static void DNSSD_API
_mdns_service_register_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
    const char *name, const char *regtype, const char *domain, void *context)
{
	/* we don't actually care about anything said in this callback - this is only here because Bonjour for windows is broken */
	if (kDNSServiceErr_NoError != errorCode)
		purple_debug_error("bonjour", "service advertisement - callback error.\n");
	else
		purple_debug_info("bonjour", "service advertisement - callback.\n");
}

static void DNSSD_API
_mdns_service_browse_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context)
{
	PurpleAccount *account = (PurpleAccount*)context;
	PurpleBuddy *gb = NULL;

	if (kDNSServiceErr_NoError != errorCode)
		purple_debug_error("bonjour", "service browser - callback error");
	else if (flags & kDNSServiceFlagsAdd) {
		/* A presence service instance has been discovered... check it isn't us! */
		if (g_ascii_strcasecmp(serviceName, account->username) != 0) {
			/* OK, lets go ahead and resolve it to add to the buddy list */
			ResolveCallbackArgs *args = g_new0(ResolveCallbackArgs, 1);
			args->buddy = bonjour_buddy_new(serviceName, account);

			if (kDNSServiceErr_NoError != DNSServiceResolve(&args->resolver, 0, 0, serviceName, regtype, replyDomain, _mdns_service_resolve_callback, args)) {
				bonjour_buddy_delete(args->buddy);
				g_free(args);
				purple_debug_error("bonjour", "service browser - failed to resolve service.\n");
			} else {
				/* get a file descriptor for this service ref, and add it to the input list */
				gint fd = DNSServiceRefSockFD(args->resolver);
				args->resolver_handler = purple_input_add(fd, PURPLE_INPUT_READ, _mdns_handle_event, args->resolver);
			}
		}
	} else {
		/* A peer has sent a goodbye packet, remove them from the buddy list */
		purple_debug_info("bonjour", "service browser - remove notification\n");
		gb = purple_find_buddy(account, serviceName);
		if (gb != NULL) {
			bonjour_buddy_delete(gb->proto_data);
			purple_blist_remove_buddy(gb);
		}
	}
}

/****************************
 * mdns_interface functions *
 ****************************/

gboolean _mdns_init_session(BonjourDnsSd *data) {
	data->mdns_impl_data = g_new0(Win32SessionImplData, 1);
	return TRUE;
}

gboolean _mdns_publish(BonjourDnsSd *data, PublishType type) {
	TXTRecordRef dns_data;
	char portstring[6];
	gboolean ret = TRUE;
	const char *jid, *aim, *email;
	DNSServiceErrorType set_ret;
	Win32SessionImplData *idata = data->mdns_impl_data;

	g_return_val_if_fail(idata != NULL, FALSE);

	TXTRecordCreate(&dns_data, 256, NULL);

	/* Convert the port to a string */
	snprintf(portstring, sizeof(portstring), "%d", data->port_p2pj);

	jid = purple_account_get_string(data->account, "jid", NULL);
	aim = purple_account_get_string(data->account, "AIM", NULL);
	email = purple_account_get_string(data->account, "email", NULL);

	/* We should try to follow XEP-0174, but some clients have "issues", so we humor them.
	 * See http://telepathy.freedesktop.org/wiki/SalutInteroperability
	 */

	/* Needed by iChat */
	set_ret = TXTRecordSetValue(&dns_data, "txtvers", 1, "1");
	/* Needed by Gaim/Pidgin <= 2.0.1 (remove at some point) */
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "1st", strlen(data->first), data->first);
	/* Needed by Gaim/Pidgin <= 2.0.1 (remove at some point) */
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "last", strlen(data->last), data->last);
	/* Needed by Adium */
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "port.p2pj", strlen(portstring), portstring);
	/* Needed by iChat, Gaim/Pidgin <= 2.0.1 */
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "status", strlen(data->status), data->status);
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "ver", strlen(VERSION), VERSION);
	/* Currently always set to "!" since we don't support AV and wont ever be in a conference */
	if (set_ret == kDNSServiceErr_NoError)
		set_ret = TXTRecordSetValue(&dns_data, "vc", strlen(data->vc), data->vc);
	if (set_ret == kDNSServiceErr_NoError && email != NULL && *email != '\0')
		set_ret = TXTRecordSetValue(&dns_data, "email", strlen(email), email);
	if (set_ret == kDNSServiceErr_NoError && jid != NULL && *jid != '\0')
		set_ret = TXTRecordSetValue(&dns_data, "jid", strlen(jid), jid);
	/* Nonstandard, but used by iChat */
	if (set_ret == kDNSServiceErr_NoError && aim != NULL && *aim != '\0')
		set_ret = TXTRecordSetValue(&dns_data, "AIM", strlen(aim), aim);
	if (set_ret == kDNSServiceErr_NoError && data->msg != NULL && *data->msg != '\0')
		set_ret = TXTRecordSetValue(&dns_data, "msg", strlen(data->msg), data->msg);
	if (set_ret == kDNSServiceErr_NoError && data->phsh != NULL && *data->phsh != '\0')
		set_ret = TXTRecordSetValue(&dns_data, "phsh", strlen(data->phsh), data->phsh);

	/* TODO: ext, nick, node */

	if (set_ret != kDNSServiceErr_NoError) {
		purple_debug_error("bonjour", "Unable to allocate memory for text record.\n");
		ret = FALSE;
	} else {
		DNSServiceErrorType err = kDNSServiceErr_NoError;

		/* OK, we're done constructing the text record, (re)publish the service */

		switch (type) {
			case PUBLISH_START:
				purple_debug_info("bonjour", "Registering service on port %d\n", data->port_p2pj);
				err = DNSServiceRegister(&idata->advertisement, 0, 0, purple_account_get_username(data->account), ICHAT_SERVICE,
					NULL, NULL, htons(data->port_p2pj), TXTRecordGetLength(&dns_data), TXTRecordGetBytesPtr(&dns_data),
					_mdns_service_register_callback, NULL);
				break;

			case PUBLISH_UPDATE:
				err = DNSServiceUpdateRecord(idata->advertisement, NULL, 0, TXTRecordGetLength(&dns_data), TXTRecordGetBytesPtr(&dns_data), 0);
				break;
		}

		if (err != kDNSServiceErr_NoError) {
			purple_debug_error("bonjour", "Failed to publish presence service.\n");
			ret = FALSE;
		} else if (type == PUBLISH_START) {
			/* hack: Bonjour on windows is broken. We don't care about the callback but we have to listen anyway */
			gint fd = DNSServiceRefSockFD(idata->advertisement);
			idata->advertisement_handler = purple_input_add(fd, PURPLE_INPUT_READ, _mdns_handle_event, idata->advertisement);
		}
	}

	/* Free the memory used by temp data */
	TXTRecordDeallocate(&dns_data);
	return ret;
}

gboolean _mdns_browse(BonjourDnsSd *data) {
	Win32SessionImplData *idata = data->mdns_impl_data;

	g_return_val_if_fail(idata != NULL, FALSE);

	return (DNSServiceBrowse(&idata->browser, 0, 0, ICHAT_SERVICE, NULL,
				 _mdns_service_browse_callback, data->account)
			== kDNSServiceErr_NoError);
}

guint _mdns_register_to_mainloop(BonjourDnsSd *data) {
	Win32SessionImplData *idata = data->mdns_impl_data;

	g_return_val_if_fail(idata != NULL, 0);

	return purple_input_add(DNSServiceRefSockFD(idata->browser),
				PURPLE_INPUT_READ, _mdns_handle_event, idata->browser);
}

void _mdns_stop(BonjourDnsSd *data) {
	Win32SessionImplData *idata = data->mdns_impl_data;

	if (idata == NULL || idata->advertisement == NULL || idata->browser == NULL)
		return;

	/* hack: for win32, we need to stop listening to the advertisement pipe too */
	purple_input_remove(idata->advertisement_handler);

	DNSServiceRefDeallocate(idata->advertisement);
	DNSServiceRefDeallocate(idata->browser);

	g_free(idata);

	data->mdns_impl_data = NULL;
}

void _mdns_init_buddy(BonjourBuddy *buddy) {
	buddy->mdns_impl_data = g_new0(Win32BuddyImplData, 1);
}

void _mdns_delete_buddy(BonjourBuddy *buddy) {
	Win32BuddyImplData *idata = buddy->mdns_impl_data;

	g_return_if_fail(idata != NULL);

	if (idata->txt_query != NULL) {
		purple_input_remove(idata->txt_query_handler);
		DNSServiceRefDeallocate(idata->txt_query);
	}

	if (idata->null_query != NULL) {
		purple_input_remove(idata->null_query_handler);
		DNSServiceRefDeallocate(idata->null_query);
	}

	g_free(idata);

	buddy->mdns_impl_data = NULL;
}

void bonjour_dns_sd_retrieve_buddy_icon(BonjourBuddy* buddy) {
	Win32BuddyImplData *idata = buddy->mdns_impl_data;

	g_return_if_fail(idata != NULL);

	/* Cancel any existing query */
	if (idata->null_query != NULL) {
		purple_input_remove(idata->null_query_handler);
		idata->null_query_handler = 0;
		DNSServiceRefDeallocate(idata->null_query);
		idata->null_query = NULL;
	}

	if (kDNSServiceErr_NoError == DNSServiceQueryRecord(&idata->null_query, 0, 0, buddy->full_service_name,
			kDNSServiceType_NULL, kDNSServiceClass_IN, _mdns_text_record_query_callback, buddy)) {
		int fd = DNSServiceRefSockFD(idata->null_query);
		idata->null_query_handler = purple_input_add(fd, PURPLE_INPUT_READ, _mdns_handle_event, idata->null_query);
	}

}

