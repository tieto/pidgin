/**
 * @file soap.h
 * 	header file for SOAP connection related process
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
#ifndef _MSN_SOAP_H_
#define _MSN_SOAP_H_

#define MSN_SOAP_READ_BUFF_SIZE		8192

typedef enum
{
	MSN_SOAP_UNCONNECTED,
	MSN_SOAP_CONNECTED,
	MSN_SOAP_PROCESSING,
	MSN_SOAP_CONNECTED_IDLE
}MsnSoapStep;

/*MSN SoapRequest structure*/
typedef struct _MsnSoapReq MsnSoapReq;

/*MSN Https connection structure*/
typedef struct _MsnSoapConn MsnSoapConn;

typedef void (*MsnSoapConnectInitFunction)(MsnSoapConn *);

struct _MsnSoapReq{
	/*request sequence*/
	int	 id;

	char *login_host;
	char *login_path;
	char *soap_action;

	char *body;

	GaimInputFunction read_cb;
	GaimInputFunction written_cb;
};

struct _MsnSoapConn{
	MsnSession *session;
	gpointer parent;

	char *login_host;
	char *login_path;
	char *soap_action;

	MsnSoapStep step;
	/*ssl connection?*/
	guint	ssl_conn;
	/*normal connection*/
	guint	fd;
	/*SSL connection*/
	GaimSslConnection *gsc;
	/*ssl connection callback*/
	GaimSslInputFunction	connect_cb;
	/*ssl error callback*/
	GaimSslErrorFunction	error_cb;

	/*read handler*/
	guint input_handler;
	/*write handler*/
	guint output_handler;

	/*Queue of SOAP request to send*/
	int soap_id;
	GQueue *soap_queue;

	/*write buffer*/
	char *write_buf;
	gsize written_len;
	GaimInputFunction written_cb;

	/*read buffer*/
	char *read_buf;
	gsize read_len;
	GaimInputFunction read_cb;

	/*HTTP reply body part*/
	char *body;
	int body_len;
};

/*Function Prototype*/
/*Soap Request Function */
MsnSoapReq *
msn_soap_request_new(const char *host,const char *post_url,const char *soap_action,
				const char *body,
				GaimInputFunction read_cb,GaimInputFunction written_cb);
void msn_soap_request_free(MsnSoapReq *request);
void msn_soap_post_request(MsnSoapConn *soapconn,MsnSoapReq *request);
void msn_soap_post_head_request(MsnSoapConn *soapconn);

/*new a soap conneciton */
MsnSoapConn *msn_soap_new(MsnSession *session,gpointer data,int sslconn);

/*destroy */
void msn_soap_destroy(MsnSoapConn *soapconn);

/*init a soap conneciton */
void msn_soap_init(MsnSoapConn *soapconn,char * host,int ssl,GaimSslInputFunction connect_cb,GaimSslErrorFunction error_cb);
void msn_soap_connect(MsnSoapConn *soapconn);

/*write to soap*/
void msn_soap_write(MsnSoapConn * soapconn, char *write_buf, GaimInputFunction written_cb);
void msn_soap_post(MsnSoapConn *soapconn,MsnSoapReq *request,MsnSoapConnectInitFunction msn_soap_init_func);

void  msn_soap_free_read_buf(MsnSoapConn *soapconn);
void msn_soap_free_write_buf(MsnSoapConn *soapconn);
void msn_soap_connect_cb(gpointer data, GaimSslConnection *gsc, GaimInputCondition cond);
void msn_soap_read_cb(gpointer data, gint source, GaimInputCondition cond);

/*check if the soap connection is connected*/
int msn_soap_connected(MsnSoapConn *soapconn);

#endif/*_MSN_SOAP_H_*/

