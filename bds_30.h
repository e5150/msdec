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

#ifndef _MS_BDS_30_H
#define _MS_BDS_30_H

#include "fields.h"

struct ms_RAC_t {
	uint8_t raw;
	bool below;
	bool above;
	bool left;
	bool right;
};

struct ms_BDS_30_t {
	bool ara41;
	bool ara42;
	bool ara43;
	bool ara44;
	bool ara45;
	bool ara46;
	bool ara47;

	struct ms_RAC_t RAC;

	bool RAT;
	bool MTE;
	
	uint8_t TTI;

	uint32_t TID;

	struct ms_AC_t TIDA;
	uint8_t  TIDR;
	uint8_t  TIDB;

};

struct ms_BDS_30_t *mk_BDS_30(const uint8_t *msg);

void pr_BDS_30(FILE *fp, const struct ms_BDS_30_t *p, int v);
void pr_ACAS_RA(FILE *fp, const struct ms_BDS_30_t *p, int v);
#endif
