#ifndef LAB2_COMMON_H
#define LAB2_COMMON_H

#include <signal.h>
#include <sys/types.h>

#define EXIT_ZERO_DIVISION 2

extern volatile sig_atomic_t interrupted;
int setup_signals(void);
int read_byte(int fd, char *byte);
int write_byte(int fd, char byte);
int receive_reply(int fd);
int close_fd(int *fd);
int reap_child(pid_t pid);

#endif
