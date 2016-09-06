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

#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "histogram.h"
#include "aircraft.h"
#include "message.h"
#include "dump.h"

static void
pr_histogram(const struct ms_histogram_t *h) {
	char timestr[20];
	FILE *fp = h->fp;

	strftime(timestr, 20, "%Y-%m-%d %H:%M", localtime(&h->time));
#define PRDF(N) do { fprintf(fp, "%lu\t", h->n_dfs[N]); } while(0)

	fprintf(fp, "%s\t%lu\t", timestr, h->n_msgs);
	PRDF(0); PRDF(4); PRDF(5);
	PRDF(11);PRDF(16);
	PRDF(17);PRDF(18);PRDF(19);
	PRDF(20);PRDF(21);PRDF(24);

	fprintf(fp, "%lu\t%lu\n", h->n_acs, h->n_nats);
	fflush(fp);
}

int
plot_histogram(const struct ms_histogram_t *h) {
	const char *filename = h->filename;
	int pfd[2] = { -1, -1 };
	pid_t pid;

	if (pipe(pfd) == -1) {
		perror("pipe");
		return -1;
	}

	pid = fork();

	if (pid == -1) {
		perror("fork");
		return -1;
	}
	if (pid == 0) {
		close(pfd[1]);
		if (dup2(pfd[0], STDIN_FILENO) < 0) {
			perror("dup2");
			exit(1);
		}
		close(pfd[0]);
		if (execlp("gnuplot", "gnuplot", "-p", (char*)NULL) < 0) {
			perror("execlp gnuplot -p");
			exit(1);
		}
		exit(0);
	} else {
		FILE *fp;
		int child_ret;
		
		close(pfd[0]);

		if (!(fp = fdopen(pfd[1], "w"))) {
			perror("fdopen");
			return -1;
		}
		fputs("set xdata time\n", fp);
		fputs("set timefmt \"%Y-%m-%d %H:%M\"\n", fp);
		fputs("set format x \"%d/%m\\n%H:%M\"\n", fp);
		fprintf(fp, "plot"
		"'%s' using 1:3  title \"msgs\"  with linespoints pointtype 6,"
#ifdef PLOT_ALL_DFS
		"'%s' using 1:4  title \"DF=0\"  with linespoints,"
		"'%s' using 1:5  title \"DF=4\"  with linespoints,"
		"'%s' using 1:6  title \"DF=5\"  with linespoints,"
		"'%s' using 1:7  title \"DF=11\" with linespoints,"
		"'%s' using 1:8  title \"DF=16\" with linespoints,"
		"'%s' using 1:9  title \"DF=17\" with linespoints,"
		"'%s' using 1:10 title \"DF=18\" with linespoints,"
		"'%s' using 1:11 title \"DF=19\" with linespoints,"
		"'%s' using 1:12 title \"DF=20\" with linespoints,"
		"'%s' using 1:13 title \"DF=21\" with linespoints,"
		"'%s' using 1:14 title \"DF=24\" with linespoints,"
#endif
		"'%s' using 1:($15*100) title \"100 * aircrafts\" with linespoints,"
		"'%s' using 1:($16*100) title \"100 * nations\" with linespoints\n",
#ifdef PLOT_ALL_DFS
		filename, filename, filename, filename,
		filename, filename, filename,
		filename, filename, filename,
#endif
		filename, filename, filename);
		fclose(fp);
		waitpid(pid, &child_ret, 0);
		return child_ret;
	}
	

	return 0;
}

struct ms_histogram_t *
mk_histogram(size_t incr, const char *filename) {
	struct ms_histogram_t *h;
	h = calloc(1, sizeof(struct ms_histogram_t));
	h->incr = incr;
	h->filename = strdup(filename);
	h->fp = open_file(h->filename);
	if (!h->fp) {
		free(h->filename);
		h->filename = NULL;
		return NULL;
	}
	printf("Dumping histogram to: %s\n", h->filename);
	return h;
}

void
destroy_histogram(struct ms_histogram_t *h) {
	if (h->fp)
		fclose(h->fp);
	if (h->nats) {
		free(h->nats);
		h->nats = NULL;
	}
	if (h->acs) {
		free(h->acs);
		h->acs = NULL;
	}
	if (h->filename) {
		free(h->filename);
		h->filename = NULL;
	}
	free(h);
}

void
update_histogram(struct ms_histogram_t *h, const struct ms_msg_t *msgs) {
	const struct ms_msg_t *msg;

	/* TODO: count number of unique aircrafts and nations */

	if (!msgs)
		return;
	else if (!h->time)
		h->time = msgs->time;

	for (msg = msgs; msg; msg = msg->next) {
		if (msg->time < h->time + h->incr) {
			bool new_nat = true;
			bool new_ac = true;
			size_t i;


			for (i = 0; i < h->n_nats; ++i) {
				if (h->nats[i] == msg->aircraft->nation) {
					new_nat = false;
					break;
				}
			}
			if (new_nat) {
				if (h->n_nats == h->nats_size) {
					h->nats_size += 100;
					h->nats = realloc(h->nats, h->nats_size * sizeof(struct ms_nation_t*));
				}
				h->nats[h->n_nats++] = msg->aircraft->nation;
			}


			for (i = 0; i < h->n_acs; ++i) {
				if (h->acs[i] == msg->aircraft) {
					new_ac = false;
					break;
				}
			}
			if (new_ac) {
				if (h->n_acs == h->acs_size) {
					h->acs_size += 100;
					h->acs = realloc(h->acs, h->acs_size * sizeof(struct ms_aircraft_t*));
				}
				h->acs[h->n_acs++] = msg->aircraft;
			}


			++h->n_msgs;
			++h->n_dfs[msg->DF];

		} else {

			pr_histogram(h);

			h->time += h->incr;
			h->n_msgs = 1;

			memset(h->n_dfs, 0, sizeof(h->n_dfs));
			
			h->n_dfs[msg->DF] = 1;


			h->acs_size  = h->n_acs  ? h->n_acs  : 100;
			h->nats_size = h->n_nats ? h->n_nats : 100;

			if (h->acs) {
				free(h->acs);
			}
			if (h->nats) {
				free(h->nats);
			}
			h->nats = malloc(h->nats_size * sizeof(struct ms_nation_t*));
			h->acs  = malloc(h->acs_size  * sizeof(struct ms_aircraft_t*));
			
			h->n_nats = 0;
			h->n_acs = 0;
		}

	}
}
