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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "df11.h"

/*
 *  DF 11 - All-Call Reply
 *  [1] 3.1.2.5.2.2
 *                            
 *           1         2         3         4         5
 *  1234567890123456789012345678901234567890123456789012345
 *  DF   CA AA                      PI
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6  
 *
 */
struct ms_DF11_t *
mk_DF11(const uint8_t *msg) {
	struct ms_DF11_t *ret;

	ret = calloc(1, sizeof(struct ms_DF11_t));
	
	ret->DF = msg[0] >> 3;
	ret->PI = (msg[4] << 16)
	        | (msg[5] <<  8)
	        | (msg[6] <<  0);

	/* CA - Capability, bits 6-8 */
	ret->CA = msg[0] & 0x07;
	/* AA - Address announced, bit 9-32 */
	ret->AA.ICAO = true;
	ret->AA.addr = (msg[1] << 16)
	             | (msg[2] <<  8)
	             | (msg[3] <<  0);

	return ret;
}

void 
pr_DF11(FILE *fp, struct ms_DF11_t *p, int v) {
	fprintf(fp, "DF11:All-call reply\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,  5, p->DF, "DF");
		pr_raw(fp,  6,  8, p->CA, "CA");
		pr_raw(fp,  9, 32, p->AA.addr, "AA");
		pr_raw(fp, 33, 56, p->PI, "PI");
		pr_raw_footer(fp);
	} else {
		pr_CA(fp, p->CA);
		pr_AA(fp, p->AA);
	}
}
