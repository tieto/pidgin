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

#ifdef _MSVC_
#include <io.h>
#define open _open
#define close _close
#define read _read
#define write _write
#endif

#include <errno.h>

#include "icqlib.h"

#include "tcp.h"
#include "stdpackets.h"
#include "filesession.h"

void icq_TCPOnFileReqReceived(icq_Link *icqlink, DWORD uin, const char *message, 
   const char *filename, unsigned long filesize, DWORD id)
{
  /* use the current system time for time received */
  time_t t=time(0);
  struct tm *ptime=localtime(&t);

#ifdef TCP_PACKET_TRACE
  printf("file request packet received from %lu { sequence=%lx, message=%s }\n",
     uin, id, message);
#endif

  invoke_callback(icqlink,icq_RecvFileReq)(icqlink, uin, ptime->tm_hour, 
    ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900,
    message, filename, filesize, id);

  /* don't send an acknowledgement to the remote client!
   * GUI is responsible for sending acknowledgement once user accepts
   * or denies using icq_TCPSendFileAck */
}

void icq_TCPProcessFilePacket(icq_Packet *p, icq_TCPLink *plink)
{
  icq_FileSession *psession=(icq_FileSession *)plink->session;
  icq_Link *icqlink = plink->icqlink;
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
      icq_FileSessionSetStatus(psession, FILE_STATUS_INITIALIZING);

      /* respond */
      presponse=icq_TCPCreateFile01Packet(speed, icqlink->icq_Nick);

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
      icq_FileSessionSetStatus(psession, FILE_STATUS_INITIALIZING);

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
      invoke_callback(icqlink, icq_FileNotify)(psession, 
        FILE_NOTIFY_STOP_FILE, 0, NULL);
      break;

    case 0x05:
      speed=icq_PacketRead32(p);
      psession->current_speed=speed;
      invoke_callback(icqlink, icq_FileNotify)(psession,
        FILE_NOTIFY_NEW_SPEED, speed, NULL);
      break;

    case 0x06:
    {
      void *data = p->data+sizeof(BYTE);
      int length = p->length-sizeof(BYTE);

      invoke_callback(icqlink, icq_FileNotify)(psession,
        FILE_NOTIFY_DATAPACKET, length, data);
      icq_FileSessionSetStatus(psession, FILE_STATUS_RECEIVING);
      result=write(psession->current_fd, data, length);
      psession->current_file_progress+=length;
      psession->total_transferred_bytes+=length;
      break;
    }

    default:
      icq_FmtLog(icqlink, ICQ_LOG_WARNING, "unknown file packet type %d!\n", type);

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

  invoke_callback(plink->icqlink, icq_RequestNotify)(plink->icqlink, 
    p->id, ICQ_NOTIFY_ACK, 0, NULL);

  pfilelink=icq_TCPLinkNew(plink->icqlink);
  pfilelink->type=TCP_LINK_FILE;
  pfilelink->id=p->id;

  /* once the ack packet has been processed, link up with the file session */
  pfile=icq_FindFileSession(plink->icqlink, plink->remote_uin, 0);

  pfile->tcplink=pfilelink;
  pfilelink->id=pfile->id;

  invoke_callback(plink->icqlink, icq_RequestNotify)(plink->icqlink,
    pfile->id, ICQ_NOTIFY_FILESESSION, sizeof(icq_FileSession), pfile);
  invoke_callback(plink->icqlink, icq_RequestNotify)(plink->icqlink,
    pfile->id, ICQ_NOTIFY_SUCCESS, 0, NULL);
  
  icq_FileSessionSetStatus(pfile, FILE_STATUS_CONNECTING);
  icq_TCPLinkConnect(pfilelink, plink->remote_uin, port);

  pfilelink->session=pfile;

  p2=icq_TCPCreateFile00Packet( pfile->total_files,
    pfile->total_bytes, pfile->current_speed, plink->icqlink->icq_Nick);
  icq_TCPLinkSend(pfilelink, p2);

}   

