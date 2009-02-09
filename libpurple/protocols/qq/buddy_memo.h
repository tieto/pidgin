
#ifndef _BUDDY_MEMO_H
#define _BUDDY_MEMO_H 

#include <glib.h>

#include "connection.h"
#include "buddy_opt.h"
#include "qq.h"



#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "request.h"
#include "utils.h"
#include "packet_parse.h"
#include "buddy_list.h"
#include "buddy_info.h"
#include "char_conv.h"
#include "im.h"
#include "qq_define.h"
#include "qq_base.h"
#include "qq_network.h"
#include "../../blist.h"





enum {
	QQ_BUDDY_MEMO_NAME = 0,
	QQ_BUDDY_MEMO_MOBILD,
	QQ_BUDDY_MEMO_TELEPHONE,
	QQ_BUDDY_MEMO_ADDRESS,
	QQ_BUDDY_MEMO_EMAIL,
	QQ_BUDDY_MEMO_ZIPCODE,
	QQ_BUDDY_MEMO_NOTE,
	QQ_BUDDY_MEMO_SIZE
};






/** 
 * 向服务器发送下载好友备注信息的请求
 * 
 * @param gc 
 * @param uid 好友qq号码
 */
void qq_request_buddy_memo_download(PurpleConnection *gc, guint32 uid);





/** 
 * 处理服务器对好友备注信息的响应
 * 
 * @param gc 
 * @param data 解密后的数据
 * @param len data数据长度
 */
void qq_process_get_buddy_memo( PurpleConnection *gc, guint8* data, gint len );


/** 
 * 在好友列表项上右键菜单中显示好友信息的响应函数
 * 
 * @param node 
 * @param data 
 */
void qq_on_show_memo(PurpleBlistNode *node, gpointer data);


#endif /* _BUDDY_MEMO_H */

