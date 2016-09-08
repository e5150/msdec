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

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <poll.h>

#include <arg.h>

#include "histogram.h"
#include "aircraft.h"
#include "message.h"
#include "util.h"
#include "stats.h"
#include "parse.h"
#include "dump.h"

#define CONF_MSDEC
#include "config.h"

static struct {
	bool print_stats;
	bool print_msgs;
	bool follow;
	bool msg_on_cmdline;
	bool dump_stats;
	bool dump_flightlogs;
	bool dump_messages;
	bool dump_histogram;
	bool plot_histogram;
	int histogram_incr;
	int print_mode;
	const char *aircraft_dir;
	const char *hist_filename;
	const char *stats_filename;
} options;

static int
init_inotify(const char *filename, int *ifd, int *iwfd) {
	if ((*ifd = inotify_init()) < 0) {
		fprintf(stderr, "%s: ERROR: inotify_init: %s\n",
			argv0, strerror(errno));
		return -1;
	}
	if ((*iwfd = inotify_add_watch(*ifd, filename, IN_MODIFY
	                                             | IN_DELETE_SELF
	                                             | IN_MOVE_SELF
	                                             | IN_UNMOUNT)) < 0) {
		fprintf(stderr, "%s: ERROR: inotify_add_watch %s: %s\n",
			argv0, filename, strerror(errno));
		return -1;
	}
	return 0;
}

static int
wait_for_inotify(int fd) {
	char ib[4 * sizeof(struct inotify_event)];
	struct inotify_event *ev;
	ssize_t len, i;

	len = read(fd, ib, sizeof(ib));

	if (len < 0) {
		if (errno == EINTR || errno == EAGAIN) {
			return 0;
		}
		return -2;
	}

	for (i = 0; i < len; i += sizeof(struct inotify_event) + ev->len) {
		ev = (struct inotify_event *)(ib + i);
		if (ev->mask & ~IN_MODIFY) {
			return -1;
		}
	}

	return 0;
}

static int
cat(const char *filename) {
	struct ms_aircraft_t *aircrafts = NULL;
	struct ms_histogram_t *histogram = NULL;
	struct ms_stats_t *stats = NULL;
	off_t offset = 0;
	int err = 0;
	int ifd = -1;
	int iwfd = -1;
	char *acdumpdir = NULL;

	if (options.follow && init_inotify(filename, &ifd, &iwfd)) {
		return -1;
	}

	if (options.dump_histogram) {
		histogram = mk_histogram(options.histogram_incr, options.hist_filename);
		if (!histogram) {
			options.dump_histogram = false;
			fprintf(stderr, "%s: WARNING: Won't dump histogram\n", argv0);
			err -= 1;
		}
	}
	if (options.dump_flightlogs || options.dump_messages) {
		acdumpdir = mk_aircraft_dump_dir(options.aircraft_dir);
		if (acdumpdir) {
			printf("Dumping aircrafts to: %s\n", acdumpdir);
		} else {
			options.dump_flightlogs = false;
			options.dump_messages = false;
			fprintf(stderr, "%s: WARNING: Won't dump aircrafts\n", argv0);
			err -= 1;
		}
	}

	if (options.print_stats || options.dump_stats) {
		stats = mk_stats();
	}


	do {
		int iret = 0;
		struct ms_msg_t *msgs, *msg;

		msgs = parse_file(filename, &offset, &aircrafts);

		if (!msgs && errno) {
			err -= 1;
			fprintf(stderr, "%s: Failed to parse file %s: %s\n",
				argv0, filename ? filename : "stdin", strerror(errno));
			break;
		}

		if (options.dump_histogram) {
			update_histogram(histogram, msgs);
		}
		
		if (stats) {
			update_stats(stats, msgs);
		}

		if (options.dump_messages) {
			err += dump_messages(msgs, acdumpdir);
		}

		while (msgs) {
			struct ms_msg_t *tmp = msgs->next;
			if (options.print_msgs) {
				pr_msg(stdout, msgs, options.print_mode);
			}
			destroy_msg(msgs);
			msgs = tmp;
		}

		if (options.follow) {
			iret = wait_for_inotify(ifd);
			if (iret < 0) {
				printf("Input file %s seems to have disappeared.\n", filename);
				close(iwfd);
				iwfd = -1;
			}
		}

	} while (iwfd >= 0);

	if (ifd >= 0) {
		close(ifd);
	}

	if (options.dump_flightlogs) {
		struct ms_aircraft_t *tmp;
		for (tmp = aircrafts; tmp; tmp = tmp->next)
			err += dump_flightlog(tmp, acdumpdir);
	}

	if (stats) {
		finalise_stats(stats, aircrafts);

		if (options.print_stats) {
			pr_stats(stats);
		}
		if (options.dump_stats) {
			err += dump_stats(options.stats_filename, stats);
		}

		destroy_stats(stats);

	}


	if (options.dump_histogram) {	
		if (options.plot_histogram) {
			err += plot_histogram(histogram);
		}
		destroy_histogram(histogram);
	}

	if (acdumpdir) {
		free(acdumpdir);
	}


	while (aircrafts) {
		struct ms_aircraft_t *tmp;
		
		tmp = aircrafts->next;
		destroy_aircraft(aircrafts);
		aircrafts = tmp;
	}

	return err ? -1 : 0;
}



static void
usage() {
	printf("usage: %s [options] <file>\n", argv0);
	printf("usage: %s -m <msg ...>\n", argv0);
	printf("options:\n"
	       " -f:\tFollow input file as it grows\n"
	       " -r:\tRaw output\n"
	       " -ns:\tNo statistics output on stdout\n"
	       " -nm:\tNo message output on stdout\n"
	       " -s:\tDump statistics\n"
	       " -S fn\tFilename for stats dump\n"
	       " -h i:\tDump histogram, i ∈ { m, q, h, 6, d, w }\n" 
	       " -H fn\tFilename for histogram dump\n"
	       " -al:\tDump aircraft logs\n"
	       " -am:\tDump aircraft messages\n"
	       " -A dn:\tDirectory for aircraft dumps\n"
	);

	exit(1);
}

int
main(int argc, char *argv[]) {
	int err = 0;
	char *s;

	setlocale(LC_NUMERIC, "");
	memset(&options, 0, sizeof(options));
	options.print_stats = true;
	options.print_msgs = true;
	options.hist_filename = default_histogram_filename;
	options.stats_filename = default_statistics_filename;
	options.aircraft_dir = default_aircraft_directory;

	ARGBEGIN {
	case 'n':
		s = EARGF(usage());
		if (!strcmp(s, "m"))
			options.print_msgs = false;
		else if (!strcmp(s, "s"))
			options.print_stats = false;
		else if (!strcmp(s, "ms") || !strcmp(s, "sm")) {
			options.print_msgs = false;
			options.print_stats = false;
		}
		else
			usage();
		break;

	case 'r':
		options.print_mode = 1;
		break;

	case 'f':
		options.follow = true;
		break;

	case 'm':
		options.msg_on_cmdline = true;
		break;

	case 'A':
		options.aircraft_dir = EARGF(usage());
		break;
	case 'a':
		s = EARGF(usage());
		if (!strcmp(s, "l"))
			options.dump_flightlogs = true;
		else if (!strcmp(s, "m"))
			options.dump_messages = true;
		else if (!strcmp(s, "ml") || !strcmp(s, "lm")) {
			options.dump_flightlogs = true;
			options.dump_messages = true;
		}
		else
			usage();
		break;

	case 'S':
		options.stats_filename = EARGF(usage());
		break;
	case 's':
		options.dump_stats = true;
		break;

	case 'H':
		options.hist_filename = EARGF(usage());
		break;
	case 'p':
		options.plot_histogram = true;
		/* FALLTHROUGH */
	case 'h':
		options.dump_histogram = true;
		s = EARGF(usage());
		switch (s[0]) {
		case 'm':
			options.histogram_incr = 60;
			break;
		case 'q':
			options.histogram_incr = 60 * 15;
			break;
		case 'h':
			options.histogram_incr = 60 * 60;
			break;
		case '6':
			options.histogram_incr = 60 * 60 * 6;
			break;
		case 'd':
			options.histogram_incr = 60 * 60 * 24;
			break;
		case 'w':
			options.histogram_incr = 60 * 60 * 24 * 7;
			break;
		default:
			usage();
		}
		break;

	default:
		usage();
	} ARGEND;

	if (options.msg_on_cmdline) {
		int i;
		for (i = 0; i < argc; ++i) {
			struct ms_msg_t *msg;
			msg = line_to_msg(argv[i]);
			if (msg) {
				pr_msg(stdout, msg, options.print_mode);
				destroy_msg(msg);
			} else {
				err -= 1;
			}
		}
		return err ? 1 : 0;
	}

	if (argc > 1)
		usage();

	err += cat(argc ? argv[0] : NULL);

	return err ? 1 : 0;
}
