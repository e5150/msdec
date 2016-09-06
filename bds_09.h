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

#ifndef _MS_BDS_09_H
#define _MS_BDS_09_H

#include "fields.h"

enum ms_BDS_09_subtype_t {
	AV_OVER_GROUND = 1,
	AV_OVER_GROUND_SUPS = 2,
	AV_AIRSPEED = 3,
	AV_AIRSPEED_SUPS = 4
};

struct ms_BDS_09_VOG_t {
	bool EW_direction;
	bool NS_direction;
	uint16_t EW_v;
	uint16_t NS_v;
};

struct ms_BDS_09_ASH_t {
	bool mag_status;
	bool as_type;
	uint16_t heading;
	uint16_t airspeed;

};

struct ms_BDS_09_t {
	uint8_t FTC;
	enum ms_BDS_09_subtype_t subtype;

	bool ICF;
	bool IFR;
	uint8_t NAC_v;

	bool GNSS_sign;
	uint8_t GNSS_diff;

	bool VR_source;
	bool VR_sign;
	uint16_t VR;

	union {
		struct ms_BDS_09_VOG_t vog;
		struct ms_BDS_09_ASH_t ash;
	} data;

	struct ms_velocity_t velocity;

};

struct ms_BDS_09_t *mk_BDS_09(const uint8_t *msg);
void pr_BDS_09(FILE *fp, const struct ms_BDS_09_t *p, int v);
#endif
