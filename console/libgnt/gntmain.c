#include "gnt.h"
#include "gntkeys.h"
#include "gntcolors.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

static GList *focus_list;
static int max_x;
static int max_y;

void gnt_screen_take_focus(GntWidget *widget)
{
	focus_list = g_list_prepend(focus_list, widget);
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	focus_list = g_list_remove(focus_list, widget);
	if (focus_list)
		gnt_widget_draw(focus_list->data);
}

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char buffer[256];

	int rd = read(0, buffer, sizeof(buffer) - 1);
	if (rd < 0)
	{
		endwin();
		printf("ERROR!\n");
		exit(1);
	}
	else if (rd == 0)
	{
		endwin();
		printf("EOF\n");
		exit(1);
	}

	buffer[rd] = 0;

	if (focus_list)
	{
		gboolean ret = FALSE;
		/*g_signal_emit_by_name(focus_list->data, "key_pressed", buffer, &ret);*/
		ret = gnt_widget_key_pressed(focus_list->data, buffer);
	}

	if (buffer[0] == 27)
	{
		/* Some special key has been pressed */
		if (strcmp(buffer+1, GNT_KEY_POPUP) == 0)
		{
			/*printf("popup\n");*/
		}
		else
		{
			/*printf("Unknown: %s\n", buffer+1);*/
		}
	}
	else
	{
		if (buffer[0] == 'q')
		{
			endwin();
			exit(1);
		}
		/*printf("%s\n", buffer);*/
	}
	refresh();

	return TRUE;
}

void gnt_init()
{
	GIOChannel *channel = g_io_channel_unix_new(0);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );

	int result = g_io_add_watch(channel, 
					(G_IO_IN | G_IO_HUP | G_IO_ERR),
					io_invoke, NULL);

	setlocale(LC_ALL, "");
	initscr();
	start_color();
	gnt_init_colors();

	max_x = getmaxx(stdscr);
	max_y = getmaxy(stdscr);

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	noecho();
	refresh();
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	g_type_init();
}

void gnt_main()
{
	GMainLoop *loop = g_main_new(FALSE);
	g_main_run(loop);
}

