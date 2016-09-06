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
#include <ctype.h>

#include "bds_08.h"

/*
 * Aircraft identification and classification
 * [1] 3.1.2.9
 *
 * Classifications:
 * [1] Table 3-7
 *
 * Character set for AI:
 * [1] Table 3-8
 */
static const char ICAO_CHARSET[] =
	"_ABCDEFG"
	"HIJKLMNO"
	"PQRSTUVW"
	"XYZ_____"
	" _______"
	"________"
	"01234567"
	"89______";


struct ms_BDS_08_t *
mk_BDS_08(const uint8_t *msg) {
	struct ms_BDS_08_t *ret;
	uint8_t chars[8];
	int i;

	ret = calloc(1, sizeof(struct ms_BDS_08_t));

	ret->FTC = msg[0] >> 3;
	ret->category = msg[0] & 0x07;

	chars[0] = msg[1] >> 2;
	chars[1] = ((msg[1] & 0x03) << 4) | (msg[2] >> 4);
	chars[2] = ((msg[2] & 0x0F) << 2) | ((msg[3] & 0xC0) >> 6);
	chars[3] = msg[3] & 0x3F;
	chars[4] = msg[4] >> 2;
	chars[5] = ((msg[4] & 0x03) << 4) | (msg[5] >> 4);
	chars[6] = ((msg[5] & 0x0F) << 2) | ((msg[6] & 0xC0) >> 6);
	chars[7] = msg[6] & 0x3F;

	for (i = 0; i < 8; ++i)
		ret->name[i] = ICAO_CHARSET[chars[i]];
	for (i = 8; i > 0; --i) {
		if (isalnum(ret->name[i]))
			break;
		ret->name[i] = '\0';
	}

	return ret;
}

static void
pr_cat_D(FILE *fp, uint8_t c) {
	fprintf(fp, "TYPE:");
	switch (c) {
	case 0:
		fprintf(fp, "D,%d:No ADS-B emitter category information\n", c);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		fprintf(fp, "D,%d:Reserved", c);
		break;
	default:
		fprintf(fp, "D,%d:Invalid category", c);
	}

}

static void
pr_cat_C(FILE *fp, uint8_t c) {
	fprintf(fp, "TYPE:");
	switch (c) {
	case 0:
		fprintf(fp, "C,%d:No ADS-B emitter category information\n", c);
		break;
	case 1:
		fprintf(fp, "Emergency vehicle\n");
		break;
	case 2:
		fprintf(fp, "Service vehicle\n");
		break;
	case 3:
		fprintf(fp, "Fixed ground or tethered obstruction\n");
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		fprintf(fp, "C,%d:Reserved\n", c);
		break;
	default:
		fprintf(fp, "C,%d:Invalid category\n", c);
	}
}

static void
pr_cat_B(FILE *fp, uint8_t c) {
	fprintf(fp, "TYPE:");
	switch (c) {
	case 0:
		fprintf(fp, "B,%d:No ADS-B emitter category information\n", c);
		break;
	case 1:
		fprintf(fp, "Glider / Sailplane\n");
		break;
	case 2:
		fprintf(fp, "Lighter-than-air\n");
		break;
	case 3:
		fprintf(fp, "Parachutist\n");
		break;
	case 4:
		fprintf(fp, "Hang-glider\n");
		break;
	case 5:
		fprintf(fp, "Reserved\n");
		break;
	case 6:
		fprintf(fp, "Unmanned aerial vehicle\n");
		break;
	case 7:
		fprintf(fp, "Trans-atmospheric vehicle\n");
		break;
	default:
		fprintf(fp, "B,%d:Invalid category\n", c);
	}
}


static void
pr_cat_A(FILE *fp, uint8_t c) {
	fprintf(fp, "TYPE:");
	switch (c) {
	case 0:
		fprintf(fp, "A,%d:No ADS-B emitter category information\n", c);
		break;
	case 1:
		fprintf(fp, "Light, weight < %'d lbs (7 Mg)\n", 15500);
		break;
	case 2:
		fprintf(fp, "Small, %'d ≤ weight < %'d lbs (7 to 34 Mg)\n", 15500, 75000);
		break;
	case 3:
		fprintf(fp, "Large, %'d lbs ≤ weight < %'d lbs (34 to 136 Mg)\n", 75000, 300000);
		break;
	case 4:
		fprintf(fp, "High-vortex aircraft\n");
		break;
	case 5:
		fprintf(fp, "Heavy, weight ≥ 300 klbs\n");
		break;
	case 6:
		fprintf(fp, "High performance, acceleration > 5g, > 400 kts\n");
		break;
	case 7:
		fprintf(fp, "Rotorcraft\n");
		break;
	default:
		fprintf(fp, "A,%d:Invalid category\n", c);
	}
}

void
pr_category(FILE *fp, uint8_t TYPE, uint8_t category) {
	switch (TYPE) {
	case 1:
		pr_cat_D(fp, category);
		break;
	case 2:
		pr_cat_C(fp, category);
		break;
	case 3:
		pr_cat_B(fp, category);
		break;
	case 4:
		pr_cat_A(fp, category);
		break;
	}

}

void
pr_BDS_08(FILE *fp, const struct ms_BDS_08_t *p, int v) {
	if (v == 1) {
		pr_raw(fp,  1 , 5, p->FTC, "FTC");
		pr_raw(fp,  6,  8, p->category, "CAT");
		pr_raw(fp,  9, 14, p->name[0], "CHAR1");
		pr_raw(fp, 15, 20, p->name[1], "CHAR2");
		pr_raw(fp, 21, 26, p->name[2], "CHAR3");
		pr_raw(fp, 27, 32, p->name[3], "CHAR4");
		pr_raw(fp, 33, 38, p->name[4], "CHAR5");
		pr_raw(fp, 39, 44, p->name[5], "CHAR6");
		pr_raw(fp, 45, 50, p->name[6], "CHAR7");
		pr_raw(fp, 51, 56, p->name[7], "CHAR8");
	} else {
		fprintf(fp, "BDS:0,8:Aircraft identification and category\n");
		pr_category(fp, p->FTC, p->category);
		fprintf(fp, "ICAO-Aircraft identifier:%s\n", p->name);
	}
}
