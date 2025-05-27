#include "inc/common.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void timer_handler(int signum) {
    (void) signum;
    fprintf(stderr, "time's up!\n");
    kill(getpid(), SIGTERM);
}

int set_timeout(int delay) {
    int res = sigaction(SIGALRM, &((struct sigaction) {.sa_handler = timer_handler}), NULL);
    if (res < 0) {
        fprintf(stderr, "set_timeout: sigaction() failed\n");
        return -1;
    }
    res = alarm(delay);
    if (res < 0) {
        fprintf(stderr, "set_timeout: alarm() failed\n");
        return -1;
    }
    return 0;
}
