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

#include "tisb_f.h" 
#include "mac.h"

/*
 *  TIS-B Fine Airborne Position
 *  [3] B.3.4.1
 *
 *  Same as BDS 0,6 except for:
 *   Bit  8: Substitute IMF for SAF
 *   Bit 21: UTC-Time → Reserved
 *
 *           1         2         3         4         5      
 *  12345678901234567890123456789012345678901234567890123456
 *  FTC  SSxAC         ] F[CPR17-lat      ][CPR17-lon      ]
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 */

struct ms_TISB_fine_t *
mk_TISB_fine(const uint8_t *msg) {
	struct ms_TISB_fine_t *ret;

	ret = calloc(1, sizeof(struct ms_TISB_fine_t));

	ret->FTC = msg[0] >> 3;
	ret->IMF = msg[0] & 0x01;

	ret->SS = (msg[0] >> 1) & 0x03;
	ret->AC.raw = (msg[1] << 4) | (msg[2] >> 4);
	ret->AC.alt_ft = decode_AC(ret->AC.raw, false);
	ret->AC.M = false;
	ret->AC.Q = ret->AC.raw & 0x10;

	ret->CPR.surface = false;
	ret->CPR.F = msg[2] & 0x04;
	ret->CPR.Nb = 17;
	ret->CPR.lat = ((msg[2] & 0x03) << 15)
	             |  (msg[3] << 7)
	             |  (msg[4] >> 1);
	ret->CPR.lon = ((msg[4] & 0x01) << 16)
	             |  (msg[5] << 8)
	             |   msg[6];

	
	return ret;
}

void
pr_TISB_fine(FILE *fp, const struct ms_TISB_fine_t *p, int v) {

	if (v == 1) {
		pr_raw(fp, 1,  5, p->FTC,    "FTC");
		pr_raw(fp, 6,  7, p->SS,     "SS");
		pr_raw(fp, 8,  8, p->IMF,    "IMF");
		pr_raw(fp, 9, 20, p->AC.raw, "AC");

		pr_raw(fp, 22, 22, p->CPR.F,     "CPR/F");
		pr_raw(fp, 23, 39, p->CPR.lat,   "CPR/LAT");
		pr_raw(fp, 40, 56, p->CPR.lon,   "CPR/LON");
	} else {
		fprintf(fp, "TIS-B:Coarse airborne position\n");
		pr_SS(fp, p->SS);
		pr_AC(fp, &p->AC);
		pr_CPR(fp, &p->CPR);
	}

}
