#include <ncurses.h>

int main()
{
	int ch;
	initscr();

	noecho();

	while ((ch = getch()) != 27) {
		printw("%d ", ch);
		refresh();
	}

	endwin();
	return 0;
}

