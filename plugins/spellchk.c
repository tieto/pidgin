#define GAIM_PLUGINS
#include "gaim.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

struct replace_words {
	char *bad;
	char *good;
};

GList *words = NULL;

int num_words(char *);
int get_word(char *, int);
char *have_word(char *, int);
void substitute(char **, int, int, char *);

void spell_check(char *who, char **message, void *m) {
	int i, l;
	int word;
	GList *w;
	char *tmp;

	if (message == NULL || *message == NULL)
		return;

	l = num_words(*message);
	for (i = 0; i < l; i++) {
		word = get_word(*message, i);
		w = words;
		while (w) {
			struct replace_words *r;
			r = (struct replace_words *)(w->data);
			tmp = have_word(*message, word);
			if (!strcmp(have_word(*message, word), r->bad))
				substitute(message, word, strlen(r->bad),
						r->good);
			free(tmp);
			w = w->next;
		}
	}
}

void gaim_plugin_init(void *handle) {
	struct replace_words *p;

	p = malloc(sizeof *p);
	p->bad = "definately";
	p->good = "definitely";
	words = g_list_append(words, p);

	p = malloc(sizeof *p);
	p->bad = "u";
	p->good = "you";
	words = g_list_append(words, p);

	p = malloc(sizeof *p);
	p->bad = "r";
	p->good = "are";
	words = g_list_append(words, p);

	p = malloc(sizeof *p);
	p->bad = "teh";
	p->good = "the";
	words = g_list_append(words, p);

	gaim_signal_connect(handle, event_im_send, spell_check, NULL);
}

char *name() {
	return "IM Spell Check";
}

char *description() {
	return "Watches outgoing IM text and corrects common spelling errors.";
}

int num_words(char *m) {
	int count = 0;
	int pos;
	int state = 0;

	for (pos = 0; pos < strlen(m); pos++) {
		switch (state) {
		case 0: /* expecting word */
			if (isalnum(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1: /* inside word */
			if (isspace(m[pos]))
				state = 0;
			break;
		case 2: /* inside HTML tag */
			if (m[pos] == '>')
				state = 0;
			break;
		}
	}
	return count;
}

int get_word(char *m, int word) {
	int count = 0;
	int pos = 0;
	int state = 0;

	for (pos = 0; pos < strlen(m) && count <= word; pos++) {
		switch (state) {
		case 0:
			if (isalnum(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1:
			if (isspace(m[pos]))
				state = 0;
			break;
		case 2:
			if (m[pos] == '>')
				state = 0;
			break;
		}
	}
	return pos - 1;
}

char *have_word(char *m, int pos) {
	char *tmp = strpbrk(&m[pos], "' \t\f\r\n");
	int len = (int)(tmp - &m[pos]);

	if (tmp == NULL) {
		tmp = strdup(&m[pos]);
		return tmp;
	}

	tmp = malloc(len + 1);
	tmp[0] = 0;
	strncat(tmp, &m[pos], len);

	return tmp;
}

void substitute(char **mes, int pos, int m, char *text) {
	char *new = malloc(strlen(*mes) + strlen(text) + 1);
	char *tmp;
	new[0] = 0;

	strncat(new, *mes, pos);
	strcat(new, text);

	strcat(new, &(*mes)[pos + m]);
	tmp = *mes;
	*mes = new;
	free(tmp);
}
