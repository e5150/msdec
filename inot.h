#ifndef _MS_INOTIFY_H
#define _MS_INOTIFY_H
int init_inotify(const char *filename, int *ifd, int *iwfd);
int wait_for_inotify(int fd);
#endif
