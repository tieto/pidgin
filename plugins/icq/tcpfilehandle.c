/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: tcpfilehandle.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.15  2000/07/24 03:10:08  bills
added support for real nickname during TCP transactions like file and
chat, instead of using Bill all the time (hmm, where'd I get that from? :)

Revision 1.14  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.13  2000/06/25 16:35:08  denis
'\n' was added at the end of log messages.

Revision 1.12  2000/06/15 01:52:59  bills
fixed bug: sending file sessions would freeze if remote side changed speed

Revision 1.11  2000/05/04 15:57:20  bills
Reworked file transfer notification, small bugfixes, and cleanups.

Revision 1.10  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.9  2000/04/10 18:11:45  denis
ANSI cleanups.

Revision 1.8  2000/04/10 16:36:04  denis
Some more Win32 compatibility from Guillaume Rosanis <grs@mail.com>

Revision 1.7  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.6  2000/01/20 20:06:00  bills
removed debugging printfs

Revision 1.5  2000/01/20 19:59:15  bills
first implementation of sending file requests

Revision 1.4  2000/01/16 21:29:31  bills
added code so icq_FileSessions now keep track of the tcplink to which
they are attached

Revision 1.3  1999/12/21 00:30:15  bills
added more file transfer logic to write file to disk

Revision 1.2  1999/11/30 09:47:04  bills
added icq_HandleFileHello

Revision 1.1  1999/09/29 19:47:21  bills
reworked chat/file handling.  fixed chat. (it's been broke since I put
non-blocking connects in)

*/

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _MSVC_
#include <io.h>
#define open _open
#define close _close
#define read _read
#define write _write
#endif

#include <errno.h>

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"

#include "tcp.h"
#include "icqpacket.h"
#include "stdpackets.h"
#include "tcplink.h"
#include "filesession.h"

void icq_TCPOnFileReqReceived(ICQLINK *link, DWORD uin, const char *message, 
   const char *filename, unsigned long filesize, DWORD id)
{
#ifdef TCP_PACKET_TRACE
  printf("file request packet received from %lu { sequence=%lx, message=%s }\n",
     uin, id, message);
#endif

  if(link->icq_RecvFileReq) {

    /* use the current system time for time received */
    time_t t=time(0);
    struct tm *ptime=localtime(&t);

    (*link->icq_RecvFileReq)(link, uin, ptime->tm_hour, ptime->tm_min,
       ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, message, 
       filename, filesize, id);

    /* don't send an acknowledgement to the remote client!
     * GUI is responsible for sending acknowledgement once user accepts
     * or denies using icq_TCPSendFileAck */
  }
}

void icq_TCPProcessFilePacket(icq_Packet *p, icq_TCPLink *plink)
{
  icq_FileSession *psession=(icq_FileSession *)plink->session;
  BYTE type;
  DWORD num_files;
  DWORD total_bytes;
  DWORD speed;
  DWORD filesize;
  const char *name;
  int result;

  icq_Packet *presponse;

  icq_PacketBegin(p);

  type=icq_PacketRead8(p);

  switch(type)
  {
    case 0x00:
      (void)icq_PacketRead32(p);
      num_files=icq_PacketRead32(p);
      total_bytes=icq_PacketRead32(p);
      speed=icq_PacketRead32(p);
      name=icq_PacketReadString(p);
      psession->total_files=num_files;
      psession->total_bytes=total_bytes;
      psession->current_speed=speed;
      icq_FileSessionSetHandle(psession, name);
      icq_FileSessionSetStatus(psession, FILE_STATUS_INITIALIZED);

      /* respond */
      presponse=icq_TCPCreateFile01Packet(speed, plink->icqlink->icq_Nick);

      icq_TCPLinkSend(plink, presponse);
#ifdef TCP_PACKET_TRACE
      printf("file 01 packet sent to uin %lu\n", plink->remote_uin);
#endif

      break;

    case 0x01:
      speed=icq_PacketRead32(p);
      name=icq_PacketReadString(p);
      psession->current_speed=speed;
      icq_FileSessionSetHandle(psession, name);
      icq_FileSessionSetStatus(psession, FILE_STATUS_INITIALIZED);

      /* respond */
      icq_FileSessionPrepareNextFile(psession);
      presponse=icq_TCPCreateFile02Packet(psession->current_file,
        psession->current_file_size, psession->current_speed);

      icq_TCPLinkSend(plink, presponse);
#ifdef TCP_PACKET_TRACE
      printf("file 02 packet sent to uin %lu\n", plink->remote_uin);
#endif       
      break;

    case 0x02:
      /* when files are skipped
      psession->total_transferred_bytes+=
        (psession->current_file_size-psession->current_file_progress);
      */

      (void)icq_PacketRead8(p);
      name=icq_PacketReadString(p);
      (void)icq_PacketReadString(p);
      filesize=icq_PacketRead32(p);
      (void)icq_PacketRead32(p);
      speed=icq_PacketRead32(p);
      icq_FileSessionSetCurrentFile(psession, name);
      psession->current_file_size=filesize;
      psession->current_speed=speed;
      psession->current_file_num++;
      icq_FileSessionSetStatus(psession, FILE_STATUS_NEXT_FILE);

      /* respond */
      presponse=icq_TCPCreateFile03Packet(psession->current_file_progress,
         speed);

      icq_TCPLinkSend(plink, presponse);
#ifdef TCP_PACKET_TRACE
      printf("file 03 packet sent to uin %lu\n", plink->remote_uin);
#endif       
      break;

    case 0x03:
      filesize=icq_PacketRead32(p);
      (void)icq_PacketRead32(p);       
      speed=icq_PacketRead32(p);
      psession->current_file_progress=filesize;
      psession->total_transferred_bytes+=filesize;
      psession->current_speed=speed;

      icq_FileSessionSetStatus(psession, FILE_STATUS_NEXT_FILE);
      icq_FileSessionSetStatus(psession, FILE_STATUS_SENDING);
      break;

    case 0x04:
      (void)icq_PacketRead32(p);
      icq_FileSessionSetStatus(psession, FILE_STATUS_STOP_FILE);
      break;

    case 0x05:
      speed=icq_PacketRead32(p);
      psession->current_speed=speed;
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, plink->id,
          ICQ_NOTIFY_FILE, FILE_STATUS_NEW_SPEED, 0);   
      break;

    case 0x06:
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, plink->id, ICQ_NOTIFY_FILEDATA,
                           p->length-sizeof(BYTE), p->data+sizeof(BYTE));
      icq_FileSessionSetStatus(psession, FILE_STATUS_RECEIVING);
      result=write(psession->current_fd, p->data+sizeof(BYTE), p->length-sizeof(BYTE));
      psession->current_file_progress+=p->length-sizeof(BYTE);
      psession->total_transferred_bytes+=p->length-sizeof(BYTE);
      break;

    default:
      icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown file packet type %d!\n", type);

  }
}

void icq_HandleFileHello(icq_TCPLink *plink)
{

  /* once the hello packet has been processed and we know which uin this
   * link is for, we can link up with a file session */
  icq_FileSession *pfile=icq_FindFileSession(plink->icqlink,
    plink->remote_uin, 0);

  if(pfile)
  {
    plink->id=pfile->id;
    plink->session=pfile;
    pfile->tcplink=plink;
    icq_FileSessionSetStatus(pfile, FILE_STATUS_CONNECTED);

  } else {

    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING,
      "unexpected file hello received from %d, closing link\n",
      plink->remote_uin);
    icq_TCPLinkClose(plink);
  }

}
  
void icq_HandleFileAck(icq_TCPLink *plink, icq_Packet *p, int port)
{
  icq_TCPLink *pfilelink;
  icq_FileSession *pfile;
  icq_Packet *p2;

  if(plink->icqlink->icq_RequestNotify)
    (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_ACK, 0, 0);
  pfilelink=icq_TCPLinkNew(plink->icqlink);
  pfilelink->type=TCP_LINK_FILE;
  pfilelink->id=p->id;

  /* once the ack packet has been processed, link up with the file session */
  pfile=icq_FindFileSession(plink->icqlink, plink->remote_uin, 0);

  pfile->tcplink=pfilelink;
  pfilelink->id=pfile->id;

  if (plink->icqlink->icq_RequestNotify)
    (*plink->icqlink->icq_RequestNotify)(plink->icqlink, pfile->id, 
      ICQ_NOTIFY_FILESESSION, sizeof(icq_FileSession), pfile);
  
  icq_FileSessionSetStatus(pfile, FILE_STATUS_CONNECTING);
  icq_TCPLinkConnect(pfilelink, plink->remote_uin, port);

  pfilelink->session=pfile;

  p2=icq_TCPCreateFile00Packet( pfile->total_files,
    pfile->total_bytes, pfile->current_speed, plink->icqlink->icq_Nick);
  icq_TCPLinkSend(pfilelink, p2);

}   

