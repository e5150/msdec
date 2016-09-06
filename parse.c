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
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "message.h"
#include "parse.h"
#include "util.h"

static bool
tok_is_df(const char *tok, size_t len) {
	return len == 4 
	 && tok[0] == 'D' && tok[1] == 'F'
	 && tok[2] <= '9' && tok[2] >= '0'
	 && tok[3] <= '9' && tok[3] >= '0';
}

static bool
is_type_of_len(const char *s, size_t len, int (*checker)(int)) {
	size_t i;

	for (i = 0; i < len; ++i)
		if (!checker(s[i]))
			return false;
	return true;
}

static bool
tok_is_time(const char *tok, size_t len) {
	if (len < 9 || len > 10) /* 1973 - 2286 */
		return false;
	return is_type_of_len(tok, len, isdigit);
}

static bool
tok_is_msg(const char *tok, size_t len) {
	if (len != 14 && len != 28)
		return false;
	return is_type_of_len(tok, len, isxdigit);
}

static bool
tok_is_icao_addr(const char *tok, size_t len) {
	if (len != 6)
		return false;
	return is_type_of_len(tok, len, isxdigit);
}

static bool
tok_is_dump_msg(const char *tok, size_t len) {
	if (len > 2 && tok[0] == '*' && tok[len - 1] == ';') {
		return tok_is_msg(tok + 1, len - 2);
	}
	return false;
}

static uint8_t
strtohex(uint8_t x) {
	if      (x <= '9') return x - '0';
	else if (x <= 'F') return x - 'A' + 10;
	else if (x <= 'f') return x - 'a' + 10;
	return 0xFF;
}

struct ms_msg_t *
line_to_msg(const char *orig_line) {
	char *tok;
	char *sep;
	char *line;
	size_t n;
	struct ms_msg_t *msg = NULL;
	bool have_addr = false;;
	bool have_time = false;

	uint8_t *raw = NULL;
	uint8_t msg_type;
	time_t msg_time = 0;
	size_t msg_len = 0;
	uint32_t addr = 0xFF000000;

	line = strdup(orig_line);

	for (tok = line, n = 0; tok; ++n, tok = sep ? sep + 1 : NULL) {
		size_t len;

		if ((sep = strchr(tok, ':')))
			*sep = '\0';

		if (!(len = strlen(tok))) {
			continue;
		}

		if (n == 0 && tok_is_df(tok, len)) {
			continue;
		}

		if (!have_time && tok_is_time(tok, len)) {
			have_time = true;
			msg_time = atol(tok);
			continue;
		}

		if (!have_addr && tok_is_icao_addr(tok, len)) {
			have_addr = true;
			addr = (strtohex(tok[0]) << 20)
			     | (strtohex(tok[1]) << 16)
			     | (strtohex(tok[2]) << 12)
			     | (strtohex(tok[3]) << 8)
			     | (strtohex(tok[4]) << 4)
			     | (strtohex(tok[5]) << 0);
			continue;
		}


		if (tok_is_dump_msg(tok, len)) {
			tok += 1;
			len -= 2;
		}
		if (!raw && tok_is_msg(tok, len)) {
			char *src = tok;
			uint8_t *dst;
			size_t i;

			msg_len = len / 2;
			raw = malloc(msg_len);
			dst = raw;


			for (i = 0; i < msg_len; ++i) {
				uint8_t a, b;

				a = strtohex(*src++);
				b = strtohex(*src++);

				dst[i] = (a << 4) | (b);
			}
		}

	}

	if (!raw) {
		free(line);
		return NULL;
	}

	msg_type = raw[0] >> 3;
	
	if (df_to_len(msg_type) != (ssize_t)msg_len) {
		free(raw);
		free(line);
		return NULL;
	}

	msg = mk_msg(raw, msg_time, addr);

	free(raw);
	free(line);
	return msg;
}

struct ms_msg_t *
parse_file(const char *filename, off_t *offset, struct ms_aircraft_t **aircrafts) {
	struct ms_msg_t *msgs = NULL;
	struct ms_msg_t *last = NULL;
	FILE *fp = NULL;
	size_t len = 0;
	char *line = NULL;
	ssize_t bytes;

	errno = 0;

	if (filename) {
		if (!(fp = fopen(filename, "r"))) {
			return NULL;
		}
		if (offset && fseek(fp, *offset, SEEK_SET) == -1) {
			return NULL;
		}
	} else {
		fp = stdin;
	}

	while ((bytes = getline(&line, &len, fp)) > 0) {
		struct ms_msg_t *msg;

		if (line[bytes - 1] == '\n')
			line[bytes - 1] = '\0';

		if ((msg = line_to_msg(line))) {
			if (aircrafts) {
				struct ms_aircraft_t *a;

				if ((a = find_aircraft(msg->addr, *aircrafts))) {
					update_aircraft(a, msg);
				} else {
					a = mk_aircraft(msg->addr);
					update_aircraft(a, msg);
					a->next = *aircrafts;
					*aircrafts = a;
				}
			}
			msg->next = NULL;
			if (last) {
				last->next = msg;
			} else {
				msgs = msg;
			}
			last = msg;
		}

	}

	/*
	 * TODO: Check for errors on fp, without
	 * having to free msgs and returning NULL.
	 */

	free(line);

	if (fp != stdin) {
		if (offset)
			*offset = ftell(fp);

		fclose(fp);
	}

	return msgs;
}
