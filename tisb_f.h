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

#ifndef _MS_TISB_F_H
#define _MS_TISB_F_H

#include "fields.h"

struct ms_TISB_fine_t {
	uint8_t FTC;
	bool IMF;
	enum ms_SS_t SS;
	struct ms_AC_t AC;
	struct ms_CPR_t CPR;
};

struct ms_TISB_fine_t *mk_TISB_fine(const uint8_t *msg);
void pr_TISB_fine(FILE *fp, const struct ms_TISB_fine_t *, int v);
#endif
