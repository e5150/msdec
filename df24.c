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

#include "df24.h"
#include "fields.h"

/*
 *  DF 24 - Comm-D 
 *  [1] 3.1.2.7.3
 *                            
 *           1         2         3         4         5         8  11
 *  12345678901234567890123456789012345678901234567890123456...8...1
 *  D -KN   M                                                  A
 *  F -ED   D                                                  P
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *
 */
struct ms_DF24_t *
mk_DF24(const uint8_t *msg) {
	struct ms_DF24_t *ret;

	ret = calloc(1, sizeof(struct ms_DF24_t));
	
	ret->DF = msg[0] >> 6;
	ret->AP = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/* KE - Control, ELM */
	ret->KE = msg[0] & 0x10;

	/* ND - Number of D-segment */
	ret->ND = msg[0] & 0x0C;

	/* MD - Message, Comm-D */
	/*ret->MD = mk_MD(msg + 1);*/
	return ret;
}

void
pr_DF24(FILE *fp, struct ms_DF24_t *p, int v) {
	fprintf(fp, "DF24:Comm-D extended message:TODO\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,   2, p->DF, "DF");
		pr_raw(fp,  4,   4, p->KE, "KE");
		pr_raw(fp,  5,   8, p->ND, "ND");
		/* MD, TODO */
		pr_raw(fp, 89, 112, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_KE(fp, p->KE);
		pr_ND(fp, p->ND);
		/* MD, TODO */
	}
}
