#ifdef __linux__
// #define _POSIX_C_SOURCE 200809L
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <math.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/soundcard.h>
#include <time.h>
#include <unistd.h>

#define ALSA_VOLUME(channel)                                                    \
    err = snd_mixer_selem_get_##channel##_dB_range(elem, &min, &max) ||         \
          snd_mixer_selem_get_##channel##_dB(elem, 0, &val);                    \
    if (err != 0 || min >= max) {                                               \
        err = snd_mixer_selem_get_##channel##_volume_range(elem, &min, &max) || \
              snd_mixer_selem_get_##channel##_volume(elem, 0, &val);            \
        force_linear = true;                                                    \
    }
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
    time_t t = time(NULL);
    struct tm tm_struct;
    localtime_r(&t, &tm_struct);
    strftime(time_str, STR_LEN * sizeof(char), "%m-%d %H:%M", &tm_struct);
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

#elif defined(__linux__)
    int err;
    snd_mixer_t *m;

    if ((err = snd_mixer_open(&m, 0)) < 0) {
        warn("ALSA: Cannot open mixer: %s\n", snd_strerror(err));
        // exit(1);
    }

    const char *card = "default";
    // Attach this mixer handle to the given device
    if ((err = snd_mixer_attach(m, card)) < 0) {
        warn("ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        // exit(1);
    }

    // Register this mixer
    if ((err = snd_mixer_selem_register(m, NULL, NULL)) < 0) {
        warn("ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        // exit(1);
    }

    if ((err = snd_mixer_load(m)) < 0) {
        warn("ALSA: snd_mixer_load: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        // exit(1);
    }

    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_malloc(&sid);
    if (sid == NULL) {
        snd_mixer_close(m);
        // exit(1);
    }

    // Find the given mixer
    unsigned selem_index = 0U;
    const char *selem_name = "Master";
    snd_mixer_selem_id_set_index(sid, selem_index);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t *elem;
    if (!(elem = snd_mixer_find_selem(m, sid))) {
        warn("i3status: ALSA: Cannot find mixer %s (index %u)\n",
             snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
        snd_mixer_close(m);
        snd_mixer_selem_id_free(sid);
        // exit(1);
    }

    // NOTE: or can find first active
    // snd_mixer_elem_t *elem;
    // snd_mixer_elem_t *found_elem = NULL;
    // for (elem = snd_mixer_first_elem(m); elem; elem = snd_mixer_elem_next(elem)) {
    //     if (snd_mixer_selem_has_playback_volume(elem)) {
    //         found_elem = elem;
    //         break;
    //     }
    // }
    // if (!found_elem) {
    //     warn("ALSA: Cannot find any element with playback volume.\n");
    //     // exit(1);
    // }
    // elem = found_elem;

    // Get the volume range to convert the volume later
    long min, max, val;
    bool force_linear = false;
    snd_mixer_handle_events(m);
    if (!strncasecmp(selem_name, "capture", strlen("capture"))) {
        ALSA_VOLUME(capture)
    } else {
        ALSA_VOLUME(playback)
    }

    if (err != 0) {
        warn("ALSA: Cannot get playback volume.\n");
        snprintf(volume_str, STR_LEN, "Error");
        goto cleanup;
    }

    const char *mixer_name = snd_mixer_selem_get_name(elem);
    if (!mixer_name) {
        warn("ALSA: NULL mixer_name.\n");
        // exit(1);
    }

    // Use linear mapping for raw register values or small ranges of 24 dB
    int avg;
    const long MAX_LINEAR_DB_SCALE = 24;
    if (force_linear || max - min <= MAX_LINEAR_DB_SCALE * 100) {
        double avgf = ((double)(val - min) / (double)(max - min)) * 100;
        avg = (int)lround(avgf);
        avg = (avgf - avg < 0.5 ? avg : (avg + 1));
    } else {
        // mapped volume to be more natural for the human ear
        double normalized = pow(10.0, (double)(val - max) / 6000.0);
        if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
            double min_norm = pow(10.0, (double)(min - max) / 6000.0);
            normalized = (normalized - min_norm) / (1 - min_norm);
        }
        avg = (int)lround(normalized * 100);
    }

    snprintf(volume_str, STR_LEN, "%d%%", avg);

cleanup:
    snd_mixer_close(m);
    snd_mixer_selem_id_free(sid);
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

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigusr1;
    sigaction(SIGUSR1, &action, NULL);

    struct timespec sleep_time = {.tv_sec = 5, .tv_nsec = 0};
    while (running) {
        printf("%s | %s | %s\n", get_cpu_usage(), get_volume(), get_time());
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
}
