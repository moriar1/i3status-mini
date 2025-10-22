#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/soundcard.h>
#include <sys/sysctl.h>
#include <time.h>

#define STR_LEN 15
static volatile sig_atomic_t running = 1;
static char time_str[STR_LEN];
static char volume_str[STR_LEN];
static char cpu_str[STR_LEN];

// For main while loop interruption
static void sigint_handler(int __attribute__((unused)) signum) {
    running = 0;
}

// SIGUSR1 will interrupt nano_sleep() and print output
static void sigusr1(int __attribute__((unused)) signum) {
}

static const char *get_time(void) {
    time_t t;
    struct tm tm_struct;

    // NOTE: there is no need to check those errors (anyway)
    if (((t = time(NULL)) == (time_t)-1)) {
        warn("Failed get time");
        return "No time";
    }
    if (localtime_r(&t, &tm_struct) == NULL) {
        warn("Failed convert time");
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
#ifdef __FreeBSD__
    const char mix_name[] = "/dev/mixer";
    int mixfd = open(mix_name, O_RDONLY);
    unsigned vol;

    if (ioctl(mixfd, SOUND_MIXER_READ_VOLUME, &vol) == -1) {
        warn("OSS: Cannot read mixer information");
        vol = 0U;
    }

    vol &= 0x7fU;
    snprintf(volume_str, STR_LEN, "%u%%", vol);

#elif defined(__linux__)  // TODO
    snprintf(volume_str, STR_LEN, "No volume (TODO)");
#else
    snprintf(volume_str, STR_LEN, "No volume");
#endif
    return volume_str;
}

static const char *get_cpu_usage(void) {
#ifdef __FreeBSD__
    size_t size;
    long cp_time[CPUSTATES];
    size = sizeof cp_time;

    if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0) {
        warn("Failed get kern.cp_time");
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

    struct timespec sleep_time = {.tv_sec = 5, .tv_nsec = 0};
    while (running) {
        printf("%s | %s\n", get_volume(), get_time());
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
}
