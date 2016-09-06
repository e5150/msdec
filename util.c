/*
 * Copyright © 2016 Lars Lindqvist <lars.lindqvist at yandex.ru>
 * Copyright 2011 Bert Muennich
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "util.h"

void* emalloc(size_t size)
{
	void *ptr;
	
	ptr = malloc(size);
	if (ptr == NULL)
		error(EXIT_FAILURE, errno, NULL);
	return ptr;
}

void* erealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL)
		error(EXIT_FAILURE, errno, NULL);
	return ptr;
}

char* estrdup(const char *s)
{
	char *d;
	size_t n = strlen(s) + 1;

	d = malloc(n);
	if (d == NULL)
		error(EXIT_FAILURE, errno, NULL);
	memcpy(d, s, n);
	return d;
}


void size_readable(float *size, const char **unit)
{
	const char *units[] = { "", "K", "M", "G" };
	int i;

	for (i = 0; i < ARRLEN(units) && *size > 1024.0; i++)
		*size /= 1024.0;
	*unit = units[MIN(i, ARRLEN(units) - 1)];
}

int r_opendir(r_dir_t *rdir, const char *dirname)
{
	if (*dirname == '\0')
		return -1;

	if ((rdir->dir = opendir(dirname)) == NULL) {
		rdir->name = NULL;
		rdir->stack = NULL;
		return -1;
	}

	rdir->stcap = 512;
	rdir->stack = (char**) emalloc(rdir->stcap * sizeof(char*));
	rdir->stlen = 0;

	rdir->name = (char*) dirname;
	rdir->d = 0;

	return 0;
}

int r_closedir(r_dir_t *rdir)
{
	int ret = 0;

	if (rdir->stack != NULL) {
		while (rdir->stlen > 0)
			free(rdir->stack[--rdir->stlen]);
		free(rdir->stack);
		rdir->stack = NULL;
	}

	if (rdir->dir != NULL) {
		if ((ret = closedir(rdir->dir)) == 0)
			rdir->dir = NULL;
	}

	if (rdir->d != 0) {
		free(rdir->name);
		rdir->name = NULL;
	}

	return ret;
}

char* r_readdir(r_dir_t *rdir)
{
	size_t len;
	char *filename;
	struct dirent *dentry;
	struct stat fstats;

	while (1) {
		if (rdir->dir != NULL && (dentry = readdir(rdir->dir)) != NULL) {
			if (dentry->d_name[0] == '.')
				continue;

			len = strlen(rdir->name) + strlen(dentry->d_name) + 2;
			filename = (char*) emalloc(len);
			snprintf(filename, len, "%s%s%s", rdir->name,
			         rdir->name[strlen(rdir->name)-1] == '/' ? "" : "/",
			         dentry->d_name);

			if (stat(filename, &fstats) < 0)
				continue;
			if (S_ISDIR(fstats.st_mode)) {
				/* put subdirectory on the stack */
				if (rdir->stlen == rdir->stcap) {
					rdir->stcap *= 2;
					rdir->stack = (char**) erealloc(rdir->stack,
					                                rdir->stcap * sizeof(char*));
				}
				rdir->stack[rdir->stlen++] = filename;
				continue;
			}
			return filename;
		}
		
		if (rdir->stlen > 0) {
			/* open next subdirectory */
			closedir(rdir->dir);
			if (rdir->d != 0)
				free(rdir->name);
			rdir->name = rdir->stack[--rdir->stlen];
			rdir->d = 1;
			if ((rdir->dir = opendir(rdir->name)) == NULL)
				error(0, errno, "%s", rdir->name);
			continue;
		}
		/* no more entries */
		break;
	}
	return NULL;
}

int r_mkdir(char *path)
{
	char c, *s = path;
	struct stat st;

	while (*s != '\0') {
		if (*s == '/') {
			s++;
			continue;
		}
		for (; *s != '\0' && *s != '/'; s++);
		c = *s;
		*s = '\0';
		if (mkdir(path, 0755) == -1)
			if (errno != EEXIST || stat(path, &st) == -1 || !S_ISDIR(st.st_mode))
				return -1;
		*s = c;
	}
	return 0;
}
uint8_t
get_msgtype(uint8_t first_byte) {
	uint8_t x = first_byte >> 3;

	if ((x & 0x18) == 0x18) {
		return 24;
	} else {
		return x;
	}
}

int
df_to_len(uint8_t df) {
        if (df > 31) {
                /* invalid 5-bit DF */
                return -1;
        }
        switch (df) {
        case  0:
        case  4:
        case  5:
        case 11:
                return 7;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 24:
                return 14;
        default:
        	if ((df & 0x18) == 0x18)
        		/* DF24 */
        		return 14;
                return 0;
        }
}
