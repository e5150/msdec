/*
 * Copyright © 2016 Lars Lindqvist <lars.lindqvist at yandex.ru>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright © 2016 Lars Lindqvist <lars.lindqvist at yandex.ru>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <poll.h>

#include <arg.h>

#include "inot.h"

#define CONF_MSFILES
#include "config.h"

static void
usage() {
	printf("usage: %s [-f filename]\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[]) {
	int err = 0;
	const char *filename = default_outfile;
	int ifd = -1;
	int iwfd = -1;
	size_t len = 0;
	char *line = NULL;
	off_t offset = 0;
	struct stat st;

	ARGBEGIN {
	case 'f':
		filename = EARGF(usage());
		break;
	} ARGEND;

	if (stat(filename, &st) < 0) {
		fprintf(stderr, "%s: FATAL: stat %s: %s\n",
		        argv0, filename, strerror(errno));
		exit(1);
	}
	offset = st.st_size;

	if (init_inotify(filename, &ifd, &iwfd)) {
		fprintf(stderr, "%s: FATAL: inotify %s: %s\n",
		        argv0, filename, strerror(errno));
		exit(1);
	}

	while (1) {
		ssize_t bytes;
		FILE *fp;

		if (!(fp = fopen(filename, "r"))
		|| fseek(fp, offset, SEEK_SET) == -1) { 
			err += 1;
			break;
		}

		while ((bytes = getline(&line, &len, fp)) > 0) {
			if (bytes == 38)
				line[30] = 0;
			else if (bytes == 52)
				line[44] = 0;
			else
				continue;

			printf("*%s;\n", line + 16);
		}

		offset = ftell(fp);
		fclose(fp);

		if (wait_for_inotify(ifd) < 0) {
			err += 1;
			break;
		}
	}

	if (line)
		free(line);

	if (ifd != -1)
		close(ifd);
	if (iwfd != -1)
		close(iwfd);


	return err ? 1 : 0;
}










