#ifndef LAB2_COMMON_H
#define LAB2_COMMON_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

enum reply { READY = 'R', ACCEPTED = 'A', INVALID = 'I', ZERO = 'Z', FAILED = 'F' };
enum { EXIT_ZERO_DIVISION = 2 };

extern volatile sig_atomic_t interrupted;
int setup_signals(void);
int close_fd(int *fd);
int write_all(int fd, const void *data, size_t size);
int receive_reply(int fd, unsigned char *reply);
int read_line(int fd, int watch_fd, char **line, size_t *size);
int reap_child(pid_t pid);

#endif
