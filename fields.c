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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "fields.h"
#include "compass.h"
#include "mac.h"
#include "nation.h"

/*
 * SS - Surveillance status
 * [1] 3.1.2.8.6.3.1.1
 */
void
pr_SS(FILE *fp, enum ms_SS_t ss) {
	fprintf(fp, "SS=%d:Surveillance status:", ss);
	switch (ss) {
	case SS_NO_CONDITION:
		fprintf(fp, "No condition\n");
		break;
	case SS_PERMANENT_ALERT:
		fprintf(fp, "Permanent alert\n");
		break;
	case SS_TEMP_ALERT:
		fprintf(fp, "Temporary alert\n");
		break;
	case SS_SPI_CONDITION:
		fprintf(fp, "SPI condition\n");
		break;
	}
}

/*
 * VS - Vertical status
 * [1] 3.1.2.8.2.1
 */
void
pr_VS(FILE *fp, bool VS) {
	fprintf(fp, "VS=%d:Vertical status:%s\n",
	       VS, VS ? "On the ground" : "Airborne");
}

/*
 * CC - Crosslink capability
 * [1] 3.1.2.8.2.3
 */
void
pr_CC(FILE *fp, bool CC) {
	fprintf(fp, "CC=%d:Cross-link capability:%s\n",
	       CC, CC ? "Supported by transpoder"
	              : "Not supported by transponder");
}

/*
 * SL - Sensitivity level
 * [1] 4.3.8.4.2.5
 */
void
pr_SL(FILE *fp, uint8_t SL) {
	if (SL == 0)
		fprintf(fp, "SL:ACAS inoperative\n");
	else
		fprintf(fp, "SL:ACAS operating at sensitivity level:%d\n", SL);
}

void
pr_velocity(FILE *fp, const struct ms_velocity_t *v) {
	char heading[20];

	fill_angle_str(heading, 20, v->heading);
	fprintf(fp, "Velocity:%'.1f kt (%'.0f km/h):%s (%s)\n",
	       v->speed, KT2KMPH(v->speed),
	       heading, get_comp_point(v->heading));
}

/*
 * CPR
 * [3] A.2.3.2.1
 */
void
pr_CPR(FILE *fp, const struct ms_CPR_t *CPR) {
	fprintf(fp, "CPR:Compact position reporting:%s\n",
	       CPR->F ? "odd" : "even");
	fprintf(fp, "CPR:Longitude:%d\n", CPR->lon);
	fprintf(fp, "CPR:Latitude:%d\n", CPR->lat);
}

/*
 * Horizontal containment radius
 * limit for position messages.
 * [3] B.2.3.1
 */
void
pr_NIC(FILE *fp, uint8_t FTC, bool ns) {
	switch (FTC) {
	/* Surface positions */
	case  5:
		fprintf(fp, "NIC=11:Navigational integrity:Rc < %.1f m\n", 7.5);
		break;
	case  6:
		fprintf(fp, "NIC=10:Navigational integrity:Rc < 25 m\n");
		break;
	case  7:
		if (ns)	fprintf(fp, "NIC=9:Navigational integrity:Rc < 75 m\n");
		else	fprintf(fp, "NIC=8:Navigational integrity:Rc < %.1f NM\n", 0.1);
		break;
	case  8:
		fprintf(fp, "NIC=0:Navigational integrity:Rc unknown\n");
		break;
	/* Airborne positions */
	case  0:
		fprintf(fp, "NIC=0:Navigational integrity:Rc unknown\n");
		break;
	case  9:
		fprintf(fp, "NIC=11:Navigational integrity:Rc < %.1f m and VPL < 11 m\n", 7.5);
		break;
	case 10:
		fprintf(fp, "NIC=10:Navigational integrity:Rc < 25 m and VPL < %.1f m\n", 37.5);
		break;
	case 11:
		if (ns)	fprintf(fp, "NIC=9:Navigational integrity:Rc < 75 and VPL < 112 m\n");
		else	fprintf(fp, "NIC=8:Navigational integrity:Rc < %.1f NM\n", 0.1);
		break;
	case 12:
		fprintf(fp, "NIC=7:Navigational integrity:Rc < %.1f NM\n", 0.2);
		break;
	case 13: /* Ought to be the other way... ? */
		if (ns)	fprintf(fp, "NIC=6:Navigational integrity:Rc < %.1f NM\n", 0.6);
		else	fprintf(fp, "NIC=6:Navigational integrity:Rc < %.1f NM", 0.5); 
		break;
	case 14:
		fprintf(fp, "NIC=5:Navigational integrity:Rc < 1 NM\n");
		break;
	case 15:
		fprintf(fp, "NIC=4:Navigational integrity:Rc < 2 NM\n");
		break;
	case 16:
		if (ns) printf("NIC=3:Navigational integrity:Rc < 4 NM\n");
		else	fprintf(fp, "NIC=2:Navigational integrity:Rc < 8 NM\n");
		break;
	case 17:
		fprintf(fp, "NIC=1:Navigational integrity:Rc < 20 NM\n");
		break;
	case 18:
		fprintf(fp, "NIC=0:Navigational integrity:Rc ≥ 20 NM / Unknown\n");
		break;
	case 20:
		fprintf(fp, "NIC=11:Navigational integrity:Rc < %.1f m and VPL < 11 m\n", 7.5);
		break;
	case 21:
		fprintf(fp, "NIC=10:Navigational integrity:Rc < 25 m and VPL < %.1f m\n", 37.5);
		break;
	case 22:
		fprintf(fp, "NIC=0:Navigational integrity:Rc ≥ 25 m or VPL ≥ %.1f m / Unknown\n", 37.5);
		break;
	default:
		fprintf(fp, "Invalid FTC\n");
	}
}

/*
 * RI - Reply Information (DF = 0, 16)
 * [1] 3.1.2.8.2.2
 * [1] 4.3.8.4.1.2
 */
void
pr_RI(FILE *fp, uint8_t ri) {
	fprintf(fp, "RI=%d:Air-air reply information:", ri);
	switch (ri) {
	case  0:
		fprintf(fp, "No operating ACAS\n");
		break;
	case  1:
		fprintf(fp, "Reserved\n");
		break;
	case  2:
		fprintf(fp, "ACAS with resolution capability inhibited\n");
		break;
	case  3:
		fprintf(fp, "ACAS with vertical-only resolution\n");
		break;
	case  4:
		fprintf(fp, "ACAS with vertical and horizontal resolution\n");
		break;
	case  5:
	case  6:
	case  7:
		fprintf(fp, "Reserved for ACAS\n");
		break;
	case  8:
		fprintf(fp, "No max-speed unavailable\n");
		break;
	case  9:
		fprintf(fp, "Max-speed ≤ 140 km/h\n");
		break;
	case 10:
		fprintf(fp, "140 km/h ≤ max-speed ≤ 280 km/h\n");
		break;
	case 11:
		fprintf(fp, "280 km/h ≤ max-speed ≤ 560 km/h\n");
		break;
	case 12:
		fprintf(fp, "560 km/h ≤ max-speed ≤ %'d km/h\n", 1100);
		break;
	case 13:
		fprintf(fp, "%'d km/h ≤ max-speed ≤ %'d km/h\n", 1100, 2220);
		break;
	case 14:
		fprintf(fp, "Max-speed ≥ %'d km/h\n", 2220);
		break;
	case 15:
		fprintf(fp, "Not assigned\n");
		break;
	default:
		fprintf(fp, "Invalid RI\n");
		break;
	}
}

/*
 * CF - Control Field (DF = 18)
 * [1] 3.1.2.8.7.2
 */
void
pr_CF(FILE *fp, uint8_t cf) {
	fprintf(fp, "CF=%d:", cf);
	switch (cf) {
	case  0:
		fprintf(fp, "ADS-B, ICAO address in AA\n");
		break;
	case  1:
		fprintf(fp, "ADS-B, Non-ICAO address in AA\n");
		break;
	case  2:
		fprintf(fp, "TIS-B, Fine format msg\n");
		break;
	case  3:
		fprintf(fp, "TIS-B, Coarse format msg\n");
		break;
	case  4:
		fprintf(fp, "TIS-B, Management msg\n");
		break;
	case  5:
		fprintf(fp, "TIS-B, Relay of ADS-B Non-ICAO\n");
		break;
	case  6:
		fprintf(fp, "ADS-B, Rebroadcast DF-17 ADS-B\n");
		break;
	case  7:
		fprintf(fp, "Reserved\n");
		break;
	default:
		fprintf(fp, "Invalid CF\n");
		break;
	}
}

/*
 * CA - Capability (DF = 11, 17)
 * [1] 3.1.2.5.2.2.1
 */
void
pr_CA(FILE *fp, uint8_t ca) {
	fprintf(fp, "CA=%d:Capability:", ca);
	switch (ca) {
	case  0:
		fprintf(fp, "Level 1: Surveillance only\n");
		break;
	case  1:
		fprintf(fp, "Reserved (TODO)\n");
		break;
	case  2:
		fprintf(fp, "Reserved (TODO)\n");
		break;
	case  3:
		fprintf(fp, "Reserved (TODO)\n");
		break;
	case  4:
		fprintf(fp, "Level 2+. On ground\n");
		break;
	case  5:
		fprintf(fp, "Level 2+. Airborne\n");
		break;
	case  6:
		fprintf(fp, "Level 2+.\n");
		break;
	case  7:
		fprintf(fp, "DR ≠ 0 ∨ FS ∈ {2, 3, 4, 5}\n");
		break;
	default:
		fprintf(fp, "Invalid CA\n");
		break;
	}
}

/*
 * Time
 * [3] A.2.3.2.2
 * ME-bit 21
 */
void
pr_UTCT(FILE *fp, bool T) {
	fprintf(fp, "T=%d:Time synchronized to UTC:%s\n", T, YESNO(T));
}

void
pr_AA(FILE *fp, struct ms_AA_t AA) {
	const struct ms_nation_t *n;

	if (AA.ICAO) {
		n = icao_addr_to_nation(AA.addr);
		fprintf(fp, "Address announced:%06X:%s\n",
		       AA.addr, n->name);
	} else {
		fprintf(fp, "Address announced:%06X:Non-ICAO\n",
		       AA.addr);
	}
}

/*
 * DR - Downlink Request (DF 4, 5, 20, 21)
 *
 * [1] 3.1.2.6.5.2
 * [1] 3.1.2.7.7.1 (ELM)
 * [1] 4.3.8.4.1.1 (ACAS)
 */
void
pr_DR(FILE *fp, uint8_t dr) {
	fprintf(fp, "DR=%02d:Downlink request:", dr);
	switch (dr) {
	case  0:
		fprintf(fp, "No downlink request\n");
		break;
	case  1: 
		fprintf(fp, "Send Comm-B\n");
		break;
	case  2: 
		fprintf(fp, "ACAS msg available\n");
		break;
	case  3: 
		fprintf(fp, "ACAS and Comm-B msg available\n");
		break;
	case  4:
		fprintf(fp, "Comm-B broadcast 1\n");
		break;
	case  5:
		fprintf(fp, "Comm-B broadcast 2\n");
		break;
	case  6:
		fprintf(fp, "Comm-B broadcast 1 and ACAS msg available\n");
		break;
	case  7:
		fprintf(fp, "Comm-B broadcast 2 and ACAS msg available\n");
		break;
	case  8:
	case  9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		fprintf(fp, "Reserved\n");
		break;
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
	case 27:
	case 28:
	case 29:
	case 30:
	case 31:
		fprintf(fp, "%d segment ELM\n", dr - 15);
		break;
	default:
		fprintf(fp, "Invalid DR\n");
	}
}

/*
 * FS - Flight status, DF ∈ {4, 5, 20, 21}
 * [1] 3.1.2.6.5.1
 */
void
pr_FS(FILE *fp, uint8_t fs) {
	fprintf(fp, "FS=%d:Flight status:", fs);
	switch (fs) {
	case 0:
		fprintf(fp, "Airborne\n");
		break;
	case 1:
		fprintf(fp, "On ground\n");
		break;
	case 2:
		fprintf(fp, "Airborne, Alert\n");
		break;
	case 3:
		fprintf(fp, "On ground, Alert\n");
		break;
	case 4:
		fprintf(fp, "SPI, Alert\n");
		break;
	case 5:
		fprintf(fp, "SPI\n");
		break;
	case 6:
		fprintf(fp, "Reserved\n");
		break;
	case 7:
		fprintf(fp, "N/A\n");
		break;
	default:
		fprintf(fp, "Invalid FS\n");
		break;
	}
}

/*
 * ID - Identity, DF = 5, 21
 * [1] 3.1.2.6.7.1
 * https://en.wikipedia.org/wiki/Transponder_(aeronautics)
 * http://www.flightradars.eu/squawkcodes.html
 * TODO: Fill up list, from an authoritative source.
 */
const char *
get_ID_desc(uint16_t ID) {
	switch (ID) {
	case 00033:
		return "Parachute dropping in progress (UK)";
	case 07500:
		return "Unlawful interference!";
	case 07600:
		return "Radio failure!";
	case 07700:
		return "Emergency!";
	default:
		return "";
	}
	return "";
}

void
pr_ID(FILE *fp, uint16_t ID) {
	fprintf(fp, "ID:%04o %s\n", ID, get_ID_desc(ID));
}

void
pr_KE(FILE *fp, bool KE) {
	fprintf(fp, "KE=%d:Control, ELM:%s\n", KE,
	       KE ? "Downlink transmission"
	          : "Uplink acknowledgement");
}

void
pr_ND(FILE *fp, uint8_t ND) {
	fprintf(fp, "ND:Number of D-segments:%d\n", ND);
}

/*
 * UM - Utility Message (DF = 4, 5, 20, 21)
 * [1] 3.1.2.6.5.3
 */
void
pr_UM(FILE *fp, struct ms_UM_t UM) {
	fprintf(fp, "UM:Utility message:IIS:%d:IDS:", UM.IIS);
	switch (UM.IDS) {
	case  0:
		fprintf(fp, "No info\n");
		break;
	case  1:
		fprintf(fp, "IIS is Comm-B\n");
		break;
	case  2:
		fprintf(fp, "IIS is Comm-C\n");
		break;
	case  3:
		fprintf(fp, "IIS is Comm-D\n");
		break;
	default:
		fprintf(fp, "Invalid IDS\n");
		break;
	}
}

/*
 * AC - Altitude code
 * [1] 3.1.2.6.5.4
 */
void
pr_AC(FILE *fp, const struct ms_AC_t *AC) {
	fprintf(fp, "AC:Altitude code:");
	if (AC->alt_ft == AC_INVALID)
		fprintf(fp, "%03X:Invalid\n", AC->raw);
	else if (AC->alt_ft == AC_M_RESERVED)
		fprintf(fp, "%03X:Reserved metric\n", AC->raw);
	else
		fprintf(fp, "%'d ± %.1f ft (%'.0f m)\n",
		       AC->alt_ft,
		       AC->Q ? 12.5 : 50,
		       FT2METRES(AC->alt_ft));
}


/*
 * AF - Application Field (DF = 19)
 * [1] 3.1.2.8.8.2
 * [2] 7.2
 */
void
pr_AF(FILE *fp, int8_t af) {
	fprintf(fp, "AF=%d:Application field:", af);
	switch (af) {
	case  1:
		fprintf(fp, "Civilian ES format\n");
		break;
	case  2:
		fprintf(fp, "Formation flight\n");
		break;
	case  3:
		fprintf(fp, "Military application\n");
		break;
	case  4:
	case  5:
	case  6:
	case  7:
		fprintf(fp, "Reserved\n");
		break;
	default:
		fprintf(fp, "Invalid AF\n");
		break;
	}
}

/*
 * NAC - Navigational accuracy category - Position
 * [3] B.2.3.10.7
 * [3] D.2.12
 */
void
pr_NAC_p(FILE *fp, uint8_t np) {
	fprintf(fp, "NAC_p=%d:Navigational accuracy:", np);

	switch (np) {
	case  0:
		fprintf(fp, "≥ 10 NM\n");
		break;
	case  1:
		fprintf(fp, "< 10 NM\n");
		break;
	case  2:
		fprintf(fp, "< 4 NM\n");
		break;
	case  3:
		fprintf(fp, "< 2 NM\n");
		break;
	case  4:
		fprintf(fp, "< 1 NM\n");
		break;
	case  5:
		fprintf(fp, "< %.1f NM\n", 0.5);
		break;
	case  6:
		fprintf(fp, "< %.1f NM\n", 0.3);
		break;
	case  7:
		fprintf(fp, "< %.1f NM\n", 0.1);
		break;
	case  8:
		fprintf(fp, "< %.2f NM\n", 0.05);
		break;
	case  9:
		fprintf(fp, "< 30 m (vert < 45 m)\n");
		break;
	case 10:
		fprintf(fp, "< 10 m (vert < 15 m)\n");
		break;
	case 11:
		fprintf(fp, "< 3 m (vert < 4 m)\n");
		break;
	case 12:
	case 13:
	case 14:
	case 15:
		fprintf(fp, "Reserved\n");
		break;
	default:
		fprintf(fp, "Invalid NACp\n");
		break;
	}
}

/*
 * NAC_v
 * [3] B.2.3.5.5
 * TODO: Which table?
 */
void
pr_NAC_v(FILE *fp, uint8_t nv) {
	fprintf(fp, "NAC_v=%d:Navigational accuracy:", nv);
	switch (nv) {
	case 4:
		fprintf(fp, "HFOM_R < %.1f m/s ∧ VFOM_R < %.2f m/s\n", 0.3, 0.46);
		break;
	case 3:
		fprintf(fp, "HFOM_R < 1 m/s ∧ VFOM_R < %.2f m/s\n", 1.52);
		break;
	case 2:
		fprintf(fp, "HFOM_R < 3 m/s ∧ VFOM_R < %.2f m/s\n", 4.57);
		break;
	case 1:
		fprintf(fp, "HFOM_R < 10 m/s ∧ VFOM_R < %.2f m/s\n", 15.24);
		break;
	case 0:
		fprintf(fp, "HFOM_R ≥ 10 m/s ∨ VFOM_R ≥ %.2f m/s, or unknown\n", 15.24);
		break;
	}
}

/*
 * NIC_baro - Barometric altitude integrity code
 * [3] B.2.3.10.10
 * [3] D.2.13
 */
void
pr_NIC_baro(FILE *fp, bool nb) {
	fprintf(fp, "Barometric altitude integrity:%s\n",
	       nb ? "Cross checked/Non-Gilham"
	          : "Unverified Gilham");
}

/*
 * SIL - Surv. integr. level
 * [3] B.2.3.10.9
 * [3] D.2.14
 */
void
pr_SIL(FILE *fp, uint8_t SIL) {
	fprintf(fp, "SIL=%d:Surveillance integrity:", SIL);
	switch (SIL) {
	case 0:
		fprintf(fp, "Unknown\n");
		break;
	case 1:
		fprintf(fp, "ℙ(NIC_ex ∨ VPL_ex) ≤ 1×10⁻³ / (hour or sample)\n");
		break;
	case 2:
		fprintf(fp, "ℙ(NIC_ex ∨ VPL_ex) ≤ 1×10⁻⁵ / (hour or sample)\n");
		break;
	case 3:
		fprintf(fp, "ℙ(NIC_ex) ≤ 1×10⁻⁷ / (hour or sample)\n");
		fprintf(fp, "ℙ(VPL_ex) ≤ 1×10⁻⁷ / (150 secs or sample)\n");
		break;
	}
}

/*
 * Emergency / priority status
 * [3] ...
 * [3] D.2.16
 */
void
pr_EMERG(FILE *fp, uint8_t es) {
	fprintf(fp, "EPS=%d:Emergency state:", es);
	switch (es) {
	case 0:
		fprintf(fp, "No emergency\n");
		break;
	case 1:
		fprintf(fp, "General emergency\n");
		break;
	case 2:
		fprintf(fp, "Lifeguard / Medical\n");
		break;
	case 3:
		fprintf(fp, "Minimum fuel\n");
		break;
	case 4:
		fprintf(fp, "No communications\n");
		break;
	case 5:
		fprintf(fp, "Unlawful interference\n");
		break;
	case 6:
		fprintf(fp, "Downed aircraft\n");
		break;
	case 7:
		fprintf(fp, "Reserved\n");
		break;
	default:
		fprintf(fp, "Invalid ES\n");
		break;
	}

}

void
pr_raw_header(FILE *fp) {
	fprintf(fp, "Bit(s)\tField\t%24s  %8s  %6s\n",
	       "Binary", "Decimal", "Hex");
}

void
pr_raw_footer(FILE *fp) {
	fprintf(fp, "Bit(s)\tField\t└──┴┴──┘└──┴┴──┘└──┴┴──┘\n");
}

void
pr_raw_subfield_header(FILE *fp, uint8_t start, uint8_t end, const char *desc) {
	fprintf(fp, "%3d-%3d\t%s\n", start, end, desc);
}

void
pr_raw_subfield_footer(FILE *fp) {
	fprintf(fp, "\tEnd of subfield\n");
}

void
pr_raw(FILE *fp, uint8_t start, uint8_t end, uint64_t val, const char *desc) {
	char binstr[113];
	uint8_t i, bits;

	bits = end - start + 1; /* start and end is inclusive */

	
	for (i = 0; i < bits; ++i)
		if ((val >> (bits - i - 1)) & 0x01)
			binstr[i] = '1';
		else
			binstr[i] = '0';
	binstr[i] = '\0';

	if (bits == 1)
		fprintf(fp, "%3d\t%s\t%24s  %8lu  %6lX\n",
		       start, desc, binstr, val, val);
	else
		fprintf(fp, "%3d-%3d\t%s\t%24s  %8lu  %6lX\n",
		       start, end, desc, binstr, val, val);
}
