#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/soundcard.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

struct cpu_usage {
    long user;
    long nice;
    long system;
    long idle;
    long total;
};

enum { STR_LEN = 16 };
static struct cpu_usage prev_all = {0, 0, 0, 0, 0};
static volatile sig_atomic_t running = 1;
static char time_str[STR_LEN];
static char volume_str[STR_LEN];
static char cpu_str[STR_LEN];

// For main while loop interruption
static void fatalsig(int __attribute__((unused)) signum) {
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
    const char mixer_path[] = "/dev/mixer";
    int mixfd = open(mixer_path, O_RDONLY);

    if (mixfd == -1) {
        warn("OSS: Cannot open mixer");
        snprintf(volume_str, STR_LEN, "No volume");
        return volume_str;
    }

    unsigned vol;
    if (ioctl(mixfd, SOUND_MIXER_READ_VOLUME, &vol) == -1) {
        warn("OSS: Cannot read mixer information");
        vol = 0U;
    }
    close(mixfd);

    vol &= 0x7fU;
    snprintf(volume_str, STR_LEN, "%u%%", vol);

#elif defined(__linux__)  // TODO
    snprintf(volume_str, STR_LEN, "No volume");
#else
    snprintf(volume_str, STR_LEN, "No volume");
#endif
    return volume_str;
}

static const char *get_cpu_usage(void) {
#ifdef __FreeBSD__
    long cp_time[CPUSTATES];
    size_t size = sizeof cp_time;
    if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0) {
        warn("Failed get kern.cp_time");
        snprintf(cpu_str, STR_LEN, "No cpu");
        return cpu_str;
    }

    struct cpu_usage curr_all = {0, 0, 0, 0, 0};
    curr_all.user = cp_time[CP_USER];
    curr_all.nice = cp_time[CP_NICE];
    curr_all.system = cp_time[CP_SYS];
    curr_all.idle = cp_time[CP_IDLE];
    curr_all.total = curr_all.user + curr_all.nice + curr_all.system + curr_all.idle;
    long diff_idle = curr_all.idle - prev_all.idle;
    long diff_total = curr_all.total - prev_all.total;
    long diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
    snprintf(cpu_str, STR_LEN, "%ld%%", diff_usage);

#elif defined(__linux__)  // TODO
    snprintf(cpu_str, STR_LEN, "No cpu");
#else
    snprintf(cpu_str, STR_LEN, "No cpu");
#endif
    return cpu_str;
}

int main(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = fatalsig;
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigusr1;
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        warn("Failed to set SIGUSR1 signal handler");
    }

    struct timespec sleep_time = {.tv_sec = 5, .tv_nsec = 0};
    while (running) {
        printf("%s | %s | %s\n", get_cpu_usage(), get_volume(), get_time());
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
}
