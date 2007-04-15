#include "module.h"

MODULE = Purple::ImgStore  PACKAGE = Purple::ImgStore  PREFIX = purple_imgstore_
PROTOTYPES: ENABLE

int 
purple_imgstore_add(data, size, filename)
	void *data
	size_t size
	const char *filename

Purple::StoredImage
purple_imgstore_get(id)
	int id

gpointer 
purple_imgstore_get_data(i)
	Purple::StoredImage i

const char *
purple_imgstore_get_filename(i)
	Purple::StoredImage i

size_t 
purple_imgstore_get_size(i)
	Purple::StoredImage i

void 
purple_imgstore_ref(id)
	int id

void 
purple_imgstore_unref(id)
	int id

