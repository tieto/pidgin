/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
   Yahoo Pager Client Library

   This code is based on code by Douglas Winslow. The original info from
   his code is listed below. This code has taken his code and has been
   altered to my naming and coding conventions and has been made more
   usable as a library of routines.

   -- Nathan Neulinger <nneul@umr.edu>
 */

/*
   Yahoo Pager Client Emulator Pro - yppro.c
   A basic reference implementation
   Douglas Winslow <douglas@min.net>
   Tue Sep  1 02:28:21 EDT 1998
   Version 2, Revision 2
   Known to compile on Linux 2.0, FreeBSD 2.2, and BSDi 3.0.
   hi to aap bdc drw jfn jrc mm mcd [cejn]b #cz and rootshell

   Finally!
   Yahoo finally patched their server-side, and things will be getting
   back to "normal".  I will continue to maintain this code as long as
   there is interest for it.  Since Yahoo will be discontinuing YPNS1.1
   login support shortly, I've upgraded this client to do YPNS1.2.  You
   *must* have a password to pass authentication to the pager server.
   This authentication is done by a weird HTTP cookie method.

   This code is distributed under the GNU General Public License (GPL)
 */

#include "config.h"
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if defined(WITH_GTK)
#include <gtk/gtk.h>
#endif
#include <unistd.h>
#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
#if defined(HAVE_STRING_H)
#include <string.h>
#endif
#include <ctype.h>
#include "libyahoo.h"
#ifdef HAVE_DMALLOC
#include "dmalloc.h"
#else
#include <stdlib.h>
#endif

#include "memtok.h"

/* allow libyahoo to be used without gtkyahoo's debug support */
#ifdef ENABLE_LIBYAHOO_DEBUG
#include "libyahoo-debug.h"
#else
static void yahoo_dbg_Print(char *tmp, ...)
{
}

#define yahoo_dbg_NullCheck(x) ((x)?(x):("[NULL]"))
#endif

/* remap functions to gtk versions */
#if defined(WITH_GTK)
#define malloc g_malloc
#define free g_free
#define calloc(x,y) g_malloc0((x)*(y))
#endif

#if (!defined(TRUE) || !defined(FALSE))
# define TRUE 1
# define FALSE 0
#endif

/* Define a quick shortcut function to free a pointer and set it to null */
#define FREE(x) if (x) { free(x); x=NULL; }

#if defined(WITH_SOCKS4)
void SOCKSinit(char *argv0);
#endif

/* pager server host */
#define YAHOO_PAGER_HOST "cs.yahoo.com"
#define YAHOO_PAGER_PORT	5050
/* pager server host for http connections */
#define YAHOO_PAGER_HTTP_HOST "http.pager.yahoo.com"
#define YAHOO_PAGER_HTTP_PORT 80
/* authentication/login host */
#define YAHOO_AUTH_HOST		"msg.edit.yahoo.com"
#define YAHOO_AUTH_PORT		80
/* buddy/identity/config host */
#define YAHOO_DATA_HOST		YAHOO_AUTH_HOST
#define YAHOO_DATA_PORT     80
/* Address book host */
#define YAHOO_ADDRESS_HOST "uk.address.yahoo.com"
#define YAHOO_ADDRESS_PORT	80

/* User agent to use for HTTP connections */
/* It needs to have Mozilla/4 in it, otherwise it fails */
#ifndef VERSION
#define VERSION "1.0"
#endif
#define YAHOO_USER_AGENT	"Mozilla/4.6 (libyahoo/" VERSION ")"

#define YAHOO_PROTOCOL_HEADER "YPNS2.0"

/*
 *  Routines and data private to this library, should not be directly
 *  accessed outside of these routines.
 */

/* Service code labels for debugging output */
static struct yahoo_idlabel yahoo_service_codes[] = {
	{YAHOO_SERVICE_LOGON, "Pager Logon"},
	{YAHOO_SERVICE_LOGOFF, "Pager Logoff"},
	{YAHOO_SERVICE_ISAWAY, "Is Away"},
	{YAHOO_SERVICE_ISBACK, "Is Back"},
	{YAHOO_SERVICE_IDLE, "Idle"},
	{YAHOO_SERVICE_MESSAGE, "Message"},
	{YAHOO_SERVICE_IDACT, "Activate Identity"},
	{YAHOO_SERVICE_IDDEACT, "Deactivate Identity"},
	{YAHOO_SERVICE_MAILSTAT, "Mail Status"},
	{YAHOO_SERVICE_USERSTAT, "User Status"},
	{YAHOO_SERVICE_NEWMAIL, "New Mail"},
	{YAHOO_SERVICE_CHATINVITE, "Chat Invitation"},
	{YAHOO_SERVICE_CALENDAR, "Calendar Reminder"},
	{YAHOO_SERVICE_NEWPERSONALMAIL, "New Personals Mail"},
	{YAHOO_SERVICE_NEWCONTACT, "New Friend"},
	{YAHOO_SERVICE_GROUPRENAME, "Group Renamed"},
	{YAHOO_SERVICE_ADDIDENT, "Add Identity"},
	{YAHOO_SERVICE_ADDIGNORE, "Add Ignore"},
	{YAHOO_SERVICE_PING, "Ping"},
	{YAHOO_SERVICE_SYSMESSAGE, "System Message"},
	{YAHOO_SERVICE_CONFINVITE, "Conference Invitation"},
	{YAHOO_SERVICE_CONFLOGON, "Conference Logon"},
	{YAHOO_SERVICE_CONFDECLINE, "Conference Decline"},
	{YAHOO_SERVICE_CONFLOGOFF, "Conference Logoff"},
	{YAHOO_SERVICE_CONFMSG, "Conference Message"},
	{YAHOO_SERVICE_CONFADDINVITE, "Conference Additional Invitation"},
	{YAHOO_SERVICE_CHATLOGON, "Chat Logon"},
	{YAHOO_SERVICE_CHATLOGOFF, "Chat Logoff"},
	{YAHOO_SERVICE_CHATMSG, "Chat Message"},
	{YAHOO_SERVICE_GAMELOGON, "Game Logon"},
	{YAHOO_SERVICE_GAMELOGOFF, "Game Logoff"},
	{YAHOO_SERVICE_FILETRANSFER, "File Transfer"},
	{YAHOO_SERVICE_PASSTHROUGH2, "Passthrough 2"},
	{0, NULL}
};

/* Status codes */
static struct yahoo_idlabel yahoo_status_codes[] = {
	{YAHOO_STATUS_AVAILABLE, "I'm Available"},
	{YAHOO_STATUS_BRB, "Be Right Back"},
	{YAHOO_STATUS_BUSY, "Busy"},
	{YAHOO_STATUS_NOTATHOME, "Not at Home"},
	{YAHOO_STATUS_NOTATDESK, "Not at my Desk"},
	{YAHOO_STATUS_NOTINOFFICE, "Not in the Office"},
	{YAHOO_STATUS_ONPHONE, "On the Phone"},
	{YAHOO_STATUS_ONVACATION, "On Vacation"},
	{YAHOO_STATUS_OUTTOLUNCH, "Out to Lunch"},
	{YAHOO_STATUS_STEPPEDOUT, "Stepped Out"},
	{YAHOO_STATUS_INVISIBLE, "Invisible"},
	{YAHOO_STATUS_IDLE, "Idle"},
	{YAHOO_STATUS_CUSTOM, "Custom Message"},
	{0, NULL}
};

/* Status codes */
static struct yahoo_idlabel yahoo_status_append[] = {
	{YAHOO_STATUS_AVAILABLE, "is now available"},
	{YAHOO_STATUS_BRB, "will be right back"},
	{YAHOO_STATUS_BUSY, "is now busy"},
	{YAHOO_STATUS_NOTATHOME, "is not at home"},
	{YAHOO_STATUS_NOTATDESK, "is not at their desk"},
	{YAHOO_STATUS_NOTINOFFICE, "is not in the office"},
	{YAHOO_STATUS_ONPHONE, "is on the phone"},
	{YAHOO_STATUS_ONVACATION, "is on vacation"},
	{YAHOO_STATUS_OUTTOLUNCH, "is out to lunch"},
	{YAHOO_STATUS_STEPPEDOUT, "has stepped out"},
	{YAHOO_STATUS_INVISIBLE, "is now invisible"},
	{YAHOO_STATUS_IDLE, "is now idle"},
	{YAHOO_STATUS_CUSTOM, ""},
	{0, NULL}
};

static int readall(int fd, void *buf, size_t count)
{
  int left, ret, cur = 0;

  left = count;

  while (left) {
    ret = read(fd, ((unsigned char *)buf)+cur, left);
    if ((ret == -1) && (errno != EINTR)) {
      return -1;
    }
    if (ret == 0)
      return cur;

    if (ret != -1) {
      cur += ret;
      left -= ret;
    }
  }
  return cur;
}

static int writeall(int fd, void *buf, size_t count)
{
  int left, ret, cur = 0;

  left = count;

  while (left) {
    ret = write(fd, ((unsigned char *)buf)+cur, left);
    if ((ret == -1) && (errno != EINTR)) {
      return -1;
    }
    if (ret == 0)
      return cur;
    if (ret != -1) {
      cur += ret;
      left -= ret;
    }
  }

  return cur;
}

/* Take a 4-byte character string in little-endian format and return
   a unsigned integer */
unsigned int yahoo_makeint(unsigned char *data)
{
	if (data)
	{
		return ((data[3] << 24) + (data[2] << 16) + (data[1] << 8) +
			(data[0]));
	}
	return 0;
}

/* Take an integer and store it into a 4 character little-endian string */
static void yahoo_storeint(unsigned char *data, unsigned int val)
{
	unsigned int tmp = val;
	int i;

	if (!data)
                return;

	for (i = 0; i < 4; i++)
        {
                data[i] = tmp % 256;
                tmp >>= 8;
        }
}

/*
   converts a comma seperated list to an array of strings
   used primarily in conference code

   allocates a string in here -- caller needs to free it
 */
char **yahoo_list2array(const char *buff)
{
	char **tmp_array = NULL;
	char *array_elem = NULL;
	char *tmp = NULL;

	char *buffer = 0;
	char *ptr_buffer = 0;

	int sublen = 0;
	int cnt = 0;
	int nxtelem = 0;
	unsigned int i = 0;
	unsigned int len = 0;

	if (0 == buff)
		return 0;

	if (!(ptr_buffer = buffer = strdup(buff)))      /* play with a copy */
                return NULL;

	/* count the number of users (commas + 1) */
	for (i = 0; i < strlen(buffer); i++)
	{
		if (buffer[i] == ',')
		{
			/*
			   if not looking at end of list
			   ( ignore extra pesky comma at end of list)
			 */
			if (i != (strlen(buffer) - 1))
				cnt++;
		}
	}

	/* add one more name than comma .. */
	cnt++;

	/* allocate the array to hold the list of buddys */
	/* null terminated array of pointers */
	if (!(tmp_array = (char **) malloc(sizeof(char *) * (cnt + 1)))) {
                FREE(buffer);
                return NULL;
        }

	memset(tmp_array, 0, (sizeof(char *) * (cnt + 1)));

	/* Parse through the list and get all the entries */
	while ((ptr_buffer[sublen] != ',') && (ptr_buffer[sublen] != '\0'))
		sublen++;
	if (!(tmp = (char *) malloc(sizeof(char) * (sublen + 1)))) {
                FREE(buffer);
                FREE(tmp_array);
                return NULL;
        }

	memcpy(tmp, ptr_buffer, sublen);
	tmp[sublen] = '\0';

	if (ptr_buffer[sublen] != '\0')
		ptr_buffer = &(ptr_buffer[sublen + 1]);
	else
		ptr_buffer = &(ptr_buffer[sublen]);	/* stay at the null char */
	sublen = 0;

	while (tmp && (strcmp(tmp, "") != 0))
	{
		len = strlen(tmp);
		array_elem = (char *) malloc(sizeof(char) * (len + 1));

		strncpy(array_elem, tmp, (len + 1));
		array_elem[len] = '\0';

		tmp_array[nxtelem++] = array_elem;
		array_elem = NULL;

		FREE(tmp);

		while ((ptr_buffer[sublen] != ',') && (ptr_buffer[sublen] != '\0'))
			sublen++;
		if (!(tmp = (char *) malloc(sizeof(char) * (sublen + 1)))) {
                        FREE(buffer);
                        FREE(tmp_array);
                        return NULL;
                }

		memcpy(tmp, ptr_buffer, sublen);
		tmp[sublen] = '\0';

		if (ptr_buffer[sublen] != '\0')
			ptr_buffer = &(ptr_buffer[sublen + 1]);
		else
			ptr_buffer = &(ptr_buffer[sublen]);	/* stay at the null char */

		sublen = 0;
	}
	tmp_array[nxtelem] = NULL;

	FREE(tmp);
	FREE(buffer);
	return (tmp_array);

}								/* yahoo_list2array() */

/*
 Free's the memory associated with an array generated bu yahoo_list2array
 */
void yahoo_arraykill(char **array)
{
	int nxtelem = 0;

	if (NULL == array)
		return;

	while (array[nxtelem] != NULL)
	{
		FREE(array[nxtelem++]);
	}

	FREE(array);
}								/* yahoo_arraykill() */

/*
   converts an array of strings to a comma seperated list
   used primarily in conference code

   allocates a string in here.. needs to be freed by caller program
 */
char *yahoo_array2list(char **array)
{
	char *list = NULL;
	int nxtelem = 0;
	int arraylength = 0;

	if (NULL == array)
		return NULL;

	while (array[nxtelem] != NULL)
	{
		arraylength += strlen(array[nxtelem++]);
		arraylength++;			/* comma */
	}

	nxtelem = 0;				/* reset array counter */

	/* allocate at least one - for NULL list - and to
	   allow my strcat to write past the end for the
	   last comma which gets converted to NULL */
	if (!(list = (char *) malloc(sizeof(char) * (arraylength + 1))))
                return NULL;

	memset(list, 0, (arraylength + 1));

	while (array[nxtelem] != NULL)
	{
		strcat(list, array[nxtelem++]);
		strcat(list, ",");
	}
	/*
	   overwrite last ',' with a NULL
	   makes the string end with two null characters, but this way
	   handles empty lists gracefully
	 */
	list[arraylength - 1] = '\0';

	return (list);
}								/* yahoo_array2list() */

/* Free a buddy list */
static void yahoo_free_buddies(struct yahoo_context *ctx)
{
	int i;

	if (!ctx->buddies)
	{
		return;
	}

	i = 0;
	while (ctx->buddies[i])
	{
		FREE(ctx->buddies[i]->group);
		FREE(ctx->buddies[i]->id);
		i++;
	}

	FREE(ctx->buddies);
}

/* Free a identities list */
static void yahoo_free_identities(struct yahoo_context *ctx)
{
	int i;

	if (!ctx->identities)
	{
		return;
	}

	i = 0;
	while (ctx->identities[i])
	{
		FREE(ctx->identities[i]);
		i++;
	}

	FREE(ctx->identities);
	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_free_identities: done\n");
}

#if 0							/* not used at the moment */
static void yahoo_hexdump(const char *label, const unsigned char *data, int datalen)
{
	int i, j;
	int val, skipped_last;
	char current[100];
	char last[100];
	char tmp[15];
	char outline[100];
	static int last_datalen = 0;
	static unsigned char *last_data = NULL;

	if (last_data)
	{
		if (last_datalen == datalen && !memcmp(data, last_data, datalen))
		{
			printf("\n%s: <same as last dump>\n", label);
			return;
		}
		FREE(last_data);
	}

	/* Copy the packet so we can don't duplicate it next time. */
	last_datalen = datalen;
	if (!(last_data = (unsigned char *) malloc(datalen))) {
                FREE(last_data);
                return;
        }
	memcpy(last_data, data, datalen);

	/* Handle printing the full entry out */
	printf("\n");
	printf("%s:\n", label);

	skipped_last = 0;
	last[0] = 0;
	for (j = 0; j * 16 < datalen; j++)
	{
		current[0] = 0;

		/* Print out in hex */
		for (i = j * 16; i < (j * 16 + 16); i++)
		{
			if (i < datalen)
			{
				val = data[i];
				sprintf(tmp, "%.2X ", val);
			}
			else
			{
				sprintf(tmp, "   ");
			}
			strcat(current, tmp);
		}

		/* Print out in ascii */
		strcat(current, "  ");
		for (i = j * 16; i < (j * 16) + 16; i++)
		{
			if (i < datalen)
			{
				if (isprint(data[i]))
				{
					sprintf(tmp, "%c", data[i]);
				}
				else
				{
					sprintf(tmp, ".");
				}
			}
			else
			{
				sprintf(tmp, " ");
			}
			strcat(current, tmp);
		}

		outline[0] = 0;
		if (!strcmp(current, last))
		{
			if (!skipped_last)
			{
				strcpy(outline, " ....:\n");
			}
			skipped_last = 1;
		}
		else
		{
			sprintf(outline, " %.4d:  %s\n", j * 16, current);
			skipped_last = 0;
		}
		printf("%s", outline);
		strcpy(last, current);
	}

	if (skipped_last)
	{
		printf("%s", outline);
	}
	printf("\n");
}
#endif

static int yahoo_socket_connect(struct yahoo_context *ctx, const char *host,
	int port)
{
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int servfd;
	int res;

	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_socket_connect - starting [%s:%d]\n", host, port);

	if (!ctx || !host || !port)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_socket_connect - nulls\n");
		return -1;
	}

	if (!(server = gethostbyname(host)))
	{
		printf("[libyahoo] failed to look up server (%s:%d): %s\n", host, port, hstrerror(h_errno));
		return -1;
	}

	if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                return -1;
        }

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);

        /* XXX should put timeouts on the connect()'s -mid */
	res = -1;
	if (ctx->connect_mode == YAHOO_CONNECT_SOCKS4)
	{
#if defined(WITH_SOCKS4)
		res =
			Rconnect(servfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr));
#endif
	}
	else if (ctx->connect_mode == YAHOO_CONNECT_SOCKS5)
	{
#if defined(WITH_SOCKS5)
#endif
	}
	else if (ctx->connect_mode == YAHOO_CONNECT_NORMAL ||
		ctx->connect_mode == YAHOO_CONNECT_HTTP ||
		ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		res =
			connect(servfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr));
	}
	else
	{
		printf("[libyahoo] unhandled connect mode (%d).\n",
			ctx->connect_mode);
                close(servfd);
		return -1;
	}

	if (res < 0)
	{
		printf("[libyahoo] failed to connect to server (%s:%d): %s\n",
                       host, port, strerror(errno));
		close(servfd);
		return -1;
	}

	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_socket_connect - finished\n");
	return servfd;
}

/*  
 *  yahoo_urlencode(char *)
 *
 *
 *  29/12/2000:
 *
 *  function modified to accept only one arg.
 *  added code to reuse the buffer and check allocs.
 *
 *  -- Hrishikesh Desai <hrishi@mediaibc.com>
 *
 */


static char *yahoo_urlencode(const char *instr)
{
	register int ipos, bpos; //input str pos., buffer pos.
	static unsigned char *str=NULL;
	int len=strlen(instr);
	int tmp;

	//attempt to reuse buffer
	if(NULL==str)
	str = (unsigned char *) malloc(3 * len + 1);
	else
	str = (unsigned char *) realloc(str,3 * len + 1);
	
	//malloc, realloc failed ?
	if(errno==ENOMEM)
	{
	perror("libyahoo[yahoo_urlencode]");
	//return ref. to empty string, so's prog. or whatever wont crash
	return "";
	}
	
	ipos=bpos=0;

	while(ipos<len)
	{
	
	//using inverted logic frm original code....
	if (!isdigit((int) (instr[ipos])) 
	&& !isalpha((int) instr[ipos]) && instr[ipos] != '_')
	{
	tmp=instr[ipos] / 16;
	str[bpos++]='%';
	str[bpos++]=( (tmp < 10)?(tmp+'0'):(tmp+'A'-10));
	tmp=instr[ipos] % 16;
	str[bpos++]=( (tmp < 10)?(tmp+'0'):(tmp+'A'-10));
	}
	else
	{
	str[bpos++]=instr[ipos];
	}
	
	ipos++;
	}

	str[bpos] = '\0';
	
	//free extra alloc'ed mem.
	tmp=strlen(str);
	str = (unsigned char *) realloc(str,tmp + 1);
	
	return ( str);
}


static int yahoo_addtobuffer(struct yahoo_context *ctx, const char *data,
	int datalen)
{
	//yahoo_hexdump("yahoo_addtobuffer", data, datalen);

	/* Check buffer, increase size if necessary */
	if (!ctx->io_buf
		|| ((ctx->io_buf_maxlen - ctx->io_buf_curlen) < (datalen + 100)))
	{
		char *new_io_buf;

		if (datalen < 10240)
		{
			ctx->io_buf_maxlen += 10240;
		}
		else
		{
			ctx->io_buf_maxlen += datalen;
		}
		if (!(new_io_buf = (char *) malloc(ctx->io_buf_maxlen)))
                        return 0;

		if (ctx->io_buf)
		{
			memcpy(new_io_buf, ctx->io_buf, ctx->io_buf_curlen);
			FREE(ctx->io_buf);
		}

		ctx->io_buf = new_io_buf;
	}

	memcpy(ctx->io_buf + ctx->io_buf_curlen, data, datalen);
	ctx->io_buf_curlen += datalen;

        return 1;
}

static int yahoo_tcp_readline(char *ptr, int maxlen, int fd)
{
	int n, rc;
	char c;

	for (n = 1; n < maxlen; n++)
	{
	  again:

		if ((rc = readall(fd, &c, 1)) == 1)
		{
			*ptr++ = c;
			if (c == '\n')
				break;
		}
		else if (rc == 0)
		{
			if (n == 1)
				return (0);		/* EOF, no data */
			else
				break;			/* EOF, w/ data */
		}
		else
		{
			if (errno == EINTR)
				goto again;
			printf
				("Yahoo: Error reading from socket in yahoo_tcp_readline: %s.\n", strerror(errno));
			return -1;
		}
	}

	*ptr = 0;
	return (n);
}

/*
 * Published library interfaces
 */

/* Initialize interface to yahoo library, sortof like a class object
   creation routine. */
struct yahoo_context *yahoo_init(const char *user, const char *password,
	struct yahoo_options *options)
{
	struct yahoo_context *tmp;

	if (!user || !password)
	{
		return NULL;
	}

	/* Allocate a new context */
	if (!(tmp = (struct yahoo_context *) calloc(1, sizeof(*tmp))))
                return NULL;

	/* Fill in any available info */
	if (!(tmp->user = strdup(user))) {
                yahoo_free_context(tmp);
                return NULL;
        }
	if (!(tmp->password = strdup(password))) {
                yahoo_free_context(tmp);
                return NULL;
        }
	if (options->proxy_host)
	{
		if (!(tmp->proxy_host = strdup(options->proxy_host))) {
                        yahoo_free_context(tmp);
                        return NULL;
                }
	}
	tmp->proxy_port = options->proxy_port;
	tmp->connect_mode = options->connect_mode;

#if defined(WITH_SOCKS4)
	if (connect_mode == YAHOO_CONNECT_SOCKS4)
	{
		static int did_socks_init = 0;

		if (did_socks_init == 0)
		{
			SOCKSinit("libyahoo");
			did_socks_init = 1;
		}
	}
#endif

	/* Fetch the cookies */
	if (!yahoo_fetchcookies(tmp))
	{
		yahoo_free_context(tmp);
		return NULL;
	}

	return tmp;
}

/* Free a yahoo context */
void yahoo_free_context(struct yahoo_context *ctx)
{
	FREE(ctx->user);
	FREE(ctx->password);
	FREE(ctx->proxy_host);
	FREE(ctx->io_buf);
	FREE(ctx->cookie);
	FREE(ctx->login_cookie);
	FREE(ctx->login_id);

	yahoo_free_buddies(ctx);
	yahoo_free_identities(ctx);

	FREE(ctx);
}

/* Turn a status code into it's corresponding string */
char *yahoo_get_status_string(int statuscode)
{
	int i;

	for (i = 0; yahoo_status_codes[i].label; i++)
	{
		if (yahoo_status_codes[i].id == statuscode)
		{
			return yahoo_status_codes[i].label;
		}
	}
	return NULL;
}

/* Turn a status code into it's corresponding string */
char *yahoo_get_status_append(int statuscode)
{
	int i;

	for (i = 0; yahoo_status_append[i].label; i++)
	{
		if (yahoo_status_append[i].id == statuscode)
		{
			return yahoo_status_append[i].label;
		}
	}
	return NULL;
}

/* Turn a service code into it's corresponding string */
char *yahoo_get_service_string(int servicecode)
{
	int i;
	char *name = "Unknown Service";
	static char tmp[50];

	for (i = 0; yahoo_service_codes[i].label; i++)
	{
		if (yahoo_service_codes[i].id == servicecode)
		{
			name = yahoo_service_codes[i].label;
			break;
		}
	}

	snprintf(tmp, 50, "(%d) %s", servicecode, name);
	return tmp;
}

/* Return a malloc()'d copy of the users cookie */
int yahoo_fetchcookies(struct yahoo_context *ctx)
{
	char buffer[5000];
	int servfd;
	int i;
	int res;
	char *tmpstr;

	if (!ctx)
	{
		return 0;
	}

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_fetchcookies: starting\n");

	/* Check for cached cookie */
	if (ctx->cookie)
	{
		FREE(ctx->cookie);
	}
	if (ctx->login_cookie)
	{
		FREE(ctx->login_cookie);
	}

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		servfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		servfd = yahoo_socket_connect(ctx, YAHOO_AUTH_HOST, YAHOO_AUTH_PORT);
	}
	if (servfd < 0)
	{
		printf("[libyahoo] failed to connect to pager auth server.\n");
		return 0;
	}

	strcpy(buffer, "GET ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, "http://" YAHOO_AUTH_HOST);
	}
	strcat(buffer, "/config/ncclogin?login=");
	if (ctx->login_id)
	{
		strcat(buffer, yahoo_urlencode(ctx->login_id));
	}
	else
	{
		strcat(buffer, yahoo_urlencode(ctx->user));
	}
	strcat(buffer, "&passwd=");
	strcat(buffer, yahoo_urlencode(ctx->password));
	strcat(buffer, "&n=1 HTTP/1.0\r\n");
	strcat(buffer, "User-Agent: " YAHOO_USER_AGENT "\r\n");
	strcat(buffer, "Host: " YAHOO_AUTH_HOST "\r\n");
	strcat(buffer, "\r\n");

	if (writeall(servfd, buffer, strlen(buffer)) < strlen(buffer)) 
        {
                close(servfd);
                return 0;
        }

	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_fetchcookies: writing buffer '%s'\n", buffer);

	ctx->cookie = NULL;
	while ((res = yahoo_tcp_readline(buffer, 5000, servfd)) > 0)
	{
		/* strip out any non-alphas */
		for (i = 0; i < strlen(buffer); i++)
		{
			if (!isprint((int) buffer[i]))
			{
				buffer[i] = 0;
			}
		}

		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_fetchcookies: read buffer '%s'\n", buffer);

		if (!strcasecmp(buffer, "ERROR: Invalid NCC Login"))
		{
			yahoo_dbg_Print("libyahoo",
				"[libyahoo] yahoo_fetchcookies: password was invalid\n");
			return (0);
		}

		if (!strncasecmp(buffer, "Set-Cookie: Y=", 14))
		{
			FREE(ctx->cookie);
			ctx->cookie = strdup(buffer + 12);

			tmpstr = strchr(ctx->cookie, ';');
			if (tmpstr)
			{
				*tmpstr = '\0';
			}
		}
	}
	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_fetchcookies: closing server connection\n");
	close(servfd);
	servfd = 0;
	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_fetchcookies: closed server connection\n");

	if (ctx->cookie)
	{
		tmpstr = strstr(ctx->cookie, "n=");
		if (tmpstr)
		{
			ctx->login_cookie = strdup(tmpstr + 2);
		}

		tmpstr = strchr(ctx->login_cookie, '&');
		if (tmpstr)
		{
			*tmpstr = '\0';
		}
	}

	if (ctx->cookie)
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_fetchcookies: cookie (%s)\n", ctx->cookie);
	if (ctx->login_cookie)
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_fetchcookies: login cookie (%s)\n",
			ctx->login_cookie);

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_fetchcookies: done\n");

	return 1;
}

/* Add a buddy to your buddy list */
int yahoo_add_buddy(struct yahoo_context *ctx, const char *addid,
	const char *active_id, const char *group, const char *msg)
{
	char buffer[5000];
	int servfd;

	if (!ctx)
	{
		return 0;
	}

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_add_buddy - connecting via proxy\n");
		servfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_add_buddy - connecting\n");
		servfd = yahoo_socket_connect(ctx, YAHOO_DATA_HOST, YAHOO_DATA_PORT);
	}
	if (servfd < 0)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_add_buddy: failed to connect\n");
		return (0);
	}

	strcpy(buffer, "GET ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, "http://" YAHOO_DATA_HOST);
	}
	strcat(buffer, "/config/set_buddygrp?.bg=");
	strcat(buffer, yahoo_urlencode(group));
	strcat(buffer, "&.src=bl&.cmd=a&.bdl=");
	strcat(buffer, yahoo_urlencode(addid));
	strcat(buffer, "&.id=");
	strcat(buffer, yahoo_urlencode(active_id));
	strcat(buffer, "&.l=");
	strcat(buffer, yahoo_urlencode(ctx->user));
	strcat(buffer, "&.amsg=");
	strcat(buffer, yahoo_urlencode(msg));
	strcat(buffer, " HTTP/1.0\r\n");

	strcat(buffer, "User-Agent: " YAHOO_USER_AGENT "\r\n");
	strcat(buffer, "Host: " YAHOO_DATA_HOST "\r\n");
	strcat(buffer, "Cookie: ");
	strcat(buffer, ctx->cookie);
	strcat(buffer, "\r\n");
	strcat(buffer, "\r\n");

	if (writeall(servfd, buffer, strlen(buffer)) < strlen(buffer))
        {
                close(servfd);
                return 0;
        }

	while (yahoo_tcp_readline(buffer, 5000, servfd) > 0)
	{
		/* just dump the output, I don't care about errors at the moment */
	}
	close(servfd);
	servfd = 0;

	/* indicate success for now with 0 */
	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_add_buddy: finished\n");
	return 1;
}

/* Remove a buddy from your buddy list */
int yahoo_remove_buddy(struct yahoo_context *ctx, const char *addid,
	const char *active_id, const char *group, const char *msg)
{
	char buffer[5000];
	int servfd;

	if (!ctx)
	{
		return 0;
	}

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_add_buddy - connecting via proxy\n");
		servfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_add_buddy - connecting\n");
		servfd = yahoo_socket_connect(ctx, YAHOO_DATA_HOST, YAHOO_DATA_PORT);
	}
	if (servfd < 0)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_add_buddy: failed to connect\n");
		return (0);
	}

	strcpy(buffer, "GET ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, "http://" YAHOO_DATA_HOST);
	}
	strcat(buffer, "/config/set_buddygrp?.bg=");
	strcat(buffer, yahoo_urlencode(group));
	strcat(buffer, "&.src=bl&.cmd=d&.bdl=");
	strcat(buffer, yahoo_urlencode(addid));
	strcat(buffer, "&.id=");
	strcat(buffer, yahoo_urlencode(active_id));
	strcat(buffer, "&.l=");
	strcat(buffer, yahoo_urlencode(ctx->user));
	strcat(buffer, "&.amsg=");
	strcat(buffer, yahoo_urlencode(msg));
	strcat(buffer, " HTTP/1.0\r\n");

	strcat(buffer, "User-Agent: " YAHOO_USER_AGENT "\r\n");
	strcat(buffer, "Host: " YAHOO_DATA_HOST "\r\n");
	strcat(buffer, "Cookie: ");
	strcat(buffer, ctx->cookie);
	strcat(buffer, "\r\n");
	strcat(buffer, "\r\n");

	if (writeall(servfd, buffer, strlen(buffer)) < strlen(buffer))
        {
                close(servfd);
                return 0;
        }

	while (yahoo_tcp_readline(buffer, 5000, servfd) > 0)
	{
		/* just dump the output, I don't care about errors at the moment */
	}
	close(servfd);
	servfd = 0;

	/* indicate success for now with 1 */
	return 1;
}

/* Retrieve the configuration from the server */
int yahoo_get_config(struct yahoo_context *ctx)
{
	char buffer[5000];
	int i, j;
	int servfd;
	int commas;
	int in_section;
	struct yahoo_buddy **buddylist = NULL;
	int buddycnt = 0;
	int nextbuddy = 0;

	/* Check for cached cookie */
	if (!ctx || !ctx->cookie)
	{
		return 0;
	}

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_get_config: starting\n");

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		servfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		servfd = yahoo_socket_connect(ctx, YAHOO_DATA_HOST, YAHOO_DATA_PORT);
	}
	if (servfd < 0)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_get_config: failed to connect\n");
		return (0);
	}

	strcpy(buffer, "GET ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, "http://" YAHOO_DATA_HOST);
	}
	strcat(buffer, "/config/get_buddylist?.src=bl HTTP/1.0\r\n");
	strcat(buffer, "Cookie: ");
	strcat(buffer, ctx->cookie);
	strcat(buffer, "\r\n");
	strcat(buffer, "\r\n");

	if (writeall(servfd, buffer, strlen(buffer)) < strlen(buffer))
        {
                close(servfd);
                return 0;
        }

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_get_config: sending '%s'\n",
		buffer);

	in_section = 0;
	while (yahoo_tcp_readline(buffer, 5000, servfd) > 0)
	{
		/* strip out any non-alphas */
		for (i = 0; i < strlen(buffer); i++)
		{
			if (!isprint((int) buffer[i]))
			{
				for (j = i; j < strlen(buffer); j++)
				{
					buffer[j] = buffer[j + 1];
				}
			}
		}

		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_get_config: read '%s'\n", buffer);

		if (!strcasecmp(buffer, "BEGIN IDENTITIES"))
		{
			in_section = 1;
		}
		else if (!strcasecmp(buffer, "END IDENTITIES"))
		{
			in_section = 0;
		}
		else if (!strcasecmp(buffer, "BEGIN BUDDYLIST"))
		{
			in_section = 2;
		}
		else if (!strcasecmp(buffer, "END BUDDYLIST"))
		{
			in_section = 0;
		}
		else if (in_section == 1)
		{
			char *tmp;

			/* count the commas */
			commas = 0;
			for (i = 0; i < strlen(buffer); i++)
			{
				if (buffer[i] == ',')
				{
					commas++;
				}
			}

			/* make sure we've gotten rid of any previous identities array */
			yahoo_free_identities(ctx);

			/* allocate the array to hold the list of identities */
			ctx->identities = (char **) calloc(commas + 2, sizeof(char *));

			/* Parse through the list and get all the entries */
			i = 0;
			tmp = strtok(buffer, ",");
			while (tmp)
			{
				yahoo_dbg_Print("libyahoo",
					"[libyahoo] yahoo_get_config: retrieved "
					"identity '%s'\n", tmp);
				ctx->identities[i++] = strdup(tmp);
				tmp = strtok(NULL, ",");
			}
			ctx->identities[i] = 0;
		}
		else if (in_section == 2)
		{
			char *group;
			char *tmp;
			struct yahoo_buddy **tmp_buddylist;
			struct yahoo_buddy *tmpbuddy;
			int tmp_buddycnt;

			/* count the buddies on this line */
			tmp_buddycnt = buddycnt;
			for (i = 0; i < strlen(buffer); i++)
			{
				if (buffer[i] == ',')
				{
					buddycnt++;
				}
			}
			buddycnt++;			/* always one more than comma count */

			/* allocate the array to hold the list of buddy */
			tmp_buddylist = (struct yahoo_buddy **)
				malloc(sizeof(struct yahoo_buddy *) * (buddycnt + 1));

			/* Free and copy the old one if necessary */
			if (buddylist)
			{
				memcpy(tmp_buddylist, buddylist,

					(tmp_buddycnt + 1) * sizeof(struct yahoo_buddy *));

				FREE(buddylist);
			}
			buddylist = tmp_buddylist;

			/* Parse through the list and get all the entries */
			tmp = strtok(buffer, ",:");
			group = NULL;
			while (tmp)
			{
				if (tmp == buffer)	/* group name */
				{
					group = tmp;
				}
				else
				{
					tmpbuddy = (struct yahoo_buddy *)

						malloc(sizeof(struct yahoo_buddy));

					tmpbuddy->id = strdup(tmp);
					tmpbuddy->group = strdup(group);
					yahoo_dbg_Print("libyahoo",
						"[libyahoo] yahoo_get_config: retrieved buddy '%s:%s'\n",
						group, tmp);
					buddylist[nextbuddy++] = tmpbuddy;
				}
				tmp = strtok(NULL, ",");
			}
			buddylist[nextbuddy] = 0;
		}
		else if (!strncasecmp(buffer, "Mail=", strlen("Mail=")))
		{
			ctx->mail = atoi(buffer + strlen("Mail="));
			yahoo_dbg_Print("libyahoo",
				"[libyahoo] yahoo_get_config: retrieved mail flag '%d'\n",
				ctx->mail);
		}
		else if (!strncasecmp(buffer, "Login=", strlen("Login=")))
		{
			FREE(ctx->login_id);
			ctx->login_id = strdup(buffer + strlen("Login="));
			yahoo_dbg_Print("libyahoo",
				"[libyahoo] yahoo_get_config: retrieved login id '%s'\n",
				ctx->login_id);
		}
	}
	close(servfd);
	servfd = 0;

	yahoo_free_buddies(ctx);
	ctx->buddies = buddylist;

	/* fill in a bogus login_in, just in case */
	if (!ctx->login_id)
	{
		ctx->login_id = strdup(ctx->user);
	}

	/* refetch the cookie if the login_id is different so that
	   it will have the correct info in it */
	if (strcmp(ctx->login_id, ctx->user))
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_get_config - refetching cookies\n");
		yahoo_fetchcookies(ctx);
	}

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_get_config - finished\n");

	return 1;
}

/* Log in, optionally activating other secondary identities */
int yahoo_cmd_logon(struct yahoo_context *ctx, unsigned int initial_status)
{
	char login_string[5000];	/* need to change to malloc ASAP */
	char *tmpid;
	char **identities = ctx->identities;
	int i;

	if (!ctx || !ctx->login_cookie)
	{
		yahoo_dbg_Print("libyahoo",
			"[libyahoo] yahoo_cmd_logon: logon called without "
			"context and/or cookie.\n");
                return 0;
	}

	strcpy(login_string, ctx->login_cookie);
/* testing with new logon code */
//  strcpy(login_string, "$1$_2S43d5f$XXXXXXXXWtRKNclLWyy8C.");

	login_string[strlen(login_string) + 1] = 0;
	login_string[strlen(login_string)] = 1;	/* control-A */

	strcat(login_string, ctx->user);

	/* Send all identities */
	if (identities)
	{
		i = 0;
		tmpid = identities[i];
		while (tmpid)
		{
			if (strcasecmp(tmpid, ctx->user))
			{
				strcat(login_string, ",");
				strcat(login_string, tmpid);
			}
			tmpid = identities[i++];
		}
	}

	if(!yahoo_sendcmd(ctx, YAHOO_SERVICE_LOGON, ctx->user, login_string,
                          initial_status))
                return 0;

	/* something that the windows one sends, not sure what it is */
#if 0
	login_string[0] = 0;
	strcat(login_string, "C=0\002");
	strcat(login_string, "F=0,P=0,H=0,S=0,W=0,O=0\002");
	strcat(login_string, "M=0,P=0,C=0,S=0");
	yahoo_sendcmd(ctx, YAHOO_SERVICE_PASSTHROUGH2, ctx->user, login_string,
		0);
#endif

	return 1;
}

int yahoo_connect(struct yahoo_context *ctx)
{
	int res;

	res = 0;
	ctx->sockfd = 0;

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_connect - starting\n");

	switch (ctx->connect_mode)
	{
		case YAHOO_CONNECT_SOCKS4:
		case YAHOO_CONNECT_SOCKS5:
		case YAHOO_CONNECT_NORMAL:
			yahoo_dbg_Print("libyahoo",
				"[libyahoo] yahoo_connect - establishing socket connection\n");
			ctx->sockfd =
				yahoo_socket_connect(ctx, YAHOO_PAGER_HOST, YAHOO_PAGER_PORT);
			if (ctx->sockfd < 0)
			{
				printf("[libyahoo] couldn't connect to pager host\n");
				return (0);
			}
			break;

		case YAHOO_CONNECT_HTTP:
		case YAHOO_CONNECT_HTTPPROXY:
			yahoo_dbg_Print("libyahoo",
				"[libyahoo] yahoo_connect - no connect for HTTP\n");
			/* no pager connection will be established for this */
			break;

		default:
			printf("[libyahoo] unhandled connect mode (%d)\n",
				ctx->connect_mode);
	}

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_connect - finished\n");
	return (1);
}

/* Send a packet to the server via http connection method */
/* at moment only handles regular http connection, once I have that
   working, this code needs to also do http proxy connections as well */
int yahoo_sendcmd_http(struct yahoo_context *ctx, struct yahoo_rawpacket *pkt)
{
	int sockfd;
	char buffer[5000];
	char tmpbuf[1000];
	int size;
	int res;

	if (!ctx || !pkt)
	{
		return (0);
	}

	size = YAHOO_PACKET_HEADER_SIZE + strlen(pkt->content) + 1;

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		sockfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		sockfd = yahoo_socket_connect(ctx, YAHOO_PAGER_HTTP_HOST,
			YAHOO_PAGER_HTTP_PORT);
	}
	if (sockfd < 0)
	{
		printf("[libyahoo] failed to connect to pager http server.\n");
		return (0);
	}

	strcpy(buffer, "POST ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, "http://" YAHOO_PAGER_HTTP_HOST);
	}
	strcat(buffer, "/notify HTTP/1.0\r\n");

	strcat(buffer, "User-Agent: " YAHOO_USER_AGENT "\r\n");
	strcat(buffer, "Host: " YAHOO_PAGER_HTTP_HOST "\r\n");
	snprintf(tmpbuf, 1000, "Content-Length: %d\r\n", size);
	strcat(buffer, tmpbuf);

	strcat(buffer, "Pragma: No-Cache\r\n");

	strcat(buffer, "Cookie: ");
	strcat(buffer, ctx->cookie);
	strcat(buffer, "\r\n");
	strcat(buffer, "\r\n");

	if ((writeall(sockfd, buffer, strlen(buffer)) < strlen(buffer)) ||
            (writeall(sockfd, pkt, size) < size) ||
            (writeall(sockfd, "\r\n", 2) < 2))
        {
                close(sockfd);
                return 0;
        }

	/* now we need to read the results */
	/* I'm taking the cheat approach and just dumping them onto the
	   buffer, headers and all, the _skip_to_YHOO_ code will handle it
	   for now */

	while ((res = readall(sockfd, buffer, sizeof(buffer))) > 0)
	{
		if (res == -1)
		{
			printf("[libyahoo] Error reading data from server.\n");
                        return 0;
		}
		if (!yahoo_addtobuffer(ctx, buffer, res))
                {
                        close(sockfd);
                        return 0;
                }
	}
	close(sockfd);

	return (1);
}

/* Send a packet to the server, called by all routines that want to issue
   a command. */
int yahoo_sendcmd(struct yahoo_context *ctx, int service, const char *active_nick,
	const char *content, unsigned int msgtype)
{
	int size;
	struct yahoo_rawpacket *pkt;
	int maxcontentsize;

	/* why the )&*@#$( did they hardwire the packet size that gets sent
	   when the size of the packet is included in what is sent, bizarre */
	size = 4 * 256 + YAHOO_PACKET_HEADER_SIZE;
	if (!(pkt = (struct yahoo_rawpacket *) calloc(1, size)))
                return 0;

	/* figure out max content length, including trailing null */
	maxcontentsize = size - sizeof(struct yahoo_rawpacket);

	/* Build the packet */
	strcpy(pkt->version, YAHOO_PROTOCOL_HEADER);
	yahoo_storeint(pkt->len, size);
	yahoo_storeint(pkt->service, service);

	/* not sure if this is valid with YPNS1.4 or if it needs 2.0 */
	yahoo_storeint(pkt->msgtype, msgtype);

	/* Not sure, but might as well send for regular connections as well. */
	yahoo_storeint(pkt->magic_id, ctx->magic_id);
	strcpy(pkt->nick1, ctx->login_id);
	strcpy(pkt->nick2, active_nick);
	strncpy(pkt->content, content, maxcontentsize);

	// yahoo_hexdump("send_cmd", (char *) pkt, size);

	switch (ctx->connect_mode)
	{
		case YAHOO_CONNECT_SOCKS4:
		case YAHOO_CONNECT_SOCKS5:
		case YAHOO_CONNECT_NORMAL:
			if (writeall(ctx->sockfd, pkt, size) < size) 
                        {
                                printf("sendcmd: writeall failed\n");
                                close(ctx->sockfd);
                                FREE(pkt);
                                return 0;
                        }
			break;
		case YAHOO_CONNECT_HTTP:
		case YAHOO_CONNECT_HTTPPROXY:
			if (!yahoo_sendcmd_http(ctx, pkt)) 
                        {
                                printf("sendcmd_http failed\n");
                                FREE(pkt);
                                return 0;
                        }
			break;
	}

	FREE(pkt);
	return (1);
}

int yahoo_cmd_ping(struct yahoo_context *ctx)
{
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_PING, ctx->user, "", 0);
}

int yahoo_cmd_idle(struct yahoo_context *ctx)
{
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_IDLE, ctx->user, "", 0);
}

int yahoo_cmd_sendfile(struct yahoo_context *ctx, const char *active_user,
	const char *touser, const char *msg, const char *filename)
{
	yahoo_dbg_Print("libyahoo", "yahoo_cmd_sendfile not implemented yet!");
	return (0);
}

int yahoo_cmd_msg(struct yahoo_context *ctx, const char *active_user,
	const char *touser, const char *msg)
{
	char *content;

	if (!(content = (char *) malloc(strlen(touser) + strlen(msg) + 5)))
                return 0;

	if (strlen(touser))
	{
		sprintf(content, "%s,%s", touser, msg);
		if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_MESSAGE, active_user, content, 0)) 
                {
                        FREE(content);
                        return 0;
                }
	}

	FREE(content);
	return (1);
}

int yahoo_cmd_msg_offline(struct yahoo_context *ctx, const char *active_user,
	const char *touser, const char *msg)
{
	char *content;

	if (!(content = (char *) malloc(strlen(touser) + strlen(msg) + 5)))
                return 0;

	if (strlen(touser))
	{
		sprintf(content, "%s,%s", touser, msg);
		if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_MESSAGE, active_user, 
                                   content, YAHOO_MSGTYPE_KNOWN_USER))
                {
                        FREE(content);
                        return 0;
                }
	}

	FREE(content);
	return (1);
}

/* appended the " " so that won't trigger yahoo bug - hack for the moment */
int yahoo_cmd_set_away_mode(struct yahoo_context *ctx, int status, const char *msg)
{
	char statusstring[500];

	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_cmd_set_away_mode: set status (%d), msg(%s)\n",
		status, yahoo_dbg_NullCheck(msg));

	if (status == YAHOO_STATUS_CUSTOM)
	{
		if (msg && msg[0] != 0)
		{
			snprintf(statusstring, 500, "%d%c%s", status, 1, msg);
		}
		else
		{
			snprintf(statusstring, 500, "%d%c---", status, 1);
		}
	}
	else
	{
		snprintf(statusstring, 500, "%d", status);
	}
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_ISAWAY, ctx->user, statusstring, 0);
}

int yahoo_cmd_set_back_mode(struct yahoo_context *ctx, int status, const char *msg)
{
	char statusstring[500];

	yahoo_dbg_Print("libyahoo",
		"[libyahoo] yahoo_cmd_set_back_mode: set status (%d), msg(%s)\n",
		status, yahoo_dbg_NullCheck(msg));

	snprintf(statusstring, 500, "%d%c%s ", status, 1, msg ? msg : "");
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_ISBACK, ctx->user, statusstring, 0);
}

int yahoo_cmd_activate_id(struct yahoo_context *ctx, const char *newid)
{
	if (strlen(newid))
		return yahoo_sendcmd(ctx, YAHOO_SERVICE_IDACT, newid, newid, 0);
	return 0;
}

int yahoo_cmd_user_status(struct yahoo_context *ctx)
{
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_USERSTAT, ctx->user, "", 0);
}

int yahoo_cmd_logoff(struct yahoo_context *ctx)
{
	return yahoo_sendcmd(ctx, YAHOO_SERVICE_LOGOFF, ctx->user, ctx->user, 0);
}

/*

yahoo_cmd_start_conf()

   Starts a conference. (You create the conference)

Arguments:
   char *conf_id == The conference id -- usually of the form name-number,
                    though it doesn't seem to matter much. ex: jaylubo-123
		    You create this id to start the conference, but pass it
		    along after that.
   char **userlist == Users to invite. Null terminated array of strings.
   car *msg == Invitiation message.
   int type == 0 - normal, 1 - voice (not supported yet)

Packet format:
   id^invited-users^msg^0or1
*/
int yahoo_cmd_start_conf(struct yahoo_context *ctx, const char *conf_id,
	char **userlist, const char *msg, int type)
{
	char ctrlb = 2;
	char *content;
	char *new_userlist = yahoo_array2list(userlist);
	int cont_len = 0;

#ifdef ENABLE_LIBYAHOO_DEBUG
	char *unraw_msg = NULL;
#endif /* def ENABLE_LIBYAHOO_DEBUG */

	int size = strlen(conf_id) + strlen(msg) + 8 + strlen(new_userlist);

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	cont_len = snprintf(content,
		size - 1,
		"%s%c%s%c%s%c%d",
		conf_id, ctrlb, new_userlist, ctrlb, msg, ctrlb, type);

#ifdef ENABLE_LIBYAHOO_DEBUG
	if ((unraw_msg = yahoo_unraw_buffer(content, cont_len)))
        {
                yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_cmd_start_conf: %s\n",
                                unraw_msg);
                FREE(unraw_msg);
        }
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFINVITE, ctx->user, content, 0))
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*
yahoo_cmd_conf_logon()

   Reply to a conference invitation, logs you into conference.

Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
		      This comes from the invitiation.
   char *host ==      The person that sent you the invitation.
   char **userlist == Everyone else invited. This comes from the invitiation.
                      Null terminated array of strings.

Packet format:
   id^all-invited-users-and-host

*/
int yahoo_cmd_conf_logon(struct yahoo_context *ctx, const char *conf_id,
	const char *host, char **userlist)
{
	char ctrlb = 2;
	char *content;
	char *new_userlist = yahoo_array2list(userlist);
	int cont_len = 0;

#ifdef ENABLE_LIBYAHOO_DEBUG
	char *unraw_msg = NULL;
#endif /* def ENABLE_LIBYAHOO_DEBUG */

	int size = strlen(conf_id) + strlen(host) + 8 + strlen(new_userlist);

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	cont_len =
		sprintf(content, "%s%c%s,%s", conf_id, ctrlb, host, new_userlist);

#ifdef ENABLE_LIBYAHOO_DEBUG
	if ((unraw_msg = yahoo_unraw_buffer(content, cont_len)))
        {
                yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_cmd_conf_logon: %s\n",
                                unraw_msg);
                FREE(unraw_msg);
        }
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFLOGON, ctx->user, content, 0))
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*

yahoo_cmd_decline_conf()

   Reply to a conference invitation, decline offer.

Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
		      This comes from the invitiation.
   char *host ==      The person that sent you the invitation.
   char **userlist == Everyone else invited. This comes from the invitiation.
                      Null terminated array of strings.
		      (Null if replying to a conference additional invite )
   char *msg ==       Reason for declining.

Packet format:
   id^all-invited-users-and-host^msg

*/
int yahoo_cmd_decline_conf(struct yahoo_context *ctx, const char *conf_id,
	const char *host, char **userlist, const char *msg)
{
	char ctrlb = 2;
	char *content;
	char *new_userlist = yahoo_array2list(userlist);

	int size =
		strlen(conf_id) + strlen(host) + strlen(msg) + 8 +
		strlen(new_userlist);

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	sprintf(content, "%s%c%s,%s%c%s", conf_id, ctrlb, host, new_userlist,
		ctrlb, msg);

	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_cmd_decline_conf: %s\n",
		content);
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFDECLINE, ctx->user, content, 0))
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*

yahoo_cmd_conf_logoff()

   Logoff of a conference.

Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
		      This comes from the invitiation.
   char **userlist == Everyone in conference.
                      Null terminated array of strings.

Packet format:
   id^all-invited-users

*/

int yahoo_cmd_conf_logoff(struct yahoo_context *ctx, const char *conf_id,
	char **userlist)
{
	char ctrlb = 2;
	char *content;
	int cont_len = 0;

#ifdef ENABLE_LIBYAHOO_DEBUG
	char *unraw_msg = NULL;
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	char *new_userlist = yahoo_array2list(userlist);

	int size = strlen(conf_id) + strlen(new_userlist) + 8;

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	cont_len =
		snprintf(content, size, "%s%c%s", conf_id, ctrlb, new_userlist);
#ifdef ENABLE_LIBYAHOO_DEBUG
	if ((unraw_msg = yahoo_unraw_buffer(content, cont_len)))
        {
                yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_cmd_conf_logoff: %s\n",
                                unraw_msg);
                FREE(unraw_msg);
        }
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFLOGOFF, ctx->user, content, 0)) 
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*

yahoo_cmd_conf_invite()

   Invite another user to an already running conference.

Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
		      This comes from the invitiation.
   char *invited_user == The person being invited to conference.
   char **userlist == Everyone else in conference.
                      Null terminated array of strings.
   char *msg ==       Invitation message.

Packet format:
   id^invited-user^who-else-in-conf^who-else-in-conf^msg^0

*/

int yahoo_cmd_conf_invite(struct yahoo_context *ctx, const char *conf_id,
	char **userlist, const char *invited_user, const char *msg)
{
	char ctrlb = 2;
	char *content;
	char *new_userlist = yahoo_array2list(userlist);

	int size = strlen(conf_id) + strlen(invited_user)
		+ (2 * strlen(new_userlist)) + strlen(msg) + 7;

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	sprintf(content, "%s%c%s%c%s%c%s%c%s%c0", conf_id, ctrlb,
		invited_user, ctrlb, new_userlist, ctrlb,
		new_userlist, ctrlb, msg, ctrlb);
	yahoo_dbg_Print("libyahoo", "[libyahoo] yahoo_cmd_conf_invite: %s\n",
		content);
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFADDINVITE, ctx->user, content, 0))
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*

yahoo_cmd_conf_msg()

   Send a message to everyone in conference.

Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
		      This comes from the invitiation.
   char **userlist == Everyone in conference.
                      Null terminated array of strings.
   char *msg ==       Message to send.

Packet format:
   id^all-invited-users^msg

*/
int yahoo_cmd_conf_msg(struct yahoo_context *ctx, const char *conf_id,
	char **userlist, const char *msg)
{
	char ctrlb = 2;
	char *content;
	int cont_len = 0;

#ifdef ENABLE_LIBYAHOO_DEBUG
	char *unraw_msg = NULL;
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	char *new_userlist = yahoo_array2list(userlist);

	int size = strlen(conf_id) + strlen(new_userlist) + strlen(msg) + 8;

	if (!(content = (char *) malloc(size)))
                return 0;
	memset(content, 0, size);

	cont_len =
		snprintf(content, size, "%s%c%s%c%s", conf_id, ctrlb, new_userlist,
		ctrlb, msg);
#ifdef ENABLE_LIBYAHOO_DEBUG
	if ((unraw_msg = yahoo_unraw_buffer(content, cont_len)))
        {
                yahoo_dbg_Print("libyahoo", "yahoo_cmd_conf_msg: %s\n", unraw_msg);
                FREE(unraw_msg);
        }
#endif /* def ENABLE_LIBYAHOO_DEBUG */
	if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_CONFMSG, ctx->user, content, 0))
        {
                FREE(new_userlist);
                FREE(content);
                return 0;
        }

	FREE(new_userlist);
	FREE(content);
	return 1;
}

/*
 * Free the rawpacket structure - primarily a placeholder
 * since all static elements at the moment
 */
void yahoo_free_rawpacket(struct yahoo_rawpacket *pkt)
{
	FREE(pkt);
}

/*
 * Free entire packet structure including string elements
 */
void yahoo_free_packet(struct yahoo_packet *pkt)
{
	int i;

	if (pkt)
	{
		FREE(pkt->real_id);
		FREE(pkt->active_id);
		FREE(pkt->conf_id);
		FREE(pkt->conf_host);
		FREE(pkt->conf_user);
		FREE(pkt->conf_msg);
		FREE(pkt->cal_url);
		FREE(pkt->cal_timestamp);
		FREE(pkt->cal_title);
		FREE(pkt->cal_description);
		FREE(pkt->chat_invite_content);
		FREE(pkt->msg_id);
		FREE(pkt->msg_timestamp);
		FREE(pkt->msg);
		FREE(pkt->file_from);
		FREE(pkt->file_flag);
		FREE(pkt->file_url);
		FREE(pkt->file_description);
		FREE(pkt->group_old);
		FREE(pkt->group_new);
		if (pkt->idstatus)
		{
			for (i = 0; i < pkt->idstatus_count; i++)
			{
				yahoo_free_idstatus(pkt->idstatus[i]);
			}
			free(pkt->idstatus);
		}
		free(pkt);
	}
}

void yahoo_free_idstatus(struct yahoo_idstatus *idstatus)
{
	if (!idstatus)
		return;

	FREE(idstatus->id);
	FREE(idstatus->connection_id);
	FREE(idstatus->status_msg);
	FREE(idstatus);
}

struct yahoo_packet *yahoo_parsepacket(struct yahoo_context *ctx,
	struct yahoo_rawpacket *inpkt)
{
	struct yahoo_packet *pkt;

	/* If no valid inpkt passed, return */
	if (!inpkt)
		return NULL;

	/* Allocate the packet structure, zeroed out */
	if (!(pkt = (struct yahoo_packet *) calloc(sizeof(*pkt), 1)))
                return NULL;

	/* Pull out the standard data */
	pkt->service = yahoo_makeint(inpkt->service);
	pkt->connection_id = yahoo_makeint(inpkt->connection_id);
	pkt->real_id = strdup(inpkt->nick1);
	pkt->active_id = strdup(inpkt->nick2);

	pkt->magic_id = yahoo_makeint(inpkt->magic_id);
	pkt->unknown1 = yahoo_makeint(inpkt->unknown1);
	pkt->msgtype = yahoo_makeint(inpkt->msgtype);

	/* doing this seems like a cleaner approach, but am not sure if it is
	   a valid one */
	if (pkt->magic_id != 0)
	{
		ctx->magic_id = pkt->magic_id;
	}
	if (pkt->connection_id != 0)
	{
		ctx->connection_id = pkt->connection_id;
	}

	/* Call a particular parse routine to pull out the content */
	switch (pkt->service)
	{
		case YAHOO_SERVICE_LOGON:
		case YAHOO_SERVICE_LOGOFF:
		case YAHOO_SERVICE_ISAWAY:
		case YAHOO_SERVICE_ISBACK:
		case YAHOO_SERVICE_USERSTAT:
		case YAHOO_SERVICE_CHATLOGON:
		case YAHOO_SERVICE_CHATLOGOFF:
		case YAHOO_SERVICE_GAMELOGON:
		case YAHOO_SERVICE_GAMELOGOFF:
			yahoo_parsepacket_status(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_IDACT:
		case YAHOO_SERVICE_IDDEACT:
			/* nothing needs done, only has main fields */
			break;
		case YAHOO_SERVICE_MESSAGE:
		case YAHOO_SERVICE_SYSMESSAGE:
		case YAHOO_SERVICE_CHATMSG:
			yahoo_parsepacket_message(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_NEWMAIL:
		case YAHOO_SERVICE_NEWPERSONALMAIL:
			yahoo_parsepacket_newmail(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CALENDAR:
			yahoo_parsepacket_calendar(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CHATINVITE:
			yahoo_parsepacket_chatinvite(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_NEWCONTACT:
			yahoo_parsepacket_newcontact(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_GROUPRENAME:
			yahoo_parsepacket_grouprename(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CONFINVITE:
			yahoo_parsepacket_conference_invite(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CONFLOGON:
		case YAHOO_SERVICE_CONFLOGOFF:
			yahoo_parsepacket_conference_user(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CONFDECLINE:
			yahoo_parsepacket_conference_decline(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CONFADDINVITE:
			yahoo_parsepacket_conference_addinvite(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_CONFMSG:
			yahoo_parsepacket_conference_msg(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_PING:
			yahoo_parsepacket_ping(ctx, pkt, inpkt);
			break;
		case YAHOO_SERVICE_FILETRANSFER:
			yahoo_parsepacket_filetransfer(ctx, pkt, inpkt);
			break;
		default:
			yahoo_dbg_Print("libyahoo",
				"yahoo_parsepacket: can't parse packet type (%d)\n",
				pkt->service);
			break;
	}

	return pkt;
}

int yahoo_parsepacket_ping(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;

	/* Make working copy of content */
	content = inpkt->content;

	pkt->msg = NULL;
	if (content)
		pkt->msg = strdup(content);

	return 0;
}

int yahoo_parsepacket_newmail(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	int len;

	/* Make working copy of content */
	content = inpkt->content;
	len = strlen(content);

	if (pkt->service == YAHOO_SERVICE_NEWMAIL)
	{
		pkt->mail_status = 0;
		if (len > 0)
		{
			pkt->mail_status = atoi(content);
		}
	}
	else if (pkt->service == YAHOO_SERVICE_NEWPERSONALMAIL)
	{
		pkt->mail_status = 0;
		if (len > 0)
		{
			pkt->mail_status = atoi(content);
		}
	}

	return 0;
}

int yahoo_parsepacket_grouprename(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp, delim[5];

	/* Make working copy of content */
	content = strdup(inpkt->content);

	/* init elements to all null */
	pkt->group_old = NULL;
	pkt->group_new = NULL;

	tmp = NULL;
	delim[0] = 1;				/* control-a */
	delim[1] = 0;

	if (content)
	{
		tmp = strtok(content, delim);
	}

	if (tmp)					/* got the conference id */
	{
		pkt->group_old = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	if (tmp)					/* conference host */
	{
		pkt->group_new = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	FREE(content);
	return (0);
}

/*

yahoo_parsepacket_conference_invite()

Packet format:
   id^host^invited-users^msg^0or1

Parses Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
   char *conf_host == The person inviting you to conference.
   char **userlist == Everyone else invited to conference.
                      Null terminated array of strings.
   char *msg ==       Invitation message.
   int conf_type ==   Type of conference ( 0 = text, 1 = voice )

*/
int yahoo_parsepacket_conference_invite(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp = 0;
	size_t found = 0, len = yahoo_makeint(inpkt->len);

	/* Make working copy of content */
	content = memdup(inpkt->content, len);

	/* init elements to all null */
	pkt->conf_id = NULL;
	pkt->conf_host = NULL;
	pkt->conf_user = pkt->active_id;
	pkt->conf_userlist = NULL;
	pkt->conf_inviter = NULL;
	pkt->conf_msg = NULL;

	if (content)
	{
		tmp = memtok(content, len, "\002", 2, &found);
	}

	if (tmp)					/* got the conference id */
	{
		pkt->conf_id = memdupasstr(tmp, found);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	if (tmp)					/* conference host */
	{
		pkt->conf_host = memdupasstr(tmp, found);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	if (tmp)					/* who else is invited */
	{
		char *userlist = memdupasstr(tmp, found);

		pkt->conf_userlist = yahoo_list2array(userlist);
		FREE(userlist);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	if (tmp)					/* msg */
	{
		pkt->conf_msg = memdupasstr(tmp, found);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	if (tmp)					/* 0 == text chat 1 == voice chat */
	{
		char *conftype = memdupasstr(tmp, found);

		if (0 != conftype)
			pkt->conf_type = atoi(conftype);
		FREE(conftype);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	FREE(content);
	return 0;
}

/*

yahoo_parsepacket_conference_decline()

Packet format:
   id^user-who-declined^msg

Parses Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
   char *conf_user == User who declined.
   char *msg ==       Reason for declining.

*/
int yahoo_parsepacket_conference_decline(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp, delim[2];

	/* Make working copy of content */
	content = strdup(inpkt->content);

	/* init elements to all null */
	pkt->conf_id = NULL;
	pkt->conf_host = NULL;
	pkt->conf_user = NULL;
	pkt->conf_userlist = NULL;
	pkt->conf_inviter = NULL;
	pkt->conf_msg = NULL;

	tmp = NULL;
	delim[0] = 2;				/* control-b */
	delim[1] = 0;

	if (content)
	{
		tmp = strtok(content, delim);
	}

	if (tmp)					/* got the conference id */
	{
		pkt->conf_id = strdup(tmp);
		tmp = strtok(NULL, delim);
	}
	if (tmp)					/* got the user who declined */
	{
		pkt->conf_user = strdup(tmp);
		tmp = strtok(NULL, delim);
	}
	if (tmp)					/* msg */
	{
		pkt->conf_msg = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	FREE(content);
	return 0;

}

/*

yahoo_parsepacket_conference_addinvite()

Packet format:
Msgtype == 1
   id^inviter^who-else-invited^who-else-in-conf^msg^0or1
Msgtype == 11
   id^inviter^invited-user

Parses Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
   char *conf_inviter == The person inviting you to conference.
   char **userlist == Everyone else in conference.
                      Null terminated array of strings.
   char *msg ==       Invitation message.
   int conf_type ==   Type of conference ( 0 = text, 1 = voice )

   char *conf_user == User invited to conference (msgtype == 11)
*/
int yahoo_parsepacket_conference_addinvite(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content = 0, *tmp = 0;
	size_t found = 0, len = yahoo_makeint(inpkt->len);

	/* Make working copy of content */
	content = memdup(inpkt->content, len);

	/* init elements to all null */
	pkt->conf_id = NULL;
	pkt->conf_host = NULL;
	pkt->conf_user = NULL;
	pkt->conf_userlist = NULL;
	pkt->conf_inviter = NULL;
	pkt->conf_msg = NULL;

	if (pkt->msgtype == 1)
	{
		if (content)
		{
			tmp = memtok(content, len, "\002", 2, &found);
		}

		if (tmp)				/* got the conference id */
		{
			pkt->conf_id = memdupasstr(tmp, found);
			tmp = memtok(0, 0, "\002", 2, &found);
		}
		if (tmp)				/* got the inviter */
		{
			pkt->conf_inviter = memdupasstr(tmp, found);
			tmp = memtok(0, 0, "\002", 2, &found);
		}
		if (tmp)				/* got who-else-invited */
		{
			/* don't use this field, its the same as the next one
			   so I'm going to use the second field */
			/* pkt->conf_userlist = yahoo_list2array(tmp); */
			tmp = memtok(0, 0, "\002", 2, &found);
		}
		if (tmp)				/* got the people in conference
								   not counting the inviter */
		{
			char *userlist = memdupasstr(tmp, found);

			pkt->conf_userlist = yahoo_list2array(userlist);
			FREE(userlist);
			tmp = memtok(0, 0, "\002", 2, &found);
		}
		if (tmp)				/* got the message */
		{
			pkt->conf_msg = memdupasstr(tmp, found);
			tmp = memtok(0, 0, "\002", 2, &found);
		}
		if (tmp)				/* 0 at the end */
		{
			char *conftype = memdupasstr(tmp, found);

			if (0 != conftype)
				pkt->conf_type = atoi(conftype);
			FREE(conftype);
			/* tmp = memtok (0, 0, "\002", 2, &found); */
		}
	}
	else
		/* msgid == 11 (someone else is being invited) */
	{
		if (content)
		{
			tmp = memtok(content, len, "\002", 2, &found);
		}

		if (tmp)				/* got the conference id */
		{
			pkt->conf_id = memdupasstr(tmp, found);
			tmp = memtok(0, 0, "\002", 2, &found);
		}

		if (tmp)				/* got the inviter */
		{
			pkt->conf_inviter = memdupasstr(tmp, found);
			tmp = memtok(0, 0, "\002", 2, &found);
		}

		if (tmp)				/* got the invited-user */
		{
			pkt->conf_user = memdupasstr(tmp, found);
			/* tmp = memtok (0, 0, "\002", 2, &found); */
		}
	}

	FREE(content);
	return 0;
}

/*

yahoo_parsepacket_conference_msg()

Packet format:
   id^who-from^msg

Parses Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
   char *conf_user == User who sent message.
   char *msg ==       Message.

*/
int yahoo_parsepacket_conference_msg(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp, delim[5];

	/* Make working copy of content */
	content = strdup(inpkt->content);

	/* init elements to all null */
	pkt->conf_id = NULL;
	pkt->conf_host = NULL;
	pkt->conf_user = NULL;
	pkt->conf_userlist = NULL;
	pkt->conf_inviter = NULL;
	pkt->conf_msg = NULL;

	tmp = NULL;
	delim[0] = 2;				/* control-b */
	delim[1] = 0;

	/* parse error messages first */
	if (pkt->msgtype == YAHOO_MSGTYPE_ERROR)
	{
		FREE(content);
		return 0;
	}

	if (content)
	{
		tmp = strtok(content, delim);
	}

	if (tmp)					/* got the conference id */
	{
		pkt->conf_id = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	if (tmp)					/* conference user */
	{
		pkt->conf_user = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	if (tmp)					/* msg */
	{
		pkt->conf_msg = strdup(tmp);
		tmp = strtok(NULL, delim);
	}

	FREE(content);
	return 0;
}

/*

yahoo_parsepacket_conference_user()
   (User logged on/off to conference)
Packet format:
   id^user_who_logged_on/off

Parses Arguments:
   char *conf_id ==   The conference id -- usually of the form name-number,
                      though it doesn't seem to matter much. ex: jaylubo-123
   char *conf_user == User who logged on to conference.

*/
int yahoo_parsepacket_conference_user(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp = NULL;
        size_t found = 0, len = yahoo_makeint(inpkt->len);

	/* Make working copy of content */
	content = memdup(inpkt->content, len);

	/* init elements to all null */
	pkt->conf_id = NULL;
	pkt->conf_host = NULL;
	pkt->conf_user = NULL;
	pkt->conf_userlist = NULL;
	pkt->conf_inviter = NULL;
	pkt->conf_msg = NULL;

	if (content)
	{
		tmp = memtok(content, len, "\002", 2, &found);
	}

	if (tmp)					/* got the conference id */
	{
		pkt->conf_id = memdupasstr(tmp, found);
		tmp = memtok(0, 0, "\002", 2, &found);
	}

	if (tmp)					
	{
                if ( pkt->msgtype == 1 )                 /* conference user */
                {
                        pkt->conf_user = memdupasstr(tmp, found);
                        tmp = memtok(0, 0, "\002", 2, &found);
                }
                else if ( pkt->msgtype == 0 )        /* conference userlist? */
                {
                        char *userlist = memdupasstr(tmp, found);
                        
                        pkt->conf_userlist = yahoo_list2array(userlist);
                        tmp = memtok(0, 0, "\002", 2, &found);
                        FREE(userlist);
                }
	}

	FREE(content);
	return 0;
}

int yahoo_parsepacket_filetransfer(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp[5];
	int i, j, section;

	/* Make working copy of content */
	content = strdup(inpkt->content);

	/* init elements to all null */
	pkt->file_from = NULL;
	pkt->file_flag = NULL;
	pkt->file_url = NULL;
	pkt->file_expires = 0;
	pkt->file_description = NULL;

	/* overkill allocation, but simple since only temporary use */
	tmp[0] = strdup(content);
	tmp[1] = strdup(content);
	tmp[2] = strdup(content);
	tmp[3] = strdup(content);
	tmp[4] = strdup(content);

	/* raw data format: from,flag,url,timestamp,description */

	i = 0;
	j = 0;
	section = 0;
	tmp[0][0] = 0;
	tmp[1][0] = 0;
	tmp[2][0] = 0;
	tmp[3][0] = 0;
	tmp[4][0] = 0;

	while (i < strlen(content))
	{
		char ch = content[i];

		if (ch == ',' && section < 4)
		{
			j = 0;
			section++;
		}
		else
		{
			tmp[section][j++] = ch;
			tmp[section][j] = 0;
		}
		i++;
	}

	/* do stuff with extracted parts */
	pkt->file_from = strdup(tmp[0]);
	pkt->file_flag = strdup(tmp[1]);
	pkt->file_url = strdup(tmp[2]);
	pkt->file_expires = atoi(tmp[3]);
	pkt->file_description = strdup(tmp[4]);

	/* free working variables */
	FREE(tmp[0]);
	FREE(tmp[1]);
	FREE(tmp[2]);
	FREE(tmp[3]);
	FREE(tmp[4]);
	FREE(content);
	return 0;
}

int yahoo_parsepacket_calendar(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp, delim[5];

	/* Make working copy of content */
	content = strdup(inpkt->content);

	/* init elements to all null */
	pkt->cal_url = NULL;
	pkt->cal_timestamp = NULL;
	pkt->cal_type = 0;
	pkt->cal_title = NULL;
	pkt->cal_description = NULL;

	tmp = NULL;
	delim[0] = 2;				/* control-b */
	delim[1] = 0;

	if (content)
	{
		tmp = strtok(content, delim);
	}

	if (tmp)					/* got the url */
	{
		pkt->cal_url = strdup(tmp);
		tmp = strtok(NULL, delim);

/*
   v= is not the type code
   i= doesn't look like it either
   tmp2 = strstr(pkt->cal_url, "v=");
   if ( tmp2 )
   {
   pkt->cal_type = atoi(tmp2);
   }
 */

	}

	if (tmp)					/* unknown (type code?) */
	{
/* appears this isn't it either, I don't see where it is */
/*      pkt->cal_type = atoi(tmp);  */
		tmp = strtok(NULL, "\r\n");
	}

	if (tmp)					/* timestamp */
	{
		pkt->cal_timestamp = strdup(tmp);
		tmp = strtok(NULL, "\r\n");
	}

	if (tmp)					/* title */
	{
		pkt->cal_title = strdup(tmp);
		tmp = strtok(NULL, delim);	/* use delim since it won't occur again */
	}

	if (tmp)
	{
		pkt->cal_description = strdup(tmp);
	}

	FREE(content);
	return 0;
}

int yahoo_parsepacket_chatinvite(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	int len;

	/* Make working copy of content */
	content = strdup(inpkt->content);
	len = strlen(content);

	/* do special parsing for invite later on */
	pkt->chat_invite_content = strdup(content);

	return 0;
}

int yahoo_parsepacket_newcontact(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	int len;

	/* Make working copy of content */
	content = strdup(inpkt->content);
	len = strlen(content);

	/* cheat for now, say if first digit is number */
	if (len > 0)
	{
		if (isdigit((int) content[0]))
		{
			return yahoo_parsepacket_status(ctx, pkt, inpkt);
		}
		else
		{
			return yahoo_parsepacket_message(ctx, pkt, inpkt);
		}
	}

	return 0;
}

int yahoo_parsepacket_status(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmpc;
	char *tmp1;
	int i;
	int len;
	int index;
	int realcount;

	/* Make working copy of content */
	content = strdup(inpkt->content);
	len = strlen(content);

	/* Pull off the flag from the initial part of the content */
	/* this flag indicates the number of buddy that're online */
	pkt->flag = 0;
	tmpc = content;
	while (tmpc[0] && isdigit((int) tmpc[0]))
	{
		pkt->flag = pkt->flag * 10 + (content[0] - '0');
		tmpc++;
	}
	if (tmpc[0] && tmpc[0] == ',')
	{
		tmpc++;
	}

	/*
	   We're receiving either this:
	   2,buddy1(0,728EE9FB,0,1,0,0),buddy2(0,7AC00000,0,1,0,0)
	   or this:
	   buddy1(0,728EE9FB,0,1,0,0)
	   hence:
	 */

	if (pkt->flag == 0)
	{
		pkt->idstatus_count = 1;
	}
	else
	{
		pkt->idstatus_count = pkt->flag;
	}

	/* print an error if I get the was not AWAY */
	if (strstr(tmpc, "was not AWAY"))
	{
		pkt->idstatus_count = 0;
		yahoo_dbg_Print("libyahoo", "yahoo_parsepacket_status: "
			"got a 'was not AWAY' message\n");
	}

	if (pkt->idstatus_count == 0)
	{
		/* No entries, so no array needed */
		pkt->idstatus = NULL;
	}
	else
	{
		/* Allocate the array */
		pkt->idstatus = (struct yahoo_idstatus **)
			calloc(sizeof(struct yahoo_idstatus), pkt->idstatus_count);

		for (i = 0; i < pkt->idstatus_count; i++)
		{
			pkt->idstatus[i] = (struct yahoo_idstatus *)

				calloc(1, sizeof(struct yahoo_idstatus));
		}
	}

	index = 0;
	tmp1 = NULL;
	realcount = 0;
	while (tmpc && tmpc[0] && pkt->idstatus)
	{
		struct yahoo_idstatus *tmpid;

		/* Get pointer to allocated structure to hold status data */
		tmpid = pkt->idstatus[index++];
		if (!tmpid)
		{
			/* shortcut, we know there can't be any more status entries
			   at this point */
			/* yahoo_dbg_Print("status", "null tmpid"); */
			break;
		}

		/* YPNS2.0 nick(status,msg,connection_id,UNK,in_pager,in_chat,in_game) */
		/* tnneul(99,test,message^A,6AD68325,0,1,0,0) */
		/*         0 1               2       3 4 5 6 */

		/* YPNS1.0 nick(status,connection_id,UNK,in_pager,in_chat,in_game) */
		/* nneul(0,7081F531,0,1,0,0) */
		/*       0 2        3 4 5 6 */

		/* rewrite this whole section in a less ugly fashion */
		/* first pull off the id */

		/* YUCK - YPNS2.0 has variable format status records, if type is 99,
		   it has 7 fields, second is msg */

#if 0
		yahoo_dbg_Print("status", "whole string = '%s'\n",
			yahoo_dbg_NullCheck(tmpc));
#endif

		if (tmp1)
		{
			tmp1 = strtok(NULL, "(");
		}
		else
		{
			tmp1 = strtok(tmpc, "(");
		}
		if (tmp1 && tmp1[0] == ',')
		{
			tmp1++;
		}

		if (tmp1)
		{
			tmpid->id = strdup(tmp1);
			realcount++;

			for (i = 0; i <= 6 && tmp1; i++)
			{
#if 0
				yahoo_dbg_Print("status", "i==%d\n", i);
#endif

				if (i == 6)		/* end of status area */
				{
					tmp1 = strtok(NULL, "),");
				}
				else if (i == 1)
				{
					char delim[3];

					if (tmpid->status == YAHOO_STATUS_CUSTOM)
					{
						delim[0] = 1;
						delim[1] = ',';
						delim[2] = 0;
						tmp1 = strtok(NULL, delim);
					}
					else
					{
						i = 2;
						tmp1 = strtok(NULL, ",");
					}
				}
				else
				{

					tmp1 = strtok(NULL, ",");
				}

				/* then pull off the particular element of the list */
				if (tmp1)
				{
					switch (i)
					{
						case 0:	/* status */
							tmpid->status = atoi(tmp1);
							break;
						case 1:	/* msg */
							if (tmpid->status == YAHOO_STATUS_CUSTOM)
							{
								tmpid->status_msg = strdup(tmp1);
							}
							break;
						case 2:	/* session id */
							tmpid->connection_id = strdup(tmp1);
							break;
						case 3:	/* dunno what this is */
							break;
						case 4:
							tmpid->in_pager = atoi(tmp1);
							break;
						case 5:
							tmpid->in_chat = atoi(tmp1);
							break;
						case 6:
							tmpid->in_game = atoi(tmp1);
							break;
					}
				}
			}
		}
	}

	for (i = realcount; i <= pkt->idstatus_count; i++)
	{
		if (pkt->idstatus && pkt->idstatus[i])
		{
			FREE(pkt->idstatus[i]);
		}
	}
	pkt->idstatus_count = realcount;

	/* Free working copy of content */
	FREE(content);

	/* Return ok for success */
	return (0);
}

int yahoo_parsepacket_message(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *tmp_id;
	int i, j, section;

	if (pkt->msgtype == YAHOO_MSGTYPE_OFFLINE)
	{
		return yahoo_parsepacket_message_offline(ctx, pkt, inpkt);
	}

	/* Make working copy of content */
	content = strdup(inpkt->content);
	tmp_id = strdup(content);

	/* initialize */
	pkt->msg_status = 0;

	/* possible message content formats: */
/*     userid(#) *//* msgtype == YAHOO_MSGTYPE_STATUS */
	/*     userid,,msg */

	/* this needed butchered */
	/* YAHOO_MSGTYPE_OFFLINE */
	/* 6,6,tnneul,nneul,Tue Mar  7 12:14:50 2000,test offline msg^A */

	i = 0;
	j = 0;
	section = 0;
	tmp_id[0] = 0;
	while (i < strlen(content))
	{
		char ch = content[i];

		if (section == 0)		/* parsing userid */
		{
			if (ch == ',')
			{
				j = 0;
				section = 1;
			}
			else if (ch == '(')
			{
				j = 0;
				section = 2;
			}
			else
			{
				tmp_id[j++] = ch;
				tmp_id[j] = 0;
			}
		}
		else if (section == 1)	/* parsing flag */
		{
			if (ch == ',')
			{
				j = 0;
				section = 3;
			}
		}
		else if (section == 2)	/* parsing status */
		{
			if (ch == ')')
			{
				j = 0;
				section = 3;
			}
			else
			{
				if (isdigit((int) ch))
				{
					pkt->msg_status *= 10;
					pkt->msg_status += ch - '0';
				}
			}
		}
		else
		{
			pkt->msg = strdup(&content[i]);
			break;
		}

		i++;
	}

	/* do stuff with extracted parts */
	pkt->msg_id = strdup(tmp_id);

	/* handle empty message case */
	/* don't pass a message if it's just a status update */
	if (!pkt->msg && pkt->msgtype != YAHOO_MSGTYPE_STATUS)
	{
		pkt->msg = strdup("");
	}

	/* free working variables */
	FREE(tmp_id);
	FREE(content);

	/* Return ok for success */
	return (0);
}

/* This parses a special format offline message, and is only currently
called from yahoo_parsepacket_message. */
int yahoo_parsepacket_message_offline(struct yahoo_context *ctx,
	struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt)
{
	char *content;
	char *to_id;
	char *from_id;
	char *timestamp;
	int i, j, section;

	/* Make working copy of content */
	content = strdup(inpkt->content);
	to_id = strdup(content);
	from_id = strdup(content);
	timestamp = strdup(content);

	/* initialize */
	pkt->msg_status = 0;

	/* 6,6,tnneul,nneul,Tue Mar  7 12:14:50 2000,test offline msg^A */
	/* sec0,sec1,sec2=to,sec3=from,sec4=tstamp,sec5=msg */

	i = 0;
	j = 0;
	section = 0;
	to_id[0] = 0;
	from_id[0] = 0;
	timestamp[0] = 0;

	while (i < strlen(content))
	{
		char ch = content[i];

		if (section == 0)		/* parsing first unknown number */
		{
			if (ch == ',')
			{
				j = 0;
				section = 1;
			}
		}
		else if (section == 1)	/* parsing second unknown number */
		{
			if (ch == ',')
			{
				j = 0;
				section = 2;
			}
		}
		else if (section == 2)	/* parsing to-id */
		{
			if (ch == ',')
			{
				j = 0;
				section = 3;
			}
			else
			{
				to_id[j++] = ch;
				to_id[j] = 0;
			}
		}
		else if (section == 3)	/* parsing from-id */
		{
			if (ch == ',')
			{
				j = 0;
				section = 4;
			}
			else
			{
				from_id[j++] = ch;
				from_id[j] = 0;
			}
		}
		else if (section == 4)	/* parsing timestamp */
		{
			if (ch == ',')
			{
				j = 0;
				section = 5;
			}
			else
			{
				timestamp[j++] = ch;
				timestamp[j] = 0;
			}
		}
		else
		{
			pkt->msg = strdup(&content[i]);
			break;
		}

		i++;
	}

	/* do stuff with extracted parts */
	pkt->msg_id = strdup(from_id);
	pkt->msg_timestamp = strdup(timestamp);
	if (pkt->active_id)
	{
		FREE(pkt->active_id);
		pkt->active_id = strdup(to_id);
	}

	/* free working variables */
	FREE(timestamp);
	FREE(from_id);
	FREE(to_id) FREE(content);

	/* Return ok for success */
	return (0);
}

int yahoo_getdata(struct yahoo_context *ctx)
{
	char buf[1000];
	int res;

	/* This is a http mode connection, so just send a ping to get any
	   new data from the server. */
	if (ctx->connect_mode == YAHOO_CONNECT_HTTP ||
		ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		if (!yahoo_sendcmd(ctx, YAHOO_SERVICE_PING, ctx->user, "", 0))
                        return 0;
		return (1);
	}

        /* Read as much data as is available. */
        /* XXX: doesnt the protocol contain header information with lengths? */
	do {
                res = read(ctx->sockfd, buf, sizeof(buf));
                if ((res == -1) && (errno == EINTR))
                        continue;
                break;
        } while (1);

	if (res == -1)
	{
		printf("yahoo_getdata: error reading data from server: %s\n",
                       strerror(errno));
		return (0);
	}
	else if (res == 0)
	{
		yahoo_dbg_Print("io",
			"[libyahoo] yahoo_getdata: got zero length read\n", res);
		return 0;
	}

        yahoo_addtobuffer(ctx, buf, res);
        yahoo_dbg_Print("io", "[libyahoo] yahoo_getdata: read (%d) bytes\n", res);

	return 1;
}

struct yahoo_rawpacket *yahoo_getpacket(struct yahoo_context *ctx)
{
	struct yahoo_rawpacket *pkt;
	struct yahoo_rawpacket *retpkt;
	int *buflen = &ctx->io_buf_curlen;
	char *buffer = ctx->io_buf;
	unsigned int contentlen;

	/* If buffer doesn't start with YHOO, skip bytes until it
	   does. This is to protect against possible packet alignment
	   errors if I size something wrong at any time. */

	while ((*buflen >= 4) && (memcmp(buffer, "YHOO", 4)))
	{
/* making quiet for now so I don't have to work too hard on the HTTP support */
#if 0
		printf("\nskipped buffer byte (%d)\n", buffer[0]);
#endif
		memmove(buffer, buffer + 1, *buflen - 1);
		*buflen = *buflen - 1;
	}

	/* Don't do anything if the buffer doesn't have at least a full
	   header */
	if (*buflen < YAHOO_PACKET_HEADER_SIZE)
	{
// printf("returning null cause buffer is too small\n");
		return NULL;
	}

/* print out the beginning of the buffer */
#if 0
	printf("Buffer (buflen = %d):\n", *buflen);
	for (i = 0; i < *buflen; i++)
	{
		if ((i) % 10 == 0)
		{
			printf("\n%.4d: ", i);
		}
		if (isprint(buffer[i]))
		{
			printf("%-3d %c  ", buffer[i], buffer[i]);
		}
		else
		{
			printf("%-3d    ", buffer[i]);
		}
	}
	printf("\n");
#endif
	/* Make pkt point to buffer for ease of use */
	pkt = (struct yahoo_rawpacket *) buffer;

	/* Determine the content size specified by the header */
	contentlen = yahoo_makeint(pkt->len) - YAHOO_PACKET_HEADER_SIZE;
#if 0
        printf("contentlen = %d\n", contentlen);
#endif

	/* Don't continue if buffer doesn't have full content in it */
	if (*buflen < (YAHOO_PACKET_HEADER_SIZE + contentlen))
	{
                printf("buffer not big enough for contentlen\n");
		return NULL;
	}

	/* Copy this packet */
	retpkt =
		(struct yahoo_rawpacket *) malloc(YAHOO_PACKET_HEADER_SIZE +
		contentlen);
	memcpy(retpkt, buffer, YAHOO_PACKET_HEADER_SIZE + contentlen);

	/* Shift the buffer */
	memmove(buffer, buffer + YAHOO_PACKET_HEADER_SIZE + contentlen,
		*buflen - YAHOO_PACKET_HEADER_SIZE - contentlen);

	/* Adjust the buffer length */
	*buflen -= (YAHOO_PACKET_HEADER_SIZE + contentlen);

	/* Return the packet */
	return retpkt;
}

int yahoo_isbuddy(struct yahoo_context *ctx, const char *id)
{
	int i;
	char *buddy = NULL;

	if (!id || !ctx || !ctx->buddies)
	{
		return FALSE;
	}

	for (i = 0; ctx->buddies[i]; i++)
	{
		buddy = (ctx->buddies[i])->id;
		if (!strcasecmp(id, buddy))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static void yahoo_free_address (struct yahoo_address *add)
{
	yahoo_dbg_Print("addressbook",
		"[libyahoo] yahoo_free_address: record at address 0x%08p for user %s (%s %s) being free'd\n",
		add, add->id, add->firstname, add->lastname);

	FREE (add->firstname);
	FREE (add->lastname);
	FREE (add->emailnickname);
	FREE (add->email);
	FREE (add->workphone);
	FREE (add->homephone);
}

void yahoo_freeaddressbook(struct yahoo_context *ctx)
{
	unsigned int count = ctx->address_count;
	struct yahoo_address *add_p = ctx->addresses;

	if (NULL == ctx || NULL == ctx->addresses)
		return;

	while (count-- > 0)
	{
		yahoo_free_address (add_p++);
	}

	ctx->address_count = 0;
	FREE (ctx->addresses);
}

static void yahoo_data_to_addressbook (char *block, struct yahoo_context *ctx)
{
	char *token = NULL;
	int record = 0;
	struct yahoo_address *add = NULL;

	if (NULL == block || NULL == ctx)
		return;

	yahoo_freeaddressbook (ctx);

	add = ctx->addresses = calloc (ctx->address_count, sizeof (struct yahoo_address));

	/*
	 Okay!
	 At this point we have a char * (block) that has \012 delimited records
	 Each record (as a string when retreived with strtok) follows the format:
	<ID>:<FIRSTNAME>\011<LASTNAME>\011<EMAILNICKNAME>\011<EMAIL>\011<HOMEPHONE>\011<WORKPHONE>\011[01]\011<ENTRYID>\000
	 */

	token = strtok (block, "\012");
	while (NULL != token)
	{
		/*
		 Here we must use memtok because we'll get some repeated tokens!!!!!
		 */
		char *field = NULL;
		size_t token_len = 0, found = 0;

		++record;
		token_len = strlen (token);

		field = memtok(token, token_len, ":", 1, &found);

		if (NULL != field)
		{
			add->id = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->firstname = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->lastname = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->emailnickname = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->email = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->homephone = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->workphone = memdupasstr(field, found);
			field = memtok(0, 0, "\011", 1, &found);
		}

		if (NULL != field)
		{
			add->primary_phone = (*field == '0' ? home : work);
			field = memtok(0, 0, "", 1, &found);
		}

		if (NULL != field)
		{
			char *entryid = memdupasstr(field, found);
			if (NULL != entryid)
			{
				add->entryid = atoi (entryid);
				FREE (entryid);
			}
		}

		yahoo_dbg_Print("addressbook",
			"[libyahoo] yahoo_fetchaddressbook: record #%d is for user %s (%s %s)\n",
			record, add->id, add->firstname, add->lastname);

		++add;

		token = strtok (NULL, "\012");
	}
}

/* retreive the details of the friends in your address book that have a Yahoo! id listed */
int yahoo_fetchaddressbook(struct yahoo_context *ctx)
{
	char buffer[5000];
	int servfd;
	int res;
	int copied = 0, size = 5000;
	char *address = NULL, *copy = NULL;

	if (!ctx)
	{
		return 0;
	}

	yahoo_dbg_Print("addressbook",
		"[libyahoo] yahoo_fetchaddressbook: starting\n");

	/* Check for cached addresses */
	if (ctx->addresses)
	{
		yahoo_freeaddressbook(ctx);
	}

	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		servfd = yahoo_socket_connect(ctx, ctx->proxy_host, ctx->proxy_port);
	}
	else
	{
		servfd = yahoo_socket_connect(ctx, YAHOO_ADDRESS_HOST, YAHOO_ADDRESS_PORT);
	}

	if (servfd < 0)
	{
		printf("[libyahoo] failed to connect to address book server.\n");
		return (0);
	}

	strcpy(buffer, "GET ");
	if (ctx->connect_mode == YAHOO_CONNECT_HTTPPROXY)
	{
		strcat(buffer, YAHOO_ADDRESS_HOST);
	}
	strcat(buffer, "/yab/uk/yab?v=PG&A=s");
	strcat(buffer, " HTTP/1.0\r\n");
	strcat(buffer, "User-Agent: " YAHOO_USER_AGENT "\r\n");
	strcat(buffer, "Host: " YAHOO_AUTH_HOST "\r\n");
	strcat(buffer, "Cookie: ");
	strcat(buffer, ctx->cookie);
	strcat(buffer, "\r\n");
	strcat(buffer, "\r\n");

	if (writeall(servfd, buffer, strlen(buffer)) < strlen(buffer)) {
                close(servfd);
                return 0;
        }

	yahoo_dbg_Print("addressbook",
		"[libyahoo] yahoo_fetchaddressbook: writing buffer '%s'\n", buffer);

	while ((res = yahoo_tcp_readline(buffer, 5000, servfd)) > 0)
	{
		if ('\012' == buffer[0])
			continue;

		if (0 == strncmp (buffer, "1\011", 2))
		{
			yahoo_dbg_Print("addressbook",
				"[libyahoo] yahoo_fetchaddressbook: found first line\n");
			if (3 == res)
			{
				yahoo_dbg_Print("addressbook",
					"[libyahoo] yahoo_fetchaddressbook: however there's been a problem\n");
				break;
			}

			address = &buffer[2];
		}
		else if (NULL != address)
		{
			address = &buffer[0];
		}

		if (NULL != address)
		{
			if (NULL == copy)
			{
				copy = malloc (size);
				memset (copy, 0, size);
			}

			if ((copied + res) > size)
			{
				char *newcopy = NULL;

				yahoo_dbg_Print("addressbook",
					"[libyahoo] yahoo_fetchaddressbook: resizing buffer from %d bytes to %d bytes\n", size, size * 2);
				size *= 2;
				newcopy = malloc (size);
				memset (newcopy, 0, size);
				memcpy (newcopy, copy, copied);
				free (copy);
				copy = newcopy;
			}

			copied += res;
			strcat (copy, address);
			++ctx->address_count;
		}
	}

	yahoo_data_to_addressbook (copy, ctx);
	FREE (copy);

	yahoo_dbg_Print("addressbook",
		"[libyahoo] yahoo_fetchaddressbook: closing server connection\n");
	close(servfd);
	servfd = 0;
	yahoo_dbg_Print("addressbook",
		"[libyahoo] yahoo_fetchaddressbook: closed server connection\n");

	yahoo_dbg_Print("addressbook", "[libyahoo] yahoo_fetchaddressbook: done (%d addresses retreived)\n", ctx->address_count);

	return ctx->address_count;
}
