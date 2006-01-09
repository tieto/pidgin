#include "module.h"

MODULE = Gaim::Buddy::Icon PACKAGE = Gaim::Buddy::Icon   PREFIX = gaim_buddy_icon_
PROTOTYPES: ENABLE

void
gaim_buddy_icon_destroy(icon)
	Gaim::Buddy::Icon icon

Gaim::Buddy::Icon
gaim_buddy_icon_ref(icon)
	Gaim::Buddy::Icon icon

Gaim::Buddy::Icon
gaim_buddy_icon_unref(icon)
	Gaim::Buddy::Icon icon

void
gaim_buddy_icon_update(icon)
	Gaim::Buddy::Icon icon

void
gaim_buddy_icon_cache(icon, buddy)
	Gaim::Buddy::Icon icon
	Gaim::BuddyList::Buddy buddy

void
gaim_buddy_icon_set_account(icon, account)
	Gaim::Buddy::Icon icon
	Gaim::Account account

void
gaim_buddy_icon_set_username(icon, username)
	Gaim::Buddy::Icon icon
	const char * username

void
gaim_buddy_icon_set_data(icon, data, len)
	Gaim::Buddy::Icon icon
	void * data
	size_t len

Gaim::Account
gaim_buddy_icon_get_account(icon)
	Gaim::Buddy::Icon icon

const char *
gaim_buddy_icon_get_username(icon)
	Gaim::Buddy::Icon icon

const void *
gaim_buddy_icon_get_data(icon, len)
	Gaim::Buddy::Icon icon
	size_t &len

const char *
gaim_buddy_icon_get_type(icon)
	Gaim::Buddy::Icon icon

void
gaim_buddy_icon_get_scale_size(spec, width, height)
	Gaim::Buddy::Icon::Spec spec
	int *width
	int *height

MODULE = Gaim::Buddy::Icon PACKAGE = Gaim::Buddy::Icons   PREFIX = gaim_buddy_icons_
PROTOTYPES: ENABLE

void
gaim_buddy_icons_set_caching(caching)
	gboolean caching

gboolean
gaim_buddy_icons_is_caching()

void
gaim_buddy_icons_set_cache_dir(cache_dir)
	const char *cache_dir

const char *
gaim_buddy_icons_get_cache_dir();

void *
gaim_buddy_icons_get_handle();

void
gaim_buddy_icons_init();

void
gaim_buddy_icons_uninit()
