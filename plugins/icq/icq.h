/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _ICQ_H_
#define _ICQ_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define ICQLIBVER   0x010100
#define ICQLIBMAJOR 1
#define ICQLIBMINOR 1
#define ICQLIBMICRO 0

#include <time.h>

#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#endif

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

#define ICQ_NOTIFY_CHAT           6
#define ICQ_NOTIFY_CHATDATA       7

#define ICQ_NOTIFY_FILE           10
#define ICQ_NOTIFY_FILESESSION    11
#define ICQ_NOTIFY_FILEDATA       12

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct
{
  const char *name;
  unsigned short code;
} icq_ArrayType;

struct icq_link_private;

/**
 * The ICQLINK structure represents a single connection to the ICQ  servers.
 * It is returned as the result of an icq_ICQLINKNew function, and contains
 * connection-specific parameters such as uin, sockets, current status, etc.
 *
 * This structure should be considered read-only.  Modifying it will cause
 * undefined results.
 */
typedef struct icq_link
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
   * @see icq_NewICQLINK */
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
  int  icq_ProxyAuth;
  
  /** Username used when logging into the proxy. */
  char *icq_ProxyName;
  
  /** Password used when logging into the proxy. */
  char *icq_ProxyPass;
  
  /** TCP socket used to communicate with the proxy. */
  int icq_ProxySok;
    
  unsigned short icq_ProxyOurPort;  /* HOST byteorder */
  unsigned long icq_ProxyDestIP;    /* HOST byteorder */
  unsigned short icq_ProxyDestPort; /* HOST byteorder */

  /*** Callbacks ***/
  void (*icq_Logged)(struct icq_link *link);
  void (*icq_Disconnected)(struct icq_link *link);
  void (*icq_RecvMessage)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *msg);
  void (*icq_RecvURL)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *url,
       const char *descr);
  void (*icq_RecvWebPager)(struct icq_link *link,unsigned char hour,
       unsigned char minute, unsigned char day, unsigned char month,
       unsigned short year, const char *nick, const char *email,
       const char *msg);
  void (*icq_RecvMailExpress)(struct icq_link *link,unsigned char hour,
       unsigned char minute, unsigned char day, unsigned char month,
       unsigned short year, const char *nick, const char *email,
       const char *msg);
  void (*icq_RecvChatReq)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *descr,
       unsigned long seq);
  void (*icq_RecvFileReq)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *descr,
       const char *filename, unsigned long filesize, unsigned long seq);
  void (*icq_RecvAdded)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *nick,
       const char *first, const char *last, const char *email);
  void (*icq_RecvAuthReq)(struct icq_link *link, unsigned long uin,
       unsigned char hour, unsigned char minute, unsigned char day,
       unsigned char month, unsigned short year, const char *nick,
       const char *first, const char *last, const char *email,
       const char *reason);
  void (*icq_UserFound)(struct icq_link *link, unsigned long uin,
       const char *nick, const char *first, const char *last,
       const char *email, char auth);
  void (*icq_SearchDone)(struct icq_link *link);
  void (*icq_UserOnline)(struct icq_link *link, unsigned long uin,
       unsigned long status, unsigned long ip, unsigned short port,
       unsigned long real_ip, unsigned char tcp_flag );
  void (*icq_UserOffline)(struct icq_link *link, unsigned long uin);
  void (*icq_UserStatusUpdate)(struct icq_link *link, unsigned long uin,
       unsigned long status);
  void (*icq_InfoReply)(struct icq_link *link, unsigned long uin,
       const char *nick, const char *first, const char *last,
       const char *email, char auth);
  void (*icq_ExtInfoReply)(struct icq_link *link, unsigned long uin,
       const char *city, unsigned short country_code, char country_stat,
       const char *state, unsigned short age, char gender,
       const char *phone, const char *hp, const char *about);
  void (*icq_WrongPassword)(struct icq_link *link);
  void (*icq_InvalidUIN)(struct icq_link *link);
  void (*icq_Log)(struct icq_link *link, time_t time, unsigned char level,
       const char *str);
  void (*icq_SrvAck)(struct icq_link *link, unsigned short seq);
  void (*icq_RequestNotify)(struct icq_link *link, unsigned long id, int result,
       unsigned int length, void *data);
  void (*icq_NewUIN)(struct icq_link *link, unsigned long uin);
  void (*icq_SetTimeout)(struct icq_link *link, long interval);
  void (*icq_MetaUserFound)(struct icq_link *link, unsigned short seq2,
       unsigned long uin, const char *nick, const char *first,
       const char *last, const char *email, char auth);
  void (*icq_MetaUserInfo)(struct icq_link *link, unsigned short seq2,
       const char *nick, const char *first, const char *last,
       const char *pri_eml, const char *sec_eml, const char *old_eml,
       const char *city, const char *state, const char *phone, const char *fax,
       const char *street, const char *cellular, unsigned long zip,
       unsigned short country, unsigned char timezone, unsigned char auth,
       unsigned char webaware, unsigned char hideip);
  void (*icq_MetaUserWork)(struct icq_link *link, unsigned short seq2,
       const char *wcity, const char *wstate, const char *wphone,
       const char *wfax, const char *waddress, unsigned long wzip,
       unsigned short wcountry, const char *company, const char *department,
       const char *job, unsigned short occupation, const char *whomepage);
  void (*icq_MetaUserMore)(struct icq_link *link, unsigned short seq2,
       unsigned short age, unsigned char gender, const char *homepage,
       unsigned char byear, unsigned char bmonth, unsigned char bday,
       unsigned char lang1, unsigned char lang2, unsigned char lang3);
  void (*icq_MetaUserAbout)(struct icq_link *link, unsigned short seq2,
       const char *about);
  void (*icq_MetaUserInterests)(struct icq_link *link, unsigned short seq2,
       unsigned char num, unsigned short icat1, const char *int1,
       unsigned short icat2, const char *int2, unsigned short icat3,
       const char *int3, unsigned short icat4, const char *int4);
  void (*icq_MetaUserAffiliations)(struct icq_link *link, unsigned short seq2,
       unsigned char anum, unsigned short acat1, const char *aff1,
       unsigned short acat2, const char *aff2, unsigned short acat3,
       const char *aff3, unsigned short acat4, const char *aff4,
       unsigned char bnum, unsigned short bcat1, const char *back1,
       unsigned short bcat2, const char *back2, unsigned short bcat3,
       const char *back3, unsigned short bcat4, const char *back4);
  void (*icq_MetaUserHomePageCategory)(struct icq_link *link,
       unsigned short seq2, unsigned char num, unsigned short hcat1,
       const char *htext1);
       
  /** Private data pointer. */
  struct icq_link_private *d;
  
} ICQLINK;

extern int icq_Russian;
extern unsigned char icq_LogLevel;
extern icq_ArrayType icq_Countries[];
extern icq_ArrayType icq_Genders[];

void icq_SetProxy(ICQLINK *link, const char *phost, unsigned short pport,
     int pauth, const char *pname, const char *ppass);
void icq_UnsetProxy(ICQLINK *link);

ICQLINK *icq_ICQLINKNew(unsigned long uin, const char *password,
  const char *nick, unsigned char useTCP);
void icq_ICQLINKDelete(ICQLINK *link);
  
void icq_Init(ICQLINK *link, unsigned long uin, const char *password,
     const char *nick, unsigned char useTCP);
void icq_Done(ICQLINK *link);
int icq_Connect(ICQLINK *link, const char *hostname, int port);
void icq_Disconnect(ICQLINK *link);
int icq_GetSok(ICQLINK *link);
int icq_GetProxySok(ICQLINK *link);
void icq_HandleServerResponse(ICQLINK *link);
void icq_HandleProxyResponse(ICQLINK *link);
void icq_HandleTimeout(ICQLINK *link);
void icq_Main(ICQLINK *link);
unsigned short icq_KeepAlive(ICQLINK *link);
void icq_Login(ICQLINK *link, unsigned long status);
void icq_Logout(ICQLINK *link);
void icq_SendContactList(ICQLINK *link);
void icq_SendVisibleList(ICQLINK *link);
void icq_SendNewUser(ICQLINK * link, unsigned long uin);
unsigned long icq_SendMessage(ICQLINK *link, unsigned long uin,
     const char *text, unsigned char thruSrv);
unsigned long icq_SendURL(ICQLINK *link, unsigned long uin, const char *url,
     const char *descr, unsigned char thruSrv);
void icq_ChangeStatus(ICQLINK *link, unsigned long status);
unsigned short icq_SendInfoReq(ICQLINK *link, unsigned long uin);
unsigned short icq_SendExtInfoReq(ICQLINK *link, unsigned long uin);
unsigned short icq_SendAuthMsg(ICQLINK *link, unsigned long uin);
void icq_SendSearchReq(ICQLINK *link, const char *email, const char *nick,
     const char* first, const char* last);
void icq_SendSearchUINReq(ICQLINK *link, unsigned long uin);

const char *icq_GetCountryName(unsigned short code);
const char *icq_GetMetaOccupationName(unsigned short code);
const char *icq_GetMetaBackgroundName(unsigned short code);
const char *icq_GetMetaAffiliationName(unsigned short code);
const char *icq_GetMetaLanguageName(unsigned short code);
void icq_RegNewUser(ICQLINK *link, const char *pass);
unsigned short icq_UpdateUserInfo(ICQLINK *link, const char *nick,
     const char *first, const char *last, const char *email);
unsigned short icq_UpdateAuthInfo(ICQLINK *link, unsigned long auth);
unsigned short icq_UpdateMetaInfoSet(ICQLINK *link, const char *nick,
     const char *first, const char *last, const char *email,
     const char *email2, const char *email3, const char *city,
     const char *state, const char *phone, const char *fax, const char *street,
     const char *cellular, unsigned long zip, unsigned short cnt_code,
     unsigned char cnt_stat, unsigned char emailhide);
unsigned short icq_UpdateMetaInfoHomepage(ICQLINK *link, unsigned char age,
     const char *homepage, unsigned char year, unsigned char month,
     unsigned char day, unsigned char lang1, unsigned char lang2,
     unsigned char lang3);
unsigned short icq_UpdateMetaInfoAbout(ICQLINK *link, const char *about);
unsigned short icq_UpdateMetaInfoSecurity(ICQLINK *link, unsigned char reqauth,
     unsigned char webpresence, unsigned char pubip);
unsigned short icq_UpdateNewUserInfo(ICQLINK *link, const char *nick,
     const char *first, const char *last, const char *email);
unsigned short icq_SendMetaInfoReq(ICQLINK *link, unsigned long uin);

void icq_FmtLog(ICQLINK *link, int level, const char *fmt, ...);

void icq_ContactAdd(ICQLINK *link, unsigned long cuin);
void icq_ContactRemove(ICQLINK *link, unsigned long cuin);
void icq_ContactClear(ICQLINK *link );
void icq_ContactSetVis(ICQLINK *link, unsigned long cuin, unsigned char vu);

/*** TCP ***/
void icq_TCPMain(ICQLINK *link);

void icq_TCPProcessReceived(ICQLINK *link);

unsigned long icq_TCPSendMessage(ICQLINK *link, unsigned long uin,
     const char *message);
unsigned long icq_TCPSendURL(ICQLINK *link, unsigned long uin,
     const char *message, const char *url);
unsigned long icq_SendChatRequest(ICQLINK *link, unsigned long uin,
     const char *message);
void icq_AcceptChatRequest(ICQLINK *link, unsigned long uin, unsigned long seq);

void icq_TCPSendChatData(ICQLINK *link, unsigned long uin, const char *data);
void icq_TCPSendChatData_n(ICQLINK *link, unsigned long uin, const char *data, int len);
void icq_TCPCloseChat(ICQLINK *link, unsigned long uin);

void icq_CancelChatRequest(ICQLINK *link, unsigned long uin, 
     unsigned long sequence);
void icq_RefuseChatRequest(ICQLINK *link, unsigned long uin,
     unsigned long sequence, const char *reason);

/*** TCP ***/

/* file session statuses- request receiver */
#define FILE_STATUS_LISTENING    1
#define FILE_STATUS_CONNECTED    3

/* file session statuses- request sender */
#define FILE_STATUS_CONNECTING   2

#define FILE_STATUS_INITIALIZED  4
#define FILE_STATUS_NEXT_FILE    5
#define FILE_STATUS_STOP_FILE    6
#define FILE_STATUS_NEW_SPEED    7

/* once negotiation is complete, file session enters proper state */
#define FILE_STATUS_READY        8
#define FILE_STATUS_SENDING      8
#define FILE_STATUS_RECEIVING    9  

struct icq_TCPLink_s;

typedef struct icq_FileSession_s {

  unsigned long id;
  int status;
  ICQLINK *icqlink;
  struct icq_TCPLink_s *tcplink;

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

} icq_FileSession;
          
icq_FileSession *icq_AcceptFileRequest(ICQLINK *link, unsigned long uin,
                 unsigned long sequence);
unsigned long icq_SendFileRequest(ICQLINK *link, unsigned long uin,
              const char *message, char **files);
void icq_CancelFileRequest(ICQLINK *link, unsigned long uin, 
     unsigned long sequence);
void icq_RefuseFileRequest(ICQLINK *link, unsigned long uin,
     unsigned long sequence, const char *reason);

void icq_FileSessionSetSpeed(icq_FileSession *p, int speed);
void icq_FileSessionClose(icq_FileSession *p);
void icq_FileSessionSetWorkingDir(icq_FileSession *p, const char *dir);
void icq_FileSessionSetFiles(icq_FileSession *p, char **files);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ICQ_H_ */
