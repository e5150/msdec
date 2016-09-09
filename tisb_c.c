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

#include "tisb_c.h" 
#include "mac.h"
#include "compass.h"

/*
 * TIS-B Coarse Airborne Position
 * [3] B.3.4.5
 * [3] Table B-3-5
 *
 *           1         2         3         4         5      
 *  12345678901234567890123456789012345678901234567890123456
 *  ImfSVID            GTV   GTS    LAT-12
 *   SS    AC-12        GTA        F           LON-12
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 */

/*
 * Ground speed
 * [3] B.3.4.5.6
 * ME-bits 26-31
 */
static double
decode_speed(uint8_t s) {
	if (!s)
		return 0.0;
	if (s == 63)
		return 1968;

	return 32 * (s - 1);
}

/*
 * GT angle
 * [3] B.3.4.5.5
 * (Degrees from true north)
 */
static double
decode_angle(uint8_t v) {
	return (double)v * 360.0 / 32.0;
}

struct ms_TISB_coarse_t *
mk_TISB_coarse(const uint8_t *msg) {
	struct ms_TISB_coarse_t *ret;

	ret = calloc(1, sizeof(struct ms_TISB_coarse_t));

	ret->IMF = (msg[0] & 0x80);

	ret->SS   = (msg[0] >> 5) & 0x03;
	ret->SVID = (msg[0] >> 1) & 0x0F;

	ret->AC.raw = ((msg[0] & 0x01) << 11)
	               |  (msg[1] << 3)
	               |  (msg[2] >> 5);
	ret->AC.alt_ft = decode_AC(ret->AC.raw, false);
	ret->AC.Q = ret->AC.raw & 0x10;
	ret->AC.M = false;
	
	ret->GTS.valid = msg[2] & 0x10;
	ret->GTS.angle = ((msg[2] & 0x0F) << 1) | (msg[3] >> 7);
	ret->GTS.speed = (msg[3] >> 1) & 0x3F;

	ret->velocity.heading = decode_angle(ret->GTS.angle);
	ret->velocity.speed = decode_speed(ret->GTS.speed);

	ret->CPR.F = msg[3] & 0x01;
	ret->CPR.Nb = 12;
	ret->CPR.lat = (msg[4] << 4) | (msg[5] >> 4);
	ret->CPR.lon = ((msg[5] & 0x0F) << 8) | msg[6];
	ret->CPR.surface = false;

	return ret;
}

static void
pr_GTS(FILE *fp, struct ms_GTS_t GTS) {
	double a = decode_angle(GTS.angle);
	double s = decode_speed(GTS.speed);
	char angle[40];

	fill_angle_str(angle, 40, a);

	fprintf(fp, "Ground track angle and speed=%d,%02X,%02X:%s\n",
	       GTS.valid, GTS.angle, GTS.speed, GTS.valid ? "Valid" : "Invalid");

	fprintf(fp, "Velocity:");
	if (!GTS.speed)
		fprintf(fp, "No info");
	else if (GTS.speed == 1)
		fprintf(fp, "< 16 kt (%.0f km/h)", KT2KMPH(16));
	else if (GTS.speed == 63)
		fprintf(fp, "≥ %'d kt (%'.0f km/h)", 1968, KT2KMPH(1968));
	else
		fprintf(fp, "≈ %'.0f ± 16 kt (%'.0f km/h)", s, KT2KMPH(s));
	fprintf(fp, ":%s (%s)\n", angle, get_comp_point(a));

}

/*
 * SVID - Service volume ID
 * [3] B.3.4.5.2
 */
static void
pr_SVID(FILE *fp, uint8_t id) {
	fprintf(fp, "Service volume ID:%d\n", id);
}

void
pr_TISB_coarse(FILE *fp, const struct ms_TISB_coarse_t *p, int v) {
	
	if (v == 1) {
		pr_raw(fp,  1,  1, p->IMF,       "IMF");
		pr_raw(fp,  2,  3, p->SS,        "SS");
		pr_raw(fp,  4,  7, p->SVID,      "SVID");
		pr_raw(fp,  8, 19, p->AC.raw,    "AC");
		pr_raw(fp, 20, 20, p->GTS.valid, "GTValid");
		pr_raw(fp, 21, 25, p->GTS.angle, "GTAngle");
		pr_raw(fp, 26, 31, p->GTS.speed, "GTSpeed");
		pr_raw(fp, 32, 32, p->CPR.F,     "CPR/F");
		pr_raw(fp, 33, 44, p->CPR.lat,   "CPR/LAT");
		pr_raw(fp, 45, 56, p->CPR.lon,   "CPR/LON");
	} else {
		fprintf(fp, "TIS-B:Coarse airborne position\n");
		pr_SVID(fp, p->SVID);
		pr_SS(fp, p->SS);
		pr_AC(fp, &p->AC);
		pr_GTS(fp, p->GTS);
		pr_CPR(fp, &p->CPR);
	}

}
