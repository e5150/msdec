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

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "crc.h"

static uint8_t
strtohex(uint8_t x) {
	if      (x <= '9') return x - '0';
	else if (x <= 'F') return x - 'A' + 10;
	else if (x <= 'f') return x - 'a' + 10;
	return 0xFF;
}

int
main(int argc, char *argv[]) {
	int i;

	for (i = 1; i < argc; ++i) {
		uint8_t msg[14];
		size_t j, len = strlen(argv[i]) / 2;
		uint8_t *p = (uint8_t *)argv[i];
		uint32_t crc;
		uint32_t sum; 

		if (len != 7 && len != 14)
			return 1;
		if (strlen(argv[i]) % 2)
			return 1;

		for (j = 0; j < len; ++j) {
			if (!isxdigit(*p) || !isxdigit(*(p+1)))
				return 1;
			uint8_t a = strtohex(*p++);
			uint8_t b = strtohex(*p++);
			msg[j] = (a << 4) | b;
		}

		crc = checksum(msg, len);
		sum = (msg[len - 3] << 16)
		    | (msg[len - 2] <<  8)
		    | (msg[len - 1] <<  0);
		printf("%s:%06X:%06X:%06X\n",argv[i], crc, sum, crc^sum);


	}
	
	return 0;
}
