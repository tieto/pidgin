/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -w LIBYAHOO_PROTO -o ./libyahoo-proto.h' */
#ifndef CFH_LIBYAHOO_PROTO
#define CFH_LIBYAHOO_PROTO

unsigned int yahoo_makeint(unsigned char *data);
char **yahoo_list2array(const char *buff);
void yahoo_arraykill(char **array);
char *yahoo_array2list(char **array);
struct yahoo_context *yahoo_init(const char *user, const char *password, struct yahoo_options *options);
void yahoo_free_context(struct yahoo_context *ctx);
char *yahoo_get_status_string(int statuscode);
char *yahoo_get_status_append(int statuscode);
char *yahoo_get_service_string(int servicecode);
int yahoo_fetchcookies(struct yahoo_context *ctx);
int yahoo_add_buddy(struct yahoo_context *ctx, const char *addid, const char *active_id, const char *group, const char *msg);
int yahoo_remove_buddy(struct yahoo_context *ctx, const char *addid, const char *active_id, const char *group, const char *msg);
int yahoo_get_config(struct yahoo_context *ctx);
int yahoo_cmd_logon(struct yahoo_context *ctx, unsigned int initial_status);
int yahoo_connect(struct yahoo_context *ctx);
int yahoo_sendcmd_http(struct yahoo_context *ctx, struct yahoo_rawpacket *pkt);
int yahoo_sendcmd(struct yahoo_context *ctx, int service, const char *active_nick, const char *content, unsigned int msgtype);
int yahoo_cmd_ping(struct yahoo_context *ctx);
int yahoo_cmd_idle(struct yahoo_context *ctx);
int yahoo_cmd_sendfile(struct yahoo_context *ctx, const char *active_user, const char *touser, const char *msg, const char *filename);
int yahoo_cmd_msg(struct yahoo_context *ctx, const char *active_user, const char *touser, const char *msg);
int yahoo_cmd_msg_offline(struct yahoo_context *ctx, const char *active_user, const char *touser, const char *msg);
int yahoo_cmd_set_away_mode(struct yahoo_context *ctx, int status, const char *msg);
int yahoo_cmd_set_back_mode(struct yahoo_context *ctx, int status, const char *msg);
int yahoo_cmd_activate_id(struct yahoo_context *ctx, const char *newid);
int yahoo_cmd_user_status(struct yahoo_context *ctx);
int yahoo_cmd_logoff(struct yahoo_context *ctx);
int yahoo_cmd_start_conf(struct yahoo_context *ctx, const char *conf_id, char **userlist, const char *msg, int type);
int yahoo_cmd_conf_logon(struct yahoo_context *ctx, const char *conf_id, const char *host, char **userlist);
int yahoo_cmd_decline_conf(struct yahoo_context *ctx, const char *conf_id, const char *host, char **userlist, const char *msg);
int yahoo_cmd_conf_logoff(struct yahoo_context *ctx, const char *conf_id, char **userlist);
int yahoo_cmd_conf_invite(struct yahoo_context *ctx, const char *conf_id, char **userlist, const char *invited_user, const char *msg);
int yahoo_cmd_conf_msg(struct yahoo_context *ctx, const char *conf_id, char **userlist, const char *msg);
void yahoo_free_rawpacket(struct yahoo_rawpacket *pkt);
void yahoo_free_packet(struct yahoo_packet *pkt);
void yahoo_free_idstatus(struct yahoo_idstatus *idstatus);
struct yahoo_packet *yahoo_parsepacket(struct yahoo_context *ctx, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_ping(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_newmail(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_grouprename(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_conference_invite(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_conference_decline(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_conference_addinvite(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_conference_msg(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_conference_user(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_filetransfer(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_calendar(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_chatinvite(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_newcontact(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_status(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_message(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_parsepacket_message_offline(struct yahoo_context *ctx, struct yahoo_packet *pkt, struct yahoo_rawpacket *inpkt);
int yahoo_getdata(struct yahoo_context *ctx);
struct yahoo_rawpacket *yahoo_getpacket(struct yahoo_context *ctx);
int yahoo_isbuddy(struct yahoo_context *ctx, const char *id);
void yahoo_freeaddressbook(struct yahoo_context *ctx);
int yahoo_fetchaddressbook(struct yahoo_context *ctx);

#endif /* CFH_LIBYAHOO_PROTO */
