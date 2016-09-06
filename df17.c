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

#include "df17.h"
#include "es.h"

/*
 *  DF 17 - Extended squitter - [1] 3.1.2.8.6
 *                            
 *           1         2         3         4         5        8  11
 *  1234567890123456789012345678901234567890123456789012345...8...1
 *  DF   CA AA                      ME                         PI
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *
 */
struct ms_DF17_t *
mk_DF17(const uint8_t *msg) {
	struct ms_DF17_t *ret;

	ret = calloc(1, sizeof(struct ms_DF17_t));

	ret->DF = msg[0] >> 3;
	ret->PI = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/* CA - Capability 3-bits */
	ret->CA = msg[0] & 0x07;

	/* AA - Address announced, bit 9-32 */
	ret->AA.ICAO = true;
	ret->AA.addr = (msg[1] << 16)
	             | (msg[2] <<  8)
	             | (msg[3] <<  0);

	/*
	 * ME - extended squitter
	 * [3] B.2.3.1
	 */
	mk_ES_TYPE(&ret->ES_TYPE, msg[4]);
	ret->aux = mk_extended_squitter(msg + 4, ret->ES_TYPE.et);
	
	return ret;
}

void
pr_DF17(FILE *fp, struct ms_DF17_t *p, int v) {
	fprintf(fp, "DF17:Extended squitter\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,   5, p->DF, "DF");
		pr_raw(fp,  6,   8, p->CA, "CA");
		pr_raw(fp,  9,  32, p->AA.addr, "AA");
		pr_raw_subfield_header(fp, 33, 88, "Extended squitter");
		pr_extended_squitter(fp, p->aux, &p->ES_TYPE, v);
		pr_raw_subfield_footer(fp);
		pr_raw(fp, 89, 112, p->PI, "PI");
		pr_raw_footer(fp);
	} else {
		pr_CA(fp, p->CA);
		pr_AA(fp, p->AA);
		pr_extended_squitter(fp, p->aux, &p->ES_TYPE, v);
	}
}
