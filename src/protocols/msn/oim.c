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
	msn_soap_read_cb(data,source,cond);
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
	msn_soap_post(oim->sendconn,soap_request);
}

void msn_oim_send_msg(MsnOim *oim,char *msg)
{
	if(msn_soap_connected(oim->sendconn) == -1){
	}
	
}

/****************************************
 * OIM delete SOAP request
 * **************************************/

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

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to OIM server"));
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
	/*call to get the message*/
	msn_oim_get_msg(oim);
}

static void
msn_oim_get_read_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnOim * oim = soapconn->session->oim;

	gaim_debug_info("MaYuan","OIM get read buffer:{%s}\n",soapconn->body);

	/*get next single Offline Message*/
	oim->oim_list = g_list_remove(oim->oim_list, oim->oim_list->data);
//	msn_oim_get_msg(oim);
}

static void
msn_oim_get_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = msn_oim_get_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*parse the oim XML data*/
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
//		msn_session_report_user(oim->session,passport,"hello");
		oim->oim_list = g_list_append(oim->oim_list,msgid);
	}
	if(msn_soap_connected(oim->retrieveconn) == -1){
		gaim_debug_info("MaYuan","retreive OIM server not connected! We need to connect it first\n");
		msn_oim_connect(oim);
		return;
	}
	msn_oim_get_msg(oim);
}

static void msn_oim_post_single_get_msg(MsnOim *oim,const char *msgid)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	gaim_debug_info("MaYuan","Get single OIM Message\n");
	oim->retrieveconn->login_path = g_strdup(MSN_OIM_RETRIEVE__URL);
	oim->retrieveconn->soap_action = g_strdup(MSN_OIM_GET_SOAP_ACTION);
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;

	soap_body = g_strdup_printf(MSN_OIM_GET_TEMPLATE,
					t,
					p,
					msgid
					);
	soap_request = msn_soap_request_new(MSN_OIM_SEND_HOST,
					MSN_OIM_SEND_URL,MSN_OIM_SEND_SOAP_ACTION,
					soap_body,
					msn_oim_get_read_cb,
					msn_oim_get_written_cb);
	msn_soap_post(oim->retrieveconn,soap_request);
}

/*MSN OIM get SOAP request*/
void msn_oim_get_msg(MsnOim *oim)
{
	gaim_debug_info("MaYuan","Get OIM with SOAP \n");
//	gaim_debug_info("MaYuan","oim->oim_list:%p,data:%s \n",oim->oim_list,oim->oim_list->data);
	if(oim->oim_list !=NULL){
		msn_oim_post_single_get_msg(oim,oim->oim_list->data);
	}
}

/*msn oim server connect*/
void
msn_oim_connect(MsnOim *oim)
{
	gaim_debug_info("MaYuan","msn_oim_connect...\n");

	if(msn_soap_connected(oim->retrieveconn) == -1){
		msn_soap_init(oim->retrieveconn,MSN_OIM_RETRIEVE_HOST,1,
					msn_oim_get_connect_cb,
					msn_oim_get_error_cb);
	}
#if 0
	if(msn_soap_connected(oim->sendconn) == -1){
		msn_soap_init(oim->sendconn,MSN_OIM_SEND_HOST,1,
					msn_oim_send_connect_cb,
					msn_oim_send_error_cb);
	}
#endif
}

/*endof oim.c*/
