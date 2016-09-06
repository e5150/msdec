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

#include "bds_62.h"

/*
 * BDS 6,2 - Target state and status information
 * [3] D.2
 * [3] Table D-2.98
 *
 *
 */
struct ms_BDS_62_t *
mk_BDS_62(const uint8_t *msg) {
	struct ms_BDS_62_t *ret;

	ret = calloc(1, sizeof(struct ms_BDS_62_t));

	ret->FTC = msg[0] >> 3;
	ret->subtype = (msg[0] >> 1) & 0x03;

	ret->VDSI = (msg[0] & 0x01) << 1 | (msg[1] >> 7);
	ret->TAT = msg[1] & 0x40 ? TAT_MSL : TAT_FL;
	ret->BCF = msg[1] & 0x20 ? 1 : 0;
	ret->TAC = (msg[1] >> 3) & 0x03;
	ret->VMI = (msg[1] >> 1) & 0x03;
	ret->TA.raw = ((msg[1] & 0x01) << 9)
	            | (msg[2] << 1)
	            | (msg[3] >> 7);
	ret->TA.ft = 100 * ret->TA.raw - 1000;

	ret->HDSI = (msg[3] & 0x60) >> 5;
	ret->HMI = (msg[4] >> 1) & 0x03;

	ret->TH = ((msg[3] & 0x1F) << 4)
	        | (msg[4] >> 4);

	ret->THTA = (msg[4] & 0x08) ? 1 : 0;

	ret->NAC_p = ((msg[4] & 0x01) << 3)
	           | (msg[5] >> 5);

	ret->NIC_baro = msg[5] & 0x10;
	ret->SIL = (msg[5] >> 2) & 0x03;

	ret->CMC = (msg[6] >> 2) & 0x03;
	ret->EMERG = (msg[6] & 0x07);

	return ret;
}

/*
 * Vertical data available / source indicator
 * [3] D.2.3
 */
void
pr_VDSI(FILE *fp, enum ms_DSI_t v) {
	fprintf(fp, "Vertical data source indicator:%d:", v);

	switch (v) {
	case DSI_NOINFO:
		fprintf(fp, "No information\n");
		break;
	case DSI_AUTOPILOT:
		fprintf(fp, "Autopilot\n");
		break;
	case DSI_HOLDING:
		fprintf(fp, "Holding altitude\n");
		break;
	case DSI_FMS:
		fprintf(fp, "FMS / RNAV system\n");
		break;
	}
}

/*
 * Horizontal data avail. / source indicator
 * [3] D.2.8
 */
void
pr_HDSI(FILE *fp, enum ms_DSI_t v) {
	fprintf(fp, "Horizontal data source indicator:%d:", v);

	switch (v) {
	case DSI_NOINFO:
		fprintf(fp, "No information\n");
		break;
	case DSI_AUTOPILOT:
		fprintf(fp, "Autopilot\n");
		break;
	case DSI_HOLDING:
		fprintf(fp, "Maintaining heading\n");
		break;
	case DSI_FMS:
		fprintf(fp, "FMS / RNAV system\n");
		break;
	}
}

/*
 * TAT - Target altitude type
 * [3] D.2.4
 */
void
pr_TAT(FILE *fp, enum ms_TAT_t tat) {
	fprintf(fp, "Target altitude type:%d:", tat);
	fprintf(fp, "Relative to %s\n",
	       tat == TAT_FL ? "Flight level" : "Mean sea level");
}

/*
 * Target altutude capability
 * [3] D.2.5
 */
void
pr_TAC(FILE *fp, enum ms_TAC_t tac) {
	fprintf(fp, "Target altitude capability:%d:", tac);

	switch (tac) {
	case TAC_HOLDING:
		fprintf(fp, "Holding altitude only\n");
		break;
	case TAC_AUTOPILOT:
		fprintf(fp, "Holding and autopilot\n");
		break;
	case TAC_FMS:
		fprintf(fp, "Holding, autopilot and FMS / RNAV\n");
		break;
	case TAC_RESERVED:
		fprintf(fp, "Reserved\n");
		break;
	}
}

static void
pr_MI(FILE *fp, enum ms_MI_t mi) {
	switch (mi) {
	case MI_NOINFO:
		fprintf(fp, "No information\n");
		break;
	case MI_ACQUIRING:
		fprintf(fp, "“Acquiring” mode\n");
		break;
	case MI_CAPTURING:
		fprintf(fp, "“Capturing” or “maintaining” mode\n");
		break;
	case MI_RESERVED:
		fprintf(fp, "Reserved\n");
		break;
	}
}

/*
 * Vertical mode indicator
 * [3] D.2.6
 */
void
pr_VMI(FILE *fp, enum ms_MI_t vmi) {
	fprintf(fp, "Vertical mode indicator:%d:", vmi);
	pr_MI(fp, vmi);
}

/*
 * Horizontal mode indicator
 * [3] D.2.11
 */
void
pr_HMI(FILE *fp, enum ms_MI_t hmi) {
	fprintf(fp, "Horizontal mode indicator:%d:", hmi);
	pr_MI(fp, hmi);
}

/*
 * Target altitude
 * [3] D.2.7
 */
void
pr_TA(FILE *fp, struct ms_TA_t ta) {
	fprintf(fp, "Target altitude:");
	if (ta.raw > 1010)
		fprintf(fp, "%d:Invalid\n", ta.raw);
	else
		fprintf(fp, "%d ft\n", ta.ft);
}

/*
 * Target heading / track angle
 * [3] D.2.9
 */
void
pr_TH(FILE *fp, uint16_t th, enum ms_THTA_t thta) {
	if (thta == THTA_TARGET_HEADING)
		fprintf(fp, "Target heading:");
	else
		fprintf(fp, "Track angle:");
	if (th > 359)
		fprintf(fp, "%d:Invalid\n", th);
	else
		fprintf(fp, "%d°\n", th);
}

/*
 * Capability / mode codes
 * [3] D.2.15
 */
void
pr_CMC(FILE *fp, uint8_t i) {
	fprintf(fp, "TCAS/ACAS operational:%s\n", YESNO(~i & 0x02));
	fprintf(fp, "TCAS/ACAS RA active:%s\n", YESNO(i & 0x01));
}

void
pr_BDS_62(FILE *fp, const struct ms_BDS_62_t *p, int v) {
	if (v == 1) {
		pr_raw(fp,  1,  5, p->FTC, "FTC");
		pr_raw(fp,  6,  7, p->subtype, "SUB");
		pr_raw(fp,  8,  9, p->VDSI, "VDSI");
		pr_raw(fp, 10, 10, p->TAT, "TAT");
		pr_raw(fp, 11, 11, p->BCF, "BCF");
		pr_raw(fp, 12, 13, p->TAC, "TAC");
		pr_raw(fp, 14, 15, p->VMI, "VMI");
		pr_raw(fp, 16, 25, p->TA.raw, "TA");
		pr_raw(fp, 26, 27, p->HDSI, "HDSI");
		pr_raw(fp, 28, 36, p->TH, "TH");
		pr_raw(fp, 37, 37, p->THTA, "THTA"); 
		pr_raw(fp, 38, 39, p->HMI, "HMI");
		pr_raw(fp, 40, 43, p->NAC_p, "NAC_p");
		pr_raw(fp, 44, 44, p->NIC_baro, "NIC_b");
		pr_raw(fp, 45, 46, p->SIL, "SIL");
		pr_raw(fp, 52, 53, p->CMC, "CMC");
		pr_raw(fp, 54, 56, p->EMERG, "E/PS");
	} else {
		fprintf(fp, "BDS:6,2:Target state and status information\n");
		pr_VDSI(fp, p->VDSI);
		pr_TAT(fp, p->TAT);
		pr_TAC(fp, p->TAC);
		pr_VMI(fp, p->VMI);
		pr_TA(fp, p->TA);

		pr_HDSI(fp, p->HDSI);
		pr_TH(fp, p->TH, p->THTA);
		pr_HMI(fp, p->HMI);

		pr_NAC_p(fp, p->NAC_p);
		pr_NIC_baro(fp, p->NIC_baro);
		pr_SIL(fp, p->SIL);

		pr_CMC(fp, p->CMC);
		pr_EMERG(fp, p->EMERG);
	}
}
