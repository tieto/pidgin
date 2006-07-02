#include <glib.h>
#include "gntwidget.h"
#include "gntcolors.h"
#include "gntkeys.h"

void gnt_init();

void gnt_main();

void gnt_screen_occupy(GntWidget *widget);

void gnt_screen_release(GntWidget *widget);

void gnt_screen_update(GntWidget *widget);

void gnt_screen_take_focus(GntWidget *widget);

gboolean gnt_widget_has_focus(GntWidget *widget);

void gnt_widget_set_urgent(GntWidget *widget);

void gnt_quit();

