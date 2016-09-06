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

#ifndef _MS_MSG_H
#define _MS_MSG_H

#include "fields.h"

#include "df00.h"
#include "df04.h"
#include "df05.h"
#include "df11.h"
#include "df16.h"
#include "df17.h"
#include "df18.h"
#include "df19.h"
#include "df20.h"
#include "df21.h"
#include "df24.h"

#include "bds_05.h"
#include "bds_06.h"
#include "bds_08.h"
#include "bds_09.h"
#include "bds_30.h"
#include "bds_61.h"
#include "bds_62.h"
#include "bds_65.h"
#include "bds_f2.h"
#include "es.h"
#include "tisb_c.h"
#include "tisb_f.h"

struct ms_aircraft_t;

struct ms_CRC_t {
	uint32_t crc;
	uint32_t AP;
	uint32_t syn;
};

struct ms_msg_t {
	uint8_t DF;
	uint8_t BDS;
	struct ms_CRC_t cksum;
	time_t time;
	uint32_t addr;
	size_t len;
	uint8_t *raw;
	void *msg;
	struct ms_msg_t *next;
	struct ms_msg_t *ac_next;
	struct ms_aircraft_t *aircraft;
	void *ext;
};


void   pr_msg(FILE *fp, const struct ms_msg_t*, int v);
struct ms_msg_t *mk_msg(uint8_t*, time_t, uint32_t);
void   destroy_msg(struct ms_msg_t*);

#endif
