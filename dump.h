#ifndef _MS_DUMP_H
#define _MS_DUMP_H
struct ms_aircraft_t;
struct ms_stats_t;
struct ms_msg_t;
FILE *open_file(char *filename);
int dump_json(const char *filename, const struct ms_aircraft_t *aircrafts, int ttl);
int dump_stats(const char *filename, const struct ms_stats_t *);
char *mk_aircraft_dump_dir(const char *dir);
int dump_flightlog(const struct ms_aircraft_t *a, const char *dir);
int dump_messages(const struct ms_msg_t *msg, const char *dir);
#endif
