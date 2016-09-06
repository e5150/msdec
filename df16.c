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

#include "df16.h"
#include "mac.h"
#include "bds_30.h"

/*
 *  DF 16 - Long Air-Air Surveillance
 *  [1] 3.1.2.8.3
 *                            
 *           1         2         3         4         5        8  11
 *  1234567890123456789012345678901234567890123456789012345...8...1
 *  D    V--S  --R   --A            M                          A
 *  F    S--L  --I   --C     m q    V                          P
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *                     2109876543210
 *
 */
struct ms_DF16_t *
mk_DF16(const uint8_t *msg) {
	struct ms_DF16_t *ret;

	ret = calloc(1, sizeof(struct ms_DF16_t));
	
	ret->DF = msg[0] >> 3;
	ret->AP = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/* VS - Vertical status */
	ret->VS = msg[0] & 0x04;
	/* SL - Sensitivity level */
	ret->SL = (msg[1] >> 5) & 0x07;
	/* RI - Reply information, bits 14-17 */
	ret->RI = ((msg[1] & 0x07) << 1) 
	        | ((msg[2] >> 7) & 0x01);
	
	/* AC - Altitude code, 20-32 */
	ret->AC.raw = ((msg[2] << 8) | msg[3]) & 0x1FFF;
	ret->AC.alt_ft = decode_AC(ret->AC.raw, true);
	ret->AC.M = ret->AC.raw & 0x40;
	ret->AC.Q = ret->AC.raw & 0x10;

	/* MV - Message, ACAS */
	ret->VDS = msg[4];

	if (ret->VDS == 0x30) {
		ret->aux = mk_BDS_30(msg + 4);
	}

	return ret;
}

static void
pr_VDS(FILE *fp, uint8_t vds, void *aux, int v) {
	fprintf(fp, "VDS:%02X\n", vds);
	if (vds == 0x30)
		pr_BDS_30(fp, aux, v);

}

void
pr_DF16(FILE *fp, struct ms_DF16_t *p, int v) {
	fprintf(fp, "DF16:Long air-air surveillance\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,  5, p->DF, "DF");
		pr_raw(fp,  6,  6, p->VS, "VS");
		pr_raw(fp,  9, 11, p->SL, "SL");
		pr_raw(fp, 14, 17, p->RI, "RI");
		pr_raw(fp, 20, 32, p->AC.raw, "AC");
		pr_raw_subfield_header(fp, 33, 88, "MV:TODO");
		pr_raw_subfield_footer(fp);
		pr_raw(fp, 89, 112, p->AP, "AP");
		pr_raw_footer(fp);
	} else {
		pr_VS(fp, p->VS);
		pr_SL(fp, p->SL);
		pr_RI(fp, p->RI);
		pr_AC(fp, &p->AC);
		pr_VDS(fp, p->VDS, p->aux, v);
	}
}
