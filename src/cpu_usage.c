#include "config.h"
#include <err.h>
#include <stdio.h>

#ifdef __linux__
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <string.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

struct cpu_usage {
#if defined(__FreeBSD__)
    long user;
    long nice;
    long system;
    long idle;
    long total;
#else
    int user;
    int nice;
    int system;
    int idle;
    int total;
#endif
};

#if defined(__linux__)
static int cpu_count = 0;
static struct cpu_usage *prev_cpus = NULL;
static struct cpu_usage *curr_cpus = NULL;
#endif
static struct cpu_usage prev_all = {0, 0, 0, 0, 0};

void get_cpu_usage(void *restrict cpu_str_) {
    char *cpu_str = (char *)cpu_str_;
    struct cpu_usage curr_all = {0, 0, 0, 0, 0};

#ifdef __FreeBSD__
    long cp_time[CPUSTATES];
    size_t size = sizeof cp_time;
    if (sysctlbyname("kern.cp_time", &cp_time, &size, NULL, 0) < 0) {
        warn("Failed get kern.cp_time");
        snprintf(cpu_str, STR_LEN, "No cpu");
        return;
    }

    curr_all.user = cp_time[CP_USER];
    curr_all.nice = cp_time[CP_NICE];
    curr_all.system = cp_time[CP_SYS];
    curr_all.idle = cp_time[CP_IDLE];
    curr_all.total = curr_all.user + curr_all.nice + curr_all.system + curr_all.idle;
    long diff_idle = curr_all.idle - prev_all.idle;
    long diff_total = curr_all.total - prev_all.total;
    int diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
    snprintf(cpu_str, STR_LEN, "%d%%", diff_usage);

#elif defined(__linux__)
    int curr_cpu_count = get_nprocs_conf();
    if (curr_cpu_count != cpu_count) {
        cpu_count = curr_cpu_count;
        free(prev_cpus);
        prev_cpus = (struct cpu_usage *)calloc((unsigned long)cpu_count, sizeof(struct cpu_usage));
        free(curr_cpus);
        curr_cpus = (struct cpu_usage *)calloc((unsigned long)cpu_count, sizeof(struct cpu_usage));
    }

    memcpy(curr_cpus, prev_cpus, (unsigned long)cpu_count * sizeof(struct cpu_usage));
    FILE *f = fopen("/proc/stat", "r");
    if (f == NULL) {
        warn("i3status: open %s\n", "/proc/stat");
        snprintf(cpu_str, STR_LEN, "No cpu");
        return;
    }
    curr_cpu_count = get_nprocs();
    char line[4096];

    /* Discard first line (cpu ), start at second line (cpu0) */
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        /* unexpected EOF or read error */
        snprintf(cpu_str, STR_LEN, "No cpu");
        return;
    }

    for (int idx = 0; idx < curr_cpu_count; ++idx) {
        if (fgets(line, sizeof(line), f) == NULL) {
            fclose(f);
            /* unexpected EOF or read error */
            snprintf(cpu_str, STR_LEN, "No cpu");
            return;
        }
        int cpu_idx, user, nice, system, idle;
        if (sscanf(line, "cpu%d %d %d %d %d", &cpu_idx, &user, &nice, &system, &idle) != 5) {
            fclose(f);
            snprintf(cpu_str, STR_LEN, "No cpu");
            return;
        }
        if (cpu_idx < 0 || cpu_idx >= cpu_count) {
            fclose(f);
            snprintf(cpu_str, STR_LEN, "No cpu");
            return;
        }
        curr_cpus[cpu_idx].user = user;
        curr_cpus[cpu_idx].nice = nice;
        curr_cpus[cpu_idx].system = system;
        curr_cpus[cpu_idx].idle = idle;
        curr_cpus[cpu_idx].total = user + nice + system + idle;
    }
    fclose(f);
    for (int cpu_idx = 0; cpu_idx < cpu_count; cpu_idx++) {
        curr_all.user += curr_cpus[cpu_idx].user;
        curr_all.nice += curr_cpus[cpu_idx].nice;
        curr_all.system += curr_cpus[cpu_idx].system;
        curr_all.idle += curr_cpus[cpu_idx].idle;
        curr_all.total += curr_cpus[cpu_idx].total;
    }

    int diff_idle = curr_all.idle - prev_all.idle;
    int diff_total = curr_all.total - prev_all.total;
    int diff_usage = (diff_total ? (1000 * (diff_total - diff_idle) / diff_total + 5) / 10 : 0);
    prev_all = curr_all;
    snprintf(cpu_str, STR_LEN, "%d%%", diff_usage);

#else
    snprintf(cpu_str, STR_LEN, "No cpu");
#endif
    return;
}
