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
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "aircraft.h"
#include "message.h"
#include "cpr.h"
#include "mac.h"

struct ms_aircraft_t *
mk_aircraft(uint32_t addr) {
	struct ms_aircraft_t *ret;

	ret = calloc(1, sizeof(struct ms_aircraft_t));

	ret->addr = addr;
	ret->nation = icao_addr_to_nation(addr);

	ret->ICAO_addr = true; /* TODO */

	ret->ext = NULL;
	return ret;
}

struct ms_aircraft_t *
find_aircraft(uint32_t addr, struct ms_aircraft_t *head) {
	struct ms_aircraft_t *tmp;

	for (tmp = head; tmp; tmp = tmp->next) {
		if (tmp->addr == addr) {
			return tmp;
		}
	}
	return NULL;
}

/*
 * TODO: keep track of different types
 * of position messages, by surface and Nb.
 */
static void
update_position(struct ms_aircraft_t *a, struct ms_CPR_t *CPR, time_t ts) {
	struct ms_ac_location_t *loc = calloc(1, sizeof(struct ms_ac_location_t));
	bool valid = false;
	
	loc->time = ts;
	loc->next = NULL;

	if (CPR->F) {
		a->last_CPRs.odd_time = ts;
		a->last_CPRs.odd = CPR;
	} else {
		a->last_CPRs.even_time = ts;
		a->last_CPRs.even = CPR;
	}

	if (a->last_CPRs.odd
	 && a->last_CPRs.even
	 && labs(a->last_CPRs.odd_time - a->last_CPRs.even_time) <= 10) {
		if (decode_cpr_global(a->last_CPRs.odd, a->last_CPRs.even, &loc->lat, &loc->lon, CPR->F) == 0)
		{
			valid = true;
		}
	}
/*
	if (!valid) {
		if (decode_cpr_local(CPR,
		                     default_lat,
		                     default_lon,
		                     &loc->loc, CPR->F) == 0)
		{
			valid = true;
		}
	}
*/
	if (valid) {
		if (a->locations.last) {
			a->locations.last->next = loc;
		} else {
			a->locations.head = loc;
		}
		a->locations.last = loc;
		a->locations.n++;
		
	} else {
		free(loc);
	}

}

static void
update_velocity(struct ms_aircraft_t *a, const struct ms_velocity_t *v, time_t ts) {
	struct ms_ac_velocity_t *vel = calloc(1, sizeof(struct ms_ac_velocity_t));

	vel->speed = v->speed;
	vel->heading = v->heading;
	vel->vrate = v->vert;
	vel->next = NULL;
	vel->time = ts;
	if (a->velocities.last)
		a->velocities.last->next = vel;
	else
		a->velocities.head = vel;
	a->velocities.last = vel;
	a->velocities.n++;
}

static void
update_altitude(struct ms_aircraft_t *a, const struct ms_AC_t *ac, time_t ts) {
	struct ms_ac_altitude_t *alt;
	
	if (ac->alt_ft < -50000) /* invalid or reserved */
		return;

	alt = calloc(1, sizeof(struct ms_ac_altitude_t));

	alt->alt = ac->alt_ft;
	alt->next = NULL;
	alt->time = ts;
	if (a->altitudes.last)
		a->altitudes.last->next = alt;
	else
		a->altitudes.head = alt;
	a->altitudes.last = alt;
	a->altitudes.n++;
}

static void
update_squawk(struct ms_aircraft_t *a, uint16_t squawk, time_t ts) {
	struct ms_ac_squawk_t *s = calloc(1, sizeof(struct ms_ac_squawk_t));

	s->ID = squawk;
	s->next = NULL;
	s->time = ts;
	if (a->squawks.last)
		a->squawks.last->next = s;
	else
		a->squawks.head = s;
	a->squawks.last = s;
	a->squawks.n++;
}

static void
update_name(struct ms_aircraft_t *a, const struct ms_BDS_08_t *bds) {
	a->type.TYPE = bds->FTC;
	a->type.category = bds->category;
	strncpy(a->name, bds->name, 9);
	a->name[8] = '\0';
}

void
update_aircraft(struct ms_aircraft_t *a, struct ms_msg_t *msg) {
	enum ms_extended_squitter_t es_t = ES_RESERVED;
	const void *es_m = NULL;

	struct ms_CPR_t *CPR = NULL;
	const struct ms_velocity_t *vel = NULL;
	const struct ms_AC_t *AC = NULL;
	const uint16_t *ID = NULL;

	if (msg->time > a->last_seen)
		a->last_seen = msg->time;

	switch (msg->DF) {
	case 17:{
		struct ms_DF17_t *m = msg->msg;
		es_m = m->aux;
		es_t = m->ES_TYPE.et;
		break;}
	case 18:{
		struct ms_DF18_t *m = msg->msg;

		switch (m->CF) {
		case 0: /* ADS-B */
		case 1: /* ADS-B non-icao */
		case 6: /* ADS-R */
			es_m = m->aux;
			es_t = m->ES_TYPE.et;
			break;
		case 2: /* TIS-B ‐ fine */
			CPR = &((struct ms_TISB_fine_t *)m->aux)->CPR;
			AC  = &((struct ms_TISB_fine_t *)m->aux)->AC;
			break;
		case 3: /* TIS-B - coarse */
			CPR = &((struct ms_TISB_coarse_t *)m->aux)->CPR;
			AC  = &((struct ms_TISB_coarse_t *)m->aux)->AC;
			vel = &((struct ms_TISB_coarse_t *)m->aux)->velocity;
			break;
		}
		break;}
	case 4:
		AC = &((struct ms_DF04_t *)msg->msg)->AC;
		break;
	case 5:
		ID = &((struct ms_DF05_t *)msg->msg)->ID;
		break;
	case 16:
		AC = &((struct ms_DF16_t *)msg->msg)->AC;
		break;
	case 20:
		AC = &((struct ms_DF20_t *)msg->msg)->AC;
		break;
	case 21:
		ID = &((struct ms_DF21_t *)msg->msg)->ID;
		break;
		
	}

	if (es_m) {
		switch (es_t) {
		case ES_IDENTIFICATION:
			update_name(a, es_m);
			break;
		case ES_AIRBORNE_POSITION:
			CPR = &((struct ms_BDS_05_t *)es_m)->CPR;
			AC  = &((struct ms_BDS_05_t *)es_m)->AC;
			break;
		case ES_SURFACE_POSITION:
			CPR = &((struct ms_BDS_06_t *)es_m)->CPR;
			vel = &((struct ms_BDS_06_t *)es_m)->velocity;
			break;
		case ES_OPERATIONAL_STATUS:
			a->NIC_s = ((struct ms_BDS_65_t *)es_m)->NIC_s;
			break;
		case ES_AIRBORNE_VELOCITY:
			vel = &((struct ms_BDS_09_t *)es_m)->velocity;
			break;
		default:
			{}
		}
	}

	if (CPR) {
		update_position(a, CPR, msg->time);
	}
	if (vel) {
		update_velocity(a, vel, msg->time);
	}
	if (AC) {
		update_altitude(a, AC, msg->time);
	}
	if (ID) {
		update_squawk(a, *ID, msg->time);
	}
	

	msg->ac_next = NULL;

	if (a->last_msg) {
		a->last_msg->ac_next = msg;
	} else {
		a->messages = msg;
	}
	a->last_msg = msg;
	msg->aircraft = a;

	++a->n_msg_aux;
	++a->n_messages;

}

void
destroy_aircraft(struct ms_aircraft_t *a) {

	while (a->messages) {
		struct ms_msg_t *msg;

		msg = a->messages->ac_next;
		destroy_msg(a->messages);
		a->messages = msg;
	}

	while (a->locations.head) {
		struct ms_ac_location_t *loc;
		loc = a->locations.head->next;
		free(a->locations.head);
		a->locations.head = loc;
	}

	while (a->altitudes.head) {
		struct ms_ac_altitude_t *alt;
		alt = a->altitudes.head->next;
		free(a->altitudes.head);
		a->altitudes.head = alt;
	}

	while (a->squawks.head) {
		struct ms_ac_squawk_t *id;
		id = a->squawks.head->next;
		free(a->squawks.head);
		a->squawks.head = id;
	}

	while (a->velocities.head) {
		struct ms_ac_velocity_t *vel;
		vel = a->velocities.head->next;
		free(a->velocities.head);
		a->velocities.head = vel;
	}


	free(a);
}
