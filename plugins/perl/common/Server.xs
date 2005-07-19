#include "module.h"

/*
TODO

*/


MODULE = Gaim::Serv  PACKAGE = Gaim::Serv  PREFIX = serv_
PROTOTYPES: ENABLE


void 
serv_add_buddies(con, list)
	Gaim::Connection con
	SV * list
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(list));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(list), i, 0), t_sl));
	}
	serv_add_buddies(con, t_GL);
	
void 
serv_add_buddy(con, buddy)
	Gaim::Connection 	con
	Gaim::BuddyList::Buddy	buddy 

void 
serv_add_deny(con, a)
	Gaim::Connection con
	const char * a

void 
serv_add_permit(a, b)
	Gaim::Connection a
	const char * b

void 
serv_alias_buddy(buddy)
	Gaim::BuddyList::Buddy buddy 

void 
serv_change_passwd(con, a, b)
	Gaim::Connection con
	const char *	a
	const char *	b

void 
serv_chat_invite(con, a, b, c)
	Gaim::Connection con
	int a
	const char * b
	const char * c

void 
serv_chat_leave(a, b)
	Gaim::Connection a
	int b

int  
serv_chat_send(con, a, b)
	Gaim::Connection con 
	int a
	const char * b

void 
serv_chat_whisper(con, a, b, c)
	Gaim::Connection con
	int a
	const char * b
	const char * c

void 
serv_get_info(con, a)
	Gaim::Connection con 
	const char * a

void 
serv_got_alias(gc, who, alias)
	Gaim::Connection gc
	const char *who
	const char *alias

void 
serv_got_chat_in(g, id, who, chatflags, message, mtime)
	Gaim::Connection g
	int id
	const char *who
	Gaim::ConvChatFlags chatflags
	const char *message
	time_t mtime

void 
serv_got_chat_invite(gc, name, who, message, components)
	Gaim::Connection gc
	const char *name
	const char *who
	const char *message
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	int i;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(NULL, NULL);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
 		t_value = SvPV(t_SV, PL_na);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	serv_got_chat_invite(gc, name, who, message, t_GHash);

void 
serv_got_chat_left(g, id)
	Gaim::Connection g
	int id

void 
serv_got_im(gc, who, msg, imflags, mtime)
	Gaim::Connection gc
	const char *who
	const char *msg
	Gaim::ConvImFlags imflags
	time_t mtime

Gaim::Conversation
serv_got_joined_chat(gc, id, name)
	Gaim::Connection gc
	int id
	const char *name

void 
serv_got_typing(gc, name, timeout, state)
	Gaim::Connection gc
	const char *name
	int timeout
	Gaim::TypingState state

void 
serv_got_typing_stopped(gc, name)
	Gaim::Connection gc
	const char *name

void 
serv_join_chat(con, components)
	Gaim::Connection con 
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	int i;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(NULL, NULL);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
 		t_value = SvPV(t_SV, PL_na);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	serv_join_chat(con, t_GHash);

void 
serv_move_buddy(buddy, group1, group2)
	Gaim::BuddyList::Buddy buddy
	Gaim::BuddyList::Group group1
	Gaim::BuddyList::Group group2

void 
serv_reject_chat(con, components)
	Gaim::Connection con 
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	int i;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(NULL, NULL);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
 		t_value = SvPV(t_SV, PL_na);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	serv_reject_chat(con, t_GHash);

void 
serv_rem_deny(con, a)
	Gaim::Connection con
	const char * 	a

void 
serv_rem_permit(con, a)
	Gaim::Connection con
	const char * 	a

void 
serv_remove_buddies(con, A, B)
	Gaim::Connection con
	SV * A
	SV * B
PREINIT:
	GList *t_GL1, *t_GL2;
	int i, t_len;
PPCODE:
	t_GL1 = NULL;
	t_len = av_len((AV *)SvRV(A));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL1 = g_list_append(t_GL1, SvPV(*av_fetch((AV *)SvRV(A), i, 0), t_sl));
	}
	
	t_GL2 = NULL;
	t_len = av_len((AV *)SvRV(B));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL2 = g_list_append(t_GL2, SvPV(*av_fetch((AV *)SvRV(B), i, 0), t_sl));
	}
	serv_remove_buddies(con, t_GL1, t_GL2);

void 
serv_remove_buddy(con, buddy, group)
	Gaim::Connection con
	Gaim::BuddyList::Buddy buddy
	Gaim::BuddyList::Group group

void 
serv_remove_group(con, group)
	Gaim::Connection con
	Gaim::BuddyList::Group group 

void 
serv_send_file(gc, who, file)
	Gaim::Connection gc
	const char *who
	const char *file

int  
serv_send_im(con, a, b, flags )
	Gaim::Connection con
	const char * a
	const char * b
	Gaim::ConvImFlags flags

int  
serv_send_typing(con, a, b)
	Gaim::Connection con
	const char * a
	int b

void 
serv_set_buddyicon(gc, filename)
	Gaim::Connection gc
	const char *filename

void 
serv_set_idle(con, a)
	Gaim::Connection con 
	int a

void 
serv_set_info(con, a)
	Gaim::Connection con 
	const char * a

void 
serv_set_permit_deny(con)
	Gaim::Connection con  

void 
serv_touch_idle(con)
	Gaim::Connection con 

void 
serv_warn(con, a, b)
	Gaim::Connection con 
	const char * a
	gboolean b

