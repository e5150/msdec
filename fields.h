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

#ifndef _MS_FIELDS_H
#define _MS_FIELDS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define YESNO(x) ((x) ? "Yes" : "No")
#define KT2KMPH(x) (1.852 * (x))
#define FT2METRES(x) (0.3048 * (x))

/*
 * Surveillance status
 */
enum ms_SS_t {
	SS_NO_CONDITION = 0,
	SS_PERMANENT_ALERT = 1,
	SS_TEMP_ALERT = 2,
	SS_SPI_CONDITION = 3
};

/*
 * UM - Utility message
 * [1] 3.1.2.6.5.3.1
 */
struct ms_UM_t {
	uint8_t IIS;
	uint8_t IDS;
};

/*
 * AC - Altitude code
 */
struct ms_AC_t {
	bool M;
	bool Q;
	uint16_t raw;
	int32_t alt_ft;
};

/*
 * AA - Address announced
 */
struct ms_AA_t {
	bool ICAO;
	uint32_t addr;
};

/*
 * GTS - Ground track and speed
 */
struct ms_GTS_t {
	bool valid;
	uint8_t speed;
	uint8_t angle;
};

struct ms_location_t {
	double lon;
	double lat;
};

/*
 * Compact position reporting
 */
struct ms_CPR_t {
	uint8_t Nb;
	bool F;
	uint32_t lat;
	uint32_t lon;
	bool surface;
	struct ms_location_t loc;
};

struct ms_velocity_t {
	double speed; /* kt */
	double heading; /* ° from mag N */
	double vert;  /* ft/min */
};

void pr_VS(FILE *fp, bool VS);
void pr_CC(FILE *fp, bool CC);
void pr_SL(FILE *fp, uint8_t SL);
void pr_RI(FILE *fp, uint8_t ri);
void pr_CF(FILE *fp, uint8_t cf);
void pr_CA(FILE *fp, uint8_t ca);
void pr_AA(FILE *fp, struct ms_AA_t AA);
void pr_DR(FILE *fp, uint8_t dr);
void pr_FS(FILE *fp, uint8_t fs);
void pr_ID(FILE *fp, uint16_t ID);
void pr_KE(FILE *fp, bool KE);
void pr_ND(FILE *fp, uint8_t ND);
void pr_UM(FILE *fp, struct ms_UM_t UM);
void pr_AC(FILE *fp, const struct ms_AC_t *AC);
void pr_altitude(FILE *fp, const struct ms_AC_t *AC);
void pr_AF(FILE *fp, int8_t af);
void pr_NIC(FILE *fp, uint8_t, bool);
void pr_CPR(FILE *fp, const struct ms_CPR_t *);
void pr_UTCT(FILE *fp, bool);
void pr_NAC_p(FILE *fp, uint8_t);
void pr_NIC_baro(FILE *fp, bool nb);
void pr_SIL(FILE *fp, uint8_t SIL);
void pr_EMERG(FILE *fp, uint8_t es);
void pr_NAC_v(FILE *fp, uint8_t nv);
void pr_SS(FILE *fp, enum ms_SS_t ss);

void pr_location(FILE *fp, const struct ms_location_t *);
void pr_velocity(FILE *fp, const struct ms_velocity_t *);
void pr_raw(FILE *fp, uint8_t start, uint8_t end, uint64_t val, const char *desc);
void pr_raw_header(FILE *fp);
void pr_raw_footer(FILE *fp);
void pr_raw_subfield_header(FILE *fp, uint8_t start, uint8_t end, const char *desc);
void pr_raw_subfield_footer(FILE *fp);

const char *get_ID_desc(uint16_t ID);

#endif
