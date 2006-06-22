#include <glib.h>
#include "gntwidget.h"
#include "gntcolors.h"
#include "gntkeys.h"

/* XXX: Find a better place for this */
#define SCROLL_HEIGHT	4096
#define SCROLL_WIDTH 512

void gnt_init();

void gnt_main();

void gnt_screen_take_focus(GntWidget *widget);

void gnt_screen_remove_widget(GntWidget *widget);

