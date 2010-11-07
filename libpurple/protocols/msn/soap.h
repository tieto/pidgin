/**
 * @file soap.h
 * 	header file for SOAP connection related process
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
#ifndef _MSN_SOAP_H_
#define _MSN_SOAP_H_

#define MSN_SOAP_READ_BUFF_SIZE		8192

/* define this to debug the communications with the SOAP server */
/* #define MSN_SOAP_DEBUG */

#define MSN_SOAP_READ 1
#define MSN_SOAP_WRITE 2

typedef enum
{
	MSN_SOAP_UNCONNECTED,
	MSN_SOAP_CONNECTING,
	MSN_SOAP_CONNECTED,
	MSN_SOAP_PROCESSING,
	MSN_SOAP_CONNECTED_IDLE
}MsnSoapStep;

/* MSN SoapRequest structure*/
typedef struct _MsnSoapReq MsnSoapReq;

/* MSN Https connection structure*/
typedef struct _MsnSoapConn MsnSoapConn;

typedef void (*MsnSoapConnectInitFunction)(MsnSoapConn *);
typedef gboolean (*MsnSoapReadCbFunction)(MsnSoapConn *);
typedef void (*MsnSoapWrittenCbFunction)(MsnSoapConn *);

typedef gboolean (*MsnSoapSslConnectCbFunction)(MsnSoapConn *, PurpleSslConnection *);
typedef void (*MsnSoapSslErrorCbFunction)(MsnSoapConn *, PurpleSslConnection *, PurpleSslErrorType);


struct _MsnSoapReq{
	/*request sequence*/
	int	 id;

	char *login_host;
	char *login_path;
	char *soap_action;

	char *body;
	
	gpointer data_cb;
	MsnSoapReadCbFunction read_cb;
	MsnSoapWrittenCbFunction written_cb;
	MsnSoapConnectInitFunction connect_init;
};

struct _MsnSoapConn{
	MsnSession *session;
	gpointer parent;

	char *login_host;
	char *login_path;
	char *soap_action;

	MsnSoapStep step;
	/*ssl connection?*/
	gboolean ssl_conn;
	/*normal connection*/
	guint fd;
	/*SSL connection*/
	PurpleSslConnection *gsc;
	/*ssl connection callback*/
	MsnSoapSslConnectCbFunction connect_cb;
	/*ssl error callback*/
	MsnSoapSslErrorCbFunction error_cb;

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
	MsnSoapWrittenCbFunction written_cb;

	/*read buffer*/
	char *read_buf;
	gsize read_len;
	gsize need_to_read;
	MsnSoapReadCbFunction read_cb;

	gpointer data_cb;

	/*HTTP reply body part*/
	char *body;
	int body_len;
};


/*Function Prototype*/
/*Soap Request Function */
MsnSoapReq *msn_soap_request_new(const char *host, const char *post_url,
				 const char *soap_action, const char *body,
				 const gpointer data_cb,
				 MsnSoapReadCbFunction read_cb,
				 MsnSoapWrittenCbFunction written_cb,
				 MsnSoapConnectInitFunction connect_init);

void msn_soap_request_free(MsnSoapReq *request);
void msn_soap_post_request(MsnSoapConn *soapconn,MsnSoapReq *request);
void msn_soap_post_head_request(MsnSoapConn *soapconn);

/*new a soap conneciton */
MsnSoapConn *msn_soap_new(MsnSession *session, gpointer data, gboolean ssl);

/*destroy */
void msn_soap_destroy(MsnSoapConn *soapconn);

/*init a soap conneciton */
void msn_soap_init(MsnSoapConn *soapconn, char * host, gboolean ssl,
		   MsnSoapSslConnectCbFunction connect_cb,
		   MsnSoapSslErrorCbFunction error_cb);
void msn_soap_connect(MsnSoapConn *soapconn);
void msn_soap_close(MsnSoapConn *soapconn);

/*write to soap*/
void msn_soap_write(MsnSoapConn * soapconn, char *write_buf, MsnSoapWrittenCbFunction written_cb);
void msn_soap_post(MsnSoapConn *soapconn,MsnSoapReq *request);

void msn_soap_free_read_buf(MsnSoapConn *soapconn);
void msn_soap_free_write_buf(MsnSoapConn *soapconn);
void msn_soap_connect_cb(gpointer data, PurpleSslConnection *gsc, PurpleInputCondition cond);

/*clean the unhandled requests*/
void msn_soap_clean_unhandled_requests(MsnSoapConn *soapconn);

/*check if the soap connection is connected*/
int msn_soap_connected(MsnSoapConn *soapconn);
void msn_soap_set_process_step(MsnSoapConn *soapconn, MsnSoapStep step);

#endif/*_MSN_SOAP_H_*/

