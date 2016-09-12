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
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

int
init_inotify(const char *filename, int *ifd, int *iwfd) {
	if ((*ifd = inotify_init()) < 0) {
		return -1;
	}
	if ((*iwfd = inotify_add_watch(*ifd, filename, IN_MODIFY
	                                             | IN_DELETE_SELF
	                                             | IN_MOVE_SELF
	                                             | IN_UNMOUNT)) < 0) {
		return -1;
	}
	return 0;
}

int
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

