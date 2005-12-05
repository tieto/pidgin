#include "module.h"

MODULE = Gaim::Xfer  PACKAGE = Gaim::Xfer  PREFIX = gaim_xfer_
PROTOTYPES: ENABLE

Gaim::Xfer
gaim_xfer_new(class, account, type, who)
	Gaim::Account account
	Gaim::XferType type
	const char *who
    C_ARGS:
	account, type, who

void 
gaim_xfer_add(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_cancel_local(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_cancel_remote(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_end(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_error(type, account, who, msg)
	Gaim::XferType type
	Gaim::Account account
	const char *who
	const char *msg

Gaim::Account
gaim_xfer_get_account(xfer)
	Gaim::Xfer xfer

size_t 
gaim_xfer_get_bytes_remaining(xfer)
	Gaim::Xfer xfer

size_t 
gaim_xfer_get_bytes_sent(xfer)
	Gaim::Xfer xfer

const char *
gaim_xfer_get_filename(xfer)
	Gaim::Xfer xfer

const char *
gaim_xfer_get_local_filename(xfer)
	Gaim::Xfer xfer

unsigned int 
gaim_xfer_get_local_port(xfer)
	Gaim::Xfer xfer

double 
gaim_xfer_get_progress(xfer)
	Gaim::Xfer xfer

const char *
gaim_xfer_get_remote_ip(xfer)
	Gaim::Xfer xfer

unsigned int 
gaim_xfer_get_remote_port(xfer)
	Gaim::Xfer xfer

size_t 
gaim_xfer_get_size(xfer)
	Gaim::Xfer xfer

Gaim::XferStatusType
gaim_xfer_get_status(xfer)
	Gaim::Xfer xfer

Gaim::XferType
gaim_xfer_get_type(xfer)
	Gaim::Xfer xfer

Gaim::XferUiOps
gaim_xfer_get_ui_ops(xfer)
	Gaim::Xfer xfer

gboolean 
gaim_xfer_is_canceled(xfer)
	Gaim::Xfer xfer

gboolean 
gaim_xfer_is_completed(xfer)
	Gaim::Xfer xfer

ssize_t 
gaim_xfer_read(xfer, buffer)
	Gaim::Xfer xfer
	guchar **buffer

void 
gaim_xfer_ref(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_request(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_request_accepted(xfer, filename)
	Gaim::Xfer xfer
	const char *filename

void 
gaim_xfer_request_denied(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_set_completed(xfer, completed)
	Gaim::Xfer xfer
	gboolean completed

void 
gaim_xfer_set_filename(xfer, filename)
	Gaim::Xfer xfer
	const char *filename

void 
gaim_xfer_set_local_filename(xfer, filename)
	Gaim::Xfer xfer
	const char *filename

void 
gaim_xfer_set_message(xfer, message)
	Gaim::Xfer xfer
	const char *message

void 
gaim_xfer_set_size(xfer, size)
	Gaim::Xfer xfer
	size_t size

void 
gaim_xfer_unref(xfer)
	Gaim::Xfer xfer

void 
gaim_xfer_update_progress(xfer)
	Gaim::Xfer xfer

ssize_t 
gaim_xfer_write(xfer, buffer, size)
	Gaim::Xfer xfer
	const guchar *buffer
	size_t size

MODULE = Gaim::Xfer  PACKAGE = Gaim::Xfers  PREFIX = gaim_xfers_
PROTOTYPES: ENABLE

Gaim::XferUiOps
gaim_xfers_get_ui_ops()
 

void 
gaim_xfers_set_ui_ops(ops)
	Gaim::XferUiOps ops

