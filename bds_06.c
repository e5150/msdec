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

#include "bds_06.h"
#include "compass.h"

/*
 * Movement
 * [3] A.2.3.3.1
 * [3] B.2.3.3.1
 * ME-bits 6-12
 */
static double
decode_speed(uint8_t mov) {
	if (!mov)            return   0.0;
	else if (mov ==   1) return   0.125;
	else if (mov <=   8) return   0.125 + 0.125 * (mov -   2);
	else if (mov <=  12) return   1.0   + 0.25  * (mov -   9);
	else if (mov <=  38) return   2.0   + 0.5   * (mov -  13);
	else if (mov <=  93) return  15.0   + 1.0   * (mov -  39);
	else if (mov <= 108) return  70.0   + 2.0   * (mov -  94);
	else if (mov <= 123) return 100.0   + 5.0   * (mov - 109);
	else if (mov == 124) return 324.1;
	else                 return  -1.0;
}

static double
decode_angle(uint8_t v) {
	return (double)v * 360.0 / 128.0;
}

/*
 * BDS 0,6 - Surface Position
 * [3] B.2.3.3
 * [3] Table B-2-6
 *
 *           1         2         3         4         5      
 *  12345678901234567890123456789012345678901234567890123456
 *  FTC  MOV    cHGT    TFlat              lon              
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 */
struct ms_BDS_06_t *
mk_BDS_06(const uint8_t *msg) {
	struct ms_BDS_06_t *ret;
	
	ret = calloc(1, sizeof(struct ms_BDS_06_t));
	
	ret->FTC = msg[0] >> 4;
	ret->UTC_SYNCED_TIME = msg[2] & 0x08;

	ret->GTS.speed = ((msg[0] & 0x07) << 4) | (msg[1] >> 4);
	ret->GTS.valid = msg[1] & 0x08;
	ret->GTS.angle = ((msg[1] & 0x07) << 4) | (msg[2] >> 4);

	ret->velocity.speed = decode_speed(ret->GTS.speed);
	ret->velocity.angle = decode_angle(ret->GTS.angle);

	ret->CPR.surface = true;
	ret->CPR.F = msg[2] & 0x04;
	ret->CPR.Nb = 17;
	ret->CPR.lat = ((msg[2] & 0x03) << 15) | (msg[3] << 7) | (msg[4] >> 1);
	ret->CPR.lon = ((msg[4] & 0x01) << 16) | (msg[5] << 8) | (msg[6] >> 0);
	
	return ret;
}

static void
pr_GTS(FILE *fp, struct ms_GTS_t GTS) {
	double a = decode_angle(GTS.angle);
	double s = decode_speed(GTS.speed);
	char angle[40];

	fill_angle_str(angle, 40, a);

	fprintf(fp, "Ground track angle / Movement=%d,%02X,%02X:%s\n",
	       GTS.valid, GTS.angle, GTS.speed, GTS.valid ? "Valid" : "Invalid");

	fprintf(fp, "Velocity:");
	if (GTS.speed == 0)
		fprintf(fp, "No info");
	else if (GTS.speed == 1)
		fprintf(fp, "< %.3f kt (%.4f km/h)", 0.125, 0.2315);
	else if (GTS.speed == 124)
		fprintf(fp, "≥ 175 kt (%.1f km/h)", 324.1);
	else if (GTS.speed > 124)
		fprintf(fp, "Reserved");
	else
		fprintf(fp, "≈ %.2f kt (%.2f km/h)", s, KT2KMPH(s));

	fprintf(fp, ":%s (%s)\n", angle, get_comp_point(a));
}

void
pr_BDS_06(FILE *fp, const struct ms_BDS_06_t *p, int v) {

	if (v == 1) {
		pr_raw(fp,  1,  4, p->FTC, "FTC");
		pr_raw(fp,  6, 12, p->GTS.speed, "GT/Speed");
		pr_raw(fp, 13, 13, p->GTS.valid, "GT/Valid");
		pr_raw(fp, 14, 20, p->GTS.angle, "GT/Angle");
		pr_raw(fp, 21, 21, p->UTC_SYNCED_TIME, "UTC/T");
		pr_raw(fp, 22, 22, p->CPR.F,   "CPR/F");
		pr_raw(fp, 23, 39, p->CPR.lon, "CPR/LON");
		pr_raw(fp, 40, 56, p->CPR.lat, "CPR/LAT");
	} else {
		fprintf(fp, "BDS:0,6:Surface position\n");
		pr_NIC(fp, p->FTC, 0);
		pr_GTS(fp, p->GTS);
		pr_UTCT(fp, p->UTC_SYNCED_TIME);
		pr_CPR(fp, &p->CPR);
	}

}
