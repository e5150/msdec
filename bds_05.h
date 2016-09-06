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

#ifndef _MS_BDS_05_H
#define _MS_BDS_05_H

#include "fields.h"

enum ms_SAF_t {
	SAF_SINGLE_ANTENNA = 0,
	SAF_DUAL_ANTENNAE = 1
};

enum ms_ALT_TYPE_t {
	ALT_BAROMETRIC,
	ALT_GNSS
};

struct ms_BDS_05_t {
	uint8_t FTC;
	enum ms_SS_t SS;
	enum ms_SAF_t SAF;
	struct ms_AC_t AC;
	bool UTC_SYNCED_TIME;
	struct ms_CPR_t CPR;
	enum ms_ALT_TYPE_t alt_type;
};

struct ms_BDS_05_t *mk_BDS_05(const uint8_t *msg);
void pr_BDS_05(FILE *fp, const struct ms_BDS_05_t *p, int v);
#endif
