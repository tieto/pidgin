#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void do_trans(int, int);

int main(int argc, char **argv) {
	char *srcn;
	char *destn;
	int   src;
	int   dest;
	char *resp;


	printf("Gaim - Buddy List Translator\n");
	printf("----------------------------\n");

	if (argc != 3) {
		printf("Syntax: %s buddy.lst gaimlist\n", argv[0]);
		exit(0);
	}

	srcn  = argv[1];
	destn = argv[2];

	if ((src = open(srcn, O_RDONLY)) != -1) {
		printf("Source=%s, Dest=%s\n", srcn, destn);

		if ((dest = open(destn, O_WRONLY | O_CREAT | O_EXCL)) == -1) {
			printf("%s exists! Should I continue? ", destn);
			scanf("%s", resp);
			if (strchr(resp, 'y') || strchr(resp, 'Y')) {
				dest = open(destn, O_WRONLY | O_CREAT |
							O_TRUNC);
				do_trans(src, dest);
			} else
				exit(0);
		} else
			do_trans(src, dest);
		printf("Conversion Complete.\n");
	} else {
		printf("Source file must exist!\n\nSyntax: %s buddy.lst gaimlist\n", argv[0]);
		exit(0);
	}
	return 0;
}

void do_trans(int source, int destin) {
	FILE *src;
	FILE *dest;
	char  line[1024];

	umask(644);
	src  = fdopen(source, "r");
	dest = fdopen(destin, "w");

	fprintf(dest, "toc_set_config {m 1\n");
	while (fgets(line, sizeof line, src)) {
		line[strlen(line) - 1] = 0;
		if (strpbrk(line, "abcdefghijklmnopqrstuvwxyz"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ")) {
			char *field, *name;
			if (line[0] == ' ' || line[0] == '\t' ||
				line[0] == '\n' || line[0] == '\r' ||
				line[0] == '\f')
				field = strdup(line + 1);
			else
				field = strdup(line);
			name = strpbrk(field, " \t\n\r\f");
			name[0] = 0;
			name += 2;
			name[strlen(name) - 1] = 0;
			printf("%s, %s\n", field, name);
			if (!strcmp("group", field)) {
				fprintf(dest, "g %s\n", name);
			} else if (!strcmp("buddy", field)) {
				fprintf(dest, "b %s\n", name);
			}
		}
	}
	fprintf(dest, "}");
	fclose(src);
	fclose(dest);
}
