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

#ifndef _MS_ES_H
#define _MS_ES_H

#define ES_SUBTYPE_NA 0xFF

enum ms_extended_squitter_t {
	ES_AIRBORNE_POSITION,
	ES_SURFACE_POSITION,
	ES_IDENTIFICATION,
	ES_AIRBORNE_VELOCITY,
	ES_EMERGENCY,
	ES_ACAS_RA_BROADCAST,
	ES_TEST_MESSAGE,
	ES_NATIONAL_USE,
	ES_TARGET_STATE,
	ES_OPERATIONAL_STATUS,
	ES_RESERVED
};

struct ms_ES_TYPE_t {
	enum ms_extended_squitter_t et;
	uint8_t tc;
	uint8_t st;
};

void mk_ES_TYPE(struct ms_ES_TYPE_t *et, uint8_t first_byte);
void *mk_extended_squitter(const uint8_t *msg, enum ms_extended_squitter_t es_type);
void pr_extended_squitter(FILE *fp, const void *p, const struct ms_ES_TYPE_t *et, int v);
bool df18_IMF(const uint8_t *msg);
#endif
