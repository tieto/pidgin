/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _UDP_H_
#define _UDP_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icq.h"
#include "util.h"
#include "icqpacket.h"

#define UDP_CMD_ACK                0x000A 
#define UDP_CMD_SEND_THRU_SRV      0x010E
#define UDP_CMD_LOGIN              0x03E8
#define UDP_CMD_CONT_LIST          0x0406
#define UDP_CMD_SEARCH_UIN         0x041A
#define UDP_CMD_SEARCH_USER        0x0424
#define UDP_CMD_KEEP_ALIVE         0x042E
#define UDP_CMD_KEEP_ALIVE2        0x051E
#define UDP_CMD_SEND_TEXT_CODE     0x0438
#define UDP_CMD_LOGIN_1            0x044C
#define UDP_CMD_INFO_REQ           0x0460
#define UDP_CMD_EXT_INFO_REQ       0x046A
#define UDP_CMD_CHANGE_PW          0x049C
#define UDP_CMD_STATUS_CHANGE      0x04D8
#define UDP_CMD_LOGIN_2            0x0528
#define UDP_CMD_UPDATE_INFO        0x050A
#define UDP_CMD_UPDATE_AUTH        0x0514
#define UDP_CMD_UPDATE_EXT_INFO    0x04B0
#define UDP_CMD_ADD_TO_LIST        0x053C
#define UDP_CMD_REQ_ADD_LIST       0x0456
#define UDP_CMD_QUERY_SERVERS      0x04BA
#define UDP_CMD_QUERY_ADDONS       0x04C4
#define UDP_CMD_NEW_USER_1         0x04EC
#define UDP_CMD_NEW_USER_INFO      0x04A6
#define UDP_CMD_ACK_MESSAGES       0x0442
#define UDP_CMD_MSG_TO_NEW_USER    0x0456
#define UDP_CMD_REG_NEW_USER       0x03FC
#define UDP_CMD_VIS_LIST           0x06AE
#define UDP_CMD_INVIS_LIST         0x06A4
#define UDP_CMD_META_USER          0x064A
#define UDP_CMD_RAND_SEARCH        0x056E
#define UDP_CMD_RAND_SET           0x0564
#define UDP_CMD_REVERSE_TCP_CONN   0x015E

#define UDP_SRV_ACK                0x000A
#define UDP_SRV_LOGIN_REPLY        0x005A
#define UDP_SRV_USER_ONLINE        0x006E
#define UDP_SRV_USER_OFFLINE       0x0078
#define UDP_SRV_USER_FOUND         0x008C
#define UDP_SRV_OFFLINE_MESSAGE    0x00DC
#define UDP_SRV_END_OF_SEARCH      0x00A0
#define UDP_SRV_INFO_REPLY         0x0118
#define UDP_SRV_EXT_INFO_REPLY     0x0122
#define UDP_SRV_STATUS_UPDATE      0x01A4
#define UDP_SRV_X1                 0x021C
#define UDP_SRV_X2                 0x00E6
#define UDP_SRV_UPDATE             0x01E0
#define UDP_SRV_UPDATE_EXT         0x00C8
#define UDP_SRV_NEW_UIN            0x0046
#define UDP_SRV_NEW_USER           0x00B4
#define UDP_SRV_QUERY              0x0082
#define UDP_SRV_SYSTEM_MESSAGE     0x01C2
#define UDP_SRV_ONLINE_MESSAGE     0x0104
#define UDP_SRV_GO_AWAY            0x00F0
#define UDP_SRV_TRY_AGAIN          0x00FA
#define UDP_SRV_FORCE_DISCONNECT   0x0028
#define UDP_SRV_MULTI_PACKET       0x0212
#define UDP_SRV_WRONG_PASSWORD     0x0064
#define UDP_SRV_INVALID_UIN        0x012C
#define UDP_SRV_META_USER          0x03DE
#define UDP_SRV_RAND_USER          0x024E
#define UDP_SRV_AUTH_UPDATE        0x01F4

#define META_CMD_SET_INFO          1000
#define META_CMD_SET_HOMEPAGE      1020
#define META_CMD_SET_ABOUT         1030
#define META_CMD_SET_SECURE        1060
#define META_CMD_SET_PASS          1070
#define META_CMD_REQ_INFO          1200
#define META_SRV_RES_INFO          100
#define META_SRV_RES_HOMEPAGE      120
#define META_SRV_RES_ABOUT         130
#define META_SRV_RES_SECURE        160
#define META_SRV_RES_PASS          170
#define META_SRV_USER_INFO         200
#define META_SRV_USER_WORK         210
#define META_SRV_USER_MORE         220
#define META_SRV_USER_ABOUT        230
#define META_SRV_USER_INTERESTS    240
#define META_SRV_USER_AFFILIATIONS 250
#define META_SRV_USER_HPCATEGORY   270
#define META_SRV_USER_FOUND        410

#define META_SRV_SUCCESS       10
#define META_SRV_FAILURE       50

#define TYPE_MSG               0x0001
#define TYPE_CHAT              0x0002
#define TYPE_FILE              0x0003
#define TYPE_URL               0x0004
#define TYPE_AUTH_REQ          0x0006
#define TYPE_AUTH              0x0008
#define TYPE_ADDED             0x000C
#define TYPE_WEBPAGER          0x000D
#define TYPE_EXPRESS           0x000E
#define TYPE_CONTACT           0x0013
#define TYPE_MASS_MASK         0x8000

#define LOGIN_X1_DEF 0x00000098
/*#define LOGIN_X1_DEF 0x000000D5*/
#define LOGIN_X3_DEF 0x00000003
/*#define LOGIN_X3_DEF 0x00000006*/
#define LOGIN_X4_DEF 0x00000000
#define LOGIN_X5_DEF 0x00980010
/*#define LOGIN_X5_DEF 0x00D50008*/
#define LOGIN_X6_DEF 0x00000050
#define LOGIN_X7_DEF 0x00000003
#define LOGIN_X8_DEF 0x00000000

#define LOGIN_SNDONLY_TCP 0x02
#define LOGIN_SNDRCV_TCP  0x04
#define LOGIN_NO_TCP      0x06

void icq_SendGotMessages(ICQLINK *link);
void icq_SendLogin1(ICQLINK *link);
void icq_StatusUpdate(ICQLINK *link, icq_Packet*);
void icq_AckSrv(ICQLINK *link, int seq);
void icq_HandleUserOffline(ICQLINK *link, icq_Packet*);
void icq_HandleUserOnline(ICQLINK *link, icq_Packet*);
void icq_DoMsg(ICQLINK *link, DWORD type, WORD len, char *data, DWORD uin,
               BYTE hour, BYTE minute, BYTE day, BYTE month, WORD year);
int icq_UDPSockWriteDirect(ICQLINK *link, icq_Packet*);
int icq_UDPSockWrite(ICQLINK *link, icq_Packet*);
int icq_UDPSockRead(ICQLINK *link, icq_Packet*);
void icq_ServerResponse(ICQLINK *link, icq_Packet*);
void icq_HandleMultiPacket(ICQLINK *link, icq_Packet*);

void icq_UDPAck(ICQLINK *link, int seq);
WORD icq_UDPSendMessage(ICQLINK *link, DWORD uin, const char *text);
WORD icq_UDPSendURL(ICQLINK *link, DWORD uin, const char *url, const char *descr);

#endif /* _UDP_H_ */
