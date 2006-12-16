#include "module.h"

MODULE = Gaim::ImgStore  PACKAGE = Gaim::ImgStore  PREFIX = gaim_imgstore_
PROTOTYPES: ENABLE

int 
gaim_imgstore_add(data, size, filename)
	void *data
	size_t size
	const char *filename

Gaim::StoredImage
gaim_imgstore_get(id)
	int id

gpointer 
gaim_imgstore_get_data(i)
	Gaim::StoredImage i

const char *
gaim_imgstore_get_filename(i)
	Gaim::StoredImage i

size_t 
gaim_imgstore_get_size(i)
	Gaim::StoredImage i

void 
gaim_imgstore_ref(id)
	int id

void 
gaim_imgstore_unref(id)
	int id

