/**
 * @file p2p.h MSN P2P functions
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef MSN_P2P_H
#define MSN_P2P_H

typedef struct {
	guint32 session_id;
	guint32 id;
	/**
	 * In a MsnSlpMessage:
	 * For outgoing messages this is the number of bytes from buffer that
	 * have already been sent out.  For incoming messages this is the
	 * number of bytes that have been written to buffer.
	 */
	guint64 offset;
	guint64 total_size;
	guint32 length;
	guint32 flags;
	guint32 ack_id;
	guint32 ack_sub_id;
	guint64 ack_size;
/*	guint8  body[1]; */
} MsnP2PHeader;
#define P2P_PACKET_HEADER_SIZE (6 * 4 + 3 * 8)

/* Used for DCs to store nonces */
#define P2P_HEADER_ACK_ID_OFFSET (2*4 + 2*8 + 2*4)

typedef struct {
	guint8  header_len;
	guint8  opcode;
	guint16 message_len;
	guint32 base_id;
	GSList 	*tlv;
	char 	*data;
} MsnP2Pv2Header;

typedef struct {
	guint8 		length;
	guint8 		TF; 			/**< Type-First combination */
	guint16 	package_num;
	guint32 	session;
	GSList 		*tlv;
	char 		*data;
} MsnP2Pv2DataHeader;

typedef struct {
	guint16 	protocol_version;
	guint16 	implementation_id;
	guint16 	version;
	guint16 	reserved;
	guint32 	caps;
} PeerInfo;

typedef enum
{
	OP_NONE		= 0x00, 		/**< None, Nothing required. */
	OP_SYN		= 0x01, 		/**< SYN, just like TCP. */
	OP_RAK 		= 0x02 		/**< Request for Ack. */
} OpCode;

typedef enum
{
	TF_FIRST 	= 0x01, 	/**< The first package. */
	TF_OBJ 		= 0x04, 	/**< Payload contains binary data for MsnObject. */
	TF_FILE 	= 0x06 	/**< Payload contains binary data. */
} TF;

typedef enum
{
	TLP_PEER_INFO 	= 0x01, 	/**< Client peer info */
	TLP_ACK 		= 0x02, 	/**< ACK */
	TLP_NAK 		= 0x03 	/**< NAK */
} TLP;

typedef enum
{
	TLP_LEN_PEER_INFO 	= 12,
	TLP_LEN_ACK 		= 4,
	TLP_LEN_NAK 		= 4
} TLPLength;

typedef enum
{
	PI_PVER 	= 0x0200,
	PI_IMP_ID 	= 0,
	PI_VER 		= 0x0e00,
	PI_RES 		= 0,
	PI_CAPS		= 0x0000010f
} PeerInfoVal;


#define DLP_REMAINING 0x01; 	/**< Indicates the remaining data to transfer.*/
#define DLP_REMAINING_LEN 8

typedef struct
{
	guint32 value;
} MsnP2PFooter;
#define P2P_PACKET_FOOTER_SIZE (1 * 4)

typedef enum
{
	P2P_NO_FLAG         = 0x0,        /**< No flags specified */
	P2P_OUT_OF_ORDER    = 0x1,        /**< Chunk out-of-order */
	P2P_ACK             = 0x2,        /**< Acknowledgement */
	P2P_PENDING_INVITE  = 0x4,        /**< There is a pending invite */
	P2P_BINARY_ERROR    = 0x8,        /**< Error on the binary level */
	P2P_FILE            = 0x10,       /**< File */
	P2P_MSN_OBJ_DATA    = 0x20,       /**< MsnObject data */
	P2P_CLOSE           = 0x40,       /**< Close session */
	P2P_TLP_ERROR       = 0x80,       /**< Error at transport layer protocol */
	P2P_DC_HANDSHAKE    = 0x100,      /**< Direct Handshake */
	P2P_WLM2009_COMP    = 0x1000000,  /**< Compatibility with WLM 2009 */
	P2P_FILE_DATA       = 0x1000030   /**< File transfer data */
} MsnP2PHeaderFlag;
/* Info From:
 * http://msnpiki.msnfanatic.com/index.php/MSNC:P2Pv1_Headers#Flags
 * http://trac.kmess.org/changeset/ba04d0c825769d23370511031c47f6be75fe9b86
 * #7180
 */

typedef enum
{
	P2P_APPID_SESSION   = 0x0,        /**< Negotiating session */
	P2P_APPID_OBJ       = 0x1,        /**< MsnObject (Display or Emoticon) */
	P2P_APPID_FILE      = 0x2,        /**< File transfer */
	P2P_APPID_EMOTE     = 0xB,        /**< CustomEmoticon */
	P2P_APPID_DISPLAY   = 0xC         /**< Display Image */
} MsnP2PAppId;

MsnP2PHeader *
msn_p2p_header_from_wire(const char *wire);

char *
msn_p2p_header_to_wire(MsnP2PHeader *header);

MsnP2PFooter *
msn_p2p_footer_from_wire(const char *wire);

char *
msn_p2p_footer_to_wire(MsnP2PFooter *footer);

gboolean
msn_p2p_msg_is_data(const MsnP2PHeaderFlag flags);

#endif /* MSN_P2P_H */
