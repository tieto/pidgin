/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "proxy.h"


/* static int proxy_inited=0; */
int proxy_type = 0;
char proxy_host[256];
int proxy_port = 3128;
char *proxy_realhost = NULL;

/* this code is borrowed from cvs 1.10 */
static int
proxy_recv_line (int sock, char **resultp)
{
    int c;
    char *result;
    size_t input_index = 0;
    size_t result_size = 80;

    result = g_malloc (result_size);

    while (1)
    {
	char ch;
	if (recv (sock, &ch, 1, 0) < 0)
	    fprintf (stderr, "recv() error from  proxy server\n");
	c = ch;

	if (c == EOF)
	{
	    g_free (result);

	    /* It's end of file.  */
	    fprintf(stderr, "end of file from  server\n");
	}

	if (c == '\012')
	    break;

	result[input_index++] = c;
	while (input_index + 1 >= result_size)
	{
	    result_size *= 2;
	    result = (char *) g_realloc (result, result_size);
	}
    }

    if (resultp)
	*resultp = result;

    /* Terminate it just for kicks, but we *can* deal with embedded NULs.  */
    result[input_index] = '\0';

    if (resultp == NULL)
	g_free (result);
    return input_index;
}


struct hostent *proxy_gethostbyname(char *host)
{
    
        if (proxy_type == PROXY_NONE)
                return (gethostbyname(host));

        if (proxy_realhost != NULL)
                g_free(proxy_realhost);

        /* we keep the real host name for the Connect command */
        proxy_realhost = (char *) strdup(host);
        
        return (gethostbyname(proxy_host));
    
}


int proxy_connect(int  sockfd, struct sockaddr *serv_addr, int
                  addrlen )
{
        struct sockaddr_in name;
        int ret;

        switch (proxy_type) {
        case PROXY_NONE:
                /* normal use */
                return (connect(sockfd,serv_addr,addrlen));
                break;
        case PROXY_HTTP:  /* Http proxy */
                /* do the  tunneling */
                /* step one : connect to  proxy */
                {
                        struct hostent *hostinfo;
                        unsigned short shortport = proxy_port;

                        memset (&name, 0, sizeof (name));
                        name.sin_family = AF_INET;
                        name.sin_port = htons (shortport);
                        hostinfo = gethostbyname (proxy_host);
                        if (hostinfo == NULL) {
                                fprintf (stderr, "Unknown host %s.\n", proxy_host);
                                return (-1);
                        }
                        name.sin_addr = *(struct in_addr *) hostinfo->h_addr;
                }
                sprintf(debug_buff,"Trying to connect ...\n");
                debug_print(debug_buff);
                if ((ret = connect(sockfd,(struct sockaddr *)&name,sizeof(name)))<0)
                        return(ret);
    
                /* step two : do  proxy tunneling init */
                {
                        char cmd[80];
                        char *inputline;
                        unsigned short realport=ntohs(((struct sockaddr_in *)serv_addr)->sin_port);
                        sprintf(cmd,"CONNECT %s:%d HTTP/1.1\n\r\n\r",proxy_realhost,realport);
                        sprintf(debug_buff,"<%s>\n",cmd);
                        debug_print(debug_buff);
                        if (send(sockfd,cmd,strlen(cmd),0)<0)
                                return(-1);
                        if (proxy_recv_line(sockfd,&inputline) < 0) {
                                return(-1);
                        }
                        sprintf(debug_buff,"<%s>\n",inputline);
                        debug_print(debug_buff);
                        if (memcmp("HTTP/1.0 200 Connection established",inputline,35))
                                if (memcmp("HTTP/1.1 200 Connection established",inputline,35)) {
                                        free(inputline);
                                        return(-1);
                                }

                        while (strlen(inputline)>1) {
                                free(inputline);
                                if (proxy_recv_line(sockfd,&inputline) < 0) {
                                        return(-1);
                                }
                                sprintf(debug_buff,"<%s>\n",inputline);
                                debug_print(debug_buff);
                        }
                        free(inputline);
                }
	        
                return ret;
                break;
        case PROXY_SOCKS:
                fprintf(stderr,"Socks proxy is not yet implemented.\n");
                return(-1);
                break;
        default:
                fprintf(stderr,"Unknown proxy type : %d.\n",proxy_type);
                break;
        }
        return(-1);
}

