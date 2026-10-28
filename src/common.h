#ifndef LAB2_COMMON_H
#define LAB2_COMMON_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

enum reply { READY = 'R', ACCEPTED = 'A', INVALID = 'I', ZERO = 'Z', FAILED = 'F' };
enum { EXIT_ZERO_DIVISION = 2 };

struct line_reader {
    int fd;
    unsigned char buffer[4096];
    size_t begin;
    size_t end;
};

extern volatile sig_atomic_t interrupted;
int setup_signals(void);
int log_message(const char *role, const char *format, ...);
int close_fd(int *fd);
int redirect_fd(int source, int target);
int write_all(int fd, const void *data, size_t size);
int receive_reply(int fd, unsigned char *reply);
int read_line(struct line_reader *reader, int watch_fd, char **line, size_t *size);
int reap_child(pid_t pid);

#endif
