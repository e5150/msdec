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

#ifndef _MS_AIRCRAFT_H
#define _MS_AIRCRAFT_H

#ifdef GTK
#include "osm-gps-map-track.h"
#endif
#include "message.h"
#include "nation.h"

struct ms_ac_velocity_t {
	time_t time;
	double speed;
	double track;
	double vrate;
	struct ms_ac_velocity_t *next;
};

struct ms_ac_altitude_t {
	time_t time;
	double alt;
	struct ms_ac_altitude_t *next;
};

struct ms_ac_squawk_t {
	time_t time;
	uint16_t ID;
	struct ms_ac_squawk_t *next;
};

struct ms_ac_location_t {
	time_t time;
	double lat;
	double lon;
	struct ms_ac_location_t *next;
};

struct ms_aircraft_t {
	uint32_t addr;
	bool ICAO_addr;
	struct {
		uint8_t TYPE;
		uint8_t category;
	} type;
	char name[9];

	bool NIC_s;

	const struct ms_nation_t *nation;
	time_t last_seen;

	struct ms_msg_t *messages;
	struct ms_msg_t *last_msg;
	uint32_t n_messages;
	uint32_t n_msg_aux;

	struct {
		time_t odd_time;
		time_t even_time;
		const struct ms_CPR_t *odd;
		const struct ms_CPR_t *even;
	} last_CPRs;

	struct {
		struct ms_ac_altitude_t *head;
		struct ms_ac_altitude_t *last;
		size_t n;
	} altitudes;

	struct {
		struct ms_ac_squawk_t *head;
		struct ms_ac_squawk_t *last;
		size_t n;
	} squawks;

	struct {
		struct ms_ac_velocity_t *head;
		struct ms_ac_velocity_t *last;
		size_t n;
	} velocities;

	struct {
		struct ms_ac_location_t *head;
		struct ms_ac_location_t *last;
		size_t n;
	} locations;

	struct ms_aircraft_t *next;
	void *ext;
};

struct ms_aircraft_t *mk_aircraft(uint32_t addr);
struct ms_aircraft_t *find_aircraft(uint32_t addr, struct ms_aircraft_t *head);
void update_aircraft(struct ms_aircraft_t *a, struct ms_msg_t *msg);
void destroy_aircraft(struct ms_aircraft_t *a);
#endif
