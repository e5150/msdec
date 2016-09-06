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

#ifndef _MS_TISB_C_H
#define _MS_TISB_C_H

#include "fields.h"

struct ms_TISB_coarse_t {
	bool IMF;
	uint8_t SVID;
	enum ms_SS_t SS;
	struct ms_AC_t AC;
	struct ms_velocity_t velocity;
	struct ms_GTS_t GTS;
	struct ms_CPR_t CPR;
};

struct ms_TISB_coarse_t *mk_TISB_coarse(const uint8_t *msg);
void pr_TISB_coarse(FILE *fp, const struct ms_TISB_coarse_t *, int v);
#endif
