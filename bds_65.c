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

#include "bds_65.h"

/*
 * OM - Operational mode
 * [3] B.2.3.10.4
 * Only OM format 00 is specified as of version 1
 */
static void
fill_OM(const uint8_t *raw, struct ms_OM_t *OM) {
	uint8_t OM_format = raw[0] & 0xC0;

	if (OM_format == 00) {
		OM->ACAS_RA  = raw[0] & 0x20 ? true : false;
		OM->IDENT    = raw[0] & 0x10 ? true : false;
		OM->recv_ATC = raw[0] & 0x08 ? true : false;
	} else {
		/* Reserved */
	}
}

/*
 * BDS 6,5 - Aircraft operational status
 * [3] B.2.3.10
 * [3] Table B-2-101
 *
 *  Subtype 0 - Airborne
 * 
 *           1         2         3         4         5    --
 *  12345678901234567890123456789012345678901234567890123HRD
 *  ME      CC             ]OM             ]   NIC  00SIL
 *       sub                                ver NAC]    NIC
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 *
 *  Subtype 1 - Surface
 *
 *           1         2         3         4         5    --
 *  12345678901234567890123456789012345678901234567890123HRD
 *  ME      CC         ]    OM             ]   NIC  --SIL
 *       sub            SIZE                ver NAC]    TRK
 *  01234567012345670123456701234567012345670123456701234567
 *  0       1       2       3       4       5       6       
 *
 */
struct ms_BDS_65_t *
mk_BDS_65(const uint8_t *msg) {
	struct ms_BDS_65_t *ret;
	ret = calloc(1, sizeof(struct ms_BDS_65_t));

	ret->FTC = msg[0] >> 3;
	ret->subtype = msg[0] & 0x07;

	switch (ret->subtype) {
	case 0: /* Airborne */
		/*
		 * CC - Airborne Capability Class
		 * [3] - B.2.3.10.3
		 * ME-bits 9-24
		 * 
		 *          1         2         3
		 * 123456789012345678901234567890123456789
		 *         SlACSlATTC
		 * 0123456701234567012345670123456701234567
		 * 0       1       2       3       4
		 *
		 */

		ret->data.AB.ACC.raw = (msg[1] << 8) | msg[2];

		/* Service level */
		ret->data.AB.ACC.SerL = 0;
		if (msg[1] & 0x80) ret->data.AB.ACC.SerL |= 0x08;
		if (msg[1] & 0x40) ret->data.AB.ACC.SerL |= 0x04;
		if (msg[1] & 0x08) ret->data.AB.ACC.SerL |= 0x02;
		if (msg[1] & 0x04) ret->data.AB.ACC.SerL |= 0x01;

		/* ACAS (Reversed from specification's “Not-ACAS”) */
		ret->data.AB.ACC.ACAS = (msg[1] & 0x20) ? false : true;

		/* CDTI - Cockpit display/Traffic information */
		ret->data.AB.ACC.CDTI = (msg[1] & 0x10) ? true : false;

		/* ARV - Air-Referenced velocity report capability */
		ret->data.AB.ACC.ARV = (msg[1] & 0x02) ? true : false;

		/* TS - Target state report capability */
		ret->data.AB.ACC.TS = (msg[1] & 0x01) ? true : false;

		/* TC - Target change report capability */
		ret->data.AB.ACC.TC = (msg[2] & 0xC0) >> 6;


		/*
		 * BAQ - Barometric altitude quality
		 * [3] B.2.3.10.8
		 * ME-bits 49-50
		 * (Shall be set to zero.)
		 */
		ret->data.AB.BAQ = msg[6] >> 6;

		/*
		 * NIC_baro - Barometric altitude integrity code
		 * [3] B.2.3.10.10
		 * ME-bit 53
		 */
		ret->data.AB.NIC_baro = (msg[6] & 0x08) ? true : false;
		break;

		/*
		 * HRD - Horizontal reference direction
		 * [3] B.2.3.10.13
		 * ME-bit 54
		 */
		ret->data.AB.HRD = (msg[6] & 0x04) ? HRD_MAGNORTH : HRD_TRUENORTH;



	case 1: /* Surface */
		/*
		 * CC - Surface Capability Class
		 * [3] B.2.3.10.3
		 * ME-bits 9-20 
		 * 
		 *          1         2
		 * 123456789012345678901234
		 *         SlPCSlB
		 * 012345670123456701234567
		 * 0       1       2
		 *
		 */

		ret->data.SF.SCC.raw = (msg[1] << 4) | (msg[2] >> 4);

		/* Service levels */
		ret->data.SF.SCC.SerL = 0;
		if (msg[1] & 0x80) ret->data.SF.SCC.SerL |= 0x08;
		if (msg[1] & 0x40) ret->data.SF.SCC.SerL |= 0x04;
		if (msg[1] & 0x08) ret->data.SF.SCC.SerL |= 0x02;
		if (msg[1] & 0x04) ret->data.SF.SCC.SerL |= 0x01;
		
		/* Position offset applied */
		ret->data.SF.SCC.POA = (msg[1] & 0x20) ? true : false;
		
		/* Cockpit display operational */
		ret->data.SF.SCC.CDTI = (msg[1] & 0x10) ? true : false;
		
		/* B2 trasmit power ≤ 70 W */
		ret->data.SF.SCC.B2_low = (msg[1] & 0x02) ? true : false;


		/*
		 * Aircraft length and width codes
		 * [3] B.2.3.10.11
		 * ME-bits 21-24
		 * (Table says ME bit 49-52, but those are
		 * the bit numbers of the overall message.)
		 */
		ret->data.SF.LW.raw = msg[2] & 0x0F;	
		{
			uint8_t lc = ret->data.SF.LW.raw >> 1;
			bool wc = ret->data.SF.LW.raw & 0x01;
			int l = 0, w = 0;
			ret->data.SF.LW.l_max = false;
			ret->data.SF.LW.w_max = false;

			switch (lc) {
			case 0:
				l = 150;
				w = wc ? 230 : 115;
				break;
			case 1:
				l = 250;
				w = wc ? 340 : 285;
				break;
			case 2:
				l = 350;
				w = wc ? 380 : 330;
				break;
			case 3:
				l = 450;
				w = wc ? 450 : 395;
				break;
			case 4:
				l = 550;
				w = wc ? 520 : 450;
				break;
			case 5:
				l = 650;
				w = wc ? 670 : 595;
				break;
			case 6:
				l = 750;
				w = wc ? 800 : 725;
				break;
			case 7:
				l = 850;
				ret->data.SF.LW.l_max = true;
				if (wc) {
					w = 900;
					ret->data.SF.LW.w_max = true;
				} else {
					w = 800;
				}
				break;
			}
			ret->data.SF.LW.length_dm = l;
			ret->data.SF.LW.width_dm = w;
		}

		/*
		 * Track angle/heading
		 * [3] B.2.3.10.12
		 * ME-bit 53
		 */
		ret->data.SF.TRK_HDG = (msg[6] & 0x08)
			     ? TARGET_HEADING_ANGLE
			     : TRACK_ANGLE;


		break;
	}

	/* --- Common for both subformats --- */

	ret->OM.raw = (msg[3] << 8) | msg[4];
	fill_OM(msg + 3, &ret->OM);
	/*
	 * VER - Extended squitter version number
	 * [3] B.2.3.10.5
	 * ME-bits 41-43
	 */
	ret->ver = msg[5] >> 5;

	/*
	 * NIC Supplement
	 * [3] B.2.3.10.6
	 * ME-bit 44
	 */
	ret->NIC_s = (msg[5] & 0x10) ? true : false;

	/*
	 * NAC_p - Navigational accuracy category - Position
	 * [3] B.2.3.10.7
	 * ME-bits 45-48
	 */
	ret->NAC_p = msg[5] & 0x0F;

	/*
	 * SIL - Surveillance integrity level
	 * [3] B.2.3.10.9
	 * ME-bits 51-52
	 */
	ret->SIL = (msg[6] >> 4) & 0x03;

	return ret;
}


static void
pr_POA(FILE *fp, bool POA) {
	fprintf(fp, "CC:Position offset applied:%s\n", YESNO(POA));
}

static void
pr_B2_low(FILE *fp, bool bl) {
	fprintf(fp, "CC:B2 transmit power ≥ 70 W:%s\n", YESNO(bl));
}

static void
pr_ACAS_OP(FILE *fp, bool ACAS) {
	fprintf(fp, "CC:ACAS operational:%s\n", 
	       ACAS ? "yes / unknown"
	            : "no / not install");
}

static void
pr_SerL(FILE *fp, uint8_t SL) {
	fprintf(fp, "CC:Service levels:%d\n", SL);
}

/* CDTI - Cockpit display/Traffic information */
static void
pr_CDTI(FILE *fp, bool CDTI) {
	fprintf(fp, "CC:Cockpit display operational:%s\n",
	       YESNO(CDTI));
}

/* ARV - Air-Referenced velocity report capability */
static void
pr_ARV(FILE *fp, bool ARV) {
	fprintf(fp, "CC:Air-referenced velocity reports supported:%s\n",
	       YESNO(ARV));
}

/* TS - Target state report capability */
static void
pr_TS(FILE *fp, bool TS) {
	fprintf(fp, "CC:Target state report supported:%s\n",
	       YESNO(TS));
}

/* TC - Target change report capability */
static void
pr_TC(FILE *fp, uint8_t TC) {
	fprintf(fp, "CC:Target change report:%s\n", 
	       TC == 0 ? "Not supported" :
	       TC == 1 ? "Only TC+0 reports" :
	       TC == 2 ? "Multiple TC reports supported"
		       : "Reserved");
}

/*
 * BAQ - Barometric altitude quality
 * [3] B.2.3.10.8
 * (Shall be set to zero.)
 */
static void
pr_BAQ(FILE *fp, uint8_t BAQ) {
	fprintf(fp, "BAQ:Barometric altitude quality:%d\n", BAQ);
}

/*
 * HRD - Horizontal reference direction
 * [3] B.2.3.10.13
 * ME-bit 54
 */
static void
pr_HRD(FILE *fp, bool HRD) {
	fprintf(fp, "HRD:Horizontal reference direction:%s\n",
	       HRD == HRD_TRUENORTH ? "True north"
	                            : "Magnetic north");
}

/*
 * VER - Extended squitter version number
 * [3] B.2.3.10.5
 * ME-bits 41-43
 */
static void
pr_VER(FILE *fp, uint8_t ver) {
	/* FIXME: I'm seeing a lot of ver = 2… */
	fprintf(fp, "ES version number:%d:%s\n", ver,
	       ver == 0 ? "ICAO-9871 ed 1, App A" :
	       ver == 1 ? "ICAO-9871 ed 1, App B"
	                : "Reserved");
}

/*
 * OM - Operational Mode
 * [3] B.2.3.10.4
 * ME-bits 25-40
 */
static void
pr_OM(FILE *fp, struct ms_OM_t OM) {
	fprintf(fp, "Operational mode:Format:00\n");
	fprintf(fp, "OM:ACAS RA active:%s\n", YESNO(OM.ACAS_RA));
	fprintf(fp, "OM:IDENT active:%s\n",   YESNO(OM.IDENT));
	fprintf(fp, "OM:Receiving ATC:%s\n",  YESNO(OM.recv_ATC));
}

/*
 * NIC Supplement
 * [3] B.2.3.10.6
 * ME-bit 44
 */
static void
pr_NIC_s(FILE *fp, bool ns) {
	fprintf(fp, "NIC supplement:%d\n", ns);
}


static void
pr_TRK_HDG(FILE *fp, bool th) {
	fprintf(fp, "Heading/track reported:%s\n",
	       th == TRACK_ANGLE ? "Track angle"
	                         : "Heading angle");
}

static void
pr_LW(FILE *fp, struct ms_LW_t LW) {
	fprintf(fp, "Length %s ", LW.l_max ? "<" : "≥");
	if (LW.length_dm % 2)
		fprintf(fp, "%.1f", LW.length_dm / 10.0);
	else
		fprintf(fp, "%d", LW.length_dm / 10);
	fprintf(fp, ":");
	fprintf(fp, "Width %s ", LW.w_max ? "<" : "≥");
	if (LW.width_dm % 2)
		fprintf(fp, "%.1f", LW.width_dm / 10.0);
	else
		fprintf(fp, "%d", LW.width_dm / 10);
	fprintf(fp, "\n");
}

void
pr_BDS_65(FILE *fp, const struct ms_BDS_65_t *p, int v) {
	if (v == 1) {
		pr_raw(fp, 1, 5, p->FTC, "FTC");
		pr_raw(fp, 6, 8, p->subtype, "SUB");
		if (p->subtype == 0) {
			pr_raw(fp, 9, 24, p->data.AB.ACC.raw, "CC");
		}
		else if (p->subtype == 1) {
			pr_raw(fp,  9, 20, p->data.SF.SCC.raw, "CC");
			pr_raw(fp, 21, 24, p->data.SF.LW.raw,  "LW");
		}
		pr_raw(fp, 25, 40, p->OM.raw, "OM"); 
		pr_raw(fp, 41, 43, p->ver, "VER");
		pr_raw(fp, 44, 44, p->NIC_s, "NIC_s");
		pr_raw(fp, 45, 48, p->NAC_p, "NAC_p");
		if (p->subtype == 0) {
			pr_raw(fp, 49, 50, p->data.AB.BAQ, "BAQ");
		}
		pr_raw(fp, 51, 52, p->SIL, "SIL");
		if (p->subtype == 0) {
			pr_raw(fp, 53, 53, p->data.AB.NIC_baro, "NIC_b");
			pr_raw(fp, 54, 54, p->data.AB.HRD, "HRD");
		}
		else if (p->subtype == 1) {
			pr_raw(fp, 53, 53, p->data.SF.TRK_HDG, "TRK/HDG");
		}
	} else {
		fprintf(fp, "BDS:6,5:Aircraft operational status\n");
		switch (p->subtype) {
		case 0:
			fprintf(fp, "CC:Capability class, airborne\n");
			pr_SerL(fp, p->data.AB.ACC.SerL);
			pr_ACAS_OP(fp, p->data.AB.ACC.ACAS);
			pr_CDTI(fp, p->data.AB.ACC.CDTI);
			pr_ARV(fp, p->data.AB.ACC.ARV);
			pr_TS(fp, p->data.AB.ACC.TS);
			pr_TC(fp, p->data.AB.ACC.TC);


			pr_BAQ(fp, p->data.AB.BAQ);
			pr_NIC_baro(fp, p->data.AB.NIC_baro);
			pr_HRD(fp, p->data.AB.HRD);
			break;
		case 1:
			fprintf(fp, "CC:Capability class, surface\n");
			pr_SerL(fp, p->data.SF.SCC.SerL);
			pr_POA(fp, p->data.SF.SCC.POA);
			pr_CDTI(fp, p->data.SF.SCC.CDTI);
			pr_B2_low(fp, p->data.SF.SCC.B2_low);

			pr_LW(fp, p->data.SF.LW);
			pr_TRK_HDG(fp, p->data.SF.TRK_HDG);
			break;
		}

		pr_OM(fp, p->OM);	
		pr_VER(fp, p->ver);
		pr_NIC_s(fp, p->NIC_s);
		pr_NAC_p(fp, p->NAC_p);
		pr_SIL(fp, p->SIL);
	}
}
