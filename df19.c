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

#include "df19.h"
#include "es.h"
#include "bds_f2.h"

/*
 * DF 19 - Extended squitter, military application
 *  [1] 3.1.2.8.8
 *  [2] Chapter 7
 *  [5] Complete
 *
 *           1         2         3         4         5        8   11
 *  12345678901234567890123456789012345678901234567890123456..8....1
 *  DF   AF <                                                 >
 *       000[AA                    ][ME                       ][PI ]
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *
 */
struct ms_DF19_t *
mk_DF19(const uint8_t *msg) {
	struct ms_DF19_t *ret;

	ret = calloc(1, sizeof(struct ms_DF19_t));
	
	ret->DF = msg[0] >> 3;
	ret->PI = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/* AF - Application Field */
	ret->AF = msg[0] & 0x07;

	switch (ret->AF) {
	case 0:
		/*
		 * AF = 0 - Reserved for civilian ES formats [2] 7.2
		 * According to [5], this seems to be how it would
		 * be laid out: [DF-5][AF-3][AA-24][ME-56][PI-24]
		 */
		ret->AA.ICAO = true;
		ret->AA.addr = (msg[1] << 16)
			     | (msg[2] <<  8)
			     | (msg[3] <<  0);
		ret->aux = mk_extended_squitter(msg + 4, ret->ES_TYPE.et);
		mk_ES_TYPE(&ret->ES_TYPE, msg[4]);
		break;
	case 1:
		/* Reserved for formation flight [2] 7.2 */
		break;
	case 2:
		/*
		 * AF = 2 - Reserved for military applications [2] 7.2
		 * So, this should contain a BDS F,2, where in the message,
		 * I don't know, starting at msg[4] or msg[1] seems likely…
		 * But since bits 1-5 of BDS F,2 should be 00001, according
		 * to [3] Table A-2-242, check all byte boundaries where
		 * a 56 bit BDS would fit.
		 */
#define TRY_MK_F2_AT(x)                                     \
		if ((msg[(x)] >> 3) == 1)                   \
			ret->aux = mk_BDS_F2(msg + (x));    \
		if (ret->aux)                               \
			break /* [sic] no semi-colon */

		TRY_MK_F2_AT(4);
		TRY_MK_F2_AT(1);
		TRY_MK_F2_AT(2);
		TRY_MK_F2_AT(3);
		TRY_MK_F2_AT(5);
		TRY_MK_F2_AT(6);
		TRY_MK_F2_AT(7);
		
		break;
	default:
		/* Reserved */
		{}
	}

	return ret;
}

void
pr_DF19(FILE *fp, struct ms_DF19_t *p, int v) {
	fprintf(fp, "DF19:Extended squitter, military application:TODO\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp, 1, 5, p->DF, "DF");
		pr_raw(fp, 6, 8, p->AF, "AF");
		pr_raw_subfield_header(fp, 33, 88, "ES / Military ?");
	} else {
		pr_AF(fp, p->AF);
	}

	switch (p->AF) {
	case 0:
		pr_AA(fp, p->AA);
		if (p->aux)
			pr_extended_squitter(fp, p->aux, &p->ES_TYPE, v);
		break;
	case 1:
		fprintf(fp, "AF=1:Formation flight:TODO\n");
		break;
	case 2:
		if (p->aux)
			pr_BDS_F2(fp, p->aux, v);
		else
			fprintf(fp, "AF=2:Military application:TODO\n");
		break;
	
	}

	if (v == 1) {
		pr_raw_subfield_footer(fp);
		pr_raw(fp, 89, 112, p->PI, "PI");
		pr_raw_footer(fp);
	}
}
