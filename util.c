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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "util.h"

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
