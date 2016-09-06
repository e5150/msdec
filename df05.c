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

#include "df05.h"
#include "mac.h"

/*
 *  DF 5 - Surveillance Identity Reply
 *  [1] 3.1.2.6.7
 *                            
 *           1         2         3         4         5
 *  12345678901234567890123456789012345678901234567890123456
 *  DF   FS DR   UM    ID           AP
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6  
 *
 */
struct ms_DF05_t*
mk_DF05(const uint8_t *msg) {
	struct ms_DF05_t *ret;
	uint16_t tmp;

	ret = calloc(1, sizeof(struct ms_DF05_t));
	
	ret->DF = msg[0] >> 3;
	ret->AP = (msg[4] << 16)
	        | (msg[5] <<  8)
	        | (msg[6] <<  0);

	/* FS - Flight status */
	ret->FS = msg[0] & 0x07;
	/* DR - Downlink request */
	ret->DR = msg[1] >> 3;
	/* UM - Utility message */
	ret->UM.IIS = ((msg[1] & 0x07) << 1) | (msg[2] >> 7);
	ret->UM.IDS = (msg[2] >> 5) & 0x03;
	/* ID - Identity 3.1.1.6 */
	tmp = ((msg[2] << 8) | msg[3]) & 0x1FFF;
	ret->ID = decode_ID(tmp);

	return ret;
}

void
pr_DF05(FILE *fp, struct ms_DF05_t *p, int v) {
	fprintf(fp, "DF05:Surveillance identity reply\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,  5, p->DF, "DF");
		pr_raw(fp,  6,  8, p->FS, "FS");
		pr_raw(fp,  9, 13, p->DR, "DR");
		pr_raw(fp, 14, 17, p->UM.IIS, "UM/IIS");
		pr_raw(fp, 18, 19, p->UM.IDS, "UM/IDS");
		pr_raw(fp, 20, 32, p->ID, "ID");
		pr_raw(fp, 33, 56, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_FS(fp, p->FS);
		pr_DR(fp, p->DR);
		pr_UM(fp, p->UM);
		pr_ID(fp, p->ID);
	}
}
