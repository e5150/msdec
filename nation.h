#ifndef _MS_NATION_H
#define _MS_NATION_H

struct ms_nation_t {
	uint32_t code;
	uint32_t mask;
	char iso3[4];
	char name[33];
	const void *flag;
};

const struct ms_nation_t *icao_addr_to_nation(uint32_t);
const char *icao_addr_to_state(uint32_t);
const char *icao_addr_to_iso3(uint32_t);
#endif
