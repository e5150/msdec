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

#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "mac.h"
#include "aircraft.h"
#include "stats.h"
#include "compass.h"
#include "message.h"

extern const char *argv0;

FILE *
open_file(char *filename) {
	FILE *fp;

	if (strstr(filename, "XXXXXX")) {
		int fd;

		if ((fd = mkstemp(filename)) < 0) {
			fprintf(stderr, "%s: ERROR: mkstemp %s: %s\n",
			        argv0, filename, strerror(errno));
			return NULL;
		}
		if (!(fp = fdopen(fd, "w"))) {
			fprintf(stderr, "%s: ERROR:fdopen %s: %s\n",
			        argv0, filename, strerror(errno));
			perror("fdopen");
			return NULL;
		}
	} else {
		if (!(fp = fopen(filename, "w"))) {
			fprintf(stderr, "%s: ERROR: fopen %s: %s\n",
			        argv0, filename, strerror(errno));
			return NULL;
		}
	}
	return fp;
}

int
dump_flightlog(const struct ms_aircraft_t *a, const char *dir) {
	const struct ms_ac_altitude_t *alt = a->altitudes.head;
	const struct ms_ac_location_t *loc = a->locations.head;
	const struct ms_ac_velocity_t *vel = a->velocities.head;
	const struct ms_ac_squawk_t   *sqw = a->squawks.head;
	char filename[PATH_MAX];
	FILE *fp;

	if (a->altitudes.n + a->locations.n + a->velocities.n + a->squawks.n == 0)
		return 0;

	if (snprintf(filename, PATH_MAX - 1,
	             "%s/%3.3s:%06X.log",
	             dir, a->nation->iso3, a->addr) < 0)
	{
		perror("snprintf");
		return -1;
	}

	if (!(fp = fopen(filename, "w"))) {
		perror("fopen");
		return -1;
	}

	fputs("%Y-%m-%d %H:%M:%S\tID\tA (ft)\tv (kt)\th (°)\tlat\tlon\n", fp);

	while (alt || loc || vel || sqw) {
		time_t ts = 0;
		time_t at = alt ? alt->time : 0;
		time_t lt = loc ? loc->time : 0;
		time_t vt = vel ? vel->time : 0;
		time_t st = sqw ? sqw->time : 0;
		char timestr[20];

		if (alt)
			ts = alt->time;
		if (loc && (!ts || loc->time < ts))
			ts = loc->time;
		if (vel && (!ts || vel->time < ts))
			ts = vel->time;
		if (sqw && (!ts || sqw->time < ts))
			ts = sqw->time;

		if (!ts)
			break;

		strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", localtime(&ts));
		fprintf(fp, "%s\t", timestr);

		if (sqw && sqw->time == ts) {
			fprintf(fp, "%04o\t", sqw->ID);
			sqw = sqw->next;
		} else {
			fprintf(fp, "-\t");
		}

		if (alt && alt->time == ts) {
			fprintf(fp, "%.0f\t", alt->alt);
			while (alt && alt->time == ts)
				alt = alt->next;
		} else {
			fprintf(fp, "-\t");
		}

		if (vel && vel->time == ts) {
			fprintf(fp, "%.0f\t%.0f\t", vel->speed, vel->heading);
			while (vel && vel->time == ts)
				vel = vel->next;
		} else {
			fprintf(fp, "-\t-\t");
		}

		if (loc && loc->time == ts) {
			fprintf(fp, "%.4f\t%.4f\n", loc->lat, loc->lon);
			while (loc && loc->time == ts)
				loc = loc->next;
		} else {
			fprintf(fp, "-\t-\n");
		}

	}

	fclose(fp);

	return 0;
}

int
dump_messages(const struct ms_msg_t *msgs, const char *dir) {
	char filename[PATH_MAX];
	const struct ms_msg_t *msg;
	FILE *fp;


	for (msg = msgs; msg; msg = msg->next) {
		struct ms_aircraft_t *a;

		if (!(a = msg->aircraft)) {
			/* CANTHAPPEN? */
			continue;
		}
		if (snprintf(filename, PATH_MAX - 1,
			     "%s/%3.3s:%06X.msg",
			     dir, a->nation->iso3, a->addr) < 0)
		{
			perror("snprintf");
			return -1;
		}

		if (!(fp = fopen(filename, "a"))) {
			perror("fopen");
			return -1;
		}


		pr_msg(fp, msg, 0);

		fclose(fp);
	}

	return 0;
}

char *
mk_aircraft_dump_dir(const char *dir) {
	char *ret = strdup(dir);
	int err = 0;

	if (strstr(ret, "XXXXXX") && !mkdtemp(ret)) {
		err -= 1;
		fprintf(stderr, "%s: ERROR: mkdtemp %s: %s\n",
			argv0, dir, strerror(errno));
	} else {
		struct stat fs;

		if (lstat(ret, &fs) == -1) {
			err -= 1;
			fprintf(stderr, "%s: ERROR: stat %s: %s\n",
				argv0, ret, strerror(errno));
		}
		else if (!S_ISDIR(fs.st_mode)) {
			err -= 1;
			fprintf(stderr, "%s: ERROR: %s: Not a directory\n",
				argv0, ret);
		}
	}

	if (err) {
		free(ret);
		return NULL;
	}
	return ret;
}

int
dump_json(const char *filename, const struct ms_aircraft_t *aircrafts, int ttl) {
	return 0;
#if 0
	const struct ms_aircraft_t *tmp;
	bool first = true;
	FILE *fp;
	time_t now = time(NULL);

	if (!(fp = fopen(filename, "w"))) {
		perror("fopen json-dump-file");
		return -1;
	}

	fprintf(fp,
	        "{\n"
	        "\"now\":%ld,\n"
	        "\"aircraft\":[{\n", now);



	for (tmp = aircrafts; tmp; tmp = tmp->next) {
		if (tmp->last_msg->time < now - ttl * 100) /* FIXME */
			continue;

		if (!first)
			fprintf(fp, "},{\n");


		if (tmp->squawks.last) {
			fprintf(fp, "  \"squawk\":\"%04o\",\n", tmp->squawks.last->ID);
		}
		if (tmp->name[0]) {
			fprintf(fp, "  \"flight\":\"%s\",\n", tmp->name);
		}
		if (tmp->locations.last) {
			double lat = tmp->locations.last->loc.lat;
			double lon = tmp->locations.last->loc.lon;
			char s_lat[20];
			char s_lon[20];
			fill_angle_str(s_lat, 20, lat);
			fill_angle_str(s_lon, 20, lon);
			fprintf(fp, "  \"lat_str\":\"%s%c\",\n", s_lat, lat < 0 ? 'S' : 'N');
			fprintf(fp, "  \"lon_str\":\"%s%c\",\n", s_lon, lon < 0 ? 'W' : 'E');
			fprintf(fp, "  \"lat\":%f,\n", lat);
			fprintf(fp, "  \"lon\":%f,\n", lon);
			fprintf(fp, "  \"seen_pos\":%ld,\n", now - tmp->locations.last->time);
		}
		if (tmp->altitudes.last && tmp->altitudes.last->p->alt_ft > -20000) {
			fprintf(fp, "  \"altitude\":%d,\n", tmp->altitudes.last->p->alt_ft);
		}
		if (tmp->velocities.last) { 
			double t = tmp->velocities.last->p->angle;
			fprintf(fp, "  \"track\":%.0f,\n", t);
			fprintf(fp, "  \"track_str\":\"%s\",\n", get_comp_point(t));
			fprintf(fp, "  \"speed\":%.0f,\n", tmp->velocities.last->p->speed);
			fprintf(fp, "  \"vert_rate\":\"%.0f\",\n", tmp->velocities.last->p->vert);
		}


			fprintf(fp, "  \"cc\":\"%s\",\n", tmp->nation->iso3);
			fprintf(fp, "  \"seen\":%ld,\n", now - tmp->last_msg->time);
			fprintf(fp, "  \"messages\":%d,\n", tmp->n_messages);
			fprintf(fp, "  \"hex\":\"%s%06X\"\n",
				tmp->ICAO_addr ? "" : "~",
			        tmp->addr);

		first = false;
		
	}

	fprintf(fp, "}]}\n");
	fflush(fp);
	fclose(fp);

	return 0;
#endif
}

int
dump_stats(const char *filename, const struct ms_stats_t *s) {
	char *fn = strdup(filename);
	FILE *fp;
	size_t i;

	if (!(fp = open_file(fn))) {
		fprintf(stderr, "%s: WARNING: Won't dump statistics\n", argv0);
		free(fn);
		return -1;
	}

	printf("Dumping statistics to: %s\n", fn);

	fprintf(fp, "start:%s", ctime(&s->first));
	fprintf(fp, "end:%s", ctime(&s->last));
	fprintf(fp, "msgs:%lu\n", s->n_msgs);
	fprintf(fp, "aircrafts:%lu\n", s->n_acs);
	fprintf(fp, "nations:%lu\n", s->n_nats);

#define PRDF(N) do { fprintf(fp, "df%d:%lu\n", N, s->n_dfs[N]); } while(0)
	PRDF(0); PRDF(4); PRDF(5);
	PRDF(11);PRDF(16);PRDF(17);
	PRDF(18);PRDF(19);PRDF(20);
	PRDF(21);PRDF(24);
#undef PRDF

	for (i = 0; i < s->n_acs; ++i) {
		fprintf(fp, "ma=%lu:%06X:%lu\n", i + 1,
		       s->mbya[i].addr, s->mbya[i].n_msgs);
	}
	for (i = 0; i < s->n_nats; ++i) {
		fprintf(fp, "mn=%lu:%3.3s:%lu\n", i + 1,
		        s->mbyn[i].name, s->mbyn[i].n_msgs);
	}
	for (i = 0; i < s->n_nats; ++i) {
		fprintf(fp, "an=%lu:%3.3s:%lu\n", i + 1,
		        s->abyn[i].name, s->abyn[i].n_acs);
	}

	free(fn);
	fclose(fp);
	return 0;
}
