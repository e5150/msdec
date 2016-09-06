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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <assert.h>

#include "aircraft.h"
#include "stats.h"

#define PRFILLER  do { printf("               │");} while(0)

static size_t
update_nat(struct nat_stats_t *l, const char *iso3, size_t n_msgs, size_t len) {
	size_t i;

	for (i = 0; i < len; ++i) {
		if (!l[i].n_msgs) {
			break;
		}
		if (!strcmp(iso3, l[i].name)) {
			l[i].n_msgs += n_msgs;
			l[i].n_acs++;
			return 0;
		}
	}
	l[i].name = iso3;
	l[i].n_msgs = n_msgs;
	l[i].n_acs = 1;
	return 1;
}

static int
ac_cmp(const void *a, const void *b) {
	return ((const struct ac_stats_t *)b)->n_msgs - ((const struct ac_stats_t *)a)->n_msgs;
}

static int
nat_cmp(const void *a, const void *b) {
	return ((const struct nat_stats_t *)b)->n_msgs - ((const struct nat_stats_t *)a)->n_msgs;
}

static int
nat_cmp2(const void *a, const void *b) {
	return ((const struct nat_stats_t *)b)->n_acs - ((const struct nat_stats_t *)a)->n_acs;
}


static void
pr_mbya(size_t n_aircrafts, const struct ac_stats_t *acs) {
	size_t maxcols;
	size_t rows = n_aircrafts / 5;
	size_t row;
	size_t i;

	if (!n_aircrafts)
		return;

	puts("  ╔══════════════════════╗");
	puts("  ║ Messages by aircraft ║");
	puts("══╩════════════╤═════════╩═════╤═══════════════╤═══════════════╤═══════════════╕");
	if (n_aircrafts % 5)
		rows++;
	maxcols = (n_aircrafts / rows) + ((n_aircrafts % rows) ? 1 : 0);

	for (row = 0; row < rows; ++row) {
		size_t cols;
		size_t col;

		if (n_aircrafts > (maxcols - 1) * rows + row)
			cols = maxcols;
		else
			cols = maxcols - 1;

		for (col = 0; col < cols; ++col) {
			i = rows * col + row;

			if (acs[i].name[0])
				printf(" %3.3s:%-8.8s  │", acs[i].iso3, acs[i].name);
			else
				printf(" %3.3s:%06X    │", acs[i].iso3, acs[i].addr);

		}
		while (col++ < 5)
			PRFILLER;
		puts("");
		for (col = 0; col < cols; ++col) {
			printf(" %'13lu │", acs[col * rows + row].n_msgs);

		}
		while (col++ < 5)
			PRFILLER;

		puts("");
	}
	puts("───────────────┴───────────────┴───────────────┴───────────────┴───────────────┘");
	puts("");

}

static void
pr_nat(size_t n_nats, const struct nat_stats_t *nat, bool msgs) {
	size_t maxcols;
	size_t rows = n_nats / 5;
	size_t row;
	size_t i;

	if (!n_nats)
		return;

	if (n_nats % 5)
		++rows;
	maxcols = (n_nats / rows) + ((n_nats % rows) ? 1 : 0);

	if (msgs) {
		puts("  ╔═════════════════════════╗");
		puts("  ║ Messages by nationality ║");
		puts("══╩════════════╤════════════╩══╤═══════════════╤═══════════════╤═══════════════╕");
	} else {
		puts("  ╔══════════════════════════╗");
		puts("  ║ Aircrafts by nationality ║");
		puts("══╩════════════╤═════════════╩═╤═══════════════╤═══════════════╤═══════════════╕");
	}
		
	for (row = 0; row < rows; ++row) {
		size_t cols;
		size_t col;

		if (n_nats > (maxcols - 1) * rows + row)
			cols = maxcols;
		else
			cols = maxcols - 1;

		for (col = 0; col < cols; ++col) {
			i = rows * col + row;
			printf(" %3.3s: %'8lu │",
			       nat[i].name,
			       msgs ? nat[i].n_msgs : nat[i].n_acs);
		}
		while (col++ < 5)
			PRFILLER;
		puts("");
	}
	puts("───────────────┴───────────────┴───────────────┴───────────────┴───────────────┘");
	puts("");
}

static void
pr_mbyn(size_t n_nats, const struct nat_stats_t *nat) {
	pr_nat(n_nats, nat, true);
}

static void
pr_abyn(size_t n_nats, const struct nat_stats_t *nat) {
	pr_nat(n_nats, nat, false);
}

struct ms_stats_t *
mk_stats() {
	struct ms_stats_t *ret = calloc(1, sizeof(struct ms_stats_t));

	return ret;
}

void
destroy_stats(struct ms_stats_t *s) {
	if (s->mbya) {
		free(s->mbya);
		s->mbya = NULL;
	}
	if (s->mbyn) {
		free(s->mbyn);
		s->mbyn = NULL;
	}
	if (s->abyn) {
		free(s->abyn);
		s->abyn = NULL;
	}
	free(s);
}

void
update_stats(struct ms_stats_t *stats, const struct ms_msg_t *msgs) {
	const struct ms_msg_t *tmp;

	for (tmp = msgs; tmp; tmp = tmp->next) {
		if (!stats->first || tmp->time < stats->first)
			stats->first = tmp->time;
		if (!stats->last || tmp->time > stats->last)
			stats->last = tmp->time;

		if (tmp->DF > 31) {
			/* CANTHAPPEN */
			continue;
		}
		++stats->n_dfs[tmp->DF];
		++stats->n_msgs;
	}
}

void
finalise_stats(struct ms_stats_t *stats, const struct ms_aircraft_t *aircrafts) {
	const struct ms_aircraft_t *a;
	size_t i;

	for (a = aircrafts; a; a = a->next) {
		stats->n_acs++;
	}
	
	stats->mbya = malloc(stats->n_acs * sizeof(struct ac_stats_t));
	stats->mbyn = calloc(stats->n_acs,  sizeof(struct nat_stats_t));
	stats->abyn = calloc(stats->n_acs,  sizeof(struct nat_stats_t));

	for (i = 0, a = aircrafts; a && i < stats->n_acs; ++i, a = a->next) {
		stats->mbya[i].addr = a->addr;
		stats->mbya[i].name = a->name;
		stats->mbya[i].iso3 = a->nation->iso3;
		stats->mbya[i].n_msgs = a->n_messages;
		stats->n_nats += update_nat(stats->mbyn, a->nation->iso3, a->n_messages, stats->n_nats);
	}

	memcpy(stats->abyn, stats->mbyn, sizeof(struct nat_stats_t) * stats->n_acs);

	qsort(stats->mbya, stats->n_acs,  sizeof(struct ac_stats_t),  ac_cmp);
	qsort(stats->mbyn, stats->n_nats, sizeof(struct nat_stats_t), nat_cmp);
	qsort(stats->abyn, stats->n_nats, sizeof(struct nat_stats_t), nat_cmp2);
}

void
init_stats(struct ms_stats_t *stats, const struct ms_aircraft_t *aircrafts) {
	const struct ms_aircraft_t *a;
	size_t i;

	memset(stats, 0, sizeof(struct ms_stats_t));

	for (a = aircrafts; a; a = a->next) {
		const struct ms_msg_t *tmp;

		stats->n_msgs += a->n_messages;
		stats->n_acs++;

		if (a->messages) {
			time_t tf = a->messages->time;
			time_t tl = a->last_msg->time;

			if (!stats->first || tf < stats->first)
				stats->first = tf;
			if (!stats->last || tl > stats->last)
				stats->last = tl;
		}

		for (tmp = a->messages; tmp; tmp = tmp->ac_next) {
			if (tmp->DF > 31) {
				/* CANTHAPPEN… */
				continue;
			}
			++stats->n_dfs[tmp->DF];
		}
	}
	
	stats->mbya = malloc(stats->n_acs * sizeof(struct ac_stats_t));
	stats->mbyn = calloc(stats->n_acs,  sizeof(struct nat_stats_t));
	stats->abyn = calloc(stats->n_acs,  sizeof(struct nat_stats_t));

	for (i = 0, a = aircrafts; a && i < stats->n_acs; ++i, a = a->next) {
		stats->mbya[i].addr = a->addr;
		stats->mbya[i].name = a->name;
		stats->mbya[i].iso3 = a->nation->iso3;
		stats->mbya[i].n_msgs = a->n_messages;
		stats->n_nats += update_nat(stats->mbyn, a->nation->iso3, a->n_messages, stats->n_nats);
	}

	memcpy(stats->abyn, stats->mbyn, sizeof(struct nat_stats_t) * stats->n_acs);

	qsort(stats->mbya, stats->n_acs,  sizeof(struct ac_stats_t),  ac_cmp);
	qsort(stats->mbyn, stats->n_nats, sizeof(struct nat_stats_t), nat_cmp);
	qsort(stats->abyn, stats->n_nats, sizeof(struct nat_stats_t), nat_cmp2);
}

void
cleanup_stats(struct ms_stats_t *s) {
	if (s->mbya) {
		free(s->mbya);
		s->mbya = NULL;
	}
	if (s->mbyn) {
		free(s->mbyn);
		s->mbyn = NULL;
	}
	if (s->abyn) {
		free(s->abyn);
		s->abyn = NULL;
	}
		
}

void
pr_stats(const struct ms_stats_t *s) {
	char s_str[30], e_str[30];

	strftime(s_str, 30, "%Y-%m-%d %H:%M:%S UTC %z", localtime(&s->first));
	strftime(e_str, 30, "%Y-%m-%d %H:%M:%S UTC %z", localtime(&s->last));

	printf("  ╔═════════╗\n");
	printf("  ║ Summary ║\n");
	printf("══╩═════════╩══╤═══════════════════════════════╕\n");
	printf(" First message │ %-29s │\n", s_str);
	printf(" Last message  │ %-29s │\n", e_str);
	printf("───────────────┼───────────────┬───────────────┤\n");
	printf(" Messages      │ Aircrafts     │ Nations       │\n");
	printf(" %'13lu │ %'13lu │ %'13lu │\n", s->n_msgs, s->n_acs, s->n_nats);
	printf("───────────────┴───────────────┴───────────────┘\n");

	puts("");
	puts("  ╔═════════════════════════════╗");
	puts("  ║ Messages by downlink format ║");
	puts("══╩════════════╤═══════════════╤╩══════════════╤═══════════════╕");
#define PRDF(N) do { printf(" DF%02d: %'7lu │", N, s->n_dfs[N]); } while(0)
	PRDF(0);PRDF(11);PRDF(17);PRDF(20);puts("");
	PRDF(4);PRDF(16);PRDF(18);PRDF(21);puts("");
	PRDF(5);PRFILLER;PRDF(19);PRDF(24);puts("");
#undef PRDF
	puts("───────────────┴───────────────┴───────────────┴───────────────┘");
	puts("");

	pr_mbya(s->n_acs,  s->mbya);
	pr_mbyn(s->n_nats, s->mbyn);
	pr_abyn(s->n_nats, s->abyn);
}
