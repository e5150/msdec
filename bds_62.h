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

#ifndef _MS_BDS_62_H
#define _MS_BDS_62_H

#include "fields.h"

enum ms_DSI_t {
	DSI_NOINFO = 0,
	DSI_AUTOPILOT = 1,
	DSI_HOLDING = 2,
	DSI_FMS = 3
};

enum ms_TAT_t {
	TAT_FL = 0,
	TAT_MSL = 1
};

enum ms_THTA_t {
	THTA_TARGET_HEADING = 0,
	THTA_TRACK_ANGLE = 1
};

enum ms_TAC_t {
	TAC_HOLDING = 0,
	TAC_AUTOPILOT = 1,
	TAC_FMS = 2,
	TAC_RESERVED = 3
};

enum ms_MI_t {
	MI_NOINFO = 0,
	MI_ACQUIRING = 1,
	MI_CAPTURING = 2,
	MI_RESERVED = 3
};

struct ms_TA_t {
	uint16_t raw;
	int32_t ft;
};

struct ms_BDS_62_t {
	uint8_t FTC;
	uint8_t subtype;

	enum ms_DSI_t VDSI;
	enum ms_TAT_t TAT;
	bool BCF;
	enum ms_TAC_t TAC;
	enum ms_MI_t VMI;
	struct ms_TA_t TA;

	enum ms_DSI_t HDSI;
	enum ms_THTA_t THTA;
	uint16_t TH;
	enum ms_MI_t HMI;

	uint8_t NAC_p;
	bool NIC_baro;

	uint8_t SIL;
	uint8_t CMC;
	uint8_t EMERG;
};

struct ms_BDS_62_t *mk_BDS_62(const uint8_t *msg);
void pr_BDS_62(FILE *fp, const struct ms_BDS_62_t *p, int v);
#endif
