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

#ifndef _MS_BDS_65_H
#define _MS_BDS_65_H

#include "fields.h"

/* -- helper structs -- */
enum TRK_HDG_t {
	TARGET_HEADING_ANGLE = 0,
	TRACK_ANGLE = 1
};

enum HRD_t {
	HRD_TRUENORTH = 0,
	HRD_MAGNORTH = 1
};

struct ms_LW_t {
	uint16_t length_dm;
	uint16_t width_dm;
	bool l_max;
	bool w_max;
	uint8_t raw;
};

/*
 * OM - Operational mode
 * [3] B.2.3.10.4
 */
struct ms_OM_t {
	uint16_t raw;
	bool ACAS_RA;
	bool IDENT;
	bool recv_ATC;
};

/*
 * ACC - Capability class, airborne
 * [3] B.2.3.10.3
 */
struct ms_ACC_t {
	uint16_t raw;
	uint8_t SerL;
	bool ACAS;
	bool CDTI;
	bool ARV;
	bool TS;
	uint8_t TC;
	/* bits 19-24 reserved */
};

/*
 * SCC - Capability class, surface
 * [3] B.2.3.10.3
 */
struct ms_SCC_t {
	uint16_t raw;
	uint8_t SerL;
	bool POA;
	bool CDTI;
	bool B2_low;
	/* bits 16-20 reserved */
};

struct ms_AB_OS_t {
	struct ms_ACC_t ACC;
	uint8_t BAQ;
	bool NIC_baro;
	enum HRD_t HRD;
};
struct ms_SF_OS_t {
	struct ms_SCC_t SCC;
	struct ms_LW_t LW;
	struct ms_OM_t OM;
	enum TRK_HDG_t TRK_HDG;
};

struct ms_BDS_65_t {
	uint8_t FTC;
	uint8_t subtype;
	union {
		struct ms_AB_OS_t AB;
		struct ms_SF_OS_t SF;
	} data;
	struct ms_OM_t OM;
	uint8_t ver;
	bool NIC_s;
	uint8_t NAC_p;
	uint8_t SIL;
};

struct ms_BDS_65_t *mk_BDS_65(const uint8_t *msg);
void pr_BDS_65(FILE *fp, const struct ms_BDS_65_t *p, int v);
#endif
