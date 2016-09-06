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

#include "fields.h"
#include "df00.h"
#include "mac.h"

/*
 *  DF 0 - Short Air-Air Surveillance
 *  [1] 3.1.2.8.2
 *                            
 *           1         2         3         4         5
 *  1234567890123456789012345678901234567890123456789012345
 *  D    VC-S  --R   --A            A
 *  F    SC-L  --I   --C            P
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6  
 *
 */
struct ms_DF00_t *
mk_DF00(const uint8_t *msg) {
	struct ms_DF00_t *ret;

	ret = calloc(1, sizeof(struct ms_DF00_t));

	/* VS - Vertical status, bit 6 */
	ret->VS = msg[0] & 0x04;
	/* CC - Cross-link capability, bit 7 */
	ret->CC = msg[0] & 0x02;
	/* SL - Sensitivity level, bit 9-11 */
	ret->SL = (msg[1] >> 5) & 0x07;
	/* RI - Reply information, bits 14-17 */
	ret->RI = ((msg[1] & 0x07) << 1) 
	         | ((msg[2] >> 7) & 0x01);
	/*
	 * AC - Altitude code, bits 20-32
	 * C1 A1 C2 A2 C4 A4 mb B1 qb B2 D2 B4 D4
	 */
	ret->AC.raw = ((msg[2] << 8) | msg[3]) & 0x1FFF;
	ret->AC.alt_ft = decode_AC(ret->AC.raw, true);
	ret->AC.Q = ret->AC.raw & 0x10;
	ret->AC.M = ret->AC.raw & 0x40;

	ret->DF = msg[0] >> 3;
	ret->AP = (msg[4] << 16)
	        | (msg[5] <<  8)
	        | (msg[6] <<  0);

	return ret;
}

void
pr_DF00(FILE *fp, struct ms_DF00_t *p, int v) {
	fprintf(fp, "DF00:Short air-air surveillance\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,  5, p->DF, "DF");
		pr_raw(fp,  6,  6, p->VS, "VS");
		pr_raw(fp,  7,  7, p->CC, "CC");
		pr_raw(fp,  9, 11, p->SL, "SL");
		pr_raw(fp, 14, 17, p->RI, "RI");
		pr_raw(fp, 20, 32, p->AC.raw, "AC");
		pr_raw(fp, 33, 56, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_VS(fp, p->VS);
		pr_CC(fp, p->CC);
		pr_SL(fp, p->SL);
		pr_RI(fp, p->RI);
		pr_AC(fp, &p->AC);
	}
}
