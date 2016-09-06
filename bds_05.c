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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bds_05.h" 
#include "mac.h"

/*
 * BDS 0,5 - Airborne Position
 * [3] A.2.3.2
 * [3] Table A-2-5
 *
 *           1         2         3         4         5      
 *  12345678901234567890123456789012345678901234567890123456
 *  FTC  SSx[ALT   q   ]TFlat              lon              
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 */
struct ms_BDS_05_t *
mk_BDS_05(const uint8_t *msg) {
	struct ms_BDS_05_t *ret;

	ret = calloc(1, sizeof(struct ms_BDS_05_t));

	ret->FTC = msg[0] >> 3;
	ret->SS  = msg[0] & 0x06;
	ret->SAF = msg[0] & 0x01;
	ret->UTC_SYNCED_TIME =msg[2] & 0x08;

	ret->CPR.surface = false;
	ret->CPR.F = msg[2] & 0x04;
	ret->CPR.Nb = 17;
	ret->CPR.lat = ((msg[2] & 0x03) << 15) | (msg[3] << 7) | (msg[4] >> 1);
	ret->CPR.lon = ((msg[4] & 0x01) << 16) | (msg[5] << 8) | (msg[6] >> 0);
	
	ret->AC.raw = (msg[1] << 4) | (msg[2] >> 4);
	ret->AC.alt_ft = decode_AC(ret->AC.raw, false);
	ret->AC.Q = ret->AC.raw & 0x10;
	ret->AC.M = false;

	ret->alt_type = ret->FTC < 19 ? ALT_BAROMETRIC : ALT_GNSS;
	return ret;
}

/*
 * SAF - Single antenna flag
 * [3] A.2.3.2.5
 */
static void
pr_SAF(FILE *fp, enum ms_SAF_t SAF) {
	fprintf(fp, "SAF=%d:", SAF);
	switch (SAF) {
	case SAF_SINGLE_ANTENNA:
		fprintf(fp, "Single antenna\n");
		break;
	case SAF_DUAL_ANTENNAE:
		fprintf(fp, "Dual antennae\n");
		break;
	}
}

void
pr_BDS_05(FILE *fp, const struct ms_BDS_05_t *p, int v) {
	if (v == 1) {
		pr_raw(fp,  1,  5, p->FTC,     "FTC");
		pr_raw(fp,  6,  7, p->SS,      "SS");
		pr_raw(fp,  8,  8, p->SAF,     "SAF");
		pr_raw(fp,  9, 20, p->AC.raw,  "AC");
		pr_raw(fp, 21, 21, p->UTC_SYNCED_TIME, "UTC-T");
		pr_raw(fp, 22, 22, p->CPR.F,   "CPR/F");
		pr_raw(fp, 23, 39, p->CPR.lat, "CPR/LAT");
		pr_raw(fp, 40, 56, p->CPR.lon, "CPR/LON");
	} else {
		fprintf(fp, "BDS:0,5:Airborne position:%d\n", p->FTC);

		if (p->FTC == 0)
			fprintf(fp, "No position information\n");

		pr_NIC(fp, p->FTC, 0);
		pr_SS(fp, p->SS);
		pr_SAF(fp, p->SAF);
		pr_UTCT(fp, p->UTC_SYNCED_TIME);
		pr_CPR(fp, &p->CPR);

		fprintf(fp, "Altitude type:");
		if (p->FTC == 0) {
			fprintf(fp, "Barometric or unknown\n");
		}
		else if (p->alt_type == ALT_BAROMETRIC) {
			fprintf(fp, "Barometric\n");
			pr_AC(fp, &p->AC);
		}
		else {
			/*
			 * FIXME? [3] A.2.3.2.4:
			 *  “Format TYPE 20 to 22 shall be reserved [...]”
			 */
			fprintf(fp, "GNSS:TODO\n");
		}
	}
	
}
