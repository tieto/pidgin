/**
 * @file soap.c 
 * 	SOAP connection related process
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

/*setup the soap process step*/
void
msn_soap_set_process_step(MsnSoapConn *soapconn, MsnSoapStep step)
{
	soapconn->step = step;
}

//msn_soap_new(MsnSession *session,gpointer data,int sslconn)
/*new a soap connection*/
MsnSoapConn *
msn_soap_new(MsnSession *session,gpointer data,int sslconn)
{
	MsnSoapConn *soapconn;

	soapconn = g_new0(MsnSoapConn, 1);
	soapconn->session = session;
	soapconn->parent = data;
	soapconn->ssl_conn = sslconn;

	soapconn->gsc = NULL;
	soapconn->input_handler = -1;
	soapconn->output_handler = -1;

	msn_soap_set_process_step(soapconn,MSN_SOAP_UNCONNECTED);
	soapconn->soap_queue = g_queue_new();
	return soapconn;
}

/*ssl soap connect callback*/
void
msn_soap_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn * soapconn;
	MsnSession *session;

	gaim_debug_info("MaYuan","Soap connection connected!\n");
	soapconn = data;
	g_return_if_fail(soapconn != NULL);

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	soapconn->gsc = gsc;

	/*connection callback*/
	if(soapconn->connect_cb != NULL){
		soapconn->connect_cb(data,gsc,cond);
	}

	msn_soap_set_process_step(soapconn,MSN_SOAP_CONNECTED);
	/*we do the SOAP request here*/
	msn_soap_post_head_request(soapconn);
}

/*ssl soap error callback*/
static void
msn_soap_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{	
	MsnSoapConn * soapconn = data;
	g_return_if_fail(data != NULL);
	gaim_debug_info("MaYuan","Soap connection error!\n");
	/*error callback*/
	if(soapconn->error_cb != NULL){
		soapconn->error_cb(gsc,error,data);
	}
	msn_soap_set_process_step(soapconn, MSN_SOAP_UNCONNECTED);
}

/*init the soap connection*/
void
msn_soap_init(MsnSoapConn *soapconn,char * host,int ssl,
				GaimSslInputFunction	connect_cb,
				GaimSslErrorFunction	error_cb)
{
	gaim_debug_info("MaYuan","msn_soap_init...\n");
	soapconn->login_host = g_strdup(host);
	soapconn->ssl_conn = ssl;
	soapconn->connect_cb = connect_cb;
	soapconn->error_cb = error_cb;
	gaim_debug_info("MaYuan","msn_soap_init...done\n");
}

/*connect the soap connection*/
void
msn_soap_connect(MsnSoapConn *soapconn)
{
	if(soapconn->ssl_conn){
		gaim_ssl_connect(soapconn->session->account, soapconn->login_host,
				GAIM_SSL_DEFAULT_PORT, msn_soap_connect_cb, msn_soap_error_cb,
				soapconn);
	}else{
	}
	msn_soap_set_process_step(soapconn,MSN_SOAP_CONNECTING);
}

/*close the soap connection*/
void
msn_soap_close(MsnSoapConn *soapconn)
{
	if(soapconn->ssl_conn){
		if(soapconn->gsc != NULL){
			gaim_ssl_close(soapconn->gsc);
			soapconn->gsc = NULL;
		}
	}else{
	}
	msn_soap_set_process_step(soapconn,MSN_SOAP_UNCONNECTED);
}

/*destroy the soap connection*/
void
msn_soap_destroy(MsnSoapConn *soapconn)
{
	MsnSoapReq *request;

	if(soapconn->login_host)
		g_free(soapconn->login_host);

	if(soapconn->login_path)
		g_free(soapconn->login_path);

	/*remove the write handler*/
	if (soapconn->output_handler > 0){
		gaim_input_remove(soapconn->output_handler);
	}
	/*remove the read handler*/
	if (soapconn->input_handler > 0){
		gaim_input_remove(soapconn->input_handler);
	}
	msn_soap_free_read_buf(soapconn);
	msn_soap_free_write_buf(soapconn);

	/*close ssl connection*/
	msn_soap_close(soapconn);

	/*process the unhandled soap request*/
	while ((request = g_queue_pop_head(soapconn->soap_queue)) != NULL){
		msn_soap_request_free(request);
	}
	g_queue_free(soapconn->soap_queue);
	g_free(soapconn);
}

/*check the soap is connected?
 * if connected return 1
 */
int
msn_soap_connected(MsnSoapConn *soapconn)
{
	if(soapconn->ssl_conn){
		return (soapconn->gsc == NULL? 0 : 1);
	}
	return(soapconn->fd>0? 1 : 0);
}

/*read and append the content to the buffer*/
static gssize
msn_soap_read(MsnSoapConn *soapconn)
{
	gssize len,requested_len;
	gssize total_len = 0;
	char temp_buf[MSN_SOAP_READ_BUFF_SIZE];

//	requested_len = (soapconn->need_to_read > 0) ? soapconn->need_to_read : MSN_SOAP_READ_BUFF_SIZE;
	requested_len = MSN_SOAP_READ_BUFF_SIZE;
	if(soapconn->ssl_conn){
		len = gaim_ssl_read(soapconn->gsc, temp_buf,requested_len);
	}else{
		len = read(soapconn->fd, temp_buf,requested_len);
	}
	if(len >0){
		total_len += len;
		soapconn->read_buf = g_realloc(soapconn->read_buf,
						soapconn->read_len + len + 1);
//		strncpy(soapconn->read_buf + soapconn->read_len, temp_buf, len);
		memcpy(soapconn->read_buf + soapconn->read_len, temp_buf, len);
		soapconn->read_len += len;
		soapconn->read_buf[soapconn->read_len] = '\0';
	}
	gaim_debug_info("MaYuan","++soap ssl read:{%d}\n",total_len);
//	gaim_debug_info("MaYuan","nexus ssl read:{%s}\n",soapconn->read_buf);
	return total_len;
}

/*read the whole SOAP server response*/
void 
msn_soap_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;
	int len;
	char * body_start,*body_len;
	char *length_start,*length_end;

//	gaim_debug_misc("MaYuan", "soap read cb\n");
	session = soapconn->session;
	g_return_if_fail(session != NULL);

	if (soapconn->input_handler == -1){
		soapconn->input_handler = gaim_input_add(soapconn->gsc->fd,
			GAIM_INPUT_READ, msn_soap_read_cb, soapconn);
	}

	/*read the request header*/
	len = msn_soap_read(soapconn);
	if (len < 0 && errno == EAGAIN){
		return;
	}else if (len < 0) {
		gaim_debug_error("msn", "read Error!len:%d\n",len);
		gaim_input_remove(soapconn->input_handler);
		soapconn->input_handler = -1;
		g_free(soapconn->read_buf);
		soapconn->read_buf = NULL;
		soapconn->read_len = 0;
		/* TODO: error handling */
		return;
	}

	if(soapconn->read_buf == NULL){
		return;
	}

	if (strstr(soapconn->read_buf, "HTTP/1.1 302") != NULL)
	{
		/* Redirect. */
		char *location, *c;

		gaim_debug_error("MaYuan", "soap redirect\n");
		location = strstr(soapconn->read_buf, "Location: ");
		if (location == NULL)
		{
			msn_soap_free_read_buf(soapconn);

			return;
		}
		location = strchr(location, ' ') + 1;

		if ((c = strchr(location, '\r')) != NULL)
			*c = '\0';

		/* Skip the http:// */
		if ((c = strchr(location, '/')) != NULL)
			location = c + 2;

		if ((c = strchr(location, '/')) != NULL)
		{
			g_free(soapconn->login_path);
			soapconn->login_path = g_strdup(c);

			*c = '\0';
		}

		g_free(soapconn->login_host);
		soapconn->login_host = g_strdup(location);

		gaim_ssl_connect(session->account, soapconn->login_host,
			GAIM_SSL_DEFAULT_PORT, msn_soap_connect_cb,
			msn_soap_error_cb, soapconn);
	}
	else if (strstr(soapconn->read_buf, "HTTP/1.1 401 Unauthorized") != NULL)
	{
		const char *error;

		gaim_debug_error("MaYuan", "soap 401\n");
		if ((error = strstr(soapconn->read_buf, "WWW-Authenticate")) != NULL)
		{
			if ((error = strstr(error, "cbtxt=")) != NULL)
			{
				const char *c;
				char *temp;

				error += strlen("cbtxt=");

				if ((c = strchr(error, '\n')) == NULL)
					c = error + strlen(error);

				temp = g_strndup(error, c - error);
				error = gaim_url_decode(temp);
				g_free(temp);
			}
		}

		msn_session_set_error(session, MSN_ERROR_SERV_UNAVAILABLE, error);
	}
	else if (strstr(soapconn->read_buf, "HTTP/1.1 200 OK"))
	{
			/*OK! process the SOAP body*/
			body_start = (char *)g_strstr_len(soapconn->read_buf, soapconn->read_len,"\r\n\r\n");
			if(!body_start){
					return;
			}
			body_start += 4;

			//	gaim_debug_misc("msn", "Soap Read: {%s}\n", soapconn->read_buf);

			/* we read the content-length*/
			length_start = strstr(soapconn->read_buf, "Content-Length: ");
			length_start += strlen("Content-Length: ");
			length_end = strstr(length_start, "\r\n");
			body_len = g_strndup(length_start,length_end - length_start);

			/*setup the conn body */
			soapconn->body		= body_start;
			soapconn->body_len	= atoi(body_len);
			gaim_debug_misc("MaYuan","SOAP Read length :%d,body len:%d\n",soapconn->read_len,soapconn->body_len);

			soapconn->need_to_read = (body_start - soapconn->read_buf +soapconn->body_len) - soapconn->read_len;
			if(soapconn->read_len < body_start - soapconn->read_buf + soapconn->body_len){
//			if(soapconn->need_to_read >0){
				return;
			}
			g_free(body_len);

#if 1
			/*remove the read handler*/
			gaim_input_remove(soapconn->input_handler);
			soapconn->input_handler = -1;
#endif

			/*call the read callback*/
			if(soapconn->read_cb != NULL){
				soapconn->read_cb(soapconn,source,0);
			}
			/*clear the read buffer*/
			msn_soap_free_read_buf(soapconn);

			/*Process the next queued SOAP request*/
			msn_soap_post_head_request(soapconn);

#if 0
	/*remove the read handler*/
	gaim_input_remove(soapconn->input_handler);
	soapconn->input_handler = -1;
	//	gaim_ssl_close(soapconn->gsc);
	//	soapconn->gsc = NULL;
#endif
	}
	return;
}

void 
msn_soap_free_read_buf(MsnSoapConn *soapconn)
{
	if(soapconn->read_buf){
		g_free(soapconn->read_buf);
	}
	soapconn->read_buf = NULL;
	soapconn->read_len = 0;
	soapconn->need_to_read = 0;
}

void
msn_soap_free_write_buf(MsnSoapConn *soapconn)
{
	if(soapconn->write_buf){
		g_free(soapconn->write_buf);
	}
	soapconn->write_buf = NULL;
	soapconn->written_len = 0;
}

/*Soap write process func*/
static void
msn_soap_write_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	int len, total_len;

	g_return_if_fail(soapconn != NULL);
	if(soapconn->write_buf == NULL){
		gaim_debug_error("MaYuan","soap buffer is NULL\n");
		gaim_input_remove(soapconn->output_handler);
		soapconn->output_handler = -1;
		return;
	}
	total_len = strlen(soapconn->write_buf);

	/* 
	 * write the content to SSL server,
	 */
	len = gaim_ssl_write(soapconn->gsc,
		soapconn->write_buf + soapconn->written_len,
		total_len - soapconn->written_len);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0){
		/*SSL write error!*/
		gaim_input_remove(soapconn->output_handler);
		soapconn->output_handler = -1;
		/* TODO: notify of the error */
		return;
	}
	soapconn->written_len += len;

	if (soapconn->written_len < total_len)
		return;

	gaim_input_remove(soapconn->output_handler);
	soapconn->output_handler = -1;

	/*clear the write buff*/
	msn_soap_free_write_buf(soapconn);

	/* Write finish!
	 * callback for write done
	 */
	if(soapconn->written_cb != NULL){
		soapconn->written_cb(soapconn, source, 0);
	}
}

/*write the buffer to SOAP connection*/
void
msn_soap_write(MsnSoapConn * soapconn, char *write_buf, GaimInputFunction written_cb)
{
	soapconn->write_buf = write_buf;
	soapconn->written_len = 0;
	soapconn->written_cb = written_cb;

	/*clear the read buffer first*/
	/*start the write*/
	soapconn->output_handler = gaim_input_add(soapconn->gsc->fd, GAIM_INPUT_WRITE,
													msn_soap_write_cb, soapconn);
	msn_soap_write_cb(soapconn, soapconn->gsc->fd, GAIM_INPUT_WRITE);
}

/* New a soap request*/
MsnSoapReq *
msn_soap_request_new(const char *host,const char *post_url,const char *soap_action,
				const char *body,
				GaimInputFunction read_cb,GaimInputFunction written_cb)
{
	MsnSoapReq *request;

	request = g_new0(MsnSoapReq, 1);
	request->id = 0;

	request->login_host = g_strdup(host);
	request->login_path = g_strdup(post_url);
	request->soap_action		= g_strdup(soap_action);
	request->body		= g_strdup(body);
	request->read_cb	= read_cb;
	request->written_cb	= written_cb;

	return request;
}

/*free a soap request*/
void
msn_soap_request_free(MsnSoapReq *request)
{
	g_return_if_fail(request != NULL);

	g_free(request->login_host);
	g_free(request->login_path);
	g_free(request->soap_action);
	g_free(request->body);
	request->read_cb	= NULL;
	request->written_cb	= NULL;

	g_free(request);
}

/*post the soap request queue's head request*/
void
msn_soap_post_head_request(MsnSoapConn *soapconn)
{
	if(!g_queue_is_empty(soapconn->soap_queue)){
		MsnSoapReq *request;
		if((request = g_queue_pop_head(soapconn->soap_queue)) != NULL){
			msn_soap_post_request(soapconn,request);
		}
	}
	msn_soap_set_process_step(soapconn,MSN_SOAP_CONNECTED_IDLE);
}

/*post the soap request ,
 * if not connected, Connected first.
 */
void
msn_soap_post(MsnSoapConn *soapconn,MsnSoapReq *request,
				MsnSoapConnectInitFunction msn_soap_init_func)
{
	g_queue_push_tail(soapconn->soap_queue, request);
	if(!msn_soap_connected(soapconn)&&(soapconn->step == MSN_SOAP_UNCONNECTED)){
		/*not connected?connect it first*/
		gaim_debug_info("Ma Yuan","soap is not connected!\n");
		msn_soap_init_func(soapconn);
		msn_soap_connect(soapconn);
		return;
	}
	gaim_debug_info("Ma Yuan","soap  connected!\n");
	/*if connected, what we only needed to do is to queue the request, 
	 * when SOAP request in the queue processed done, will do this command.
	 * we just waiting...
	 */
	if(soapconn->step == MSN_SOAP_CONNECTED_IDLE){
		msn_soap_post_head_request(soapconn);
	}
}

/*Post the soap request action*/
void
msn_soap_post_request(MsnSoapConn *soapconn,MsnSoapReq *request)
{
	char * soap_head = NULL;
	char * request_str = NULL;

	gaim_debug_info("MaYuan","msn_soap_post()...\n");
	msn_soap_set_process_step(soapconn,MSN_SOAP_PROCESSING);
	soap_head = g_strdup_printf(
					"POST %s HTTP/1.1\r\n"
					"SOAPAction: %s\r\n"
					"Content-Type:text/xml; charset=utf-8\r\n"
					"Cookie: MSPAuth=%s\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
					"Accept: text/*\r\n"
					"Host: %s\r\n"
					"Content-Length: %d\r\n"
					"Connection: Keep-Alive\r\n"
					"Cache-Control: no-cache\r\n\r\n",
					request->login_path,
					request->soap_action,
					soapconn->session->passport_info.mspauth,
					request->login_host,
					strlen(request->body)
					);
	request_str = g_strdup_printf("%s%s", soap_head,request->body);
	g_free(soap_head);

	/*free read buffer*/
	msn_soap_free_read_buf(soapconn);
	gaim_debug_info("MaYuan","send to  server{%s}\n",request_str);
	msn_soap_write(soapconn,request_str,request->written_cb);
}

