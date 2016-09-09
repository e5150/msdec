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
#include <math.h>

#include "bds_09.h"

/*
 * BDS 0,9 - Airborne velocity
 * [3] A.2.3.5
 * [3] B.2.3.5
 * [3] Tables B-2-9a, B-2-9b
 */
struct ms_BDS_09_t *
mk_BDS_09(const uint8_t *msg) {
	struct ms_BDS_09_t *ret;
	double ew, ns;

	ret = calloc(1, sizeof(struct ms_BDS_09_t));

	ret->FTC = msg[0] >> 3;
	ret->subtype = msg[0] & 0x07;

	ret->ICF = msg[1] & 0x80;
	ret->IFR = msg[1] & 0x40;
	ret->NAC_v = (msg[1] >> 3) & 0x07;

	ret->GNSS_sign = msg[6] & 0x80;
	ret->GNSS_diff = msg[6] & 0x7F;

	ret->VR_source = msg[4] & 0x01;
	ret->VR_sign   = msg[4] & 0x08;
	ret->VR = ((msg[4] & 0x07) << 6) | (msg[5] >> 2);
	ret->velocity.vert = 64 * (ret->VR - 1);
	if (ret->VR_sign)
		ret->velocity.vert *= -1;



	switch (ret->subtype) {
	case AV_OVER_GROUND:
	case AV_OVER_GROUND_SUPS:
		ret->data.vog.NS_direction = msg[3] & 0x80;
		ret->data.vog.EW_direction = msg[1] & 0x04;
		ret->data.vog.NS_v = ((msg[3] & 0x7F) << 3) | (msg[4] >> 5);
		ret->data.vog.EW_v = ((msg[1] & 0x03) << 8) | (msg[2]);
		ns = ret->data.vog.NS_v - 1.0;
		if (ret->data.vog.NS_direction && ns > 0.0)
			ns = -ns;
		ew = ret->data.vog.EW_v - 1.0;
		if (ret->data.vog.EW_direction && ew > 0.0)
			ew = -ew;
		ret->velocity.heading = 180.0 / M_PI * atan2(ew, ns);
		ret->velocity.speed = sqrt(ew * ew + ns * ns);
		break;
	case AV_AIRSPEED:
	case AV_AIRSPEED_SUPS:
		ret->data.ash.mag_status = msg[1] & 0x04;
		ret->data.ash.as_type    = msg[3] & 0x80;
		ret->data.ash.heading    = ((msg[1] & 0x03) << 8) | (msg[2]);
		ret->data.ash.airspeed   = ((msg[3] & 0x7F) << 3) | (msg[4] >> 5);

		ret->velocity.heading = ret->data.ash.heading * 360.0 / 1024.0;
		ret->velocity.speed = ret->data.ash.airspeed - 1;
		break;
	}

	if (ret->subtype == AV_AIRSPEED_SUPS || ret->subtype == AV_OVER_GROUND_SUPS) {
		ret->velocity.speed *= 4.0; 
	}
	if (ret->velocity.heading <= 0.0)
		ret->velocity.heading += 360.0;
	return ret;
}

void
pr_GNSS_diff(FILE *fp, bool sign, uint8_t val) {
	fprintf(fp, "GBD=%d,%d:", sign, val);
	if (val == 0)
		fprintf(fp, "No information\n");
	else if (val == 127)
		fprintf(fp, "GNSS > %'.1f ft %s barometric altitude\n", 3137.5,
		       sign ? "below" : "above");
	else 
		fprintf(fp, "GNSS = %d ± %.1f ft %s barometric altitude\n", 
		       25 * (val - 1),
		       12.5,
		       sign ? "below" : "above");
}

/*
 * ICF - Intent change flag
 * [3] A.2.3.5.3
 */
void
pr_ICF(FILE *fp, bool ICF) {
	fprintf(fp, "ICF=%d:Change in intent:%s\n", ICF, YESNO(ICF));
}

/*
 * IFR capability flag
 * [3] A.2.3.5.4
 */
void
pr_IFR(FILE *fp, bool IFR) {
	fprintf(fp, "IFR=%d:ADS-B class A1+ capability:%s\n", IFR, YESNO(IFR));
}

/*
 * Vertical rate
 * [3] Table B-2-9a
 */
void
pr_VR(FILE *fp, bool source, bool sign, uint16_t rate) {
	fprintf(fp, "VR=%d,%d,%0X:%s:",
	       source, sign, rate, source ? "Barometric" : "GNSS");
	if (rate == 0) {
		fprintf(fp, "No vertical rate information\n");
	} else {
		fprintf(fp, "Rate of %s ", sign ? "descent" : "climb");
		if (rate == 511) {
			fprintf(fp, "> %'d ft/min\n", 32608);
		} else {
			fprintf(fp, "= %'d ± 32 ft/min\n", 64 * (rate - 1));
		}
	}
}

/*
 * Airspeed
 * [3] Table B-2-9b
 */
void
pr_airspeed(FILE *fp, bool type, uint16_t as) {
	fprintf(fp, "Airspeed:%s:%04X\n", type ? "TAS" : "IAS", as);
}

/*
 * Heading
 * [3] A.2.3.5.6
 */
void
pr_heading(FILE *fp, bool status, uint16_t raw) {
	fprintf(fp, "Magnetic heading available:%s:%04X\n", YESNO(status), raw);

}

static void
pr_speed(FILE *fp, uint16_t val, bool supersonic) {
	if (val == 0)
		fprintf(fp, "No information\n");
	else if (val == 1023)
		fprintf(fp, "> %'.1f kt\n", supersonic ? 4086 : 1021.5);
	else
		fprintf(fp, "%'d kt\n", (val - 1) * (supersonic ? 4 : 1));
}

void
pr_velocity_ns(FILE *fp, bool direction, uint16_t v, bool supersonic) {
	fprintf(fp, "NS=%d,%04X:%swards ",
	       direction, v, direction ? "South" : "North");
	pr_speed(fp, v, supersonic);
}

void
pr_velocity_ew(FILE *fp, bool direction, uint16_t v, bool supersonic) {
	fprintf(fp, "EW=%d,%04X:%swards ",
	       direction, v, direction ? "West" : "East");
	pr_speed(fp, v, supersonic);
}

void
pr_BDS_09(FILE *fp, const struct ms_BDS_09_t *p, int v) {

	if (v == 1) {
		pr_raw(fp,  1,  5, p->FTC, "FTC");
		pr_raw(fp,  6,  8, p->subtype, "SUB");
		pr_raw(fp,  9,  9, p->ICF, "ICF");
		pr_raw(fp, 10, 10, p->IFR, "IFR");
		pr_raw(fp, 11, 13, p->NAC_v, "NAC_v");
		switch (p->subtype) {
		case AV_OVER_GROUND:
		case AV_OVER_GROUND_SUPS:
			pr_raw(fp, 14, 14, p->data.vog.EW_direction, "E/W");
			pr_raw(fp, 15, 24, p->data.vog.EW_v, "E/W-v");
			pr_raw(fp, 25, 25, p->data.vog.NS_direction, "N/S");
			pr_raw(fp, 26, 35, p->data.vog.NS_v, "N/S-v");
			break;
		case AV_AIRSPEED:
		case AV_AIRSPEED_SUPS:
			pr_raw(fp, 14, 14, p->data.ash.mag_status, "MAG/S");
			pr_raw(fp, 15, 24, p->data.ash.heading, "HEADING");
			pr_raw(fp, 25, 25, p->data.ash.as_type, "AS/TYPE");
			pr_raw(fp, 26, 35, p->data.ash.airspeed, "SPEED");
			break;
		}
		pr_raw(fp, 36, 36, p->VR_source, "VR/SRC");
		pr_raw(fp, 37, 37, p->VR_sign, "VR±");
		pr_raw(fp, 38, 46, p->VR, "VR");
		pr_raw(fp, 49, 49, p->GNSS_sign, "GNSS±");
		pr_raw(fp, 50, 56, p->GNSS_diff, "GNSS");
	} else {
		fprintf(fp, "BDS:0,9:Airborne velocity\n");
		switch (p->subtype) {
		case AV_OVER_GROUND:
			fprintf(fp, "Velocity over ground\n");
			break;
		case AV_OVER_GROUND_SUPS:
			fprintf(fp, "Velocity over ground, supersonic\n");
			break;
		case AV_AIRSPEED:
			fprintf(fp, "Airspeed and heading\n");
			break;
		case AV_AIRSPEED_SUPS:
			fprintf(fp, "Airspeed and heading, supersonic\n");
			break;
		}
		
		pr_ICF(fp, p->ICF);
		pr_IFR(fp, p->IFR);
		pr_NAC_v(fp, p->NAC_v);

		switch (p->subtype) {
		case AV_OVER_GROUND:
		case AV_OVER_GROUND_SUPS:
			pr_velocity_ns(fp, p->data.vog.NS_direction,
				       p->data.vog.NS_v,
				       p->subtype == AV_OVER_GROUND_SUPS);
			pr_velocity_ew(fp, p->data.vog.EW_direction,
				       p->data.vog.EW_v,
				       p->subtype == AV_OVER_GROUND_SUPS);
			break;
		case AV_AIRSPEED:
		case AV_AIRSPEED_SUPS:
			pr_airspeed(fp, p->data.ash.as_type, p->data.ash.airspeed);
			pr_heading(fp, p->data.ash.mag_status, p->data.ash.heading);
			break;
		}

		pr_velocity(fp, &p->velocity);


		pr_VR(fp, p->VR_source, p->VR_sign, p->VR);
		pr_GNSS_diff(fp, p->GNSS_sign, p->GNSS_diff);
	}
}
