#ifndef _MS_HISTOGRAM_H
#define _MS_HISTOGRAM_H

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

struct ms_msg_t;
struct ms_nation_t;
struct ms_aircraft_t;

struct ms_histogram_t {
	FILE *fp;
	char *filename;
	ssize_t incr;
	time_t time;
	size_t n_msgs;
	size_t n_dfs[32];
	size_t n_nats;
	size_t nats_size;
	struct ms_nation_t const **nats;
	size_t n_acs;
	size_t acs_size;
	struct ms_aircraft_t const **acs;
};

void destroy_histogram(struct ms_histogram_t *);
void update_histogram(struct ms_histogram_t *, const struct ms_msg_t *);
int  plot_histogram(const struct ms_histogram_t *);

struct ms_histogram_t *mk_histogram(size_t incr, const char *filename);

#endif
