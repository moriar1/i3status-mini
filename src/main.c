#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mixer.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define STR_LEN 20
static volatile sig_atomic_t running = 1;
static void sigint_handler(int __attribute__((unused)) signum) {
    running = 0;
}

// SIGUSR1 will interrupt nano_sleep() and print output
static void sigusr1(int __attribute__((unused)) signum) {
}

static const char *get_time(void) {
    time_t t;
    struct tm tm_struct;
    char *const time_str = malloc(STR_LEN * sizeof(char));

    // TODO: no need to check errors
    if (((t = time(NULL)) == (time_t)-1)) {
        perror("Failed get time");
        return "No time";
    }
    if (localtime_r(&t, &tm_struct) == NULL) {
        perror("Failed convert time");
        return "No time";
    }
    if (strftime(time_str, STR_LEN * sizeof(char), "%m-%d %H:%M", &tm_struct) ==
        0) {
        fprintf(stderr, "Failed to format time");
        return "No time";
    }
    return time_str;
}

static const char *get_volume(void) {
    char *const volume_str = malloc(STR_LEN * sizeof(char));

#ifdef __FreeBSD__
    struct mixer *m;

    // Use default mixer (if exists)
    const char *const mix_name = "/dev/mixer";
    if ((m = mixer_open(mix_name)) == NULL) {
        err(1, "mixer_open: %s", mix_name);
    }

    snprintf(volume_str, STR_LEN, "%s %.2f:%.2f", m->dev->name, (double)m->dev->vol.left,
             (double)m->dev->vol.right);

    if (mixer_close(m) == -1) {
        err(1, "mixer_close: %s", mix_name);
    }

#elif defined(__linux__)  // TODO
    snprintf(volume_str, STR_LEN, "No volume (TODO)");
#else
    snprintf(volume_str, STR_LEN, "No volume");
#endif
    return volume_str;
}

static const char *get_cpu_usage(void) {
    char *const cpu_str = malloc(STR_LEN * sizeof(char));
#ifdef __FreeBSD__
    size_t size;
    long cp_time[CPUSTATES];
    size = sizeof cp_time;

    if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0) {
        perror("Failed get kern.cp_time");
    }
    // TODO: get usage
    snprintf(cpu_str, STR_LEN, "%ld ", cp_time[2]);

#elif defined(__linux__)
    snprintf(cpu_str, STR_LEN, "No volume");
#else
    snprintf(cpu_str, STR_LEN, "No volume");
#endif
    return cpu_str;
}

int main(void) {
    if (signal(SIGUSR1, sigusr1) == SIG_ERR) {
        warn("Failed to set SIGUSR1 signal handler");
    }
    if (signal(SIGTERM, sigint_handler) == SIG_ERR) {
        warn("Failed to set SIGTERM signal handler");
    }

    struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 100};
    while (running) {
        printf("Time\t%s\nSound\t%s\nCPU\t%s\n\n", get_time(), get_volume(),
               get_cpu_usage());
        nanosleep(&sleep_time, NULL);
        // TODO: add free() or use static buffers or buffers from main
    }
}
