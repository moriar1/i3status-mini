#include "config.h"
#include "header.h"
#include <err.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>

static const int NUM_THREADS = 6;

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

    // Set signal handlers
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = fatalsig;
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigusr1;
    sigaction(SIGUSR1, &action, NULL);

    char cpu_buf[STR_LEN];
    char time_buf[STR_LEN];
    char volume_buf[STR_LEN];
    struct timespec sleep_time = {.tv_sec = 1, .tv_nsec = 0};
    tpool_t *tm = tpool_create(NUM_THREADS);

    while (running) {
        tpool_add_work(tm, get_volume, volume_buf);
        tpool_add_work(tm, get_cpu_usage, cpu_buf);
        tpool_add_work(tm, get_time, time_buf);
        tpool_wait(tm);
        printf("cpu: %s | vol: %s | %s\n", cpu_buf, volume_buf, time_buf);
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
    tpool_destroy(tm);
}
