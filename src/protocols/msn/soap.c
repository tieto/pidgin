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

/*new a soap connection*/
MsnSoapConn *
msn_soap_new(MsnSession *session)
{
	MsnSoapConn *soapconn;

	soapconn = g_new0(MsnSoapConn, 1);
	soapconn->session = session;
	soapconn->input_handler = -1;
	soapconn->output_handler = -1;
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
}

/*init the soap connection*/
void
msn_soap_init(MsnSoapConn *soapconn,char * host,int ssl,
				GaimSslInputFunction	connect_cb,
				GaimSslErrorFunction	error_cb)
{
	soapconn->login_host = g_strdup(host);
	soapconn->ssl_conn = ssl;
	soapconn->connect_cb = connect_cb;
	soapconn->error_cb = error_cb;
	if(soapconn->ssl_conn){
		gaim_ssl_connect(soapconn->session->account, soapconn->login_host,
				GAIM_SSL_DEFAULT_PORT, msn_soap_connect_cb, msn_soap_error_cb,
				soapconn);
	}else{
	}
}

/*destroy the soap connection*/
void
msn_soap_destroy(MsnSoapConn *soapconn)
{
	g_free(soapconn->login_host);
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
	gaim_ssl_close(soapconn->gsc);
	soapconn->gsc = NULL;

	g_free(soapconn);
}

/*read and append the content to the buffer*/
static gssize
msn_soap_read(MsnSoapConn *soapconn)
{
	gssize len;
	gssize total_len = 0;
	char temp_buf[10240];

	if(soapconn->ssl_conn){
		len = gaim_ssl_read(soapconn->gsc, temp_buf,sizeof(temp_buf));
	}else{
		len = read(soapconn->fd, temp_buf,sizeof(temp_buf));
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

	gaim_debug_misc("MaYuan", "soap read cb\n");
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
//	gaim_debug_misc("MaYuan","content length :%d",soapconn->body_len);

	if(soapconn->read_len < body_start - soapconn->read_buf + atoi(body_len)){
			return;
	}

	g_free(body_len);

	/*call the read callback*/
	if(soapconn->read_cb != NULL){
		soapconn->read_cb(soapconn,source,0);
	}
#if 0
	/*clear the read buffer*/
	msn_soap_free_read_buf(soapconn);

	/*remove the read handler*/
	gaim_input_remove(soapconn->input_handler);
	soapconn->input_handler = -1;
	gaim_ssl_close(soapconn->gsc);
	soapconn->gsc = NULL;
#endif
}

void 
msn_soap_free_read_buf(MsnSoapConn *soapconn)
{
	if(soapconn->read_buf){
		g_free(soapconn->read_buf);
	}
	soapconn->read_buf = NULL;
	soapconn->read_len = 0;
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
	/*start the write*/
	soapconn->output_handler = gaim_input_add(soapconn->gsc->fd, GAIM_INPUT_WRITE,
													msn_soap_write_cb, soapconn);
	msn_soap_write_cb(soapconn, soapconn->gsc->fd, GAIM_INPUT_WRITE);
}

