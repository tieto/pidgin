/**
 * @file soap.c 
 * 	SOAP connection related process
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
#include "soap.h"

#define MSN_SOAP_DEBUG
/*local function prototype*/
void msn_soap_set_process_step(MsnSoapConn *soapconn, MsnSoapStep step);

/*setup the soap process step*/
void
msn_soap_set_process_step(MsnSoapConn *soapconn, MsnSoapStep step)
{
#ifdef MSN_SOAP_DEBUG
	const char *MsnSoapStepText[] =
	{
		"Unconnected",
		"Connecting",
		"Connected",
		"Processing",
		"Connected Idle"
	};

	purple_debug_info("MSN SOAP", "Setting SOAP process step to %s\n", MsnSoapStepText[step]);
#endif
	soapconn->step = step;
}

/*new a soap connection*/
MsnSoapConn *
msn_soap_new(MsnSession *session,gpointer data, gboolean ssl)
{
	MsnSoapConn *soapconn;

	soapconn = g_new0(MsnSoapConn, 1);
	soapconn->session = session;
	soapconn->parent = data;
	soapconn->ssl_conn = ssl;

	soapconn->gsc = NULL;
	soapconn->input_handler = 0;
	soapconn->output_handler = 0;

	msn_soap_set_process_step(soapconn, MSN_SOAP_UNCONNECTED);
	soapconn->soap_queue = g_queue_new();

	return soapconn;
}

/*ssl soap connect callback*/
void
msn_soap_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnSoapConn * soapconn;
	MsnSession *session;
	gboolean soapconn_is_valid = FALSE;

	purple_debug_misc("MSN SOAP","SOAP server connection established!\n");

	soapconn = data;
	g_return_if_fail(soapconn != NULL);

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	soapconn->gsc = gsc;

	msn_soap_set_process_step(soapconn, MSN_SOAP_CONNECTED);

	/*connection callback*/
	if (soapconn->connect_cb != NULL) {
		soapconn_is_valid = soapconn->connect_cb(soapconn, gsc);
	}

	if (!soapconn_is_valid) {
		return;
	}

	/*we do the SOAP request here*/
	msn_soap_post_head_request(soapconn);
}

/*ssl soap error callback*/
static void
msn_soap_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{	
	MsnSoapConn * soapconn = data;

	g_return_if_fail(data != NULL);

	purple_debug_warning("MSN SOAP","Soap connection error!\n");

	msn_soap_set_process_step(soapconn, MSN_SOAP_UNCONNECTED);

	/*error callback*/
	if (soapconn->error_cb != NULL) {
		soapconn->error_cb(soapconn, gsc, error);
	} else {
		msn_soap_post(soapconn, NULL);
	}
}

/*init the soap connection*/
void
msn_soap_init(MsnSoapConn *soapconn,char * host, gboolean ssl,
				MsnSoapSslConnectCbFunction connect_cb,
				MsnSoapSslErrorCbFunction error_cb)
{
	purple_debug_misc("MSN SOAP","Initializing SOAP connection\n");
	g_free(soapconn->login_host);
	soapconn->login_host = g_strdup(host);
	soapconn->ssl_conn = ssl;
	soapconn->connect_cb = connect_cb;
	soapconn->error_cb = error_cb;
}

/*connect the soap connection*/
void
msn_soap_connect(MsnSoapConn *soapconn)
{
	if (soapconn->ssl_conn) {
		purple_ssl_connect(soapconn->session->account, soapconn->login_host,
				PURPLE_SSL_DEFAULT_PORT, msn_soap_connect_cb, msn_soap_error_cb,
				soapconn);
	} else {
	}

	msn_soap_set_process_step(soapconn, MSN_SOAP_CONNECTING);
}


static void
msn_soap_close_handler(guint *handler)
{
	if (*handler > 0) {
		purple_input_remove(*handler);
		*handler = 0;
	} 
#ifdef MSN_SOAP_DEBUG
	else {
		purple_debug_misc("MSN SOAP", "Handler inactive, not removing\n");
	}
#endif

}


/*close the soap connection*/
void
msn_soap_close(MsnSoapConn *soapconn)
{
	if (soapconn->ssl_conn) {
		if (soapconn->gsc != NULL) {
			purple_ssl_close(soapconn->gsc);
			soapconn->gsc = NULL;
		}
	} else {
	}
	msn_soap_set_process_step(soapconn, MSN_SOAP_UNCONNECTED);
}

/*clean the unhandled SOAP request*/
void
msn_soap_clean_unhandled_requests(MsnSoapConn *soapconn)
{
	MsnSoapReq *request;

	g_return_if_fail(soapconn != NULL);

	soapconn->body = NULL;

	while ((request = g_queue_pop_head(soapconn->soap_queue)) != NULL){
		if (soapconn->read_cb) {
			soapconn->read_cb(soapconn);
		}
		msn_soap_request_free(request);
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
		purple_input_remove(soapconn->output_handler);
		soapconn->output_handler = 0;
	}
	/*remove the read handler*/
	if (soapconn->input_handler > 0){
		purple_input_remove(soapconn->input_handler);
		soapconn->input_handler = 0;
	}
	msn_soap_free_read_buf(soapconn);
	msn_soap_free_write_buf(soapconn);

	/*close ssl connection*/
	msn_soap_close(soapconn);

	/*process the unhandled soap request*/
	msn_soap_clean_unhandled_requests(soapconn);

	g_queue_free(soapconn->soap_queue);
	g_free(soapconn);
}

/*check the soap is connected?
 * if connected return 1
 */
int
msn_soap_connected(MsnSoapConn *soapconn)
{
	if (soapconn->ssl_conn) {
		return (soapconn->gsc == NULL ? 0 : 1);
	}
	return (soapconn->fd > 0 ? 1 : 0);
}

/*read and append the content to the buffer*/
static gssize
msn_soap_read(MsnSoapConn *soapconn)
{
	gssize len, requested_len;
	char temp_buf[MSN_SOAP_READ_BUFF_SIZE];
	
	if ( soapconn->need_to_read == 0 || soapconn->need_to_read > MSN_SOAP_READ_BUFF_SIZE) {
		requested_len = MSN_SOAP_READ_BUFF_SIZE;
	}
	else {
		requested_len = soapconn->need_to_read;
	}

	if ( soapconn->ssl_conn ) {
		len = purple_ssl_read(soapconn->gsc, temp_buf, requested_len);
	} else {
		len = read(soapconn->fd, temp_buf, requested_len);
	}

	
	if ( len <= 0 ) {
		switch (errno) {

			case 0:
			case EBADF: /* we are sometimes getting this in Windows */
			case EAGAIN: return len;

			default : purple_debug_error("MSN SOAP", "Read error!"
						"read len: %" G_GSSIZE_FORMAT ", error = %s\n",
						len, g_strerror(errno));
				  purple_input_remove(soapconn->input_handler);
				  //soapconn->input_handler = 0;
				  g_free(soapconn->read_buf);
				  soapconn->read_buf = NULL;
				  soapconn->read_len = 0;
				  /* TODO: error handling */
				  return len;
		}
	}
	else {
		soapconn->read_buf = g_realloc(soapconn->read_buf,
						soapconn->read_len + len + 1);
		if ( soapconn->read_buf != NULL ) {
			memcpy(soapconn->read_buf + soapconn->read_len, temp_buf, len);
			soapconn->read_len += len;
			soapconn->read_buf[soapconn->read_len] = '\0';
		}
		else {
			purple_debug_error("MSN SOAP",
				"Failure re-allocating %" G_GSIZE_FORMAT " bytes of memory!\n",
				soapconn->read_len + len + 1);
			exit(EXIT_FAILURE);
		}
			
	}

#if defined(MSN_SOAP_DEBUG)
	if (len > 0)
		purple_debug_info("MSN SOAP",
			"Read %" G_GSIZE_FORMAT " bytes from SOAP server:\n%s\n", len,
			soapconn->read_buf + soapconn->read_len - len);
#endif

	return len;
}

/*read the whole SOAP server response*/
static void 
msn_soap_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;
	int len;
	char * body_start,*body_len;
	char *length_start,*length_end;
#ifdef MSN_SOAP_DEBUG
#if !defined(_WIN32)
	gchar * formattedxml = NULL;
	gchar * http_headers = NULL;
	xmlnode * node = NULL;
#endif
	purple_debug_misc("MSN SOAP", "msn_soap_read_cb()\n");
#endif
	session = soapconn->session;
	g_return_if_fail(session != NULL);

	
	/*read the request header*/
	len = msn_soap_read(soapconn);
	
	if ( len < 0 )
		return;

	if (soapconn->read_buf == NULL) {
		return;
	}

	if ( (strstr(soapconn->read_buf, "HTTP/1.1 302") != NULL) 
		|| ( strstr(soapconn->read_buf, "HTTP/1.1 301") != NULL ) )
	{
		/* Redirect. */
		char *location, *c;

		purple_debug_info("MSN SOAP", "HTTP Redirect\n");
		location = strstr(soapconn->read_buf, "Location: ");
		if (location == NULL)
		{
			c = (char *) g_strstr_len(soapconn->read_buf, soapconn->read_len,"\r\n\r\n");
			if (c != NULL) {
				/* we have read the whole HTTP headers and found no Location: */
				msn_soap_free_read_buf(soapconn);
				msn_soap_post(soapconn, NULL);
			}

			return;
		}
		location = strchr(location, ' ') + 1;

		if ((c = strchr(location, '\r')) != NULL)
			*c = '\0';
		else
			return;

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
		
		msn_soap_close_handler( &(soapconn->input_handler) );
		msn_soap_close(soapconn);

		if (purple_ssl_connect(session->account, soapconn->login_host,
			PURPLE_SSL_DEFAULT_PORT, msn_soap_connect_cb,
			msn_soap_error_cb, soapconn) == NULL) {
		
			purple_debug_error("MSN SOAP", "Unable to connect to %s !\n", soapconn->login_host);
			// dispatch next request
			msn_soap_post(soapconn, NULL);
		}
	}
	/* Another case of redirection, active on May, 2007
	   See http://msnpiki.msnfanatic.com/index.php/MSNP13:SOAPTweener#Redirect
	 */
	else if (strstr(soapconn->read_buf,
                    "<faultcode>psf:Redirect</faultcode>") != NULL)
	{
		char *location, *c;

		if ( (location = strstr(soapconn->read_buf, "<psf:redirectUrl>") ) == NULL)
			return;

		/* Omit the tag preceding the URL */
		location += strlen("<psf:redirectUrl>");
		if (location > soapconn->read_buf + soapconn->read_len)
			return;
		if ( (location = strstr(location, "://")) == NULL)
			return;

		location += strlen("://"); /* Skip http:// or https:// */

		if ( (c = strstr(location, "</psf:redirectUrl>")) != NULL )
			*c = '\0';
		else
			return;

		if ( (c = strstr(location, "/")) != NULL )
		{
			g_free(soapconn->login_path);
			soapconn->login_path = g_strdup(c);
			*c = '\0';
		}

		g_free(soapconn->login_host);
		soapconn->login_host = g_strdup(location);
		
		msn_soap_close_handler( &(soapconn->input_handler) );
		msn_soap_close(soapconn);

		if (purple_ssl_connect(session->account, soapconn->login_host,
				PURPLE_SSL_DEFAULT_PORT, msn_soap_connect_cb,
				msn_soap_error_cb, soapconn) == NULL) {

			purple_debug_error("MSN SOAP", "Unable to connect to %s !\n", soapconn->login_host);
			// dispatch next request
			msn_soap_post(soapconn, NULL);
		}
	}
	else if (strstr(soapconn->read_buf, "HTTP/1.1 401 Unauthorized") != NULL)
	{
		const char *error;

		purple_debug_error("MSN SOAP", "Received HTTP error 401 Unauthorized\n");
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
				error = purple_url_decode(temp);
				g_free(temp);
			}
		}

		msn_session_set_error(session, MSN_ERROR_AUTH, error);
	}
	/* Handle Passport 3.0 authentication failures.
	 * Further info: http://msnpiki.msnfanatic.com/index.php/MSNP13:SOAPTweener
	 */
	else if (strstr(soapconn->read_buf,
				"<faultcode>wsse:FailedAuthentication</faultcode>") != NULL)
	{
		gchar *faultstring;

		faultstring = strstr(soapconn->read_buf, "<faultstring>");

		if (faultstring != NULL)
		{
			gchar *c;
			faultstring += strlen("<faultstring>");
			if (faultstring < soapconn->read_buf + soapconn->read_len) {
				c = strstr(soapconn->read_buf, "</faultstring>");
				if (c != NULL) {
					*c = '\0';
					msn_session_set_error(session, MSN_ERROR_AUTH, faultstring);
				}
			}
		}
		
	}
	else if (strstr(soapconn->read_buf, "HTTP/1.1 503 Service Unavailable"))
	{
		msn_session_set_error(session, MSN_ERROR_SERV_UNAVAILABLE, NULL);
	}
	else if ((strstr(soapconn->read_buf, "HTTP/1.1 200 OK"))
		||(strstr(soapconn->read_buf, "HTTP/1.1 500")))
	{
		gboolean soapconn_is_valid = FALSE;

		/*OK! process the SOAP body*/
		body_start = (char *)g_strstr_len(soapconn->read_buf, soapconn->read_len,"\r\n\r\n");
		if (!body_start) {
			return;
		}
		body_start += 4;

		if (body_start > soapconn->read_buf + soapconn->read_len)
			return;

		/* we read the content-length*/
		if ( (length_start = g_strstr_len(soapconn->read_buf, soapconn->read_len, "Content-Length: ")) != NULL)
			length_start += strlen("Content-Length: ");

		if (length_start > soapconn->read_buf + soapconn->read_len)
			return;

		if ( (length_end = strstr(length_start, "\r\n")) == NULL )
			return;

		body_len = g_strndup(length_start, length_end - length_start);

		/*setup the conn body */
		soapconn->body		= body_start;
		soapconn->body_len	= atoi(body_len);
		g_free(body_len);
#ifdef MSN_SOAP_DEBUG
		purple_debug_misc("MSN SOAP",
			"SOAP bytes read so far: %" G_GSIZE_FORMAT ", Content-Length: %d\n",
			soapconn->read_len, soapconn->body_len);
#endif
		soapconn->need_to_read = (body_start - soapconn->read_buf + soapconn->body_len) - soapconn->read_len;
		if ( soapconn->need_to_read > 0 ) {
			return;
		}

#if defined(MSN_SOAP_DEBUG) && !defined(_WIN32)

		node = xmlnode_from_str(soapconn->body, soapconn->body_len);
	
		if (node != NULL) {
			formattedxml = xmlnode_to_formatted_str(node, NULL);
			http_headers = g_strndup(soapconn->read_buf, soapconn->body - soapconn->read_buf);
				
			purple_debug_info("MSN SOAP","Data with XML payload received from the SOAP server:\n%s%s\n", http_headers, formattedxml);
			g_free(http_headers);
			g_free(formattedxml);
			xmlnode_free(node);
		}
		else
			purple_debug_info("MSN SOAP","Data received from the SOAP server:\n%s\n", soapconn->read_buf);
#endif

		/*remove the read handler*/
		msn_soap_close_handler( &(soapconn->input_handler) );
//		purple_input_remove(soapconn->input_handler);
//		soapconn->input_handler = 0;
		/*
		 * close the soap connection,if more soap request came,
		 * Just reconnect to do it,
		 *
		 * To solve the problem described below:
		 * When I post the soap request in one socket one after the other,
		 * The first read is ok, But the second soap read always got 0 bytes,
		 * Weird!
		 * */
		msn_soap_close(soapconn);

		/*call the read callback*/
		if ( soapconn->read_cb != NULL ) {
			soapconn_is_valid = soapconn->read_cb(soapconn);
		}
		
		if (!soapconn_is_valid) {
			return;
		}

		/* dispatch next request in queue */
		msn_soap_post(soapconn, NULL);
	}	
	return;
}

void 
msn_soap_free_read_buf(MsnSoapConn *soapconn)
{
	g_return_if_fail(soapconn != NULL);
	
	if (soapconn->read_buf) {
		g_free(soapconn->read_buf);
	}
	soapconn->read_buf = NULL;
	soapconn->read_len = 0;
	soapconn->need_to_read = 0;
}

void
msn_soap_free_write_buf(MsnSoapConn *soapconn)
{
	g_return_if_fail(soapconn != NULL);

	if (soapconn->write_buf) {
		g_free(soapconn->write_buf);
	}
	soapconn->write_buf = NULL;
	soapconn->written_len = 0;
}

/*Soap write process func*/
static void
msn_soap_write_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	int len, total_len;

	g_return_if_fail(soapconn != NULL);
	if ( soapconn->write_buf == NULL ) {
		purple_debug_error("MSN SOAP","SOAP write buffer is NULL\n");
	//	msn_soap_check_conn_errors(soapconn);
	//	purple_input_remove(soapconn->output_handler);
	//	soapconn->output_handler = 0;
		msn_soap_close_handler( &(soapconn->output_handler) );
		return;
	}
	total_len = strlen(soapconn->write_buf);

	/* 
	 * write the content to SSL server,
	 */
	len = purple_ssl_write(soapconn->gsc,
		soapconn->write_buf + soapconn->written_len,
		total_len - soapconn->written_len);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0){
		/*SSL write error!*/
//		msn_soap_check_conn_errors(soapconn);

		msn_soap_close_handler( &(soapconn->output_handler) );
//		purple_input_remove(soapconn->output_handler);
//		soapconn->output_handler = 0;

		msn_soap_close(soapconn);

		/* TODO: notify of the error */
		purple_debug_error("MSN SOAP", "Error writing to SSL connection!\n");
		msn_soap_post(soapconn, NULL);
		return;
	}
	soapconn->written_len += len;

	if (soapconn->written_len < total_len)
		return;

	msn_soap_close_handler( &(soapconn->output_handler) );
//	purple_input_remove(soapconn->output_handler);
//	soapconn->output_handler = 0;

	/*clear the write buff*/
	msn_soap_free_write_buf(soapconn);

	/* Write finish!
	 * callback for write done
	 */
	if(soapconn->written_cb != NULL){
		soapconn->written_cb(soapconn);
	}
	/*maybe we need to read the input?*/
	if ( soapconn->input_handler == 0 ) {
		soapconn->input_handler = purple_input_add(soapconn->gsc->fd,
			PURPLE_INPUT_READ, msn_soap_read_cb, soapconn);
	}
}

/*write the buffer to SOAP connection*/
void
msn_soap_write(MsnSoapConn * soapconn, char *write_buf, MsnSoapWrittenCbFunction written_cb)
{
	if (soapconn == NULL) {
		return;
	}

	msn_soap_set_process_step(soapconn, MSN_SOAP_PROCESSING);

	/* Ideally this wouldn't ever be necessary, but i believe that it is leaking the previous value */
	g_free(soapconn->write_buf);
	soapconn->write_buf = write_buf;
	soapconn->written_len = 0;
	soapconn->written_cb = written_cb;
	
	msn_soap_free_read_buf(soapconn);

	/*clear the read buffer first*/
	/*start the write*/
	soapconn->output_handler = purple_input_add(soapconn->gsc->fd, PURPLE_INPUT_WRITE,
						    msn_soap_write_cb, soapconn);
	msn_soap_write_cb(soapconn, soapconn->gsc->fd, PURPLE_INPUT_WRITE);
}

/* New a soap request*/
MsnSoapReq *
msn_soap_request_new(const char *host,const char *post_url,const char *soap_action,
				const char *body, const gpointer data_cb,
				MsnSoapReadCbFunction read_cb,
				MsnSoapWrittenCbFunction written_cb,
				MsnSoapConnectInitFunction connect_init)
{
	MsnSoapReq *request;

	request = g_new0(MsnSoapReq, 1);
	request->id = 0;

	request->login_host = g_strdup(host);
	request->login_path = g_strdup(post_url);
	request->soap_action		= g_strdup(soap_action);
	request->body		= g_strdup(body);
	request->data_cb 	= data_cb;
	request->read_cb	= read_cb;
	request->written_cb	= written_cb;
	request->connect_init	= connect_init;

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
	request->connect_init	= NULL;

	g_free(request);
}

/*post the soap request queue's head request*/
void
msn_soap_post_head_request(MsnSoapConn *soapconn)
{
	g_return_if_fail(soapconn != NULL);
	g_return_if_fail(soapconn->soap_queue != NULL);
	
	if (soapconn->step == MSN_SOAP_CONNECTED ||
	    soapconn->step == MSN_SOAP_CONNECTED_IDLE) {

		purple_debug_info("MSN SOAP", "Posting new request from head of the queue\n");

		if ( !g_queue_is_empty(soapconn->soap_queue) ) {
			MsnSoapReq *request;

			if ( (request = g_queue_pop_head(soapconn->soap_queue)) != NULL ) {
				msn_soap_post_request(soapconn,request);
			}
		} else {
			purple_debug_info("MSN SOAP", "No requests to process found.\n");
			msn_soap_set_process_step(soapconn, MSN_SOAP_CONNECTED_IDLE);
		}
	}
}

/*post the soap request ,
 * if not connected, Connected first.
 */
void
msn_soap_post(MsnSoapConn *soapconn, MsnSoapReq *request)
{
	MsnSoapReq *head_request;

	if (soapconn == NULL)
		return;

	if (request != NULL) {
#ifdef MSN_SOAP_DEBUG
		purple_debug_misc("MSN SOAP", "Request added to the queue\n");
#endif
		g_queue_push_tail(soapconn->soap_queue, request);
	}

	if ( !g_queue_is_empty(soapconn->soap_queue)) {

		/* we may have to reinitialize the soap connection, so avoid
		 * reusing the connection for now */

		if (soapconn->step == MSN_SOAP_CONNECTED_IDLE) {
			purple_debug_misc("MSN SOAP","Already connected to SOAP server, re-initializing\n");
			msn_soap_close_handler( &(soapconn->input_handler) );
			msn_soap_close_handler( &(soapconn->output_handler) );
			msn_soap_close(soapconn);
		}

		if (!msn_soap_connected(soapconn) && (soapconn->step == MSN_SOAP_UNCONNECTED)) {

			/*not connected?and we have something to process connect it first*/
			purple_debug_misc("MSN SOAP","No connection to SOAP server. Connecting...\n");
			head_request = g_queue_peek_head(soapconn->soap_queue);

			if (head_request == NULL) {
				purple_debug_error("MSN SOAP", "Queue is not empty, but failed to peek the head request!\n");
				return;
			}

			if (head_request->connect_init != NULL) {
				head_request->connect_init(soapconn);
			}
			msn_soap_connect(soapconn);
			return;
		}

#ifdef MSN_SOAP_DEBUG
		purple_debug_info("MSN SOAP", "Currently processing another SOAP request\n");
	} else {
		purple_debug_info("MSN SOAP", "No requests left to dispatch\n");
#endif
	}

}

/*Post the soap request action*/
void
msn_soap_post_request(MsnSoapConn *soapconn, MsnSoapReq *request)
{
	char * request_str = NULL;
#ifdef MSN_SOAP_DEBUG
#if !defined(_WIN32)
	xmlnode * node;
#endif
	purple_debug_misc("MSN SOAP","msn_soap_post_request()\n");
#endif

	msn_soap_set_process_step(soapconn, MSN_SOAP_PROCESSING);
	request_str = g_strdup_printf(
					"POST %s HTTP/1.1\r\n"
					"SOAPAction: %s\r\n"
					"Content-Type:text/xml; charset=utf-8\r\n"
					"Cookie: MSPAuth=%s\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
					"Accept: */*\r\n"
					"Host: %s\r\n"
					"Content-Length: %" G_GSIZE_FORMAT "\r\n"
					"Connection: Keep-Alive\r\n"
					"Cache-Control: no-cache\r\n\r\n"
					"%s",
					request->login_path,
					request->soap_action,
					soapconn->session->passport_info.mspauth,
					request->login_host,
					strlen(request->body),
					request->body
					);

#if defined(MSN_SOAP_DEBUG) && !defined(_WIN32)
	node = xmlnode_from_str(request->body, -1);
	if (node != NULL) {
		char *formattedstr = xmlnode_to_formatted_str(node, NULL);
		purple_debug_info("MSN SOAP","Posting request to SOAP server:\n%s%s\n",request_str, formattedstr);
		g_free(formattedstr);
		xmlnode_free(node);
	}
	else
		purple_debug_info("MSN SOAP","Failed to parse SOAP request being sent:\n%s\n", request_str);
#endif
	
	/*free read buffer*/
	// msn_soap_free_read_buf(soapconn);
	/*post it to server*/
	soapconn->data_cb = request->data_cb;
	msn_soap_write(soapconn, request_str, request->written_cb);
}

