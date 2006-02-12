/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * oscar_internal.h -- prototypes/structs for the guts of libfaim
 *
 */

#ifndef _OSCAR_INTERNAL_H_
#define _OSCAR_INTERNAL_H_

typedef struct {
	guint16 family;
	guint16 subtype;
	guint16 flags;
	guint32 id;
} aim_modsnac_t;

#define AIM_MODULENAME_MAXLEN 16
#define AIM_MODFLAG_MULTIFAMILY 0x0001
typedef struct aim_module_s {
	guint16 family;
	guint16 version;
	guint16 toolid;
	guint16 toolversion;
	guint16 flags;
	char name[AIM_MODULENAME_MAXLEN+1];
	int (*snachandler)(OscarSession *sess, struct aim_module_s *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs);

	void (*shutdown)(OscarSession *sess, struct aim_module_s *mod);
	void *priv;
	struct aim_module_s *next;
} aim_module_t;

faim_internal int aim__registermodule(OscarSession *sess, int (*modfirst)(OscarSession *, aim_module_t *));
faim_internal void aim__shutdownmodules(OscarSession *sess);
faim_internal aim_module_t *aim__findmodulebygroup(OscarSession *sess, guint16 group);
faim_internal aim_module_t *aim__findmodule(OscarSession *sess, const char *name);

faim_internal int admin_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int buddylist_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int bos_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int search_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int stats_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int auth_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int msg_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int misc_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int chatnav_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int chat_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int locate_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int service_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int invite_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int translate_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int popups_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int adverts_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int odir_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int bart_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int ssi_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int icq_modfirst(OscarSession *sess, aim_module_t *mod);
faim_internal int email_modfirst(OscarSession *sess, aim_module_t *mod);

faim_internal int aim_genericreq_n(OscarSession *, OscarConnection *conn, guint16 family, guint16 subtype);
faim_internal int aim_genericreq_n_snacid(OscarSession *, OscarConnection *conn, guint16 family, guint16 subtype);
faim_internal int aim_genericreq_l(OscarSession *, OscarConnection *conn, guint16 family, guint16 subtype, guint32 *);
faim_internal int aim_genericreq_s(OscarSession *, OscarConnection *conn, guint16 family, guint16 subtype, guint16 *);

#define AIMBS_CURPOSPAIR(x) ((x)->data + (x)->offset), ((x)->len - (x)->offset)

/* bstream.c */
faim_internal int aim_bstream_init(ByteStream *bs, guint8 *data, int len);
faim_internal int aim_bstream_empty(ByteStream *bs);
faim_internal int aim_bstream_curpos(ByteStream *bs);
faim_internal int aim_bstream_setpos(ByteStream *bs, unsigned int off);
faim_internal void aim_bstream_rewind(ByteStream *bs);
faim_internal int aim_bstream_advance(ByteStream *bs, int n);
faim_internal guint8 aimbs_get8(ByteStream *bs);
faim_internal guint16 aimbs_get16(ByteStream *bs);
faim_internal guint32 aimbs_get32(ByteStream *bs);
faim_internal guint8 aimbs_getle8(ByteStream *bs);
faim_internal guint16 aimbs_getle16(ByteStream *bs);
faim_internal guint32 aimbs_getle32(ByteStream *bs);
faim_internal int aimbs_getrawbuf(ByteStream *bs, guint8 *buf, int len);
faim_internal guint8 *aimbs_getraw(ByteStream *bs, int len);
faim_internal char *aimbs_getstr(ByteStream *bs, int len);
faim_internal int aimbs_put8(ByteStream *bs, guint8 v);
faim_internal int aimbs_put16(ByteStream *bs, guint16 v);
faim_internal int aimbs_put32(ByteStream *bs, guint32 v);
faim_internal int aimbs_putle8(ByteStream *bs, guint8 v);
faim_internal int aimbs_putle16(ByteStream *bs, guint16 v);
faim_internal int aimbs_putle32(ByteStream *bs, guint32 v);
faim_internal int aimbs_putraw(ByteStream *bs, const guint8 *v, int len);
faim_internal int aimbs_putstr(ByteStream *bs, const char *str);
faim_internal int aimbs_putbs(ByteStream *bs, ByteStream *srcbs, int len);
faim_internal int aimbs_putcaps(ByteStream *bs, guint32 caps);

/* conn.c */
faim_internal OscarConnection *aim_cloneconn(OscarSession *sess, OscarConnection *src);

/* rxhandlers.c */
faim_internal aim_rxcallback_t aim_callhandler(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 type);
faim_internal int aim_callhandler_noparam(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 type, FlapFrame *ptr);
faim_internal int aim_parse_unknown(OscarSession *, FlapFrame *, ...);
faim_internal void aim_clonehandlers(OscarSession *sess, OscarConnection *dest, OscarConnection *src);

/* rxqueue.c */
faim_internal int aim_recv(int fd, void *buf, size_t count);
faim_internal int aim_bstream_recv(ByteStream *bs, int fd, size_t count);
faim_internal void aim_rxqueue_cleanbyconn(OscarSession *sess, OscarConnection *conn);
faim_internal void aim_frame_destroy(FlapFrame *);

/* txqueue.c */
faim_internal FlapFrame *aim_tx_new(OscarSession *sess, OscarConnection *conn, guint8 framing, guint16 chan, int datalen);
faim_internal int aim_tx_enqueue(OscarSession *, FlapFrame *);
faim_internal int aim_bstream_send(ByteStream *bs, OscarConnection *conn, size_t count);
faim_internal int aim_tx_sendframe(OscarSession *sess, FlapFrame *cur);
faim_internal void aim_tx_cleanqueue(OscarSession *, OscarConnection *);

/*
 * Generic SNAC structure.  Rarely if ever used.
 */
typedef struct aim_snac_s {
	aim_snacid_t id;
	guint16 family;
	guint16 type;
	guint16 flags;
	void *data;
	time_t issuetime;
	struct aim_snac_s *next;
} aim_snac_t;

/* snac.c */
faim_internal void aim_initsnachash(OscarSession *sess);
faim_internal aim_snacid_t aim_newsnac(OscarSession *, aim_snac_t *newsnac);
faim_internal aim_snacid_t aim_cachesnac(OscarSession *sess, const guint16 family, const guint16 type, const guint16 flags, const void *data, const int datalen);
faim_internal aim_snac_t *aim_remsnac(OscarSession *, aim_snacid_t id);
faim_internal int aim_putsnac(ByteStream *, guint16 family, guint16 type, guint16 flags, aim_snacid_t id);

/* Stored in ->priv of the service request SNAC for chats. */
struct chatsnacinfo {
	guint16 exchange;
	char name[128];
	guint16 instance;
};

/*
 * In SNACland, the terms 'family' and 'group' are synonymous -- the former
 * is my term, the latter is AOL's.
 */
struct snacgroup {
	guint16 group;
	struct snacgroup *next;
};

struct snacpair {
	guint16 group;
	guint16 subtype;
	struct snacpair *next;
};

struct rateclass {
	guint16 classid;
	guint32 windowsize;
	guint32 clear;
	guint32 alert;
	guint32 limit;
	guint32 disconnect;
	guint32 current;
	guint32 max;
	guint8 unknown[5]; /* only present in versions >= 3 */
	struct snacpair *members;
	struct rateclass *next;
};

/*
 * This is inside every connection.  But it is a void * to anything
 * outside of libfaim.  It should remain that way.  It's called data
 * abstraction.  Maybe you've heard of it.  (Probably not if you're a
 * libfaim user.)
 *
 */
typedef struct aim_conn_inside_s {
	struct snacgroup *groups;
	struct rateclass *rates;
} aim_conn_inside_t;

faim_internal void aim_conn_addgroup(OscarConnection *conn, guint16 group);

faim_internal int aim_cachecookie(OscarSession *sess, IcbmCookie *cookie);
faim_internal IcbmCookie *aim_uncachecookie(OscarSession *sess, guint8 *cookie, int type);
faim_internal IcbmCookie *aim_mkcookie(guint8 *, int, void *);
faim_internal IcbmCookie *aim_checkcookie(OscarSession *, const unsigned char *, const int);
faim_internal int aim_freecookie(OscarSession *sess, IcbmCookie *cookie);
faim_internal int aim_msgcookie_gettype(int reqclass);
faim_internal int aim_cookie_free(OscarSession *sess, IcbmCookie *cookie);

/* 0x0002 - locate.c */
faim_internal void aim_locate_requestuserinfo(OscarSession *sess, const char *sn);
faim_internal guint32 aim_locate_getcaps(OscarSession *sess, ByteStream *bs, int len);
faim_internal guint32 aim_locate_getcaps_short(OscarSession *sess, ByteStream *bs, int len);
faim_internal void aim_info_free(aim_userinfo_t *);
faim_internal int aim_info_extract(OscarSession *sess, ByteStream *bs, aim_userinfo_t *);
faim_internal int aim_putuserinfo(ByteStream *bs, aim_userinfo_t *info);

faim_internal int aim_chat_readroominfo(ByteStream *bs, struct aim_chat_roominfo *outinfo);

faim_internal void aim_conn_kill_chat(OscarSession *sess, OscarConnection *conn);

/* These are all handled internally now. */
faim_internal int aim_setversions(OscarSession *sess, OscarConnection *conn);
faim_internal int aim_reqrates(OscarSession *, OscarConnection *);
faim_internal int aim_rates_addparam(OscarSession *, OscarConnection *);
faim_internal int aim_rates_delparam(OscarSession *, OscarConnection *);

#endif /* _OSCAR_INTERNAL_H_ */
