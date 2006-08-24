/**
 * @file oim.c 
 * 	get and send MSN offline Instant Message via SOAP request
 *	Author
 * 		MaYuan<mayuan2006@gmail.com>
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
 */
#include "msn.h"
#include "soap.h"
#include "oim.h"

/*Local Function Prototype*/
static void msn_oim_post_single_get_msg(MsnOim *oim,const char *msgid);
void msn_oim_retrieve_connect_init(MsnSoapConn *soapconn);
void msn_oim_send_connect_init(MsnSoapConn *soapconn);

/*new a OIM object*/
MsnOim *
msn_oim_new(MsnSession *session)
{
	MsnOim *oim;

	oim = g_new0(MsnOim, 1);
	oim->session = session;
	oim->retrieveconn = msn_soap_new(session,oim,1);
	oim->oim_list	= NULL;
	oim->sendconn = msn_soap_new(session,oim,1);

	return oim;
}

/*destroy the oim object*/
void
msn_oim_destroy(MsnOim *oim)
{
	msn_soap_destroy(oim->retrieveconn);
	msn_soap_destroy(oim->sendconn);
	g_free(oim);
}

/****************************************
 * OIM send SOAP request
 * **************************************/
/*oim SOAP server login error*/
static void
msn_oim_send_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to OIM server"));
}

/*msn oim SOAP server connect process*/
static void
msn_oim_send_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnOim *oim;

	oim = soapconn->parent;
	g_return_if_fail(oim != NULL);

	session = oim->session;
	g_return_if_fail(session != NULL);
}

static void
msn_oim_send_read_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnOim * msnoim;

	gaim_debug_info("MaYuan","read buffer:{%s}\n",soapconn->body);
}

static void
msn_oim_send_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = msn_oim_send_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*pose single message to oim server*/
void 
msn_oim_send_single_msg(MsnOim *oim,char * msg)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	gaim_debug_info("MaYuan","send single OIM Message\n");
	oim->sendconn->login_path = g_strdup(MSN_OIM_SEND_URL);
	oim->sendconn->soap_action = g_strdup(MSN_OIM_SEND_SOAP_ACTION);
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;
#if 0
	oimsoapbody = g_strdup_printf(MSN_OIM_SEND_TEMPLATE,
					membername,
					friendname,
					tomember,
					mspauth,
					prod_id,
					lock_key,
					msg_num,
					msg
					);
#endif
	soap_request = msn_soap_request_new(MSN_OIM_SEND_HOST,
					MSN_OIM_SEND_URL,MSN_OIM_SEND_SOAP_ACTION,
					soap_body,
					msn_oim_send_read_cb,
					msn_oim_send_written_cb);
	msn_soap_post(oim->sendconn,soap_request,msn_oim_send_connect_init);
}

void msn_oim_send_msg(MsnOim *oim,char *msg)
{
	if(msn_soap_connected(oim->sendconn) == -1){
	}
	
}

/****************************************
 * OIM delete SOAP request
 * **************************************/
static void
msn_oim_delete_read_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnOim * oim = soapconn->session->oim;

	gaim_debug_info("MaYuan","OIM delete read buffer:{%s}\n",soapconn->body);

	msn_soap_free_read_buf(soapconn);
	/*get next single Offline Message*/
	msn_soap_post(soapconn,NULL,msn_oim_retrieve_connect_init);
}

static void
msn_oim_delete_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = msn_oim_delete_read_cb;
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_delete_msg(MsnOim *oim,const char *msgid)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	g_return_if_fail(oim != NULL);
	g_return_if_fail(msgid != NULL);

	gaim_debug_info("MaYuan","Delete single OIM Message {%s}\n",msgid);
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;

	soap_body = g_strdup_printf(MSN_OIM_DEL_TEMPLATE,
					t,
					p,
					msgid
					);
	soap_request = msn_soap_request_new(MSN_OIM_RETRIEVE_HOST,
					MSN_OIM_RETRIEVE_URL,MSN_OIM_DEL_SOAP_ACTION,
					soap_body,
					msn_oim_delete_read_cb,
					msn_oim_delete_written_cb);
	msn_soap_post(oim->retrieveconn,soap_request,msn_oim_retrieve_connect_init);
}

/****************************************
 * OIM get SOAP request
 * **************************************/
/*oim SOAP server login error*/
static void
msn_oim_get_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);
	msn_soap_clean_unhandled_request(soapconn);

//	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to OIM server"));
}

/*msn oim SOAP server connect process*/
static void
msn_oim_get_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnOim *oim;

	oim = soapconn->parent;
	g_return_if_fail(oim != NULL);

	session = oim->session;
	g_return_if_fail(session != NULL);

	gaim_debug_info("MaYuan","oim get SOAP Server connected!\n");
}

/*Post the Offline Instant Message to User Conversation*/
void
msn_oim_report_to_user(MsnOim *oim,char *msg_str)
{
	MsnMessage *message;
	char *date,*from,*decode_msg;
	gsize body_len;
	char **tokens;
	char *start,*end;
	int has_nick = 0;
	char *passport_str,*passport;
	char *msg_id;

	message = msn_message_new(MSN_MSG_UNKNOWN);

	msn_message_parse_payload(message,msg_str,strlen(msg_str),
					MSG_OIM_LINE_DEM, MSG_OIM_BODY_DEM);
	gaim_debug_info("MaYuan","oim body:{%s}\n",message->body);
	decode_msg = gaim_base64_decode(message->body,&body_len);
	date =	(char *)g_hash_table_lookup(message->attr_table, "Date");
	from =	(char *)g_hash_table_lookup(message->attr_table, "From");
	if(strstr(from," ")){
		has_nick = 1;
	}
	if(has_nick){
		tokens = g_strsplit(from , " " , 2);
		passport_str = g_strdup(tokens[1]);
		gaim_debug_info("MaYuan","oim Date:{%s},nickname:{%s},tokens[1]:{%s} passport{%s}\n",
							date,tokens[0],tokens[1],passport_str);
		g_strfreev(tokens);
	}else{
		passport_str = g_strdup(from);
		gaim_debug_info("MaYuan","oim Date:{%s},passport{%s}\n",
					date,passport_str);
	}
	start = strstr(passport_str,"<");
	start += 1;
	end = strstr(passport_str,">");
	passport = g_strndup(start,end - start);
	g_free(passport_str);
	gaim_debug_info("MaYuan","oim Date:{%s},passport{%s}\n",date,passport);

	msn_session_report_user(oim->session,passport,decode_msg,GAIM_MESSAGE_SYSTEM);

	/*Now get the oim message ID from the oim_list.
	 * and append to read list to prepare for deleting the Offline Message when sign out
	 */
	if(oim->oim_list != NULL){
		msg_id = oim->oim_list->data;
		msn_oim_post_delete_msg(oim,msg_id);
		oim->oim_list = g_list_remove(oim->oim_list, oim->oim_list->data);
	}

	g_free(passport);
}

/* Parse the XML data,
 * prepare to report the OIM to user
 */
void
msn_oim_get_process(MsnOim *oim,char *oim_msg)
{
	xmlnode *oimNode,*bodyNode,*responseNode,*msgNode;
	char *msg_data,*msg_str;

	oimNode = xmlnode_from_str(oim_msg, strlen(oim_msg));
	bodyNode = xmlnode_get_child(oimNode,"Body");
	responseNode = xmlnode_get_child(bodyNode,"GetMessageResponse");
	msgNode = xmlnode_get_child(responseNode,"GetMessageResult");
	msg_data = xmlnode_get_data(msgNode);
	msg_str = g_strdup(msg_data);
	gaim_debug_info("OIM","msg:{%s}\n",msg_str);
	msn_oim_report_to_user(oim,msg_str);

	g_free(msg_str);
}

static void
msn_oim_get_read_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnOim * oim = soapconn->session->oim;

	gaim_debug_info("MaYuan","OIM get read buffer:{%s}\n",soapconn->body);

	/*we need to process the read message!*/
	msn_oim_get_process(oim,soapconn->body);
	msn_soap_free_read_buf(soapconn);

	/*get next single Offline Message*/
	msn_soap_post(soapconn,NULL,msn_oim_retrieve_connect_init);
}

static void
msn_oim_get_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = msn_oim_get_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/* parse the oim XML data 
 * and post it to the soap server to get the Offline Message
 * */
void
msn_parse_oim_msg(MsnOim *oim,const char *xmlmsg)
{
	xmlnode *mdNode,*mNode,*INode,*nNode,*ENode,*rtNode;
	char *passport,*rTime,*msgid,*nickname;

	mdNode = xmlnode_from_str(xmlmsg, strlen(xmlmsg));
	for(mNode = xmlnode_get_child(mdNode, "M"); mNode;
					mNode = xmlnode_get_next_twin(mNode)){
		INode = xmlnode_get_child(mNode,"E");
		passport = xmlnode_get_data(INode);
		INode = xmlnode_get_child(mNode,"I");
		msgid = xmlnode_get_data(INode);
		rtNode = xmlnode_get_child(mNode,"RT");
		rTime = xmlnode_get_data(rtNode);
		nNode = xmlnode_get_child(mNode,"N");
		nickname = xmlnode_get_data(nNode);
		gaim_debug_info("MaYuan","E:{%s},I:{%s},rTime:{%s}\n",passport,msgid,rTime);

		oim->oim_list = g_list_append(oim->oim_list,msgid);
		msn_oim_post_single_get_msg(oim,msgid);
	}
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_single_get_msg(MsnOim *oim,const char *msgid)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	gaim_debug_info("MaYuan","Get single OIM Message\n");
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;

	soap_body = g_strdup_printf(MSN_OIM_GET_TEMPLATE,
					t,
					p,
					msgid
					);
	soap_request = msn_soap_request_new(MSN_OIM_RETRIEVE_HOST,
					MSN_OIM_RETRIEVE_URL,MSN_OIM_GET_SOAP_ACTION,
					soap_body,
					msn_oim_get_read_cb,
					msn_oim_get_written_cb);
	msn_soap_post(oim->retrieveconn,soap_request,msn_oim_retrieve_connect_init);
}

/*msn oim retrieve server connect init */
void
msn_oim_retrieve_connect_init(MsnSoapConn *soapconn)
{
	gaim_debug_info("MaYuan","msn_oim_connect...\n");
	msn_soap_init(soapconn,MSN_OIM_RETRIEVE_HOST,1,
					msn_oim_get_connect_cb,
					msn_oim_get_error_cb);
}

/*Msn OIM Send Server Connect Init Function*/
void msn_oim_send_connect_init(MsnSoapConn *sendconn)
{
	gaim_debug_info("MaYuan","msn oim send connect init...\n");
	msn_soap_init(sendconn,MSN_OIM_SEND_HOST,1,
					msn_oim_send_connect_cb,
					msn_oim_send_error_cb);
}

/*endof oim.c*/
