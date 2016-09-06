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
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "config.h"
#include "aircraft.h"
#include "message.h"
#include "util.h"
#include "crc.h"
#include "nation.h"
#include "compass.h"

struct ms_msg_t *
mk_msg(uint8_t *msg, time_t tme, uint32_t addr) {
	struct ms_msg_t *ret;
	size_t i;

	ret = calloc(1, sizeof(struct ms_msg_t));

	/*
	 * [1] Figure-3.7 Note 3, DF24 is any
	 * message starting with binary 11
	 */
	ret->DF = msg[0] >> 3;
	if (ret->DF > 24)
		ret->DF = 24;
	
	if (tme)
		ret->time = tme;
	else
		ret->time = time(NULL);

	if (ret->DF > 11)
		ret->len = 14;
	else
		ret->len = 7;
	
	ret->raw = calloc(ret->len, sizeof(uint8_t));
	for (i = 0; i < ret->len; ++i)
		ret->raw[i] = msg[i];

	ret->cksum.crc = checksum(ret->raw, ret->len);
	ret->cksum.AP  = (ret->raw[ret->len - 3] << 16)
		       | (ret->raw[ret->len - 2] <<  8)
		       | (ret->raw[ret->len - 1] <<  0);
	ret->cksum.syn = ret->cksum.crc ^ ret->cksum.AP;

	if (addr & 0x00FFFFFF) {
		ret->addr = addr;
		/* TODO ... */
		if (ret->DF == 20 || ret->DF == 21) {
			uint32_t BDS = ret->cksum.syn ^ ret->addr;
			if (BDS & 0x00FF0000) {
				ret->BDS = BDS >> 16;
			}
		}
	} else {
		switch (ret->DF) {
		case 11:
		case 17:
		case 18:
		case 19:
			/* AA */
			ret->addr = ret->raw[1] << 16
			          | ret->raw[2] << 8
			          | ret->raw[3];
			break;
		case 20:
		case 21:
		case  0:
		case  4:
		case  5:
		case 16:
		case 24:
			/* AP */
			ret->addr = ret->cksum.syn & 0x00FFFFFF;
			break;
		}
	

	}

#define MKDF(X, NN) case X: ret->msg = mk_DF ## NN (ret->raw); break; 
	switch (ret->DF) {
	MKDF( 0, 00)
	MKDF( 4, 04)
	MKDF( 5, 05)
	MKDF(11, 11)
	MKDF(16, 16)
	MKDF(17, 17)
	MKDF(18, 18)
	MKDF(19, 19)
	MKDF(20, 20)
	MKDF(21, 21)
	MKDF(24, 24)
	}

	ret->ext = NULL;
	return ret;

}

void
pr_msg(FILE *fp, const struct ms_msg_t *msg, int v) {
	struct ms_aircraft_t *a = msg->aircraft;
	char timestr[20];
	struct tm *tmp;
	size_t i;

	if ((tmp = localtime(&msg->time)) && strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", tmp)) {
		fprintf(fp, "recv:%s\n", timestr);
	} else {
		fprintf(fp, "recv:%ld\n", msg->time);
	}

	fprintf(fp, "data:");
	for (i = 0; i < msg->len; ++i)
		fprintf(fp, "%02X", msg->raw[i]);
	fprintf(fp, "\n");

	fprintf(fp, "hash:%06X:%06X:%06X\n",
	       msg->cksum.crc,
	       msg->cksum.AP,
	       msg->cksum.syn);

	if (msg->BDS)
		fprintf(fp, "BDS:%02X\n", msg->BDS);

	if (!(msg->addr & 0xFF000000)) {
		fprintf(fp, "addr:%06X", msg->addr);
		fprintf(fp, ":%s", a ? a->nation->iso3 : icao_addr_to_iso3(msg->addr));

		if (a && a->name[0] != '\0') {
			fprintf(fp, ":%s", a->name);
		}
		fprintf(fp, "\n");
	}

	if (a) {
		if (a->locations.last && msg->time - a->locations.last->time < location_ttl) {
			char s_lat[40];
			char s_lon[40];
			fill_angle_str(s_lat, 40, a->locations.last->lat);
			fill_angle_str(s_lon, 40, a->locations.last->lon);
			fprintf(fp, "Location:%s%c %s%c\n",
			       s_lat, a->locations.last->lat < 0 ? 'S' : 'N',
			       s_lon, a->locations.last->lon < 0 ? 'W' : 'E');
		}
		if (a->altitudes.last && msg->time - a->altitudes.last->time < altitude_ttl) {
			double alt = a->altitudes.last->alt;
			fprintf(fp, "Altitude:%'.0f ft (%'.0f m)\n",
			        alt, FT2METRES(alt));
		}
		if (a->velocities.last && msg->time - a->velocities.last->time < velocity_ttl) {
			char angle[40];
			double s, t;
			s = a->velocities.last->speed;
			t = a->velocities.last->track;

			fill_angle_str(angle, 40, t);
			fprintf(fp, "Velocity:%'.1f kt (%'.0f km/h):%s (%s)\n",
			       s, KT2KMPH(s),
			       angle, get_comp_point(t));
		}
		if (a->squawks.last && msg->time - a->squawks.last->time < squawk_ttl) {
			pr_ID(fp, a->squawks.last->ID);
		}
	}
	

#define PRINTDF(X, NN) case X: pr_DF ## NN (fp, msg->msg, v); break; 

	switch (msg->DF) {
	PRINTDF( 0, 00)
	PRINTDF( 4, 04)
	PRINTDF( 5, 05)
	PRINTDF(11, 11)
	PRINTDF(16, 16)
	PRINTDF(17, 17)
	PRINTDF(18, 18)
	PRINTDF(19, 19)
	PRINTDF(20, 20)
	PRINTDF(21, 21)
	PRINTDF(24, 24)
	}
	fprintf(fp, "---\n");
	fflush(fp);
}

void
destroy_msg(struct ms_msg_t *msg) {
	if (msg->raw)
		free(msg->raw);

	if (msg->aircraft) {
/*
		if (msg->aircraft->last_msg == msg)
			msg->aircraft->last_msg = NULL;
*/
		if (msg->aircraft->messages == msg)
			msg->aircraft->messages = NULL;
	}

	if (!msg->msg) {
		free(msg);
		return;
	}

#define FREEAUX(NN) case NN: { 			\
	struct ms_DF ## NN ## _t *p = msg->msg;	\
	if (p->aux) free(p->aux); break; }

	switch (msg->DF) {
	FREEAUX(16)
	FREEAUX(17)
	FREEAUX(18)
	FREEAUX(19)
	FREEAUX(20)
	FREEAUX(21)
	FREEAUX(24)
	}

	free(msg->msg);
	free(msg);
}
