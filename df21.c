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

#include "df21.h"
#include "mac.h"

/*
 *  DF 21 - Comm-B Identity Reply - [1] 3.1.2.6.8
 *                            
 *           1         2         3         4         5         8  11
 *  12345678901234567890123456789012345678901234567890123456...8...1
 *  DF   FS DR   UM    ID           MB                         AP
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *
 */
struct ms_DF21_t *
mk_DF21(const uint8_t *msg) {
	struct ms_DF21_t *ret;
	uint16_t tmp;

	ret = calloc(1, sizeof(struct ms_DF21_t));
	
	ret->DF = msg[0] >> 3;
	ret->AP = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/* FS - Flight status */
	ret->FS = msg[0] & 0x07;

	/* DR - Downlink request */
	ret->DR = msg[1] >> 3;

	/* UM - Utility message */
	ret->UM.IIS = ((msg[1] & 0x07) << 1) | (msg[2] >> 7);
	ret->UM.IDS = (msg[2] >> 5) & 0x03;

	/* ID - Identity */
	tmp = ((msg[2] << 8) | msg[3]) & 0x1FFF;
	ret->ID = decode_ID(tmp);

	return ret;
}

void
pr_DF21(FILE *fp, struct ms_DF21_t *p, int v) {
	fprintf(fp, "DF21:Comm-B identity reply:TODO\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,   5, p->DF, "DF");
		pr_raw(fp,  6,   8, p->FS, "FS");
		pr_raw(fp,  9,  13, p->DR, "DR");
		pr_raw(fp, 14,  17, p->UM.IIS, "UM/IIS");
		pr_raw(fp, 18,  19, p->UM.IDS, "UM/IDS");
		pr_raw(fp, 20,  32, p->ID, "ID");
		pr_raw_subfield_header(fp, 33, 88, "Comm-B message:TODO");
		/* MB, TODO */
		pr_raw_subfield_footer(fp);
		pr_raw(fp, 89, 112, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_FS(fp, p->FS);
		pr_DR(fp, p->DR);
		pr_UM(fp, p->UM);
		pr_ID(fp, p->ID);
		/* TODO, MB */
	}
}
