#ifndef _MS_STATS_H
#define _MS_STATS_H

struct ac_stats_t {
	uint32_t addr;
	const char *name;
	const char *iso3;
	size_t n_msgs;
};

struct nat_stats_t {
	const char *name;
	size_t n_msgs;
	size_t n_acs;
};

struct ms_stats_t {
	struct ac_stats_t *mbya;
	struct nat_stats_t *mbyn;
	struct nat_stats_t *abyn;
	time_t first;
	time_t last;
	size_t n_msgs;
	size_t n_acs;
	size_t n_nats;
	size_t n_dfs[32];
};
	

struct ms_aircraft_t;
void init_stats(struct ms_stats_t *, const struct ms_aircraft_t *);
void cleanup_stats(struct ms_stats_t *);
void pr_stats(const struct ms_stats_t *);

void finalise_stats(struct ms_stats_t *stats, const struct ms_aircraft_t *aircrafts);
void update_stats(struct ms_stats_t *stats, const struct ms_msg_t *msgs);
struct ms_stats_t *mk_stats();
void destroy_stats(struct ms_stats_t *s);

#endif
