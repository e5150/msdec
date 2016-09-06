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

#include "df04.h"
#include "mac.h"

/*
 *  DF 4 - Surveillance Altitude Reply
 *  [1] 3.1.2.6.5
 *                            
 *           1         2         3         4         5
 *  12345678901234567890123456789012345678901234567890123456
 *  DF   FS DR   UM    AC           AP
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6  
 *
 */
struct ms_DF04_t*
mk_DF04(const uint8_t *msg) {
	struct ms_DF04_t *ret;

	ret = calloc(1, sizeof(struct ms_DF04_t));
	
	ret->DF = msg[0] >> 3;
	ret->AP = (msg[4] << 16)
	        | (msg[5] <<  8)
	        | (msg[6] <<  0);

	/* FS - Flight status, bit 6-8 */
	ret->FS = msg[0] & 0x07;
	/* DR - Downlink request, bit 9-13 */
	ret->DR = msg[1] >> 3;
	/* 
	 * UM - Utility message, bit 14-19
	 * [1] 3.1.2.6.5.3.1
	 * IIS = bit 14-17
	 * IDS = bit 18-19
	 */
	ret->UM.IIS = ((msg[1] & 0x07) << 1) | (msg[2] >> 7);
	ret->UM.IDS = (msg[2] >> 5) & 0x03;

	/* AC - Altitude code, 20-32 */
	ret->AC.raw = ((msg[2] << 8) | msg[3]) & 0x1FFF;
	ret->AC.alt_ft = decode_AC(ret->AC.raw, true);
	ret->AC.Q = ret->AC.raw & 0x10;
	ret->AC.M = ret->AC.raw & 0x40;


	return ret;
}

void
pr_DF04(FILE *fp, struct ms_DF04_t *p, int v) {
	fprintf(fp, "DF04:Surveillance altitude reply\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,  5, p->DF, "DF");
		pr_raw(fp,  6,  8, p->FS, "FS");
		pr_raw(fp,  9, 13, p->DR, "DR");
		pr_raw(fp, 14, 17, p->UM.IIS, "UM/IIS");
		pr_raw(fp, 18, 19, p->UM.IDS, "UM/IDS");
		pr_raw(fp, 20, 32, p->AC.raw, "AC");
		pr_raw(fp, 33, 56, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_FS(fp, p->FS);
		pr_DR(fp, p->DR);
		pr_UM(fp, p->UM);
		pr_AC(fp, &p->AC);
	};
}
