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

#ifndef _MS_DF18_H
#define _MS_DF18_H

#include "fields.h"
#include "es.h"

struct ms_MATF_t {
	uint16_t mode_a;
	uint16_t track_file;
};

struct ms_DF18_t {
	uint8_t DF;
	uint8_t CF;
	union {
		struct ms_AA_t AA;
		struct ms_MATF_t MA;
	} AAu;
	bool IMF;
	struct ms_ES_TYPE_t ES_TYPE;
	void *aux;
	uint32_t PI;
};

struct ms_DF18_t *mk_DF18(const uint8_t *msg);
void pr_DF18(FILE *fp, struct ms_DF18_t *p, int);

#endif
