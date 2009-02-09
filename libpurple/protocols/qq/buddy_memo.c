
#include "buddy_memo.h"
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


#include<string.h>
#include<stdlib.h>
#include<stdio.h>


static const gchar* buddy_memo_txt[] = {
	"Name",
	"Mobile",
	"Telephone",
	"Address",
	"Email",
	"ZipCode",
	"Note"
};/* 备注信息的名称 */


/** 
 * 打印出好友备注信息
 * 
 * @param memo 
 */
static void buddy_memo_debug( gchar* memo[] );

/** 
 * 好友备注对话框中上传按钮的响应函数
 * 
 * @param info_request 
 * @param fields 
 */
static void buddy_memo_on_upload(void *info_request, PurpleRequestFields *fields);


static gchar** buddy_memo_init_data(  );



/** 
 * 弹出窗口显示好友备注信息
 * 
 * @param node 
 * @param buddy_data 
 */
static void qq_show_buddy_memo( void* node, void* buddy_data );





/** 
 * 向服务器发送更新好友信息请求
 * 
 * @param gc 
 * @param buddy_data 
 */
static void qq_request_buddy_memo_upload( PurpleBuddy * buddy );



/*********************************************************************************************/





void buddy_memo_on_upload(void *bd, PurpleRequestFields *fields)
{
	int index;
	PurpleBuddy *buddy;
	qq_buddy_data* buddy_data;
	int memoChanged;
	const char *utf8_str;
	buddy = ( PurpleBuddy* )bd;
	buddy_data = ( qq_buddy_data* )( buddy->proto_data );
	
	
	purple_debug_info("QQ", "update memo\n");
	memoChanged = 0;
	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		utf8_str = purple_request_fields_get_string(fields, buddy_memo_txt[index]);
		if( utf8_str == NULL ){
			if( buddy_data->memo[index] != NULL ){
				g_free( buddy_data->memo[index] );
				memoChanged = 1;
			}
			buddy_data->memo[index] = g_new0( gchar,1 );
		}
		else if( buddy_data->memo[index] == NULL ||
			strcmp( utf8_str, buddy_data->memo[index] ) != 0 )
		{
			if( buddy_data->memo[index] != NULL )
				g_free( buddy_data->memo[index] );
			buddy_data->memo[index] = g_new( gchar,strlen(utf8_str)+2 );
			strcpy( buddy_data->memo[index], utf8_str );
			memoChanged = 1;
			purple_debug_info( "QQ","%s=%s\n",buddy_memo_txt[index],utf8_str );
		}

	}
	if( memoChanged == 1 ){
		qq_request_buddy_memo_upload( buddy );
		purple_blist_alias_buddy( buddy,buddy_data->memo[QQ_BUDDY_MEMO_NAME] );
	}
	return;
}

void qq_request_buddy_memo_upload( PurpleBuddy * buddy )
{
	PurpleConnection* gc;
	qq_buddy_data* buddy_data;
	guint8* rawData;
	guint8* pos;
	int rawDataSize;
	int index;
	int memoItemSize[QQ_BUDDY_MEMO_SIZE];
	gchar* qqCharSetTxt[QQ_BUDDY_MEMO_SIZE];

	gc = buddy->account->gc;
	buddy_data = ( qq_buddy_data* )buddy->proto_data;
	purple_debug_info( "QQ","call qq_request_buddy_memo_download_upload\n" );
	rawDataSize = 7;
	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		qqCharSetTxt[index] = utf8_to_qq( buddy_data->memo[index], QQ_CHARSET_DEFAULT );
		memoItemSize[index] = strlen( qqCharSetTxt[index] );
		rawDataSize += memoItemSize[index]+1;
	}
	rawData = g_new0( guint8,rawDataSize );
	pos = rawData;
	(*pos) = QQ_BUDDY_MEMO_UPLOAD;
	pos += 2;
	(*(guint32*)pos) = htonl( buddy_data->uid );
	pos += 5;
	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		(*pos) = 0xff&memoItemSize[index];
		pos++;
		memcpy( pos, qqCharSetTxt[index], memoItemSize[index] );
		pos += memoItemSize[index];
	}

	qq_send_cmd( gc, QQ_CMD_BUDDY_MEMO, rawData, rawDataSize );
	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		g_free( qqCharSetTxt[index] );
	}
}



void qq_request_buddy_memo_download(PurpleConnection *gc, guint32 uid)
{
	gchar raw_data[16] = {0};
	unsigned int tmp;
	purple_debug_info("QQ", "Call qq_request_buddy_memo_download! qq number =%u\n", uid);
	g_return_if_fail(uid != 0);
	raw_data[0] = QQ_BUDDY_MEMO_GET;

	tmp = htonl((unsigned int)uid);
	memcpy(raw_data+1, &tmp, 4);
	
	qq_send_cmd(gc, QQ_CMD_BUDDY_MEMO, (guint8*)raw_data, 5);
}


void qq_process_get_buddy_memo( PurpleConnection *gc, guint8* data, gint len )
{
	qq_data *qd;
	//_buddy_memo memo;
	PurpleBuddy *buddy;
	gchar *who;
	qq_buddy_data* bd;
	guint8* pos;
	gint lenth;
	guint32 qq_number;
	int k;

	pos = data;
	switch( *pos ){
	    case QQ_BUDDY_MEMO_UPLOAD :
    	case QQ_BUDDY_MEMO_REMOVE :
			if( data[1] == QQ_BUDDY_MEMO_REQUEST_SUCCESS ){//显示服务器接受请求对话框
				//TODO:
				purple_debug_info( "QQ","服务器接受了请求\n" );
				purple_notify_message( gc,
									   PURPLE_NOTIFY_MSG_INFO,
									   _( "Your request was accepted" ),
									   _( "Your request was accepted" ),
									   _( "Your request was accepted" ),
									   NULL,
									   NULL);
			}
			break;
	    case QQ_BUDDY_MEMO_GET:
			qd = (qq_data *) gc->proto_data;
			pos++;
			qq_number = ntohl( *(uint32_t*)pos ); 
			pos += 5;//qq号后面有一个字节不知道什么作用
			who = uid_to_purple_name( qq_number );
			buddy = purple_find_buddy( gc->account, who );
			if (buddy == NULL || buddy->proto_data == NULL) {
				g_free(who);
				purple_debug_info("QQ", "Error Can not find %d!\n", qq_number);
				return;
			}
			bd = (qq_buddy_data *)buddy->proto_data;
			
			if( bd->memo == NULL ){
				bd->memo = g_new0( gchar*,QQ_BUDDY_MEMO_SIZE );
			}
			for( k=0; k<QQ_BUDDY_MEMO_SIZE; k++ ){
				lenth = pos[0];
				pos++;
				if( bd->memo[k] != NULL )
					g_free( bd->memo[k] );
				bd->memo[k] = qq_to_utf8_len( (gchar*)pos, lenth, QQ_CHARSET_DEFAULT );
				pos += lenth;
			}
			buddy_memo_debug( bd->memo );
			purple_blist_alias_buddy( buddy,
									  (const char*)bd->memo[QQ_BUDDY_MEMO_NAME] );//改名
			break;
	default:
			purple_debug_info( "QQ","error: unknown memo cmd\n" );
	        break;
	}
	
	
}

void buddy_memo_debug( gchar* memo[] )
{
	gint k=0;
	for( k=0; k<QQ_BUDDY_MEMO_SIZE; k++ ){
		purple_debug_info( "QQ","备注： %s=%s\n",buddy_memo_txt[k],memo[k] );
	}
}

void qq_show_buddy_memo( void* node, void* buddy_data )
{
	qq_buddy_data* data;
	PurpleRequestField *field;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	int index;
	
	data = ( qq_buddy_data* )buddy_data;
	
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		if( data->memo == NULL ){
			data->memo = buddy_memo_init_data(  );
		}
		field = purple_request_field_string_new(buddy_memo_txt[index],
												buddy_memo_txt[index],
												data->memo[index],
												FALSE);
		purple_request_field_group_add_field(group, field);	
	}
	
	purple_request_fields(node,
						  _( "Buddy_memo" ),
						  "",
						  NULL,
						  fields,
						  _("_Upload"), G_CALLBACK(buddy_memo_on_upload),
						  _("_Cancel"), NULL,
						  ((PurpleBuddy *)node)->account, ((PurpleBuddy *)node)->name, NULL,
						  node);
}



void qq_on_show_memo(PurpleBlistNode *node, gpointer data)
{

	PurpleBuddy *buddy;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;

	qq_show_buddy_memo( node, buddy->proto_data );

	
	purple_debug_info( "QQ","show memo" );
}


static gchar** buddy_memo_init_data(  )
{
	gchar** pmemo;
	int index;
	pmemo = g_new0( gchar*,QQ_BUDDY_MEMO_SIZE );
	for( index=0; index<QQ_BUDDY_MEMO_SIZE; index++ ){
		pmemo[index] = g_new0( gchar,1 );
	}
	return pmemo;
}
