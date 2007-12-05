/**
 * @file oim.c 
 * 	get and send MSN offline Instant Message via SOAP request
 *	Author
 * 		MaYuan<mayuan2006@gmail.com>
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 */
#include "msn.h"
#include "soap2.h"
#include "oim.h"
#include "msnutils.h"

typedef struct _MsnOimSendReq {
	char *from_member;
	char *friendname;
	char *to_member;
	char *oim_msg;
} MsnOimSendReq;

typedef struct {
	MsnOim *oim;
	char *msg_id;
} MsnOimRecvData;

/*Local Function Prototype*/
static void msn_oim_post_single_get_msg(MsnOim *oim, char *msgid);
static MsnOimSendReq *msn_oim_new_send_req(const char *from_member,
										   const char *friendname,
										   const char* to_member,
										   const char *msg);
static void msn_oim_free_send_req(MsnOimSendReq *req);
static void msn_oim_report_to_user(MsnOimRecvData *rdata, const char *msg_str);
static char *msn_oim_msg_to_str(MsnOim *oim, const char *body);

/*new a OIM object*/
MsnOim *
msn_oim_new(MsnSession *session)
{
	MsnOim *oim;

	oim = g_new0(MsnOim, 1);
	oim->session = session;
	oim->oim_list	= NULL;
	oim->run_id = rand_guid();
	oim->challenge = NULL;
	oim->send_queue = g_queue_new();
	oim->send_seq = 1;
	return oim;
}

/*destroy the oim object*/
void
msn_oim_destroy(MsnOim *oim)
{
	MsnOimSendReq *request;
	
	purple_debug_info("OIM","destroy the OIM \n");
	g_free(oim->run_id);
	g_free(oim->challenge);
	
	while((request = g_queue_pop_head(oim->send_queue)) != NULL){
		msn_oim_free_send_req(request);
	}
	g_queue_free(oim->send_queue);
	
	g_free(oim);
}

static MsnOimSendReq *
msn_oim_new_send_req(const char *from_member, const char*friendname,
	const char* to_member, const char *msg)
{
	MsnOimSendReq *request;
	
	request = g_new0(MsnOimSendReq, 1);
	request->from_member	=g_strdup(from_member);
	request->friendname		= g_strdup(friendname);
	request->to_member		= g_strdup(to_member);
	request->oim_msg		= g_strdup(msg);
	return request;
}

static void
msn_oim_free_send_req(MsnOimSendReq *req)
{
	g_return_if_fail(req != NULL);

	g_free(req->from_member);
	g_free(req->friendname);
	g_free(req->to_member);
	g_free(req->oim_msg);
	
	g_free(req);
}

/****************************************
 * OIM send SOAP request
 * **************************************/
/*encode the message to OIM Message Format*/
static gchar *
msn_oim_msg_to_str(MsnOim *oim, const char *body)
{
	char *oim_body,*oim_base64;
	
	purple_debug_info("MSN OIM","encode OIM Message...\n");	
	oim_base64 = purple_base64_encode((const guchar *)body, strlen(body));
	purple_debug_info("MSN OIM","encoded base64 body:{%s}\n",oim_base64);	
	oim_body = g_strdup_printf(MSN_OIM_MSG_TEMPLATE,
				oim->run_id,oim->send_seq,oim_base64);
	g_free(oim_base64);

	return oim_body;
}

/*
 * Process the send return SOAP string
 * If got SOAP Fault,get the lock key,and resend it.
 */
static void
msn_oim_send_read_cb(MsnSoapMessage *request, MsnSoapMessage *response,
	gpointer data)
{
	MsnOim *oim = data;
	MsnOimSendReq *msg = g_queue_pop_head(oim->send_queue);

	g_return_if_fail(msg != NULL);

	if (response == NULL) {
		purple_debug_info("MSNP14", "cannot send OIM: %s\n", msg->oim_msg);
	} else {
		xmlnode	*faultNode = msn_soap_xml_get(response->xml, "Body/Fault");

		if (faultNode == NULL) {
			/*Send OK! return*/
			purple_debug_info("MSNP14", "sent OIM: %s\n", msg->oim_msg);
		} else {
			xmlnode *faultcode = xmlnode_get_child(faultNode, "faultcode");

			if (faultcode) {
				char *faultcode_str = xmlnode_get_data(faultcode);

				if (g_str_equal(faultcode_str, "q0:AuthenticationFailed")) {
					xmlnode *challengeNode = msn_soap_xml_get(faultNode,
						"detail/LockKeyChallenge");

					if (challengeNode == NULL) {
						if (oim->challenge) {
							g_free(oim->challenge);
							oim->challenge = NULL;

							purple_debug_info("msnoim","resending OIM: %s\n",
								msg->oim_msg);
							g_queue_push_head(oim->send_queue, msg);
							msn_oim_send_msg(oim);
							return;
						} else {
							purple_debug_info("msnoim",
								"can't find lock key for OIM: %s\n",
								msg->oim_msg);
						}
					} else {
						char buf[33];

						char *challenge = xmlnode_get_data(challengeNode);
						msn_handle_chl(challenge, buf);

						g_free(oim->challenge);
						oim->challenge = g_strndup(buf, sizeof(buf));
						g_free(challenge);
						purple_debug_info("MSNP14","lockkey:{%s}\n",oim->challenge);

						/*repost the send*/
						purple_debug_info("MSNP14","resending OIM: %s\n", msg->oim_msg);
						g_queue_push_head(oim->send_queue, msg);
						msn_oim_send_msg(oim);
						return;
					}
				}
			}
		}
	}

	msn_oim_free_send_req(msg);
}

void
msn_oim_prep_send_msg_info(MsnOim *oim, const char *membername,
						   const char* friendname, const char *tomember,
						   const char * msg)
{
	g_return_if_fail(oim != NULL);

	g_queue_push_tail(oim->send_queue,
		msn_oim_new_send_req(membername, friendname, tomember, msg));
}

/*post send single message request to oim server*/
void 
msn_oim_send_msg(MsnOim *oim)
{
	MsnOimSendReq *oim_request;
	char *soap_body,*mspauth;
	char *msg_body;

	g_return_if_fail(oim != NULL);
	oim_request = g_queue_peek_head(oim->send_queue);
	g_return_if_fail(oim_request != NULL);

	purple_debug_info("MSNP14","sending OIM: %s\n", oim_request->oim_msg);
	mspauth = g_strdup_printf("t=%s&amp;p=%s",
		oim->session->passport_info.t,
		oim->session->passport_info.p
		);

	/* if we got the challenge lock key, we compute it
	 * else we go for the SOAP fault and resend it.
	 */
	if(oim->challenge == NULL){
		purple_debug_info("MSNP14","no lock key challenge,wait for SOAP Fault and Resend\n");
	}

	msg_body = msn_oim_msg_to_str(oim, oim_request->oim_msg);
	soap_body = g_strdup_printf(MSN_OIM_SEND_TEMPLATE,
					oim_request->from_member,
					oim_request->friendname,
					oim_request->to_member,
					mspauth,
					MSNP13_WLM_PRODUCT_ID,
					oim->challenge ? oim->challenge : "",
					oim->send_seq,
					msg_body);

	msn_soap_message_send(oim->session,
		msn_soap_message_new(MSN_OIM_SEND_SOAP_ACTION,
			xmlnode_from_str(soap_body, -1)),
		MSN_OIM_SEND_HOST, MSN_OIM_SEND_URL, msn_oim_send_read_cb, oim);

	/*increase the offline Sequence control*/
	if (oim->challenge != NULL) {
		oim->send_seq++;
	}

	g_free(mspauth);
	g_free(msg_body);
	g_free(soap_body);
}

/****************************************
 * OIM delete SOAP request
 * **************************************/
static void
msn_oim_delete_read_cb(MsnSoapMessage *request, MsnSoapMessage *response,
	gpointer data)
{
	MsnOimRecvData *rdata = data;

	if (response && msn_soap_xml_get(response->xml, "Body/Fault") == NULL) {
		purple_debug_info("msnoim", "delete OIM success\n");
		rdata->oim->oim_list = g_list_remove(rdata->oim->oim_list,
			rdata->msg_id);
		g_free(rdata->msg_id);
	} else {
		purple_debug_info("msnoim", "delete OIM failed\n");
	}

	g_free(rdata);
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_delete_msg(MsnOimRecvData *rdata)
{
	MsnOim *oim = rdata->oim;
	char *msgid = rdata->msg_id;
	char *soap_body;

	purple_debug_info("MSNP14","Delete single OIM Message {%s}\n",msgid);

	soap_body = g_strdup_printf(MSN_OIM_DEL_TEMPLATE,
		oim->session->passport_info.t, oim->session->passport_info.p, msgid);

	msn_soap_message_send(oim->session,
		msn_soap_message_new(MSN_OIM_DEL_SOAP_ACTION,
			xmlnode_from_str(soap_body, -1)),
		MSN_OIM_RETRIEVE_HOST, MSN_OIM_RETRIEVE_URL,
		msn_oim_delete_read_cb, rdata);

	g_free(soap_body);
}

/****************************************
 * OIM get SOAP request
 * **************************************/
/* like purple_str_to_time, but different. The format of the timestamp
 * is like this: 5 Sep 2007 21:42:12 -0700 */
static time_t
msn_oim_parse_timestamp(const char *timestamp)
{
	char month_str[4], tz_str[6];
	char *tz_ptr = tz_str;
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL
	};
	time_t tval = 0;
	struct tm t;
	memset(&t, 0, sizeof(t));

	time(&tval);
	localtime_r(&tval, &t);

	if (sscanf(timestamp, "%02d %03s %04d %02d:%02d:%02d %05s",
					&t.tm_mday, month_str, &t.tm_year,
					&t.tm_hour, &t.tm_min, &t.tm_sec, tz_str) == 7) {
		gboolean offset_positive = TRUE;
		int tzhrs;
		int tzmins;
		
		for (t.tm_mon = 0;
			 months[t.tm_mon] != NULL &&
				 strcmp(months[t.tm_mon], month_str) != 0; t.tm_mon++);
		if (months[t.tm_mon] != NULL) {
			if (*tz_str == '-') {
				offset_positive = FALSE;
				tz_ptr++;
			} else if (*tz_str == '+') {
				tz_ptr++;
			}

			if (sscanf(tz_ptr, "%02d%02d", &tzhrs, &tzmins) == 2) {
				time_t tzoff = tzhrs * 60 * 60 + tzmins * 60;
#ifdef _WIN32
				long sys_tzoff;
#endif

				if (!offset_positive)
					tzoff *= -1;

				t.tm_year -= 1900;

#ifdef _WIN32
				if ((sys_tzoff = wpurple_get_tz_offset()) != -1)
					tzoff += sys_tzoff;
#else
#ifdef HAVE_TM_GMTOFF
				tzoff -= t.tm_gmtoff;
#else
#	ifdef HAVE_TIMEZONE
				tzset();    /* making sure */
				tzoff -= timezone;
#	endif
#endif
#endif /* _WIN32 */

				return mktime(&t) + tzoff;
			}
		}
	}

	purple_debug_info("MSNP14:OIM", "Can't parse timestamp %s\n", timestamp);
	return tval;
}

/*Post the Offline Instant Message to User Conversation*/
static void
msn_oim_report_to_user(MsnOimRecvData *rdata, const char *msg_str)
{
	MsnMessage *message;
	char *date,*from,*decode_msg;
	gsize body_len;
	char **tokens;
	char *start,*end;
	int has_nick = 0;
	char *passport_str, *passport;
	time_t stamp;

	message = msn_message_new(MSN_MSG_UNKNOWN);

	msn_message_parse_payload(message, msg_str, strlen(msg_str),
							  MSG_OIM_LINE_DEM, MSG_OIM_BODY_DEM);
	purple_debug_info("MSNP14","oim body:{%s}\n",message->body);
	decode_msg = (char *)purple_base64_decode(message->body,&body_len);
	date =	(char *)g_hash_table_lookup(message->attr_table, "Date");
	from =	(char *)g_hash_table_lookup(message->attr_table, "From");
	if(strstr(from," ")){
		has_nick = 1;
	}
	if(has_nick){
		tokens = g_strsplit(from , " " , 2);
		passport_str = g_strdup(tokens[1]);
		purple_debug_info("MSNP14","oim Date:{%s},nickname:{%s},tokens[1]:{%s} passport{%s}\n",
							date,tokens[0],tokens[1],passport_str);
		g_strfreev(tokens);
	}else{
		passport_str = g_strdup(from);
		purple_debug_info("MSNP14","oim Date:{%s},passport{%s}\n",
					date,passport_str);
	}
	start = strstr(passport_str,"<");
	start += 1;
	end = strstr(passport_str,">");
	passport = g_strndup(start,end - start);
	g_free(passport_str);
	purple_debug_info("MSN OIM","oim Date:{%s},passport{%s}\n",date,passport);

	stamp = msn_oim_parse_timestamp(date);

	serv_got_im(rdata->oim->session->account->gc, passport, decode_msg, 0,
		stamp);

	/*Now get the oim message ID from the oim_list.
	 * and append to read list to prepare for deleting the Offline Message when sign out
	 */
	msn_oim_post_delete_msg(rdata);

	g_free(passport);
	g_free(decode_msg);
}

/* Parse the XML data,
 * prepare to report the OIM to user
 */
static void
msn_oim_get_read_cb(MsnSoapMessage *request, MsnSoapMessage *response,
	gpointer data)
{
	MsnOimRecvData *rdata = data;

	if (response != NULL) {
		xmlnode *msg_node = msn_soap_xml_get(response->xml,
			"Body/GetMessageResponse/GetMessageResult");

		if (msg_node) {
			char *msg_str = xmlnode_get_data(msg_node);
			msn_oim_report_to_user(rdata, msg_str);
			g_free(msg_str);
		} else {
			char *str = xmlnode_to_str(response->xml, NULL);
			purple_debug_info("msnoim", "Unknown response: %s\n", str);
			g_free(str);
		}
	} else {
		purple_debug_info("msnoim", "Failed to get OIM\n");
	}
}

/* parse the oim XML data 
 * and post it to the soap server to get the Offline Message
 * */
void
msn_parse_oim_msg(MsnOim *oim,const char *xmlmsg)
{
	xmlnode *node, *mNode;
	xmlnode *iu_node;
	MsnSession *session = oim->session;

	purple_debug_info("MSNP14:OIM", "%s", xmlmsg);

	node = xmlnode_from_str(xmlmsg, -1);
	if (strcmp(node->name, "MD") != 0) {
		purple_debug_info("msnoim", "WTF is this? %s\n", xmlmsg);
		xmlnode_free(node);
		return;
	}

	iu_node = msn_soap_xml_get(node, "E/IU");

	if (iu_node != NULL && purple_account_get_check_mail(session->account))
	{
		char *unread = xmlnode_get_data(iu_node);
		const char *passport = msn_user_get_passport(session->user);
		const char *url = session->passport_info.file;
		int count = atoi(unread);

		/* XXX/khc: pretty sure this is wrong */
		if (count > 0)
			purple_notify_emails(session->account->gc, count, FALSE, NULL,
				NULL, &passport, &url, NULL, NULL);
		g_free(unread);
	}

	for(mNode = xmlnode_get_child(node, "M"); mNode;
					mNode = xmlnode_get_next_twin(mNode)){
		char *passport, *msgid, *nickname, *rtime = NULL;
		xmlnode *e_node, *i_node, *n_node, *rt_node;

		e_node = xmlnode_get_child(mNode, "E");
		passport = xmlnode_get_data(e_node);

		i_node = xmlnode_get_child(mNode, "I");
		msgid = xmlnode_get_data(i_node);

		n_node = xmlnode_get_child(mNode, "N");
		nickname = xmlnode_get_data(n_node);

		rt_node = xmlnode_get_child(mNode, "RT");
		if (rt_node != NULL) {
			rtime = xmlnode_get_data(rt_node);
		}
/*		purple_debug_info("msnoim","E:{%s},I:{%s},rTime:{%s}\n",passport,msgid,rTime); */

		if (!g_list_find_custom(oim->oim_list, msgid, (GCompareFunc)strcmp)) {
			oim->oim_list = g_list_append(oim->oim_list, msgid);
			msn_oim_post_single_get_msg(oim, msgid);
			msgid = NULL;
		}

		g_free(passport);
		g_free(msgid);
		g_free(rtime);
		g_free(nickname);
	}

	xmlnode_free(node);
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_single_get_msg(MsnOim *oim, char *msgid)
{
	char *soap_body;
	MsnOimRecvData *data = g_new0(MsnOimRecvData, 1);

	purple_debug_info("MSNP14","Get single OIM Message\n");

	data->oim = oim;
	data->msg_id = msgid;

	soap_body = g_strdup_printf(MSN_OIM_GET_TEMPLATE,
		oim->session->passport_info.t, oim->session->passport_info.p, msgid);

	msn_soap_message_send(oim->session,
		msn_soap_message_new(MSN_OIM_GET_SOAP_ACTION,
			xmlnode_from_str(soap_body, -1)),
		MSN_OIM_RETRIEVE_HOST, MSN_OIM_RETRIEVE_URL,
		msn_oim_get_read_cb, data);

	g_free(soap_body);
}
