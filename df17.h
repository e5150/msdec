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

#ifndef _MS_DF17_H
#define _MS_DF17_H

#include "fields.h"
#include "es.h"

struct ms_DF17_t {
	uint8_t DF;
	uint8_t CA;
	struct ms_AA_t AA;
	struct ms_ES_TYPE_t ES_TYPE;
	void *aux; /* ME */
	uint32_t PI;
};
struct ms_DF17_t *mk_DF17(const uint8_t *msg);
void pr_DF17(FILE *fp, struct ms_DF17_t *p, int);


#endif
