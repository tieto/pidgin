/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Copyright (C) 1998-2001, Denis V. Dmitrienko <denis@null.net> and
 *                          Bill Soudan <soudan@kde.org>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _ICQ_H_
#define _ICQ_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <winsock.h>
#endif /* _WIN32 */

#ifdef __BEOS__
#include <socket.h>
#endif

#include <time.h>

/* ICQLIB version defines */
#define ICQLIBVER   0x010105
#define ICQLIBMAJOR 1
#define ICQLIBMINOR 1
#define ICQLIBMICRO 5


#define ICQ_LOG_OFF     0
#define ICQ_LOG_FATAL   1
#define ICQ_LOG_ERROR   2
#define ICQ_LOG_WARNING 3
#define ICQ_LOG_MESSAGE 4

#define STATUS_OFFLINE     (-1L)
#define STATUS_ONLINE      0x0000L
#define STATUS_AWAY        0x0001L
#define STATUS_DND         0x0002L /* 0x13L */
#define STATUS_NA          0x0004L /* 0x05L */
#define STATUS_OCCUPIED    0x0010L /* 0x11L */
#define STATUS_FREE_CHAT   0x0020L
#define STATUS_INVISIBLE   0x0100L

#define ICQ_SEND_THRUSERVER       0
#define ICQ_SEND_DIRECT           1
#define ICQ_SEND_BESTWAY          2

#define ICQ_NOTIFY_SUCCESS        0
#define ICQ_NOTIFY_FAILED         1
#define ICQ_NOTIFY_CONNECTING     2
#define ICQ_NOTIFY_CONNECTED      3
#define ICQ_NOTIFY_SENT           4
#define ICQ_NOTIFY_ACK            5

#define ICQ_NOTIFY_CHATSESSION    7
#define ICQ_NOTIFY_FILESESSION    8

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct
{
  const char *name;
  unsigned short code;
} icq_ArrayType;

/* dummy forward declarations */
typedef struct icq_LinkPrivate_s icq_LinkPrivate;
typedef struct icq_TCPLink_s icq_TCPLink;
typedef struct icq_FileSession_s icq_FileSession;
typedef struct icq_ChatSession_s icq_ChatSession;
typedef struct icq_Link_s icq_Link;

/* Legacy compatibility - remove for icqlib 2.0.0 */
typedef struct icq_Link_s ICQLINK;
#define icq_ICQLINKNew icq_LinkNew
#define icq_ICQLINKDelete icq_LinkDelete

/**
 * The icq_Link structure represents a single connection to the ICQ servers.
 * It is returned as the result of an icq_ICQLINKNew function, and contains
 * connection-specific parameters such as uin, sockets, current status, etc.
 *
 * This structure should be considered read-only.  Modifying it will cause
 * undefined results.
 */
struct icq_Link_s
{
  /* General parameters */
  
  /** User Identification Number.  This is your ICQ 'account' number. */
  unsigned long icq_Uin;
  
  /** Our IP as understood by the ICQ server.  This will be set once an
   * UDP_SRV_LOGIN_REPLY has been received from the ICQ servers, in host
   * byteorder.  Note this may be different from the actual IP in cases
   * such as firewalls, ip masquerading, etc. */
  unsigned long icq_OurIP; 
  
  /** The UDP port used to connect to the ICQ server, in host byteorder.  */
  unsigned short icq_OurPort;

  /** Our current ICQ status: one of the STATUS_* defines.
   * @see icq_StatusUpdate */
  unsigned long icq_Status;
  
  /** The password used to log into the ICQ server. */  
  char *icq_Password;
  
  /** The user's desired nickname. */
  char *icq_Nick;

  /* UDP stuff */

  /** socket used to send and received UDP messages */
  int icq_UDPSok;

  /** Time, in seconds, that a sent UDP message can go without an ACK from the
   * server before being retransmitted. */
  int icq_UDPExpireInterval;

  /* TCP stuff */

  /** TCP listen port, in host byte order.  The TCP implementation will listen
   * here for new connections from other clients.  This is transmitted as
   * part of the ICQ login process. */
  unsigned short icq_TCPSrvPort;
  
  /** Has TCP been enabled for this connection?
   * @see icq_Newicq_Link */
  unsigned char icq_UseTCP;
  
  /* SOCKS5 Proxy stuff */
  
  /** Should all network traffic be redirected through a proxy? 
   * @see icq_SetProxy */
  unsigned char icq_UseProxy;
  
  /** Hostname of the SOCKS5 proxy to use. */
  char *icq_ProxyHost;
  
  /** IP Address of the SOCKS5 proxy after DNS resolution, in host byteorder. */
  unsigned long icq_ProxyIP;
  
  /** Port of the SOCKS5 proxy to use, in host byteorder. */
  unsigned short icq_ProxyPort;
  
  /** What's this? :) */
  int icq_ProxyAuth;
  
  /** Username used when logging into the proxy. */
  char *icq_ProxyName;
  
  /** Password used when logging into the proxy. */
  char *icq_ProxyPass;
  
  /** TCP socket used to communicate with the proxy. */
  int icq_ProxySok;
    
  unsigned short icq_ProxyOurPort;  /* HOST byteorder */
  unsigned long icq_ProxyDestIP;    /* HOST byteorder */
  unsigned short icq_ProxyDestPort; /* HOST byteorder */

  /* Begin Callbacks */
  void (*icq_Logged)(icq_Link *icqlink);
  void (*icq_Disconnected)(icq_Link *icqlink);
  void (*icq_RecvMessage)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *msg);
  void (*icq_RecvURL)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *url,
       const char *descr);
  void (*icq_RecvContactList)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, int nr,
       const char **contact_uin, const char **contact_nick);
  void (*icq_RecvWebPager)(icq_Link *icqlink,unsigned char hour,
       unsigned char minute, unsigned char day, unsigned char month,
       unsigned short year, const char *nick, const char *email,
       const char *msg);
  void (*icq_RecvMailExpress)(icq_Link *icqlink,unsigned char hour,
       unsigned char minute, unsigned char day, unsigned char month,
       unsigned short year, const char *nick, const char *email,
       const char *msg);
  void (*icq_RecvChatReq)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *descr,
       unsigned long seq);
  void (*icq_RecvFileReq)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *descr,
       const char *filename, unsigned long filesize, unsigned long seq);
  void (*icq_RecvAdded)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *nick,
       const char *first, const char *last, const char *email);
  void (*icq_RecvAuthReq)(icq_Link *icqlink, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *nick,
       const char *first, const char *last, const char *email,
       const char *reason);
  void (*icq_UserFound)(icq_Link *icqlink, unsigned long uin,
       const char *nick, const char *first, const char *last,
       const char *email, char auth);
  void (*icq_SearchDone)(icq_Link *icqlink);
  void (*icq_UserOnline)(icq_Link *icqlink, unsigned long uin,
       unsigned long status, unsigned long ip, unsigned short port,
       unsigned long real_ip, unsigned char tcp_flag );
  void (*icq_UserOffline)(icq_Link *icqlink, unsigned long uin);
  void (*icq_UserStatusUpdate)(icq_Link *icqlink, unsigned long uin,
       unsigned long status);
  void (*icq_InfoReply)(icq_Link *icqlink, unsigned long uin,
       const char *nick, const char *first, const char *last,
       const char *email, char auth);
  void (*icq_ExtInfoReply)(icq_Link *icqlink, unsigned long uin,
       const char *city, unsigned short country_code, char country_stat,
       const char *state, unsigned short age, char gender,
       const char *phone, const char *hp, const char *about);
  void (*icq_WrongPassword)(icq_Link *icqlink);
  void (*icq_InvalidUIN)(icq_Link *icqlink);
  void (*icq_Log)(icq_Link *icqlink, time_t log_time, unsigned char level,
       const char *str);
  void (*icq_SrvAck)(icq_Link *icqlink, unsigned short seq);
  void (*icq_RequestNotify)(icq_Link *icqlink, unsigned long id, 
       int type, int arg, void *data);
  void (*icq_FileNotify)(icq_FileSession *session, int type, int arg,
       void *data);
  void (*icq_ChatNotify)(icq_ChatSession *session, int type, int arg,
       void *data);
  void (*icq_NewUIN)(icq_Link *icqlink, unsigned long uin);
  void (*icq_MetaUserFound)(icq_Link *icqlink, unsigned short seq2,
       unsigned long uin, const char *nick, const char *first,
       const char *last, const char *email, char auth);
  void (*icq_MetaUserInfo)(icq_Link *icqlink, unsigned short seq2,
       const char *nick, const char *first, const char *last,
       const char *pri_eml, const char *sec_eml, const char *old_eml,
       const char *city, const char *state, const char *phone, const char *fax,
       const char *street, const char *cellular, unsigned long zip,
       unsigned short country, unsigned char timezone, unsigned char auth,
       unsigned char webaware, unsigned char hideip);
  void (*icq_MetaUserWork)(icq_Link *icqlink, unsigned short seq2,
       const char *wcity, const char *wstate, const char *wphone,
       const char *wfax, const char *waddress, unsigned long wzip,
       unsigned short wcountry, const char *company, const char *department,
       const char *job, unsigned short occupation, const char *whomepage);
  void (*icq_MetaUserMore)(icq_Link *icqlink, unsigned short seq2,
       unsigned short age, unsigned char gender, const char *homepage,
       unsigned char byear, unsigned char bmonth, unsigned char bday,
       unsigned char lang1, unsigned char lang2, unsigned char lang3);
  void (*icq_MetaUserAbout)(icq_Link *icqlink, unsigned short seq2,
       const char *about);
  void (*icq_MetaUserInterests)(icq_Link *icqlink, unsigned short seq2,
       unsigned char num, unsigned short icat1, const char *int1,
       unsigned short icat2, const char *int2, unsigned short icat3,
       const char *int3, unsigned short icat4, const char *int4);
  void (*icq_MetaUserAffiliations)(icq_Link *icqlink, unsigned short seq2,
       unsigned char anum, unsigned short acat1, const char *aff1,
       unsigned short acat2, const char *aff2, unsigned short acat3,
       const char *aff3, unsigned short acat4, const char *aff4,
       unsigned char bnum, unsigned short bcat1, const char *back1,
       unsigned short bcat2, const char *back2, unsigned short bcat3,
       const char *back3, unsigned short bcat4, const char *back4);
  void (*icq_MetaUserHomePageCategory)(icq_Link *icqlink,
       unsigned short seq2, unsigned char num, unsigned short hcat1,
       const char *htext1);
  /* End Callbacks */
       
  /** Private data pointer. */
  icq_LinkPrivate *d;
  
  /** Space for user data */
  void *icq_UserData;
};

extern int icq_Russian;
extern unsigned char icq_LogLevel;
extern icq_ArrayType icq_Countries[];
extern icq_ArrayType icq_Genders[];

icq_Link *icq_LinkNew(unsigned long uin, const char *password,
  const char *nick, unsigned char useTCP);
void icq_LinkInit(icq_Link *icqlink, unsigned long uin, const char *password,
  const char *nick, unsigned char useTCP);
void icq_LinkDestroy(icq_Link *icqlink);
void icq_LinkDelete(icq_Link *icqlink);

void icq_Main(void);
const char *icq_GetCountryName(unsigned short code);
const char *icq_GetMetaOccupationName(unsigned short code);
const char *icq_GetMetaBackgroundName(unsigned short code);
const char *icq_GetMetaAffiliationName(unsigned short code);
const char *icq_GetMetaLanguageName(unsigned short code);

/* Begin icq_Link methods */  
void icq_SetProxy(icq_Link *icqlink, const char *phost, unsigned short pport,
     int pauth, const char *pname, const char *ppass);
void icq_UnsetProxy(icq_Link *icqlink);

int icq_Connect(icq_Link *icqlink, const char *hostname, int port);
void icq_Disconnect(icq_Link *icqlink);
int icq_GetSok(icq_Link *icqlink);
int icq_GetProxySok(icq_Link *icqlink);
void icq_HandleServerResponse(icq_Link *icqlink);
void icq_HandleProxyResponse(icq_Link *icqlink);
unsigned short icq_KeepAlive(icq_Link *icqlink);
void icq_Login(icq_Link *icqlink, unsigned long status);
void icq_Logout(icq_Link *icqlink);
void icq_SendContactList(icq_Link *icqlink);
void icq_SendVisibleList(icq_Link *icqlink);
void icq_SendInvisibleList(icq_Link *icqlink);
void icq_SendNewUser(icq_Link *icqlink, unsigned long uin);
unsigned long icq_SendMessage(icq_Link *icqlink, unsigned long uin,
     const char *text, unsigned char thruSrv);
unsigned long icq_SendURL(icq_Link *icqlink, unsigned long uin, const char *url,
     const char *descr, unsigned char thruSrv);
void icq_ChangeStatus(icq_Link *icqlink, unsigned long status);
unsigned short icq_SendInfoReq(icq_Link *icqlink, unsigned long uin);
unsigned short icq_SendExtInfoReq(icq_Link *icqlink, unsigned long uin);
unsigned short icq_SendAuthMsg(icq_Link *icqlink, unsigned long uin);
void icq_SendSearchReq(icq_Link *icqlink, const char *email, const char *nick,
     const char* first, const char* last);
void icq_SendSearchUINReq(icq_Link *icqlink, unsigned long uin);

void icq_RegNewUser(icq_Link *icqlink, const char *pass);
unsigned short icq_UpdateUserInfo(icq_Link *icqlink, const char *nick,
     const char *first, const char *last, const char *email);
unsigned short icq_UpdateAuthInfo(icq_Link *icqlink, unsigned long auth);
unsigned short icq_UpdateMetaInfoSet(icq_Link *icqlink, const char *nick,
     const char *first, const char *last, const char *email,
     const char *email2, const char *email3, const char *city,
     const char *state, const char *phone, const char *fax, const char *street,
     const char *cellular, unsigned long zip, unsigned short cnt_code,
     unsigned char cnt_stat, unsigned char emailhide);
unsigned short icq_UpdateMetaInfoHomepage(icq_Link *icqlink, unsigned char age,
     const char *homepage, unsigned char year, unsigned char month,
     unsigned char day, unsigned char lang1, unsigned char lang2,
     unsigned char lang3);
unsigned short icq_UpdateMetaInfoAbout(icq_Link *icqlink, const char *about);
unsigned short icq_UpdateMetaInfoSecurity(icq_Link *icqlink, unsigned char reqauth,
     unsigned char webpresence, unsigned char pubip);
unsigned short icq_UpdateNewUserInfo(icq_Link *icqlink, const char *nick,
     const char *first, const char *last, const char *email);
unsigned short icq_SendMetaInfoReq(icq_Link *icqlink, unsigned long uin);

void icq_FmtLog(icq_Link *icqlink, int level, const char *fmt, ...);

void icq_ContactAdd(icq_Link *icqlink, unsigned long cuin);
void icq_ContactRemove(icq_Link *icqlink, unsigned long cuin);
void icq_ContactClear(icq_Link *icqlink );
void icq_ContactSetVis(icq_Link *icqlink, unsigned long cuin, unsigned char vu);
void icq_ContactSetInvis(icq_Link *icqlink, unsigned long cuin, unsigned char vu);

/*** TCP ***/
void icq_TCPMain(icq_Link *icqlink);

void icq_TCPProcessReceived(icq_Link *icqlink);

unsigned long icq_TCPSendMessage(icq_Link *icqlink, unsigned long uin,
     const char *message);
unsigned long icq_TCPSendURL(icq_Link *icqlink, unsigned long uin,
     const char *message, const char *url);
unsigned long icq_SendChatRequest(icq_Link *icqlink, unsigned long uin,
     const char *message);
void icq_AcceptChatRequest(icq_Link *icqlink, unsigned long uin, unsigned long seq);

void icq_CancelChatRequest(icq_Link *icqlink, unsigned long uin, 
     unsigned long sequence);
void icq_RefuseChatRequest(icq_Link *icqlink, unsigned long uin,
     unsigned long sequence, const char *reason);

/* End icq_Link Methods */

/*** TCP ***/

/** \defgroup ChatSession Chat Session Documentation
 * icqlib's 'Chat Session' abstraction represents ICQ's 'chat' function
 * between two participants.  Multi-party chat is not yet supported.
 *
 * An icq_ChatSession is instantiated when a 'Chat Request' event is
 * accepted.  Upon receipt of a 'Chat Accept' event or a call to
 * icq_AcceptChatRequest, icqlib will create a new chat session and pass the
 * new chat session pointer back to the library client through the
 * icq_RequestNotify / ICQ_NOTIFY_CHATSESSION callback.  This pointer should
 * be stored by the library client, as multiple chat sessions may be in
 * progress at any given time.  The icq_ChatSession pointer is used as a key
 * for all future communication between the library and the library client to
 * indicate which icq_ChatSession is currently being dealt with.
 *
 * icqlib communicates chat session events through use of the icq_ChatNotify
 * callback, such as the CHAT_NOTIFY_DATA event.  The library client
 * can perform operations on a chat session by use of the icq_ChatSession*
 * functions, such as sending data to the remote uin by using the
 * icq_ChatSessionSendData function.  
 *
 * A new chat session must first undergo an initialization sequence before is
 * ready to transmit and receive data.  As this initialization is in progress
 * the chat session will transition through various statuses depending on
 * whether icqlib sent the accept event or it received the accept event.
 * Each change in chat session status will be reported to the library
 * client through use of the icq_ChatNotify callback, with a @type parameter
 * of CHAT_NOTIFY_STATUS and an @a arg parameter of the status value.
 *
 * Once the chat session initialization is complete, both sides will enter
 * the CHAT_STATUS_READY state, indicating that the chat session is
 * ready to send and receive data.  Received data is reported through the
 * icq_ChatNotify callback, with a @type of CHAT_NOTIFY_DATA.  The library
 * client can send data using icq_ChatSessionSendData or
 * icq_ChatSessionSendData_n.
 *
 * Chat sessions may be terminated at any time, by either side.  The library
 * client may terminate a chat session by using icq_ChatSessionClose, or 
 * the remote uin may terminate a chat session.  In either instance, a
 * CHAT_STATUS_CLOSE event will be reported through the icq_ChatNotify
 * callback.  Once this callback is complete (e.g. your application's
 * callback handler returns), the icq_ChatSession will be deleted by icqlib 
 * and the session pointer becomes invalid.
 */

/** @name Type Constants
 * @ingroup ChatSession
 * These values are used as the @a type parameter in the icq_ChatNotify
 * callback to indicate the type of chat session event that has occured.
 * The remaining @a arg and @a data parameters passed by the callback
 * are specific to each event;  see the documentation for each type
 * constant.
 */

/*@{*/

/** Status has changed.
 * @param arg new session status - one of the CHAT_STATUS_* defines
 * @param data unused.
 * @ingroup ChatSession
 */
#define CHAT_NOTIFY_STATUS       1

/** Data has been received from a chat participant.
 * @param arg length of data received
 * @param data pointer to buffer containing received data
 * @ingroup ChatSession
 */
#define CHAT_NOTIFY_DATA         2

/** Session has been closed, either automatically by icqlib or
 * explicitly by a call to icq_ChatSessionClose.
 * @param arg unused
 * @param data unused
 * @ingroup ChatSession
 */ 
#define CHAT_NOTIFY_CLOSE        3

/*@}*/

/** @name Status Constants
 * @ingroup ChatSession
 * These constants are used as the @a arg parameter during in the
 * icq_ChatNotify/CHAT_NOTIFY_STATUS callback to indicate the
 * new status of the chat session.
 */

/*@{*/

/** icqlib is listening for a chat connection from the remote uin.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_LISTENING    1

/** A connection has been established with the remote uin.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_CONNECTED    3

/** icqlib is currently waiting for the remote uin to send the chat
 * initialization packet which contains the remote uin's chat handle.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_WAIT_NAME    4

/** icqlib is currently waiting for the remote uin to send the chat
 * initialization packet which contains the remote uin's font information.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_WAIT_FONT    6

/** A connection to the chat session port of the remote uin is in 
 * progress.
 * @ingroup ChatSession
 */
/* chat session statuses - request sender */
#define CHAT_STATUS_CONNECTING   2

/** icqlib is currently waiting for the remote uin to send the chat
 * initialization packet which contains the remote uin's chat handle
 * and font information.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_WAIT_ALLINFO 5

/** Chat session initialization has completed successfully.  The session
 * is now fully established - both sides can begin to send data and
 * should be prepared to accept data.
 * @ingroup ChatSession
 */
#define CHAT_STATUS_READY        7

/*@}*/

/** Chat Session state structure.  This structure is used internally by
 * icqlib to maintain state information about each chat session.  All
 * members should be considered read-only!  Use the appropriate 
 * icq_ChatSession* function to change the state of a chat session,
 * results are undefined if your application attempts to manipulate this
 * structure itself.
 */
struct icq_ChatSession_s {        

  /** For internal icqlib use only. */
  unsigned long id;
  
  /** Current status of the chat session.  See 'Status Constants' group. */
  int status;
  
  /** ICQLINK that spawned this chat session. */
  icq_Link *icqlink;
  
  /** For internal icqlib use only. */
  icq_TCPLink *tcplink;

  /** Remote uin number. */
  unsigned long remote_uin;
  
  /** Remote uin's chat handle. */
  char remote_handle[64];

  /** Space for user data */
  void *user_data; 
};

void icq_ChatSessionClose(icq_ChatSession *session);
void icq_ChatSessionSendData(icq_ChatSession *session, const char *data);
void icq_ChatSessionSendData_n(icq_ChatSession *session, const char *data,
  int length);


/* FileNotify constants */
#define FILE_NOTIFY_DATAPACKET   1
#define FILE_NOTIFY_STATUS       2
#define FILE_NOTIFY_CLOSE        3
#define FILE_NOTIFY_NEW_SPEED    4
#define FILE_NOTIFY_STOP_FILE    5

/* file session statuses- request receiver */
#define FILE_STATUS_LISTENING    1
#define FILE_STATUS_CONNECTED    3

/* file session statuses- request sender */
#define FILE_STATUS_CONNECTING   2

#define FILE_STATUS_INITIALIZING 4

#define FILE_STATUS_NEXT_FILE    5

/* once negotiation is complete, file session enters proper state */
#define FILE_STATUS_SENDING      6
#define FILE_STATUS_RECEIVING    7  

struct icq_FileSession_s {

  unsigned long id;
  int status;
  icq_Link *icqlink;
  icq_TCPLink *tcplink;

  int direction;

  unsigned long remote_uin;
  char remote_handle[64];

  char **files;
  int total_files;
  int current_file_num;
  unsigned long total_bytes;
  unsigned long total_transferred_bytes;

  char working_dir[512];
  char current_file[64];
  int current_fd;
  unsigned long current_file_size;
  unsigned long current_file_progress;

  int current_speed;

  /** Space for user data */
  void *user_data;
};
          
icq_FileSession *icq_AcceptFileRequest(icq_Link *icqlink, unsigned long uin,
                 unsigned long sequence);
unsigned long icq_SendFileRequest(icq_Link *icqlink, unsigned long uin,
              const char *message, char **files);
void icq_CancelFileRequest(icq_Link *icqlink, unsigned long uin, 
     unsigned long sequence);
void icq_RefuseFileRequest(icq_Link *icqlink, unsigned long uin,
     unsigned long sequence, const char *reason);

void icq_FileSessionSetSpeed(icq_FileSession *p, int speed);
void icq_FileSessionClose(icq_FileSession *p);
void icq_FileSessionSetWorkingDir(icq_FileSession *p, const char *dir);
void icq_FileSessionSetFiles(icq_FileSession *p, char **files);

/* Socket Manager */

#define ICQ_SOCKET_READ  0
#define ICQ_SOCKET_WRITE 1
#define ICQ_SOCKET_MAX   2

extern void (*icq_SocketNotify)(int socket_fd, int type, int status);

void icq_HandleReadySocket(int socket_fd, int type);

/* Timeout Manager */

extern void (*icq_SetTimeout)(long interval);

void icq_HandleTimeout(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ICQ_H_ */
