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

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "es.h"
#include "bds_05.h"
#include "bds_06.h"
#include "bds_08.h"
#include "bds_09.h"
#include "bds_61.h"
#include "bds_62.h"
#include "bds_65.h"

void
mk_ES_TYPE(struct ms_ES_TYPE_t *et, uint8_t first_byte) {
	et->tc = first_byte >> 3;
	et->st = first_byte & 0x03;
	et->et = ES_RESERVED;

	switch (et->tc) {
	case  0:
	case  9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 20:
	case 21:
	case 22:
		et->st = ES_SUBTYPE_NA;
		et->et = ES_AIRBORNE_POSITION;
		break;

	case  1:
	case  2:
	case  3:
	case  4:
		et->st = ES_SUBTYPE_NA;
		et->et = ES_IDENTIFICATION;
		break;

	case  5:
	case  6:
	case  7:
	case  8:
		et->st = ES_SUBTYPE_NA;
		et->et = ES_SURFACE_POSITION;
		break;
	
	case 19:
		if (0 < et->st && et->st < 5) {
			et->et = ES_AIRBORNE_VELOCITY;
		}
		break;

	case 23:
		if (et->st == 0) {
			et->et = ES_TEST_MESSAGE;
		}
		else if (et->st == 7) {
			et->et = ES_NATIONAL_USE;
		}
		break;

	case 28:
		if (et->st == 1) {
			et->et = ES_EMERGENCY;
		}
		else if (et->st == 2) {
			et->et = ES_ACAS_RA_BROADCAST;
		}
		break;

	case 29:
		/* Only two bit subtype */
		et->st = (first_byte >> 1) & 0x03;
		if (et->st == 0) {
			et->et = ES_TARGET_STATE;
		}
		break;

	case 31:
		if (et->st < 2) {
			et->et = ES_OPERATIONAL_STATUS;
		}
		break;
	}
}

void *
mk_extended_squitter(const uint8_t *msg, enum ms_extended_squitter_t es_type) {

	switch (es_type) {
	case ES_AIRBORNE_POSITION:
		return mk_BDS_05(msg);
	case ES_IDENTIFICATION:
		return mk_BDS_08(msg);
	case ES_SURFACE_POSITION:
		return mk_BDS_06(msg);
	case ES_AIRBORNE_VELOCITY:
		return mk_BDS_09(msg);
	case ES_EMERGENCY:
		return mk_BDS_61_1(msg);
	case ES_ACAS_RA_BROADCAST:
		return mk_BDS_61_2(msg);
	case ES_TARGET_STATE:
		return mk_BDS_62(msg);
	case ES_OPERATIONAL_STATUS:
		return mk_BDS_65(msg);
	case ES_NATIONAL_USE:
	case ES_TEST_MESSAGE:
	case ES_RESERVED:
		return NULL;
	}
	return NULL;
}

bool
df18_IMF(const uint8_t *msg) {
	uint8_t CF = msg[0] & 0x07;
	struct ms_ES_TYPE_t et;


	switch (CF) {
	case 0:
		return false;
	case 2:
		return msg[4] & 0x01;
	case 3:
		return msg[4] & 0x80;
	case 1:
	case 6:
		mk_ES_TYPE(&et, msg[4]);
		switch (et.et) {
		case ES_AIRBORNE_POSITION:
			/* [3] B.4.4.1 */
			return msg[4] & 0x01;
		case ES_SURFACE_POSITION:
			/* [3] B.4.4.2 */
			return msg[6] & 0x80;
		case ES_IDENTIFICATION:
			/* [3] B.4.4.3 */
			return false;
		case ES_AIRBORNE_VELOCITY:
			/* [3] B.4.4.4 */
			return msg[5] & 0x80;
		case ES_OPERATIONAL_STATUS:
			/* [3] B.4.4.5 */
			return msg[10] & 0x01;
		default:
			return true;
		}
		break;
	/* Return true for Reserved/TODO/unknwon */
	case 4:
	case 5:
	case 7:
		return true;
	}
	return true;

}

void
pr_extended_squitter(FILE *fp, const void *p, const struct ms_ES_TYPE_t *et, int v) {
	switch (et->et) {
	case ES_AIRBORNE_POSITION:
		pr_BDS_05(fp, p, v);
		break;
	case ES_IDENTIFICATION:
		pr_BDS_08(fp, p, v);
		break;
	case ES_SURFACE_POSITION:
		pr_BDS_06(fp, p, v);
		break;
	case ES_AIRBORNE_VELOCITY:
		pr_BDS_09(fp, p, v);
		break;
	case ES_EMERGENCY:
		pr_BDS_61_1(fp, p, v);
		break;
	case ES_ACAS_RA_BROADCAST:
		pr_BDS_61_2(fp, p, v);
		break;
	case ES_TARGET_STATE:
		pr_BDS_62(fp, p, v);
		break;
	case ES_OPERATIONAL_STATUS:
		pr_BDS_65(fp, p, v);
		break;
	case ES_NATIONAL_USE:
		fprintf(fp, "ES=%d,%d:National use:TODO\n", et->tc, et->st);
		break;
	case ES_TEST_MESSAGE:
		fprintf(fp, "ES=%d,%d:Test message:TODO\n", et->tc, et->st);
		break;
	case ES_RESERVED:
		fprintf(fp, "ES=%d,%d:Reserved\n", et->tc, et->st);
		break;
	default:
		fprintf(fp, "ES=%d,%d:Invalid ES type\n", et->tc, et->st);
		break;
	}
}
