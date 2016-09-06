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

#ifndef _MS_BDS_F2_H
#define _MS_BDS_F2_H

#include "fields.h"

struct ms_BDS_F2_t {
	uint8_t TYPE;
	bool M1CF;
	struct {
		bool status;
		uint16_t raw;
		uint16_t val;
	} modes[3];
	uint8_t reserved;
};

void pr_BDS_F2(FILE *fp, const struct ms_BDS_F2_t*p, int v);
struct ms_BDS_F2_t *mk_BDS_F2(const uint8_t *msg);

#endif


