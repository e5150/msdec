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

#ifndef _MS_BDS_61_H
#define _MS_BDS_61_H

#include "bds_30.h"
#include "fields.h"


/*
 * BDS code 6,1 - Aircraft status
 * Subtype 1: Emergency / Priority status
 * Subtype 2: ACAS RA (handled as BDS 3,0)
 */

/*
 * [3] Table B-2-97a
 */
struct ms_BDS_61_1_t {
	uint8_t emergency_state;
};

#define ms_BDS_61_2_t ms_BDS_30_t
#define mk_BDS_61_2   mk_BDS_30

struct ms_BDS_61_1_t *mk_BDS_61_1(const uint8_t *msg);

void pr_BDS_61_1(FILE *fp, const struct ms_BDS_61_1_t *pp, int v);
void pr_BDS_61_2(FILE *fp, const struct ms_BDS_61_2_t *pp, int v);

#endif
