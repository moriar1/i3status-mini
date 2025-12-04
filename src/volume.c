#include "config.h"
#include <err.h>

#ifdef __linux__
#include <stdbool.h>
#include <math.h>
#include <strings.h>
#include <alsa/asoundlib.h>
#elif defined(__FreeBSD__)
#include <sys/soundcard.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#endif

#ifdef __linux__
#define ALSA_VOLUME(channel)                                                    \
    err = snd_mixer_selem_get_##channel##_dB_range(elem, &min, &max) ||         \
          snd_mixer_selem_get_##channel##_dB(elem, 0, &val);                    \
    if (err != 0 || min >= max) {                                               \
        err = snd_mixer_selem_get_##channel##_volume_range(elem, &min, &max) || \
              snd_mixer_selem_get_##channel##_volume(elem, 0, &val);            \
        force_linear = true;                                                    \
    }
#endif

static void get_volume_impl(char[static STR_LEN]);

void* get_volume(void* volume_str) {
    get_volume_impl((char*)volume_str);
    return NULL;
}

static void get_volume_impl(char volume_str[static STR_LEN]) {
#ifdef __FreeBSD__
    const char mixer_path[] = "/dev/mixer";
    int mixfd = open(mixer_path, O_RDONLY);

    if (mixfd == -1) {
        warn("OSS: Cannot open mixer");
        snprintf(volume_str, STR_LEN, "No volume");
        return;
    }

    unsigned vol;
    if (ioctl(mixfd, SOUND_MIXER_READ_VOLUME, &vol) == -1) {
        warn("OSS: Cannot read mixer information");
        vol = 0U;
    }
    close(mixfd);

    vol &= 0x7fU;
    snprintf(volume_str, STR_LEN, "%u%%", vol);

// TODO: check memory leaks in error checking
#elif defined(__linux__)
    int err;
    snd_mixer_t *m;

    if ((err = snd_mixer_open(&m, 0)) < 0) {
        warn("ALSA: Cannot open mixer: %s\n", snd_strerror(err));
        return;
    }

    const char *card = "default";
    // Attach this mixer handle to the given device
    if ((err = snd_mixer_attach(m, card)) < 0) {
        warn("ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        return;
    }

    // Register this mixer
    if ((err = snd_mixer_selem_register(m, NULL, NULL)) < 0) {
        warn("ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        return;
    }

    if ((err = snd_mixer_load(m)) < 0) {
        warn("ALSA: snd_mixer_load: %s\n", snd_strerror(err));
        snd_mixer_close(m);
        return;
    }

    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_malloc(&sid);
    if (sid == NULL) {
        snd_mixer_close(m);
        return;
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
        return;
    }

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
        return;
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
}
