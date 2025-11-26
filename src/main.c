#include "config.h"
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>

static volatile sig_atomic_t running = 1;

// For main while loop interruption
static void fatalsig(int __attribute__((unused)) signum) {
    running = 0;
}

// SIGUSR1 will interrupt nano_sleep() and print output
static void sigusr1(int __attribute__((unused)) signum) {
}

int main(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = fatalsig;
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigusr1;
    sigaction(SIGUSR1, &action, NULL);

    struct timespec sleep_time = {.tv_sec = 5, .tv_nsec = 0};
    while (running) {
        printf("cpu: %s | vol: %s | %s\n", get_cpu_usage(), get_volume(), get_time());
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
}
