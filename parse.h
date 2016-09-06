#ifndef _MS_PARSE_H
#define _MS_PARSE_H

#include "aircraft.h"
#include "message.h"

struct ms_msg_t *line_to_msg(const char *orig_line);
struct ms_msg_t *parse_file(const char *filename, off_t *offset, struct ms_aircraft_t **);
#endif
