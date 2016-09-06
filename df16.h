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

#ifndef _MS_DF16_H
#define _MS_DF16_H

#include "fields.h"

struct ms_DF16_t {
	uint8_t DF;
	bool VS;
	uint8_t SL;
	uint8_t RI;
	struct ms_AC_t AC;
	uint8_t VDS;
	void *aux;
	uint32_t AP;
};
struct ms_DF16_t *mk_DF16(const uint8_t *msg);
void pr_DF16(FILE *fp, struct ms_DF16_t *p, int);

#endif
